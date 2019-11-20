/*
 * This is an implementation of a tty driver for a USB acm moden device in
 * linux, used for a serial communication with an Arduino.
 * Anything of this driver is made from zero, all functions are
 * already implemented in the linux kernel and are just reduced or deleted
 * for the usage purposes.
 * The original driver had the following terms
 * SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (c) 1999 Armin Fuerst  <fuerst@in.tum.de>
 * Copyright (c) 1999 Pavel Machek  <pavel@ucw.cz>
 * Copyright (c) 1999 Johannes Erdfelt  <johannes@erdfelt.com>
 * Copyright (c) 2000 Vojtech Pavlik    <vojtech@suse.cz>
 * Copyright (c) 2004 Oliver Neukum <oliver@neukum.name>
 * Copyright (c) 2005 David Kubicek <dave@awk.cz>
 * Copyright (c) 2011 Johan Hovold  <jhovold@gmail.com>
 *
 * USB Abstract Control Model driver for USB modems and ISDN adapters
 *
 * Sponsored by SuSE
 * 
 * The reduction is made out of a real driver found at :
 * https://elixir.bootlin.com/linux/latest/source/drivers/usb/class/cdc-acm.c
 * or at Linus Torvalds github repository at:
 * https://github.com/torvalds/linux/blob/master/drivers/usb/class/cdc-acm.c
 */

#undef DEBUG
#undef VERBOSE_DEBUG

#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/idr.h>
#include <linux/list.h>

#include "arduino.h"

static struct usb_driver arduino_driver;
static struct tty_driver *arduino_tty_driver;

static DEFINE_IDR(arduino_minors);
static DEFINE_MUTEX(arduino_minors_lock);

static void arduino_tty_set_termios(struct tty_struct *tty,
                                struct ktermios *termios_old);

/*
 * arduino_minors accessors
 */

/*
 * Look up an ARDUINO structure by minor. If found and not disconnected, increment
 * its refcount and return it with its mutex held.
 */
static struct arduino *arduino_get_by_minor(unsigned int minor) {
    //printk(KERN_INFO "Arduino Driver: arduino_get_by_minor arduino module\n");

    struct arduino *arduino;

    mutex_lock(&arduino_minors_lock);
    arduino = idr_find(&arduino_minors, minor);
    if (arduino) {
        mutex_lock(&arduino->mutex);
        if (arduino->disconnected) {
            mutex_unlock(&arduino->mutex);
            arduino = NULL;
        } else {
            tty_port_get(&arduino->port);
            mutex_unlock(&arduino->mutex);
        }
    }
    mutex_unlock(&arduino_minors_lock);
    return arduino;
}

/*
 * Try to find an available minor number and if found, associate it with 'arduino'.
 */
static int arduino_alloc_minor(struct arduino *arduino) {    
    //printk(KERN_INFO "Arduino Driver: arduino_alloc_minor arduino module\n");

    int minor;

    mutex_lock(&arduino_minors_lock);
    minor = idr_alloc(&arduino_minors, arduino, 0, ARDUINO_TTY_MINORS, GFP_KERNEL);
    mutex_unlock(&arduino_minors_lock);

    return minor;
}

/* Release the minor number associated with 'arduino'.  */
static void arduino_release_minor(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_release_minor arduino module\n");
    mutex_lock(&arduino_minors_lock);
    idr_remove(&arduino_minors, arduino->minor);
    mutex_unlock(&arduino_minors_lock);
}

/*
 * Functions for ARDUINO control messages.
 */

static int arduino_ctrl_msg(struct arduino *arduino, int request, int value,
                        void *buf, int len) {
    //printk(KERN_INFO "Arduino Driver: arduino_ctrl_msg arduino module\n");
    int retval;

    retval = usb_autopm_get_interface(arduino->control);
    if (retval)
        return retval;

    retval = usb_control_msg(arduino->dev, usb_sndctrlpipe(arduino->dev, 0),
                             request, USB_RT_ARDUINO, value,
                             arduino->control->altsetting[0].desc.bInterfaceNumber,
                             buf, len, 5000);

    usb_autopm_put_interface(arduino->control);

    return retval < 0 ? retval : 0;
}

/* devices aren't required to support these requests.
 * the cdc arduino descriptor tells whether they do...
 */
static inline int arduino_set_control(struct arduino *arduino, int control) {
    //printk(KERN_INFO "Arduino Driver: arduino_set_control arduino module\n");

    if (arduino->quirks & QUIRK_CONTROL_LINE_STATE)
        return -EOPNOTSUPP;

    return arduino_ctrl_msg(arduino, USB_CDC_REQ_SET_CONTROL_LINE_STATE,
                        control, NULL, 0);
}

#define arduino_set_line(arduino, line) \
    arduino_ctrl_msg(arduino, USB_CDC_REQ_SET_LINE_CODING, 0, line, sizeof *(line))
#define arduino_send_break(arduino, ms) \
    arduino_ctrl_msg(arduino, USB_CDC_REQ_SEND_BREAK, ms, NULL, 0)

static void arduino_kill_urbs(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_kill_urbs arduino module\n");
    int i;

    usb_kill_urb(arduino->ctrlurb);
    for (i = 0; i < ARDUINO_NW; i++)
        usb_kill_urb(arduino->wb[i].urb);
    for (i = 0; i < arduino->rx_buflimit; i++)
        usb_kill_urb(arduino->read_urbs[i]);
}

/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */

static int arduino_wb_alloc(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_wb_alloc arduino module\n");
    int i, wbn;
    struct arduino_wb *wb;

    wbn = 0;
    i = 0;
    for (;;) {
        wb = &arduino->wb[wbn];
        if (!wb->use) {
            wb->use = 1;
            return wbn;
        }
        wbn = (wbn + 1) % ARDUINO_NW;
        if (++i >= ARDUINO_NW)
            return -1;
    }
}

static int arduino_wb_is_avail(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_wb_is_avail arduino module\n");

    int i, n;
    unsigned long flags;

    n = ARDUINO_NW;
    spin_lock_irqsave(&arduino->write_lock, flags);
    for (i = 0; i < ARDUINO_NW; i++)
        n -= arduino->wb[i].use;
    spin_unlock_irqrestore(&arduino->write_lock, flags);
    return n;
}

/*
 * Finish write. Caller must hold arduino->write_lock
 */
static void arduino_write_done(struct arduino *arduino, struct arduino_wb *wb) {
    //printk(KERN_INFO "Arduino Driver: arduino_write_done arduino module\n");
    wb->use = 0;
    arduino->transmitting--;
    usb_autopm_put_interface_async(arduino->control);
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */

static int arduino_start_wb(struct arduino *arduino, struct arduino_wb *wb) {
    //printk(KERN_INFO "Arduino Driver: arduino_start_wb arduino module\n");
    int rc;

    arduino->transmitting++;

    wb->urb->transfer_buffer = wb->buf;
    wb->urb->transfer_dma = wb->dmah;
    wb->urb->transfer_buffer_length = wb->len;
    wb->urb->dev = arduino->dev;

    rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
    if (rc < 0) {
        arduino_write_done(arduino, wb);
    }
    return rc;
}

/*
 * attributes exported through sysfs
 */
static ssize_t show_caps(struct device *dev, struct device_attribute *attr, char *buf) {
    //printk(KERN_INFO "Arduino Driver: show_caps arduino module\n");
    struct usb_interface *intf = to_usb_interface(dev);
    struct arduino *arduino = usb_get_intfdata(intf);

    return sprintf(buf, "%d", arduino->ctrl_caps);
}
static DEVICE_ATTR(bmCapabilities, S_IRUGO, show_caps, NULL);

static void arduino_process_notification(struct arduino *arduino, unsigned char *buf) {
    //printk(KERN_INFO "Arduino Driver: arduino_process_notification arduino module\n");
    int newctrl;
    int difference;
    struct usb_cdc_notification *dr = (struct usb_cdc_notification *)buf;
    unsigned char *data = buf + sizeof(struct usb_cdc_notification);

    switch (dr->bNotificationType) {
    case USB_CDC_NOTIFY_NETWORK_CONNECTION:
        break;

    case USB_CDC_NOTIFY_SERIAL_STATE:
        if (le16_to_cpu(dr->wLength) != 2) {
            break;
        }

        newctrl = get_unaligned_le16(data);

        if (!arduino->clocal && (arduino->ctrlin & ~newctrl & ARDUINO_CTRL_DCD)) {
            tty_port_tty_hangup(&arduino->port, false);
        }

        difference = arduino->ctrlin ^ newctrl;
        spin_lock(&arduino->read_lock);
        arduino->ctrlin = newctrl;
        arduino->oldcount = arduino->iocount;

        if (difference & ARDUINO_CTRL_DSR)
            arduino->iocount.dsr++;
        if (difference & ARDUINO_CTRL_BRK)
            arduino->iocount.brk++;
        if (difference & ARDUINO_CTRL_RI)
            arduino->iocount.rng++;
        if (difference & ARDUINO_CTRL_DCD)
            arduino->iocount.dcd++;
        if (difference & ARDUINO_CTRL_FRAMING)
            arduino->iocount.frame++;
        if (difference & ARDUINO_CTRL_PARITY)
            arduino->iocount.parity++;
        if (difference & ARDUINO_CTRL_OVERRUN)
            arduino->iocount.overrun++;
        spin_unlock(&arduino->read_lock);

        if (difference)
            wake_up_all(&arduino->wioctl);

        break;

    default:
        break;
    }
}

/* control interface reports status changes with "interrupt" transfers */
static void arduino_ctrl_irq(struct urb *urb) {
    //printk(KERN_INFO "Arduino Driver: arduino_ctrl_irq arduino module\n");
    struct arduino *arduino = urb->context;
    struct usb_cdc_notification *dr = urb->transfer_buffer;
    unsigned int current_size = urb->actual_length;
    unsigned int expected_size, copy_size, alloc_size;
    int retval;
    int status = urb->status;

    switch (status) {
    case 0:

        /* success */
        break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        /* this urb is terminated, clean up */
        arduino->nb_index = 0;
        return;
    default:
        goto exit;
    }

    usb_mark_last_busy(arduino->dev);

    if (arduino->nb_index)
        dr = (struct usb_cdc_notification *)arduino->notification_buffer;

    /* size = notification-header + (optional) data */
    expected_size = sizeof(struct usb_cdc_notification) +
                    le16_to_cpu(dr->wLength);

    if (current_size < expected_size) {
        /* notification is transmitted fragmented, reassemble */
        if (arduino->nb_size < expected_size) {
            if (arduino->nb_size) {
                kfree(arduino->notification_buffer);
                arduino->nb_size = 0;
            }
            alloc_size = roundup_pow_of_two(expected_size);
            /*
             * kmalloc ensures a valid notification_buffer after a
             * use of kfree in case the previous allocation was too
             * small. Final freeing is done on disconnect.
             */
            arduino->notification_buffer =
                kmalloc(alloc_size, GFP_ATOMIC);
            if (!arduino->notification_buffer)
                goto exit;
            arduino->nb_size = alloc_size;
        }

        copy_size = min(current_size,
                        expected_size - arduino->nb_index);

        memcpy(&arduino->notification_buffer[arduino->nb_index],
               urb->transfer_buffer, copy_size);
        arduino->nb_index += copy_size;
        current_size = arduino->nb_index;
    }

    if (current_size >= expected_size) {
        /* notification complete */
        arduino_process_notification(arduino, (unsigned char *)dr);
        arduino->nb_index = 0;
    }

exit:
    retval = usb_submit_urb(urb, GFP_ATOMIC);
}

static int arduino_submit_read_urb(struct arduino *arduino, int index, gfp_t mem_flags) {
    //printk(KERN_INFO "Arduino Driver: arduino_submit_read_urb arduino module\n");
    int res;

    if (!test_and_clear_bit(index, &arduino->read_urbs_free))
        return 0;

    res = usb_submit_urb(arduino->read_urbs[index], mem_flags);
    if (res) {
        set_bit(index, &arduino->read_urbs_free);
        return res;
    } 

    return 0;
}

static int arduino_submit_read_urbs(struct arduino *arduino, gfp_t mem_flags) {
    //printk(KERN_INFO "Arduino Driver: arduino_submit_read_urbs arduino module\n");
    int res;
    int i;

    for (i = 0; i < arduino->rx_buflimit; ++i) {
        res = arduino_submit_read_urb(arduino, i, mem_flags);
        if (res)
            return res;
    }

    return 0;
}

static void arduino_process_read_urb(struct arduino *arduino, struct urb *urb) {
    //printk(KERN_INFO "Arduino Driver: arduino_process_read_urb arduino module\n");
    if (!urb->actual_length)
        return;

    tty_insert_flip_string(&arduino->port, urb->transfer_buffer,
                           urb->actual_length);
    tty_flip_buffer_push(&arduino->port);
}

static void arduino_read_bulk_callback(struct urb *urb) {
    //printk(KERN_INFO "Arduino Driver: arduino_read_bulk_callback arduino module\n");
    struct arduino_rb *rb = urb->context;
    struct arduino *arduino = rb->instance;
    unsigned long flags;
    int status = urb->status;

    set_bit(rb->index, &arduino->read_urbs_free);

    if (!arduino->dev) {
        return;
    }

    switch (status) {
    case 0:
        usb_mark_last_busy(arduino->dev);
        arduino_process_read_urb(arduino, urb);
        break;
    case -EPIPE:
        set_bit(EVENT_RX_STALL, &arduino->flags);
        schedule_work(&arduino->work);
        return;
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        return;
    default:
        break;
    }

    /* 
     * Unthrottle may run on another CPU which needs to see events
     * in the same order. Submission has an implict barrier
     */
    smp_mb__before_atomic();

    /* throttle device if requested by tty */
    spin_lock_irqsave(&arduino->read_lock, flags);
    arduino->throttled = arduino->throttle_req;
    if (!arduino->throttled) {
        spin_unlock_irqrestore(&arduino->read_lock, flags);
        arduino_submit_read_urb(arduino, rb->index, GFP_ATOMIC);
    } else {
        spin_unlock_irqrestore(&arduino->read_lock, flags);
    }
}

/* data interface wrote those outgoing bytes */
static void arduino_write_bulk(struct urb *urb) {
    //printk(KERN_INFO "Arduino Driver: arduino_write_bulk arduino module\n");
    struct arduino_wb *wb = urb->context;
    struct arduino *arduino = wb->instance;
    unsigned long flags;


    spin_lock_irqsave(&arduino->write_lock, flags);
    arduino_write_done(arduino, wb);
    spin_unlock_irqrestore(&arduino->write_lock, flags);
    set_bit(EVENT_TTY_WAKEUP, &arduino->flags);
    schedule_work(&arduino->work);
}

static void arduino_softint(struct work_struct *work) {
    //printk(KERN_INFO "Arduino Driver: arduino_softint arduino module\n");
    int i;
    struct arduino *arduino = container_of(work, struct arduino, work);

    if (test_bit(EVENT_RX_STALL, &arduino->flags)) {
        if (!(usb_autopm_get_interface(arduino->data))) {
            for (i = 0; i < arduino->rx_buflimit; i++)
                usb_kill_urb(arduino->read_urbs[i]);
            usb_clear_halt(arduino->dev, arduino->in);
            arduino_submit_read_urbs(arduino, GFP_KERNEL);
            usb_autopm_put_interface(arduino->data);
        }
        clear_bit(EVENT_RX_STALL, &arduino->flags);
    }

    if (test_bit(EVENT_TTY_WAKEUP, &arduino->flags)) {
        tty_port_tty_wakeup(&arduino->port);
        clear_bit(EVENT_TTY_WAKEUP, &arduino->flags);
    }
}

/*
 * TTY handlers
 */

static int arduino_tty_install(struct tty_driver *driver, struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_install arduino module\n");
    struct arduino *arduino;
    int retval;

    arduino = arduino_get_by_minor(tty->index);
    if (!arduino)
        return -ENODEV;

    retval = tty_standard_install(driver, tty);
    if (retval)
        goto error_init_termios;

    tty->driver_data = arduino;

    return 0;

error_init_termios:
    tty_port_put(&arduino->port);
    return retval;
}

static int arduino_tty_open(struct tty_struct *tty, struct file *filp) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_open arduino module\n");

    struct arduino *arduino = tty->driver_data;

    return tty_port_open(&arduino->port, tty, filp);
}

static void arduino_port_dtr_rts(struct tty_port *port, int raise) {
    //printk(KERN_INFO "Arduino Driver: arduino_port_dtr_rts arduino module\n");
    struct arduino *arduino = container_of(port, struct arduino, port);
    int val;
    int res;

    if (raise)
        val = ARDUINO_CTRL_DTR | ARDUINO_CTRL_RTS;
    else
        val = 0;

    /* FIXME: add missing ctrlout locking throughout driver */
    arduino->ctrlout = val;

    res = arduino_set_control(arduino, val);
}

static int arduino_port_activate(struct tty_port *port, struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_port_activate arduino module\n");
    struct arduino *arduino = container_of(port, struct arduino, port);
    int retval = -ENODEV;
    int i;

    mutex_lock(&arduino->mutex);
    if (arduino->disconnected)
        goto disconnected;

    retval = usb_autopm_get_interface(arduino->control);
    if (retval)
        goto error_get_interface;

    /*
     * FIXME: Why do we need this? Allocating 64K of physically contiguous
     * memory is really nasty...
     */
    set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
    arduino->control->needs_remote_wakeup = 1;

    arduino->ctrlurb->dev = arduino->dev;
    retval = usb_submit_urb(arduino->ctrlurb, GFP_KERNEL);
    if (retval) {
        goto error_submit_urb;
    }

    arduino_tty_set_termios(tty, NULL);

    /*
     * Unthrottle device in case the TTY was closed while throttled.
     */
    spin_lock_irq(&arduino->read_lock);
    arduino->throttled = 0;
    arduino->throttle_req = 0;
    spin_unlock_irq(&arduino->read_lock);

    retval = arduino_submit_read_urbs(arduino, GFP_KERNEL);
    if (retval)
        goto error_submit_read_urbs;

    usb_autopm_put_interface(arduino->control);

    mutex_unlock(&arduino->mutex);

    return 0;

error_submit_read_urbs:
    for (i = 0; i < arduino->rx_buflimit; i++)
        usb_kill_urb(arduino->read_urbs[i]);
    usb_kill_urb(arduino->ctrlurb);
error_submit_urb:
    usb_autopm_put_interface(arduino->control);
error_get_interface:
disconnected:
    mutex_unlock(&arduino->mutex);

    return usb_translate_errors(retval);
}

static void arduino_port_destruct(struct tty_port *port) {
    //printk(KERN_INFO "Arduino Driver: arduino_port_destruct arduino module\n");
    struct arduino *arduino = container_of(port, struct arduino, port);

    arduino_release_minor(arduino);
    usb_put_intf(arduino->control);
    kfree(arduino);
}

static void arduino_port_shutdown(struct tty_port *port) {
    //printk(KERN_INFO "Arduino Driver: arduino_port_shutdown arduino module\n");
    struct arduino *arduino = container_of(port, struct arduino, port);
    struct urb *urb;
    struct arduino_wb *wb;

    /*
     * Need to grab write_lock to prevent race with resume, but no need to
     * hold it due to the tty-port initialised flag.
     */
    spin_lock_irq(&arduino->write_lock);
    spin_unlock_irq(&arduino->write_lock);

    usb_autopm_get_interface_no_resume(arduino->control);
    arduino->control->needs_remote_wakeup = 0;
    usb_autopm_put_interface(arduino->control);

    for (;;) {
        urb = usb_get_from_anchor(&arduino->delayed);
        if (!urb)
            break;
        wb = urb->context;
        wb->use = 0;
        usb_autopm_put_interface_async(arduino->control);
    }

    arduino_kill_urbs(arduino);
}

static void arduino_tty_cleanup(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_cleanup arduino module\n");
    struct arduino *arduino = tty->driver_data;

    tty_port_put(&arduino->port);
}

static void arduino_tty_close(struct tty_struct *tty, struct file *filp) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_close arduino module\n");
    struct arduino *arduino = tty->driver_data;

    tty_port_close(&arduino->port, tty, filp);
}

static int arduino_tty_write(struct tty_struct *tty,
                         const unsigned char *buf, int count) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_write arduino module\n");
    struct arduino *arduino = tty->driver_data;
    int stat;
    unsigned long flags;
    int wbn;
    struct arduino_wb *wb;

    if (!count)
        return 0;

    spin_lock_irqsave(&arduino->write_lock, flags);
    wbn = arduino_wb_alloc(arduino);
    if (wbn < 0) {
        spin_unlock_irqrestore(&arduino->write_lock, flags);
        return 0;
    }
    wb = &arduino->wb[wbn];

    if (!arduino->dev) {
        wb->use = 0;
        spin_unlock_irqrestore(&arduino->write_lock, flags);
        return -ENODEV;
    }

    count = (count > arduino->writesize) ? arduino->writesize : count;
    memcpy(wb->buf, buf, count);
    wb->len = count;

    stat = usb_autopm_get_interface_async(arduino->control);
    if (stat) {
        wb->use = 0;
        spin_unlock_irqrestore(&arduino->write_lock, flags);
        return stat;
    }

    if (arduino->susp_count) {
        if (arduino->putbuffer) {
            /* now to preserve order */
            usb_anchor_urb(arduino->putbuffer->urb, &arduino->delayed);
            arduino->putbuffer = NULL;
        }
        usb_anchor_urb(wb->urb, &arduino->delayed);
        spin_unlock_irqrestore(&arduino->write_lock, flags);
        return count;
    } else {
        if (arduino->putbuffer) {
            /* at this point there is no good way to handle errors */
            arduino_start_wb(arduino, arduino->putbuffer);
            arduino->putbuffer = NULL;
        }
    }

    stat = arduino_start_wb(arduino, wb);
    spin_unlock_irqrestore(&arduino->write_lock, flags);

    if (stat < 0)
        return stat;
    return count;
}

static void arduino_tty_flush_chars(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_flush_chars arduino module\n");
    struct arduino *arduino = tty->driver_data;
    struct arduino_wb *cur = arduino->putbuffer;
    int err;
    unsigned long flags;

    if (!cur) /* nothing to do */
        return;

    arduino->putbuffer = NULL;
    err = usb_autopm_get_interface_async(arduino->control);
    spin_lock_irqsave(&arduino->write_lock, flags);
    if (err < 0) {
        cur->use = 0;
        arduino->putbuffer = cur;
        goto out;
    }

    if (arduino->susp_count)
        usb_anchor_urb(cur->urb, &arduino->delayed);
    else
        arduino_start_wb(arduino, cur);
out:
    spin_unlock_irqrestore(&arduino->write_lock, flags);
    return;
}

static int arduino_tty_put_char(struct tty_struct *tty, unsigned char ch) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_put_char arduino module\n");
    struct arduino *arduino = tty->driver_data;
    struct arduino_wb *cur;
    int wbn;
    unsigned long flags;

overflow:
    cur = arduino->putbuffer;
    if (!cur) {
        spin_lock_irqsave(&arduino->write_lock, flags);
        wbn = arduino_wb_alloc(arduino);
        if (wbn >= 0) {
            cur = &arduino->wb[wbn];
            arduino->putbuffer = cur;
        }
        spin_unlock_irqrestore(&arduino->write_lock, flags);
        if (!cur)
            return 0;
    }

    if (cur->len == arduino->writesize) {
        arduino_tty_flush_chars(tty);
        goto overflow;
    }

    cur->buf[cur->len++] = ch;
    return 1;
}

static int arduino_tty_write_room(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_write_room arduino module\n");
    struct arduino *arduino = tty->driver_data;
    /*
     * Do not let the line discipline to know that we have a reserve,
     * or it might get too enthusiastic.
     */
    return arduino_wb_is_avail(arduino) ? arduino->writesize : 0;
}

static int arduino_tty_chars_in_buffer(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_chars_in_buffer arduino module\n");
    struct arduino *arduino = tty->driver_data;
    /*
     * if the device was unplugged then any remaining characters fell out
     * of the connector ;)
     */
    if (arduino->disconnected)
        return 0;
    /*
     * This is inaccurate (overcounts), but it works.
     */
    return (ARDUINO_NW - arduino_wb_is_avail(arduino)) * arduino->writesize;
}

static int arduino_tty_tiocmget(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_tiocmget arduino module\n");
    struct arduino *arduino = tty->driver_data;

    return (arduino->ctrlout & ARDUINO_CTRL_DTR ? TIOCM_DTR : 0) |
           (arduino->ctrlout & ARDUINO_CTRL_RTS ? TIOCM_RTS : 0) |
           (arduino->ctrlin  & ARDUINO_CTRL_DSR ? TIOCM_DSR : 0) |
           (arduino->ctrlin  & ARDUINO_CTRL_RI  ? TIOCM_RI  : 0) |
           (arduino->ctrlin  & ARDUINO_CTRL_DCD ? TIOCM_CD  : 0) |
           TIOCM_CTS;
}

static int arduino_tty_tiocmset(struct tty_struct *tty,
                            unsigned int set, unsigned int clear) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_tiocmset arduino module\n");
    struct arduino *arduino = tty->driver_data;
    unsigned int newctrl;

    newctrl = arduino->ctrlout;
    set = (set & TIOCM_DTR ? ARDUINO_CTRL_DTR : 0) |
          (set & TIOCM_RTS ? ARDUINO_CTRL_RTS : 0);
    clear = (clear & TIOCM_DTR ? ARDUINO_CTRL_DTR : 0) |
            (clear & TIOCM_RTS ? ARDUINO_CTRL_RTS : 0);

    newctrl = (newctrl & ~clear) | set;

    if (arduino->ctrlout == newctrl)
        return 0;
    return arduino_set_control(arduino, arduino->ctrlout = newctrl);
}

static int get_serial_info(struct arduino *arduino, struct serial_struct __user *info) {
    //printk(KERN_INFO "Arduino Driver: get_serial_info arduino module\n");
    struct serial_struct tmp;

    memset(&tmp, 0, sizeof(tmp));
    tmp.xmit_fifo_size = arduino->writesize;
    tmp.baud_base = le32_to_cpu(arduino->line.dwDTERate);
    tmp.close_delay = arduino->port.close_delay / 10;
    tmp.closing_wait = arduino->port.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
                       ASYNC_CLOSING_WAIT_NONE :
                       arduino->port.closing_wait / 10;

    if (copy_to_user(info, &tmp, sizeof(tmp)))
        return -EFAULT;
    else
        return 0;
}

static int set_serial_info(struct arduino *arduino,
                           struct serial_struct __user *newinfo) {
    //printk(KERN_INFO "Arduino Driver: set_serial_info arduino module\n");
    struct serial_struct new_serial;
    unsigned int closing_wait, close_delay;
    int retval = 0;

    if (copy_from_user(&new_serial, newinfo, sizeof(new_serial)))
        return -EFAULT;

    close_delay = new_serial.close_delay * 10;
    closing_wait = new_serial.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
                   ASYNC_CLOSING_WAIT_NONE : new_serial.closing_wait * 10;

    mutex_lock(&arduino->port.mutex);

    if (!capable(CAP_SYS_ADMIN)) {
        if ((close_delay != arduino->port.close_delay) ||
                (closing_wait != arduino->port.closing_wait))
            retval = -EPERM;
        else
            retval = -EOPNOTSUPP;
    } else {
        arduino->port.close_delay  = close_delay;
        arduino->port.closing_wait = closing_wait;
    }

    mutex_unlock(&arduino->port.mutex);
    return retval;
}

static int wait_serial_change(struct arduino *arduino, unsigned long arg) {
    //printk(KERN_INFO "Arduino Driver: wait_serial_change arduino module\n");
    int rv = 0;
    DECLARE_WAITQUEUE(wait, current);
    struct async_icount old, new;

    do {
        spin_lock_irq(&arduino->read_lock);
        old = arduino->oldcount;
        new = arduino->iocount;
        arduino->oldcount = new;
        spin_unlock_irq(&arduino->read_lock);

        if ((arg & TIOCM_DSR) &&
                old.dsr != new.dsr)
            break;
        if ((arg & TIOCM_CD)  &&
                old.dcd != new.dcd)
            break;
        if ((arg & TIOCM_RI) &&
                old.rng != new.rng)
            break;

        add_wait_queue(&arduino->wioctl, &wait);
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        remove_wait_queue(&arduino->wioctl, &wait);
        if (arduino->disconnected) {
            if (arg & TIOCM_CD)
                break;
            else
                rv = -ENODEV;
        } else {
            if (signal_pending(current))
                rv = -ERESTARTSYS;
        }
    } while (!rv);



    return rv;
}

static int arduino_tty_ioctl(struct tty_struct *tty,
                         unsigned int cmd, unsigned long arg) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_ioctl arduino module\n");
    struct arduino *arduino = tty->driver_data;
    int rv = -ENOIOCTLCMD;

    switch (cmd) {
    case TIOCGSERIAL: /* gets serial port data */
        rv = get_serial_info(arduino, (struct serial_struct __user *) arg);
        break;
    case TIOCSSERIAL:
        rv = set_serial_info(arduino, (struct serial_struct __user *) arg);
        break;
    case TIOCMIWAIT:
        rv = usb_autopm_get_interface(arduino->control);
        if (rv < 0) {
            rv = -EIO;
            break;
        }
        rv = wait_serial_change(arduino, arg);
        usb_autopm_put_interface(arduino->control);
        break;
    }

    return rv;
}

static void arduino_tty_set_termios(struct tty_struct *tty,
                                struct ktermios *termios_old) {
    //printk(KERN_INFO "Arduino Driver: arduino_tty_set_termios arduino module\n");
    struct arduino *arduino = tty->driver_data;
    struct ktermios *termios = &tty->termios;
    struct usb_cdc_line_coding newline;
    int newctrl = arduino->ctrlout;

    newline.dwDTERate = cpu_to_le32(tty_get_baud_rate(tty));
    newline.bCharFormat = termios->c_cflag & CSTOPB ? 2 : 0;
    newline.bParityType = termios->c_cflag & PARENB ?
                          (termios->c_cflag & PARODD ? 1 : 2) +
                          (termios->c_cflag & CMSPAR ? 2 : 0) : 0;
    switch (termios->c_cflag & CSIZE) {
    case CS5:
        newline.bDataBits = 5;
        break;
    case CS6:
        newline.bDataBits = 6;
        break;
    case CS7:
        newline.bDataBits = 7;
        break;
    case CS8:
    default:
        newline.bDataBits = 8;
        break;
    }
    /* FIXME: Needs to clear unsupported bits in the termios */
    arduino->clocal = ((termios->c_cflag & CLOCAL) != 0);

    if (C_BAUD(tty) == B0) {
        newline.dwDTERate = arduino->line.dwDTERate;
        newctrl &= ~ARDUINO_CTRL_DTR;
    } else if (termios_old && (termios_old->c_cflag & CBAUD) == B0) {
        newctrl |=  ARDUINO_CTRL_DTR;
    }

    if (newctrl != arduino->ctrlout)
        arduino_set_control(arduino, arduino->ctrlout = newctrl);

    if (memcmp(&arduino->line, &newline, sizeof newline)) {
        memcpy(&arduino->line, &newline, sizeof newline);
        arduino_set_line(arduino, &arduino->line);
    }
}

static const struct tty_port_operations arduino_port_ops = {
    .dtr_rts = arduino_port_dtr_rts,
    .shutdown = arduino_port_shutdown,
    .activate = arduino_port_activate,
    .destruct = arduino_port_destruct,
};

/*
 * USB probe and disconnect routines.
 */

/* Little helpers: write/read buffers free */
static void arduino_write_buffers_free(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_write_buffers_free arduino module\n");
    int i;
    struct arduino_wb *wb;

    for (wb = &arduino->wb[0], i = 0; i < ARDUINO_NW; i++, wb++)
        usb_free_coherent(arduino->dev, arduino->writesize, wb->buf, wb->dmah);
}

static void arduino_read_buffers_free(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_read_buffers_free arduino module\n");
    int i;

    for (i = 0; i < arduino->rx_buflimit; i++)
        usb_free_coherent(arduino->dev, arduino->readsize,
                          arduino->read_buffers[i].base, arduino->read_buffers[i].dma);
}

/* Little helper: write buffers allocate */
static int arduino_write_buffers_alloc(struct arduino *arduino) {
    //printk(KERN_INFO "Arduino Driver: arduino_write_buffers_alloc arduino module\n");
    int i;
    struct arduino_wb *wb;

    for (wb = &arduino->wb[0], i = 0; i < ARDUINO_NW; i++, wb++) {
        wb->buf = usb_alloc_coherent(arduino->dev, arduino->writesize, GFP_KERNEL,
                                     &wb->dmah);
        if (!wb->buf) {
            while (i != 0) {
                --i;
                --wb;
                usb_free_coherent(arduino->dev, arduino->writesize,
                                  wb->buf, wb->dmah);
            }
            return -ENOMEM;
        }
    }
    return 0;
}

static int arduino_probe(struct usb_interface *intf,
                     const struct usb_device_id *id) {
    //printk(KERN_INFO "Arduino Driver: arduino_probe arduino module\n");
    struct usb_cdc_union_desc *union_header = NULL;
    struct usb_cdc_call_mgmt_descriptor *cmgmd = NULL;
    unsigned char *buffer = intf->altsetting->extra;
    int buflen = intf->altsetting->extralen;
    struct usb_interface *control_interface;
    struct usb_interface *data_interface;
    struct usb_endpoint_descriptor *epctrl = NULL;
    struct usb_endpoint_descriptor *epread = NULL;
    struct usb_endpoint_descriptor *epwrite = NULL;
    struct usb_device *usb_dev = interface_to_usbdev(intf);
    struct usb_cdc_parsed_header h;
    struct arduino *arduino;
    int minor;
    int ctrlsize, readsize;
    u8 *buf;
    int call_intf_num = -1;
    int data_intf_num = -1;
    unsigned long quirks;
    int num_rx_buf;
    int i;
    int combined_interfaces = 0;
    struct device *tty_dev;
    int rv = -ENOMEM;

    /* normal quirks */
    quirks = (unsigned long)id->driver_info;

    memset(&h, 0x00, sizeof(struct usb_cdc_parsed_header));

    num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ARDUINO_NR;

    /* normal probing*/
    if (!buffer) {
        return -EINVAL;
    }

    if (!intf->cur_altsetting)
        return -EINVAL;

    if (!buflen) {
        if (intf->cur_altsetting->endpoint &&
                intf->cur_altsetting->endpoint->extralen &&
                intf->cur_altsetting->endpoint->extra) {
            buflen = intf->cur_altsetting->endpoint->extralen;
            buffer = intf->cur_altsetting->endpoint->extra;
        } else {
            return -EINVAL;
        }
    }

    cdc_parse_cdc_header(&h, intf, buffer, buflen);
    union_header = h.usb_cdc_union_desc;
    cmgmd = h.usb_cdc_call_mgmt_descriptor;
    if (cmgmd)
        call_intf_num = cmgmd->bDataInterface;

    data_intf_num = union_header->bSlaveInterface0;
    control_interface = usb_ifnum_to_if(usb_dev, union_header->bMasterInterface0);
    data_interface = usb_ifnum_to_if(usb_dev, data_intf_num);

    epctrl = &control_interface->cur_altsetting->endpoint[0].desc;
    epread = &data_interface->cur_altsetting->endpoint[0].desc;
    epwrite = &data_interface->cur_altsetting->endpoint[1].desc;


    
    if (!usb_endpoint_dir_in(epread)) { /* workaround for switched endpoints */
        swap(epread, epwrite);/* descriptors are swapped */
    }
    printk(KERN_INFO "Arduino Driver: compressing_probe arduino module\n");
    arduino = kzalloc(sizeof(struct arduino), GFP_KERNEL);
    if (arduino == NULL)
        goto alloc_fail;

    minor = arduino_alloc_minor(arduino);
    if (minor < 0)
        goto alloc_fail1;

    ctrlsize = usb_endpoint_maxp(epctrl);
    readsize = usb_endpoint_maxp(epread) *
               (quirks == SINGLE_RX_URB ? 1 : 2);
    arduino->combined_interfaces = combined_interfaces;
    arduino->writesize = usb_endpoint_maxp(epwrite) * 20;
    arduino->control = control_interface;
    arduino->data = data_interface;
    arduino->minor = minor;
    arduino->dev = usb_dev;
    if (h.usb_cdc_acm_descriptor)
        arduino->ctrl_caps = h.usb_cdc_acm_descriptor->bmCapabilities;
    if (quirks & NO_CAP_LINE)
        arduino->ctrl_caps &= ~USB_CDC_CAP_LINE;
    arduino->ctrlsize = ctrlsize;
    arduino->readsize = readsize;
    arduino->rx_buflimit = num_rx_buf;
    INIT_WORK(&arduino->work, arduino_softint);
    init_waitqueue_head(&arduino->wioctl);
    spin_lock_init(&arduino->write_lock);
    spin_lock_init(&arduino->read_lock);
    mutex_init(&arduino->mutex);
    if (usb_endpoint_xfer_int(epread)) {
        arduino->bInterval = epread->bInterval;
        arduino->in = usb_rcvintpipe(usb_dev, epread->bEndpointAddress);
    } else {
        arduino->in = usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress);
    }
    if (usb_endpoint_xfer_int(epwrite))
        arduino->out = usb_sndintpipe(usb_dev, epwrite->bEndpointAddress);
    else
        arduino->out = usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress);
    tty_port_init(&arduino->port);
    arduino->port.ops = &arduino_port_ops;
    init_usb_anchor(&arduino->delayed);
    arduino->quirks = quirks;

    buf = usb_alloc_coherent(usb_dev, ctrlsize, GFP_KERNEL, &arduino->ctrl_dma);
    if (!buf)
        goto alloc_fail2;
    arduino->ctrl_buffer = buf;

    if (arduino_write_buffers_alloc(arduino) < 0)
        goto alloc_fail4;

    arduino->ctrlurb = usb_alloc_urb(0, GFP_KERNEL);
    if (!arduino->ctrlurb)
        goto alloc_fail5;

    for (i = 0; i < num_rx_buf; i++) {
        struct arduino_rb *rb = &(arduino->read_buffers[i]);
        struct urb *urb;

        rb->base = usb_alloc_coherent(arduino->dev, readsize, GFP_KERNEL,
                                      &rb->dma);
        if (!rb->base)
            goto alloc_fail6;
        rb->index = i;
        rb->instance = arduino;

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
            goto alloc_fail6;

        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        urb->transfer_dma = rb->dma;
        if (usb_endpoint_xfer_int(epread))
            usb_fill_int_urb(urb, arduino->dev, arduino->in, rb->base,
                             arduino->readsize,
                             arduino_read_bulk_callback, rb,
                             arduino->bInterval);
        else
            usb_fill_bulk_urb(urb, arduino->dev, arduino->in, rb->base,
                              arduino->readsize,
                              arduino_read_bulk_callback, rb);

        arduino->read_urbs[i] = urb;
        __set_bit(i, &arduino->read_urbs_free);
    }
    for (i = 0; i < ARDUINO_NW; i++) {
        struct arduino_wb *snd = &(arduino->wb[i]);

        snd->urb = usb_alloc_urb(0, GFP_KERNEL);
        if (snd->urb == NULL)
            goto alloc_fail7;

        if (usb_endpoint_xfer_int(epwrite))
            usb_fill_int_urb(snd->urb, usb_dev, arduino->out,
                             NULL, arduino->writesize, arduino_write_bulk, snd, epwrite->bInterval);
        else
            usb_fill_bulk_urb(snd->urb, usb_dev, arduino->out,
                              NULL, arduino->writesize, arduino_write_bulk, snd);
        snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        if (quirks & SEND_ZERO_PACKET)
            snd->urb->transfer_flags |= URB_ZERO_PACKET;
        snd->instance = arduino;
    }

    usb_set_intfdata(intf, arduino);

    i = device_create_file(&intf->dev, &dev_attr_bmCapabilities);
    if (i < 0)
        goto alloc_fail7;

    usb_fill_int_urb(arduino->ctrlurb, usb_dev,
                     usb_rcvintpipe(usb_dev, epctrl->bEndpointAddress),
                     arduino->ctrl_buffer, ctrlsize, arduino_ctrl_irq, arduino,
                     /* works around buggy devices */
                     epctrl->bInterval ? epctrl->bInterval : 16);
    arduino->ctrlurb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    arduino->ctrlurb->transfer_dma = arduino->ctrl_dma;
    arduino->notification_buffer = NULL;
    arduino->nb_index = 0;
    arduino->nb_size = 0;

    dev_info(&intf->dev, "arduino%d: USB Arduino device!\n", minor);

    arduino->line.dwDTERate = cpu_to_le32(9600);
    arduino->line.bDataBits = 8;
    arduino_set_line(arduino, &arduino->line);

    usb_driver_claim_interface(&arduino_driver, data_interface, arduino);
    usb_set_intfdata(data_interface, arduino);

    usb_get_intf(control_interface);
    tty_dev = tty_port_register_device(&arduino->port, arduino_tty_driver, minor,
                                       &control_interface->dev);
    if (IS_ERR(tty_dev)) {
        rv = PTR_ERR(tty_dev);
        goto alloc_fail8;
    }

    if (quirks & CLEAR_HALT_CONDITIONS) {
        usb_clear_halt(usb_dev, arduino->in);
        usb_clear_halt(usb_dev, arduino->out);
    }

    return 0;
alloc_fail8:
    device_remove_file(&arduino->control->dev, &dev_attr_bmCapabilities);
alloc_fail7:
    usb_set_intfdata(intf, NULL);
    for (i = 0; i < ARDUINO_NW; i++)
        usb_free_urb(arduino->wb[i].urb);
alloc_fail6:
    for (i = 0; i < num_rx_buf; i++)
        usb_free_urb(arduino->read_urbs[i]);
    arduino_read_buffers_free(arduino);
    usb_free_urb(arduino->ctrlurb);
alloc_fail5:
    arduino_write_buffers_free(arduino);
alloc_fail4:
    usb_free_coherent(usb_dev, ctrlsize, arduino->ctrl_buffer, arduino->ctrl_dma);
alloc_fail2:
    arduino_release_minor(arduino);
alloc_fail1:
    kfree(arduino);
alloc_fail:
    return rv;
}

static void arduino_disconnect(struct usb_interface *intf) {
    //printk(KERN_INFO "Arduino Driver: arduino_disconnect arduino module\n");
    struct arduino *arduino = usb_get_intfdata(intf);
    struct tty_struct *tty;

    /* sibling interface is already cleaning up */
    if (!arduino)
        return;

    mutex_lock(&arduino->mutex);
    arduino->disconnected = true;
    wake_up_all(&arduino->wioctl);
    device_remove_file(&arduino->control->dev, &dev_attr_bmCapabilities);
    usb_set_intfdata(arduino->control, NULL);
    usb_set_intfdata(arduino->data, NULL);
    mutex_unlock(&arduino->mutex);

    tty = tty_port_tty_get(&arduino->port);
    if (tty) {
        tty_vhangup(tty);
        tty_kref_put(tty);
    }

    arduino_kill_urbs(arduino);
    cancel_work_sync(&arduino->work);

    tty_unregister_device(arduino_tty_driver, arduino->minor);

    arduino_write_buffers_free(arduino);
    usb_free_coherent(arduino->dev, arduino->ctrlsize, arduino->ctrl_buffer, arduino->ctrl_dma);
    arduino_read_buffers_free(arduino);

    kfree(arduino->notification_buffer);

    if (!arduino->combined_interfaces)
        usb_driver_release_interface(&arduino_driver, intf == arduino->control ?
                                     arduino->data : arduino->control);

    tty_port_put(&arduino->port);
}

/*
 * USB driver structure.
 */
static const struct usb_device_id arduino_ids[] = {{USB_DEVICE(0x2341, 0x0043)}, {}};

MODULE_DEVICE_TABLE(usb, arduino_ids);

static struct usb_driver arduino_driver = {
    .name =     "Arduino USB driver",
    .probe =    arduino_probe,
    .disconnect =   arduino_disconnect,
    .id_table = arduino_ids,
#ifdef CONFIG_PM
    .supports_autosuspend = 1,
#endif
    .disable_hub_initiated_lpm = 1,
};

/*
 * TTY driver structures.
 */

static const struct tty_operations arduino_ops = {
    .install =      arduino_tty_install,
    .open =         arduino_tty_open,
    .close =        arduino_tty_close,
    .cleanup =      arduino_tty_cleanup,
    .write =        arduino_tty_write,
    .put_char =     arduino_tty_put_char,
    .flush_chars =      arduino_tty_flush_chars,
    .write_room =       arduino_tty_write_room,
    .ioctl =        arduino_tty_ioctl,
    .chars_in_buffer =  arduino_tty_chars_in_buffer,
    .set_termios =      arduino_tty_set_termios,
    .tiocmget =     arduino_tty_tiocmget,
    .tiocmset =     arduino_tty_tiocmset,
};

/*
 * Init / exit.
 */

static int __init arduino_init(void) {
    //printk(KERN_INFO "Arduino Driver: arduino_init arduino module\n");
    int retval;
    arduino_tty_driver = alloc_tty_driver(ARDUINO_TTY_MINORS);
    if (!arduino_tty_driver)
        return -ENOMEM;
    arduino_tty_driver->driver_name = "arduino",
    arduino_tty_driver->name = "arduino",
    arduino_tty_driver->major = ARDUINO_TTY_MAJOR,
    arduino_tty_driver->minor_start = 0,
    arduino_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
    arduino_tty_driver->subtype = SERIAL_TYPE_NORMAL,
    arduino_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    arduino_tty_driver->init_termios = tty_std_termios;
    arduino_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD |
                                           HUPCL | CLOCAL;
    tty_set_operations(arduino_tty_driver, &arduino_ops);

    retval = tty_register_driver(arduino_tty_driver);
    if (retval) {
        put_tty_driver(arduino_tty_driver);
        return retval;
    }

    retval = usb_register(&arduino_driver);
    if (retval) {
        tty_unregister_driver(arduino_tty_driver);
        put_tty_driver(arduino_tty_driver);
        return retval;
    }

    //printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_DESC "\n");

    return 0;
}

static void __exit arduino_exit(void) {
    //printk(KERN_INFO "Arduino Driver: arduino_exit arduino module\n");
    usb_deregister(&arduino_driver);
    tty_unregister_driver(arduino_tty_driver);
    put_tty_driver(arduino_tty_driver);
    idr_destroy(&arduino_minors);
}

module_init(arduino_init);
module_exit(arduino_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(ARDUINO_TTY_MAJOR);
