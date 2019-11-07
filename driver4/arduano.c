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

#include "arduano.h"

static struct usb_driver acm_driver;
static struct tty_driver *acm_tty_driver;

static DEFINE_IDR(acm_minors);
static DEFINE_MUTEX(acm_minors_lock);

static void acm_tty_set_termios(struct tty_struct *tty,
                                struct ktermios *termios_old);

/*
 * acm_minors accessors
 */

/*
 * Look up an ACM structure by minor. If found and not disconnected, increment
 * its refcount and return it with its mutex held.
 */
static struct acm *acm_get_by_minor(unsigned int minor) {
    //printk(KERN_INFO "Arduano Driver: acm_get_by_minor arduino module\n");

    struct acm *acm;

    mutex_lock(&acm_minors_lock);
    acm = idr_find(&acm_minors, minor);
    if (acm) {
        mutex_lock(&acm->mutex);
        if (acm->disconnected) {
            mutex_unlock(&acm->mutex);
            acm = NULL;
        } else {
            tty_port_get(&acm->port);
            mutex_unlock(&acm->mutex);
        }
    }
    mutex_unlock(&acm_minors_lock);
    return acm;
}

/*
 * Try to find an available minor number and if found, associate it with 'acm'.
 */
static int acm_alloc_minor(struct acm *acm) {    
    //printk(KERN_INFO "Arduano Driver: acm_alloc_minor arduino module\n");

    int minor;

    mutex_lock(&acm_minors_lock);
    minor = idr_alloc(&acm_minors, acm, 0, ACM_TTY_MINORS, GFP_KERNEL);
    mutex_unlock(&acm_minors_lock);

    return minor;
}

/* Release the minor number associated with 'acm'.  */
static void acm_release_minor(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_release_minor arduino module\n");
    mutex_lock(&acm_minors_lock);
    idr_remove(&acm_minors, acm->minor);
    mutex_unlock(&acm_minors_lock);
}

/*
 * Functions for ACM control messages.
 */

static int acm_ctrl_msg(struct acm *acm, int request, int value,
                        void *buf, int len) {
    //printk(KERN_INFO "Arduano Driver: acm_ctrl_msg arduino module\n");
    int retval;

    retval = usb_autopm_get_interface(acm->control);
    if (retval)
        return retval;

    retval = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
                             request, USB_RT_ACM, value,
                             acm->control->altsetting[0].desc.bInterfaceNumber,
                             buf, len, 5000);

    usb_autopm_put_interface(acm->control);

    return retval < 0 ? retval : 0;
}

/* devices aren't required to support these requests.
 * the cdc acm descriptor tells whether they do...
 */
static inline int acm_set_control(struct acm *acm, int control) {
    //printk(KERN_INFO "Arduano Driver: acm_ctrl_msg arduino module\n");

    if (acm->quirks & QUIRK_CONTROL_LINE_STATE)
        return -EOPNOTSUPP;

    return acm_ctrl_msg(acm, USB_CDC_REQ_SET_CONTROL_LINE_STATE,
                        control, NULL, 0);
}

#define acm_set_line(acm, line) \
    acm_ctrl_msg(acm, USB_CDC_REQ_SET_LINE_CODING, 0, line, sizeof *(line))
#define acm_send_break(acm, ms) \
    acm_ctrl_msg(acm, USB_CDC_REQ_SEND_BREAK, ms, NULL, 0)

static void acm_kill_urbs(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_kill_urbs arduino module\n");
    int i;

    usb_kill_urb(acm->ctrlurb);
    for (i = 0; i < ACM_NW; i++)
        usb_kill_urb(acm->wb[i].urb);
    for (i = 0; i < acm->rx_buflimit; i++)
        usb_kill_urb(acm->read_urbs[i]);
}

/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */

static int acm_wb_alloc(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_wb_alloc arduino module\n");
    int i, wbn;
    struct acm_wb *wb;

    wbn = 0;
    i = 0;
    for (;;) {
        wb = &acm->wb[wbn];
        if (!wb->use) {
            wb->use = 1;
            return wbn;
        }
        wbn = (wbn + 1) % ACM_NW;
        if (++i >= ACM_NW)
            return -1;
    }
}

static int acm_wb_is_avail(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_wb_is_avail arduino module\n");

    int i, n;
    unsigned long flags;

    n = ACM_NW;
    spin_lock_irqsave(&acm->write_lock, flags);
    for (i = 0; i < ACM_NW; i++)
        n -= acm->wb[i].use;
    spin_unlock_irqrestore(&acm->write_lock, flags);
    return n;
}

/*
 * Finish write. Caller must hold acm->write_lock
 */
static void acm_write_done(struct acm *acm, struct acm_wb *wb) {
    //printk(KERN_INFO "Arduano Driver: acm_write_done arduino module\n");
    wb->use = 0;
    acm->transmitting--;
    usb_autopm_put_interface_async(acm->control);
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */

static int acm_start_wb(struct acm *acm, struct acm_wb *wb) {
    //printk(KERN_INFO "Arduano Driver: acm_start_wb arduino module\n");
    int rc;

    acm->transmitting++;

    wb->urb->transfer_buffer = wb->buf;
    wb->urb->transfer_dma = wb->dmah;
    wb->urb->transfer_buffer_length = wb->len;
    wb->urb->dev = acm->dev;

    rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
    if (rc < 0) {
        acm_write_done(acm, wb);
    }
    return rc;
}

/*
 * attributes exported through sysfs
 */
static ssize_t show_caps(struct device *dev, struct device_attribute *attr, char *buf) {
    //printk(KERN_INFO "Arduano Driver: show_caps arduino module\n");
    struct usb_interface *intf = to_usb_interface(dev);
    struct acm *acm = usb_get_intfdata(intf);

    return sprintf(buf, "%d", acm->ctrl_caps);
}
static DEVICE_ATTR(bmCapabilities, S_IRUGO, show_caps, NULL);

static void acm_process_notification(struct acm *acm, unsigned char *buf) {
    //printk(KERN_INFO "Arduano Driver: acm_process_notification arduino module\n");
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

        if (!acm->clocal && (acm->ctrlin & ~newctrl & ACM_CTRL_DCD)) {
            tty_port_tty_hangup(&acm->port, false);
        }

        difference = acm->ctrlin ^ newctrl;
        spin_lock(&acm->read_lock);
        acm->ctrlin = newctrl;
        acm->oldcount = acm->iocount;

        if (difference & ACM_CTRL_DSR)
            acm->iocount.dsr++;
        if (difference & ACM_CTRL_BRK)
            acm->iocount.brk++;
        if (difference & ACM_CTRL_RI)
            acm->iocount.rng++;
        if (difference & ACM_CTRL_DCD)
            acm->iocount.dcd++;
        if (difference & ACM_CTRL_FRAMING)
            acm->iocount.frame++;
        if (difference & ACM_CTRL_PARITY)
            acm->iocount.parity++;
        if (difference & ACM_CTRL_OVERRUN)
            acm->iocount.overrun++;
        spin_unlock(&acm->read_lock);

        if (difference)
            wake_up_all(&acm->wioctl);

        break;

    default:
        break;
    }
}

/* control interface reports status changes with "interrupt" transfers */
static void acm_ctrl_irq(struct urb *urb) {
    //printk(KERN_INFO "Arduano Driver: acm_ctrl_irq arduino module\n");
    struct acm *acm = urb->context;
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
        acm->nb_index = 0;
        return;
    default:
        goto exit;
    }

    usb_mark_last_busy(acm->dev);

    if (acm->nb_index)
        dr = (struct usb_cdc_notification *)acm->notification_buffer;

    /* size = notification-header + (optional) data */
    expected_size = sizeof(struct usb_cdc_notification) +
                    le16_to_cpu(dr->wLength);

    if (current_size < expected_size) {
        /* notification is transmitted fragmented, reassemble */
        if (acm->nb_size < expected_size) {
            if (acm->nb_size) {
                kfree(acm->notification_buffer);
                acm->nb_size = 0;
            }
            alloc_size = roundup_pow_of_two(expected_size);
            /*
             * kmalloc ensures a valid notification_buffer after a
             * use of kfree in case the previous allocation was too
             * small. Final freeing is done on disconnect.
             */
            acm->notification_buffer =
                kmalloc(alloc_size, GFP_ATOMIC);
            if (!acm->notification_buffer)
                goto exit;
            acm->nb_size = alloc_size;
        }

        copy_size = min(current_size,
                        expected_size - acm->nb_index);

        memcpy(&acm->notification_buffer[acm->nb_index],
               urb->transfer_buffer, copy_size);
        acm->nb_index += copy_size;
        current_size = acm->nb_index;
    }

    if (current_size >= expected_size) {
        /* notification complete */
        acm_process_notification(acm, (unsigned char *)dr);
        acm->nb_index = 0;
    }

exit:
    retval = usb_submit_urb(urb, GFP_ATOMIC);
}

static int acm_submit_read_urb(struct acm *acm, int index, gfp_t mem_flags) {
    //printk(KERN_INFO "Arduano Driver: acm_submit_read_urb arduino module\n");
    int res;

    if (!test_and_clear_bit(index, &acm->read_urbs_free))
        return 0;

    res = usb_submit_urb(acm->read_urbs[index], mem_flags);
    if (res) {
        set_bit(index, &acm->read_urbs_free);
        return res;
    } 

    return 0;
}

static int acm_submit_read_urbs(struct acm *acm, gfp_t mem_flags) {
    //printk(KERN_INFO "Arduano Driver: acm_submit_read_urbs arduino module\n");
    int res;
    int i;

    for (i = 0; i < acm->rx_buflimit; ++i) {
        res = acm_submit_read_urb(acm, i, mem_flags);
        if (res)
            return res;
    }

    return 0;
}

static void acm_process_read_urb(struct acm *acm, struct urb *urb) {
    //printk(KERN_INFO "Arduano Driver: acm_process_read_urb arduino module\n");
    if (!urb->actual_length)
        return;

    tty_insert_flip_string(&acm->port, urb->transfer_buffer,
                           urb->actual_length);
    tty_flip_buffer_push(&acm->port);
}

static void acm_read_bulk_callback(struct urb *urb) {
    //printk(KERN_INFO "Arduano Driver: acm_read_bulk_callback arduino module\n");
    struct acm_rb *rb = urb->context;
    struct acm *acm = rb->instance;
    unsigned long flags;
    int status = urb->status;

    set_bit(rb->index, &acm->read_urbs_free);

    if (!acm->dev) {
        return;
    }

    switch (status) {
    case 0:
        usb_mark_last_busy(acm->dev);
        acm_process_read_urb(acm, urb);
        break;
    case -EPIPE:
        set_bit(EVENT_RX_STALL, &acm->flags);
        schedule_work(&acm->work);
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
    spin_lock_irqsave(&acm->read_lock, flags);
    acm->throttled = acm->throttle_req;
    if (!acm->throttled) {
        spin_unlock_irqrestore(&acm->read_lock, flags);
        acm_submit_read_urb(acm, rb->index, GFP_ATOMIC);
    } else {
        spin_unlock_irqrestore(&acm->read_lock, flags);
    }
}

/* data interface wrote those outgoing bytes */
static void acm_write_bulk(struct urb *urb) {
    //printk(KERN_INFO "Arduano Driver: acm_write_bulk arduino module\n");
    struct acm_wb *wb = urb->context;
    struct acm *acm = wb->instance;
    unsigned long flags;


    spin_lock_irqsave(&acm->write_lock, flags);
    acm_write_done(acm, wb);
    spin_unlock_irqrestore(&acm->write_lock, flags);
    set_bit(EVENT_TTY_WAKEUP, &acm->flags);
    schedule_work(&acm->work);
}

static void acm_softint(struct work_struct *work) {
    //printk(KERN_INFO "Arduano Driver: acm_softint arduino module\n");
    int i;
    struct acm *acm = container_of(work, struct acm, work);

    if (test_bit(EVENT_RX_STALL, &acm->flags)) {
        if (!(usb_autopm_get_interface(acm->data))) {
            for (i = 0; i < acm->rx_buflimit; i++)
                usb_kill_urb(acm->read_urbs[i]);
            usb_clear_halt(acm->dev, acm->in);
            acm_submit_read_urbs(acm, GFP_KERNEL);
            usb_autopm_put_interface(acm->data);
        }
        clear_bit(EVENT_RX_STALL, &acm->flags);
    }

    if (test_bit(EVENT_TTY_WAKEUP, &acm->flags)) {
        tty_port_tty_wakeup(&acm->port);
        clear_bit(EVENT_TTY_WAKEUP, &acm->flags);
    }
}

/*
 * TTY handlers
 */

static int acm_tty_install(struct tty_driver *driver, struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_install arduino module\n");
    struct acm *acm;
    int retval;

    acm = acm_get_by_minor(tty->index);
    if (!acm)
        return -ENODEV;

    retval = tty_standard_install(driver, tty);
    if (retval)
        goto error_init_termios;

    tty->driver_data = acm;

    return 0;

error_init_termios:
    tty_port_put(&acm->port);
    return retval;
}

static int acm_tty_open(struct tty_struct *tty, struct file *filp) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_open arduino module\n");

    struct acm *acm = tty->driver_data;

    return tty_port_open(&acm->port, tty, filp);
}

static void acm_port_dtr_rts(struct tty_port *port, int raise) {
    //printk(KERN_INFO "Arduano Driver: acm_port_dtr_rts arduino module\n");
    struct acm *acm = container_of(port, struct acm, port);
    int val;
    int res;

    if (raise)
        val = ACM_CTRL_DTR | ACM_CTRL_RTS;
    else
        val = 0;

    /* FIXME: add missing ctrlout locking throughout driver */
    acm->ctrlout = val;

    res = acm_set_control(acm, val);
}

static int acm_port_activate(struct tty_port *port, struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_port_activate arduino module\n");
    struct acm *acm = container_of(port, struct acm, port);
    int retval = -ENODEV;
    int i;

    mutex_lock(&acm->mutex);
    if (acm->disconnected)
        goto disconnected;

    retval = usb_autopm_get_interface(acm->control);
    if (retval)
        goto error_get_interface;

    /*
     * FIXME: Why do we need this? Allocating 64K of physically contiguous
     * memory is really nasty...
     */
    set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
    acm->control->needs_remote_wakeup = 1;

    acm->ctrlurb->dev = acm->dev;
    retval = usb_submit_urb(acm->ctrlurb, GFP_KERNEL);
    if (retval) {
        goto error_submit_urb;
    }

    acm_tty_set_termios(tty, NULL);

    /*
     * Unthrottle device in case the TTY was closed while throttled.
     */
    spin_lock_irq(&acm->read_lock);
    acm->throttled = 0;
    acm->throttle_req = 0;
    spin_unlock_irq(&acm->read_lock);

    retval = acm_submit_read_urbs(acm, GFP_KERNEL);
    if (retval)
        goto error_submit_read_urbs;

    usb_autopm_put_interface(acm->control);

    mutex_unlock(&acm->mutex);

    return 0;

error_submit_read_urbs:
    for (i = 0; i < acm->rx_buflimit; i++)
        usb_kill_urb(acm->read_urbs[i]);
    usb_kill_urb(acm->ctrlurb);
error_submit_urb:
    usb_autopm_put_interface(acm->control);
error_get_interface:
disconnected:
    mutex_unlock(&acm->mutex);

    return usb_translate_errors(retval);
}

static void acm_port_destruct(struct tty_port *port) {
    //printk(KERN_INFO "Arduano Driver: acm_port_destruct arduino module\n");
    struct acm *acm = container_of(port, struct acm, port);

    acm_release_minor(acm);
    usb_put_intf(acm->control);
    kfree(acm);
}

static void acm_port_shutdown(struct tty_port *port) {
    //printk(KERN_INFO "Arduano Driver: acm_port_shutdown arduino module\n");
    struct acm *acm = container_of(port, struct acm, port);
    struct urb *urb;
    struct acm_wb *wb;

    /*
     * Need to grab write_lock to prevent race with resume, but no need to
     * hold it due to the tty-port initialised flag.
     */
    spin_lock_irq(&acm->write_lock);
    spin_unlock_irq(&acm->write_lock);

    usb_autopm_get_interface_no_resume(acm->control);
    acm->control->needs_remote_wakeup = 0;
    usb_autopm_put_interface(acm->control);

    for (;;) {
        urb = usb_get_from_anchor(&acm->delayed);
        if (!urb)
            break;
        wb = urb->context;
        wb->use = 0;
        usb_autopm_put_interface_async(acm->control);
    }

    acm_kill_urbs(acm);
}

static void acm_tty_cleanup(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_cleanup arduino module\n");
    struct acm *acm = tty->driver_data;

    tty_port_put(&acm->port);
}

static void acm_tty_close(struct tty_struct *tty, struct file *filp) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_close arduino module\n");
    struct acm *acm = tty->driver_data;

    tty_port_close(&acm->port, tty, filp);
}

static int acm_tty_write(struct tty_struct *tty,
                         const unsigned char *buf, int count) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_write arduino module\n");
    struct acm *acm = tty->driver_data;
    int stat;
    unsigned long flags;
    int wbn;
    struct acm_wb *wb;

    if (!count)
        return 0;

    spin_lock_irqsave(&acm->write_lock, flags);
    wbn = acm_wb_alloc(acm);
    if (wbn < 0) {
        spin_unlock_irqrestore(&acm->write_lock, flags);
        return 0;
    }
    wb = &acm->wb[wbn];

    if (!acm->dev) {
        wb->use = 0;
        spin_unlock_irqrestore(&acm->write_lock, flags);
        return -ENODEV;
    }

    count = (count > acm->writesize) ? acm->writesize : count;
    memcpy(wb->buf, buf, count);
    wb->len = count;

    stat = usb_autopm_get_interface_async(acm->control);
    if (stat) {
        wb->use = 0;
        spin_unlock_irqrestore(&acm->write_lock, flags);
        return stat;
    }

    if (acm->susp_count) {
        if (acm->putbuffer) {
            /* now to preserve order */
            usb_anchor_urb(acm->putbuffer->urb, &acm->delayed);
            acm->putbuffer = NULL;
        }
        usb_anchor_urb(wb->urb, &acm->delayed);
        spin_unlock_irqrestore(&acm->write_lock, flags);
        return count;
    } else {
        if (acm->putbuffer) {
            /* at this point there is no good way to handle errors */
            acm_start_wb(acm, acm->putbuffer);
            acm->putbuffer = NULL;
        }
    }

    stat = acm_start_wb(acm, wb);
    spin_unlock_irqrestore(&acm->write_lock, flags);

    if (stat < 0)
        return stat;
    return count;
}

static void acm_tty_flush_chars(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_flush_chars arduino module\n");
    struct acm *acm = tty->driver_data;
    struct acm_wb *cur = acm->putbuffer;
    int err;
    unsigned long flags;

    if (!cur) /* nothing to do */
        return;

    acm->putbuffer = NULL;
    err = usb_autopm_get_interface_async(acm->control);
    spin_lock_irqsave(&acm->write_lock, flags);
    if (err < 0) {
        cur->use = 0;
        acm->putbuffer = cur;
        goto out;
    }

    if (acm->susp_count)
        usb_anchor_urb(cur->urb, &acm->delayed);
    else
        acm_start_wb(acm, cur);
out:
    spin_unlock_irqrestore(&acm->write_lock, flags);
    return;
}

static int acm_tty_put_char(struct tty_struct *tty, unsigned char ch) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_put_char arduino module\n");
    struct acm *acm = tty->driver_data;
    struct acm_wb *cur;
    int wbn;
    unsigned long flags;

overflow:
    cur = acm->putbuffer;
    if (!cur) {
        spin_lock_irqsave(&acm->write_lock, flags);
        wbn = acm_wb_alloc(acm);
        if (wbn >= 0) {
            cur = &acm->wb[wbn];
            acm->putbuffer = cur;
        }
        spin_unlock_irqrestore(&acm->write_lock, flags);
        if (!cur)
            return 0;
    }

    if (cur->len == acm->writesize) {
        acm_tty_flush_chars(tty);
        goto overflow;
    }

    cur->buf[cur->len++] = ch;
    return 1;
}

static int acm_tty_write_room(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_write_room arduino module\n");
    struct acm *acm = tty->driver_data;
    /*
     * Do not let the line discipline to know that we have a reserve,
     * or it might get too enthusiastic.
     */
    return acm_wb_is_avail(acm) ? acm->writesize : 0;
}

static int acm_tty_chars_in_buffer(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_chars_in_buffer arduino module\n");
    struct acm *acm = tty->driver_data;
    /*
     * if the device was unplugged then any remaining characters fell out
     * of the connector ;)
     */
    if (acm->disconnected)
        return 0;
    /*
     * This is inaccurate (overcounts), but it works.
     */
    return (ACM_NW - acm_wb_is_avail(acm)) * acm->writesize;
}

static int acm_tty_break_ctl(struct tty_struct *tty, int state) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_break_ctl arduino module\n");
    struct acm *acm = tty->driver_data;
    int retval;

    retval = acm_send_break(acm, state ? 0xffff : 0);
    return retval;
}

static int acm_tty_tiocmget(struct tty_struct *tty) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_tiocmget arduino module\n");
    struct acm *acm = tty->driver_data;

    return (acm->ctrlout & ACM_CTRL_DTR ? TIOCM_DTR : 0) |
           (acm->ctrlout & ACM_CTRL_RTS ? TIOCM_RTS : 0) |
           (acm->ctrlin  & ACM_CTRL_DSR ? TIOCM_DSR : 0) |
           (acm->ctrlin  & ACM_CTRL_RI  ? TIOCM_RI  : 0) |
           (acm->ctrlin  & ACM_CTRL_DCD ? TIOCM_CD  : 0) |
           TIOCM_CTS;
}

static int acm_tty_tiocmset(struct tty_struct *tty,
                            unsigned int set, unsigned int clear) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_tiocmset arduino module\n");
    struct acm *acm = tty->driver_data;
    unsigned int newctrl;

    newctrl = acm->ctrlout;
    set = (set & TIOCM_DTR ? ACM_CTRL_DTR : 0) |
          (set & TIOCM_RTS ? ACM_CTRL_RTS : 0);
    clear = (clear & TIOCM_DTR ? ACM_CTRL_DTR : 0) |
            (clear & TIOCM_RTS ? ACM_CTRL_RTS : 0);

    newctrl = (newctrl & ~clear) | set;

    if (acm->ctrlout == newctrl)
        return 0;
    return acm_set_control(acm, acm->ctrlout = newctrl);
}

static int get_serial_info(struct acm *acm, struct serial_struct __user *info) {
    //printk(KERN_INFO "Arduano Driver: get_serial_info arduino module\n");
    struct serial_struct tmp;

    memset(&tmp, 0, sizeof(tmp));
    tmp.xmit_fifo_size = acm->writesize;
    tmp.baud_base = le32_to_cpu(acm->line.dwDTERate);
    tmp.close_delay = acm->port.close_delay / 10;
    tmp.closing_wait = acm->port.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
                       ASYNC_CLOSING_WAIT_NONE :
                       acm->port.closing_wait / 10;

    if (copy_to_user(info, &tmp, sizeof(tmp)))
        return -EFAULT;
    else
        return 0;
}

static int set_serial_info(struct acm *acm,
                           struct serial_struct __user *newinfo) {
    //printk(KERN_INFO "Arduano Driver: set_serial_info arduino module\n");
    struct serial_struct new_serial;
    unsigned int closing_wait, close_delay;
    int retval = 0;

    if (copy_from_user(&new_serial, newinfo, sizeof(new_serial)))
        return -EFAULT;

    close_delay = new_serial.close_delay * 10;
    closing_wait = new_serial.closing_wait == ASYNC_CLOSING_WAIT_NONE ?
                   ASYNC_CLOSING_WAIT_NONE : new_serial.closing_wait * 10;

    mutex_lock(&acm->port.mutex);

    if (!capable(CAP_SYS_ADMIN)) {
        if ((close_delay != acm->port.close_delay) ||
                (closing_wait != acm->port.closing_wait))
            retval = -EPERM;
        else
            retval = -EOPNOTSUPP;
    } else {
        acm->port.close_delay  = close_delay;
        acm->port.closing_wait = closing_wait;
    }

    mutex_unlock(&acm->port.mutex);
    return retval;
}

static int wait_serial_change(struct acm *acm, unsigned long arg) {
    //printk(KERN_INFO "Arduano Driver: wait_serial_change arduino module\n");
    int rv = 0;
    DECLARE_WAITQUEUE(wait, current);
    struct async_icount old, new;

    do {
        spin_lock_irq(&acm->read_lock);
        old = acm->oldcount;
        new = acm->iocount;
        acm->oldcount = new;
        spin_unlock_irq(&acm->read_lock);

        if ((arg & TIOCM_DSR) &&
                old.dsr != new.dsr)
            break;
        if ((arg & TIOCM_CD)  &&
                old.dcd != new.dcd)
            break;
        if ((arg & TIOCM_RI) &&
                old.rng != new.rng)
            break;

        add_wait_queue(&acm->wioctl, &wait);
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        remove_wait_queue(&acm->wioctl, &wait);
        if (acm->disconnected) {
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

static int acm_tty_get_icount(struct tty_struct *tty,
                              struct serial_icounter_struct *icount) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_get_icount arduino module\n");
    struct acm *acm = tty->driver_data;

    icount->dsr = acm->iocount.dsr;
    icount->rng = acm->iocount.rng;
    icount->dcd = acm->iocount.dcd;
    icount->frame = acm->iocount.frame;
    icount->overrun = acm->iocount.overrun;
    icount->parity = acm->iocount.parity;
    icount->brk = acm->iocount.brk;

    return 0;
}

static int acm_tty_ioctl(struct tty_struct *tty,
                         unsigned int cmd, unsigned long arg) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_ioctl arduino module\n");
    struct acm *acm = tty->driver_data;
    int rv = -ENOIOCTLCMD;

    switch (cmd) {
    case TIOCGSERIAL: /* gets serial port data */
        rv = get_serial_info(acm, (struct serial_struct __user *) arg);
        break;
    case TIOCSSERIAL:
        rv = set_serial_info(acm, (struct serial_struct __user *) arg);
        break;
    case TIOCMIWAIT:
        rv = usb_autopm_get_interface(acm->control);
        if (rv < 0) {
            rv = -EIO;
            break;
        }
        rv = wait_serial_change(acm, arg);
        usb_autopm_put_interface(acm->control);
        break;
    }

    return rv;
}

static void acm_tty_set_termios(struct tty_struct *tty,
                                struct ktermios *termios_old) {
    //printk(KERN_INFO "Arduano Driver: acm_tty_set_termios arduino module\n");
    struct acm *acm = tty->driver_data;
    struct ktermios *termios = &tty->termios;
    struct usb_cdc_line_coding newline;
    int newctrl = acm->ctrlout;

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
    acm->clocal = ((termios->c_cflag & CLOCAL) != 0);

    if (C_BAUD(tty) == B0) {
        newline.dwDTERate = acm->line.dwDTERate;
        newctrl &= ~ACM_CTRL_DTR;
    } else if (termios_old && (termios_old->c_cflag & CBAUD) == B0) {
        newctrl |=  ACM_CTRL_DTR;
    }

    if (newctrl != acm->ctrlout)
        acm_set_control(acm, acm->ctrlout = newctrl);

    if (memcmp(&acm->line, &newline, sizeof newline)) {
        memcpy(&acm->line, &newline, sizeof newline);
        acm_set_line(acm, &acm->line);
    }
}

static const struct tty_port_operations acm_port_ops = {
    .dtr_rts = acm_port_dtr_rts,
    .shutdown = acm_port_shutdown,
    .activate = acm_port_activate,
    .destruct = acm_port_destruct,
};

/*
 * USB probe and disconnect routines.
 */

/* Little helpers: write/read buffers free */
static void acm_write_buffers_free(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_write_buffers_free arduino module\n");
    int i;
    struct acm_wb *wb;

    for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++)
        usb_free_coherent(acm->dev, acm->writesize, wb->buf, wb->dmah);
}

static void acm_read_buffers_free(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_read_buffers_free arduino module\n");
    int i;

    for (i = 0; i < acm->rx_buflimit; i++)
        usb_free_coherent(acm->dev, acm->readsize,
                          acm->read_buffers[i].base, acm->read_buffers[i].dma);
}

/* Little helper: write buffers allocate */
static int acm_write_buffers_alloc(struct acm *acm) {
    //printk(KERN_INFO "Arduano Driver: acm_write_buffers_alloc arduino module\n");
    int i;
    struct acm_wb *wb;

    for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++) {
        wb->buf = usb_alloc_coherent(acm->dev, acm->writesize, GFP_KERNEL,
                                     &wb->dmah);
        if (!wb->buf) {
            while (i != 0) {
                --i;
                --wb;
                usb_free_coherent(acm->dev, acm->writesize,
                                  wb->buf, wb->dmah);
            }
            return -ENOMEM;
        }
    }
    return 0;
}

static int acm_probe(struct usb_interface *intf,
                     const struct usb_device_id *id) {
    //printk(KERN_INFO "Arduano Driver: acm_probe arduino module\n");
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
    struct acm *acm;
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
    int res;

    /* normal quirks */
    quirks = (unsigned long)id->driver_info;

    if (quirks == IGNORE_DEVICE)
        return -ENODEV;

    memset(&h, 0x00, sizeof(struct usb_cdc_parsed_header));

    num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ACM_NR;

    /* handle quirks deadly to normal probing*/
    if (quirks == NO_UNION_NORMAL) {
        data_interface = usb_ifnum_to_if(usb_dev, 1);
        control_interface = usb_ifnum_to_if(usb_dev, 0);
        /* we would crash */
        if (!data_interface || !control_interface)
            return -ENODEV;
        goto skip_normal_probe;
    }

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

    if (!union_header) {
        if (call_intf_num > 0) {
            /* quirks for Droids MuIn LCD TODO*/ 
            if (quirks & NO_DATA_INTERFACE) {
                data_interface = usb_ifnum_to_if(usb_dev, 0);
            } else {
                data_intf_num = call_intf_num;
                data_interface = usb_ifnum_to_if(usb_dev, data_intf_num);
            }
            control_interface = intf;
        } else {
            if (intf->cur_altsetting->desc.bNumEndpoints != 3) {
                return -ENODEV;
            } else {
                combined_interfaces = 1;
                control_interface = data_interface = intf;
                goto look_for_collapsed_interface;
            }
        }
    } else {
        data_intf_num = union_header->bSlaveInterface0;
        control_interface = usb_ifnum_to_if(usb_dev, union_header->bMasterInterface0);
        data_interface = usb_ifnum_to_if(usb_dev, data_intf_num);
    }

    if (!control_interface || !data_interface) {
        return -ENODEV;
    }
    if (!data_interface->cur_altsetting || !control_interface->cur_altsetting)
        return -ENODEV;

    if (control_interface == data_interface) {
        /* some broken devices designed for windows work this way */
        combined_interfaces = 1;
        /* a popular other OS doesn't use it */
        quirks |= NO_CAP_LINE; // TODO
        if (data_interface->cur_altsetting->desc.bNumEndpoints != 3) {
            return -EINVAL;
        }
look_for_collapsed_interface:
        res = usb_find_common_endpoints(data_interface->cur_altsetting,
                                        &epread, &epwrite, &epctrl, NULL);
        if (res)
            return res;

        goto made_compressed_probe;
    }

skip_normal_probe:

    /*workaround for switched interfaces */
    if (data_interface->cur_altsetting->desc.bInterfaceClass
            != CDC_DATA_INTERFACE_TYPE) {
        if (control_interface->cur_altsetting->desc.bInterfaceClass
                == CDC_DATA_INTERFACE_TYPE) {
            swap(control_interface, data_interface);
        } else {
            return -EINVAL;
        }
    }

    /* Accept probe requests only for the control interface */
    if (!combined_interfaces && intf != control_interface)
        return -ENODEV;

    if (!combined_interfaces && usb_interface_claimed(data_interface)) {
        return -EBUSY;
    }


    if (data_interface->cur_altsetting->desc.bNumEndpoints < 2 ||
            control_interface->cur_altsetting->desc.bNumEndpoints == 0)
        return -EINVAL;

    epctrl = &control_interface->cur_altsetting->endpoint[0].desc;
    epread = &data_interface->cur_altsetting->endpoint[0].desc;
    epwrite = &data_interface->cur_altsetting->endpoint[1].desc;


    /* workaround for switched endpoints */
    if (!usb_endpoint_dir_in(epread)) {
        /* descriptors are swapped */
        swap(epread, epwrite);
    }
made_compressed_probe:

    acm = kzalloc(sizeof(struct acm), GFP_KERNEL);
    if (acm == NULL)
        goto alloc_fail;

    minor = acm_alloc_minor(acm);
    if (minor < 0)
        goto alloc_fail1;

    ctrlsize = usb_endpoint_maxp(epctrl);
    readsize = usb_endpoint_maxp(epread) *
               (quirks == SINGLE_RX_URB ? 1 : 2);
    acm->combined_interfaces = combined_interfaces;
    acm->writesize = usb_endpoint_maxp(epwrite) * 20;
    acm->control = control_interface;
    acm->data = data_interface;
    acm->minor = minor;
    acm->dev = usb_dev;
    if (h.usb_cdc_acm_descriptor)
        acm->ctrl_caps = h.usb_cdc_acm_descriptor->bmCapabilities;
    if (quirks & NO_CAP_LINE)
        acm->ctrl_caps &= ~USB_CDC_CAP_LINE;
    acm->ctrlsize = ctrlsize;
    acm->readsize = readsize;
    acm->rx_buflimit = num_rx_buf;
    INIT_WORK(&acm->work, acm_softint);
    init_waitqueue_head(&acm->wioctl);
    spin_lock_init(&acm->write_lock);
    spin_lock_init(&acm->read_lock);
    mutex_init(&acm->mutex);
    if (usb_endpoint_xfer_int(epread)) {
        acm->bInterval = epread->bInterval;
        acm->in = usb_rcvintpipe(usb_dev, epread->bEndpointAddress);
    } else {
        acm->in = usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress);
    }
    if (usb_endpoint_xfer_int(epwrite))
        acm->out = usb_sndintpipe(usb_dev, epwrite->bEndpointAddress);
    else
        acm->out = usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress);
    tty_port_init(&acm->port);
    acm->port.ops = &acm_port_ops;
    init_usb_anchor(&acm->delayed);
    acm->quirks = quirks;

    buf = usb_alloc_coherent(usb_dev, ctrlsize, GFP_KERNEL, &acm->ctrl_dma);
    if (!buf)
        goto alloc_fail2;
    acm->ctrl_buffer = buf;

    if (acm_write_buffers_alloc(acm) < 0)
        goto alloc_fail4;

    acm->ctrlurb = usb_alloc_urb(0, GFP_KERNEL);
    if (!acm->ctrlurb)
        goto alloc_fail5;

    for (i = 0; i < num_rx_buf; i++) {
        struct acm_rb *rb = &(acm->read_buffers[i]);
        struct urb *urb;

        rb->base = usb_alloc_coherent(acm->dev, readsize, GFP_KERNEL,
                                      &rb->dma);
        if (!rb->base)
            goto alloc_fail6;
        rb->index = i;
        rb->instance = acm;

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
            goto alloc_fail6;

        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        urb->transfer_dma = rb->dma;
        if (usb_endpoint_xfer_int(epread))
            usb_fill_int_urb(urb, acm->dev, acm->in, rb->base,
                             acm->readsize,
                             acm_read_bulk_callback, rb,
                             acm->bInterval);
        else
            usb_fill_bulk_urb(urb, acm->dev, acm->in, rb->base,
                              acm->readsize,
                              acm_read_bulk_callback, rb);

        acm->read_urbs[i] = urb;
        __set_bit(i, &acm->read_urbs_free);
    }
    for (i = 0; i < ACM_NW; i++) {
        struct acm_wb *snd = &(acm->wb[i]);

        snd->urb = usb_alloc_urb(0, GFP_KERNEL);
        if (snd->urb == NULL)
            goto alloc_fail7;

        if (usb_endpoint_xfer_int(epwrite))
            usb_fill_int_urb(snd->urb, usb_dev, acm->out,
                             NULL, acm->writesize, acm_write_bulk, snd, epwrite->bInterval);
        else
            usb_fill_bulk_urb(snd->urb, usb_dev, acm->out,
                              NULL, acm->writesize, acm_write_bulk, snd);
        snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
        if (quirks & SEND_ZERO_PACKET)
            snd->urb->transfer_flags |= URB_ZERO_PACKET;
        snd->instance = acm;
    }

    usb_set_intfdata(intf, acm);

    i = device_create_file(&intf->dev, &dev_attr_bmCapabilities);
    if (i < 0)
        goto alloc_fail7;

    usb_fill_int_urb(acm->ctrlurb, usb_dev,
                     usb_rcvintpipe(usb_dev, epctrl->bEndpointAddress),
                     acm->ctrl_buffer, ctrlsize, acm_ctrl_irq, acm,
                     /* works around buggy devices */
                     epctrl->bInterval ? epctrl->bInterval : 16);
    acm->ctrlurb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    acm->ctrlurb->transfer_dma = acm->ctrl_dma;
    acm->notification_buffer = NULL;
    acm->nb_index = 0;
    acm->nb_size = 0;

    dev_info(&intf->dev, "arduano%d: USB Arduano device!\n", minor);

    acm->line.dwDTERate = cpu_to_le32(9600);
    acm->line.bDataBits = 8;
    acm_set_line(acm, &acm->line);

    usb_driver_claim_interface(&acm_driver, data_interface, acm);
    usb_set_intfdata(data_interface, acm);

    usb_get_intf(control_interface);
    tty_dev = tty_port_register_device(&acm->port, acm_tty_driver, minor,
                                       &control_interface->dev);
    if (IS_ERR(tty_dev)) {
        rv = PTR_ERR(tty_dev);
        goto alloc_fail8;
    }

    if (quirks & CLEAR_HALT_CONDITIONS) {
        usb_clear_halt(usb_dev, acm->in);
        usb_clear_halt(usb_dev, acm->out);
    }

    return 0;
alloc_fail8:
    device_remove_file(&acm->control->dev, &dev_attr_bmCapabilities);
alloc_fail7:
    usb_set_intfdata(intf, NULL);
    for (i = 0; i < ACM_NW; i++)
        usb_free_urb(acm->wb[i].urb);
alloc_fail6:
    for (i = 0; i < num_rx_buf; i++)
        usb_free_urb(acm->read_urbs[i]);
    acm_read_buffers_free(acm);
    usb_free_urb(acm->ctrlurb);
alloc_fail5:
    acm_write_buffers_free(acm);
alloc_fail4:
    usb_free_coherent(usb_dev, ctrlsize, acm->ctrl_buffer, acm->ctrl_dma);
alloc_fail2:
    acm_release_minor(acm);
alloc_fail1:
    kfree(acm);
alloc_fail:
    return rv;
}

static void acm_disconnect(struct usb_interface *intf) {
    //printk(KERN_INFO "Arduano Driver: acm_disconnect arduino module\n");
    struct acm *acm = usb_get_intfdata(intf);
    struct tty_struct *tty;

    /* sibling interface is already cleaning up */
    if (!acm)
        return;

    mutex_lock(&acm->mutex);
    acm->disconnected = true;
    wake_up_all(&acm->wioctl);
    device_remove_file(&acm->control->dev, &dev_attr_bmCapabilities);
    usb_set_intfdata(acm->control, NULL);
    usb_set_intfdata(acm->data, NULL);
    mutex_unlock(&acm->mutex);

    tty = tty_port_tty_get(&acm->port);
    if (tty) {
        tty_vhangup(tty);
        tty_kref_put(tty);
    }

    acm_kill_urbs(acm);
    cancel_work_sync(&acm->work);

    tty_unregister_device(acm_tty_driver, acm->minor);

    acm_write_buffers_free(acm);
    usb_free_coherent(acm->dev, acm->ctrlsize, acm->ctrl_buffer, acm->ctrl_dma);
    acm_read_buffers_free(acm);

    kfree(acm->notification_buffer);

    if (!acm->combined_interfaces)
        usb_driver_release_interface(&acm_driver, intf == acm->control ?
                                     acm->data : acm->control);

    tty_port_put(&acm->port);
}

/*
 * USB driver structure.
 */
static const struct usb_device_id acm_ids[] = {{USB_DEVICE(0x2341, 0x0043)}, {}};

MODULE_DEVICE_TABLE(usb, acm_ids);

static struct usb_driver acm_driver = {
    .name =     "Arduino USB driver",
    .probe =    acm_probe,
    .disconnect =   acm_disconnect,
    .id_table = acm_ids,
#ifdef CONFIG_PM
    .supports_autosuspend = 1,
#endif
    .disable_hub_initiated_lpm = 1,
};

/*
 * TTY driver structures.
 */

static const struct tty_operations acm_ops = {
    .install =      acm_tty_install,
    .open =         acm_tty_open,
    .close =        acm_tty_close,
    .cleanup =      acm_tty_cleanup,
    .write =        acm_tty_write,
    .put_char =     acm_tty_put_char,
    .flush_chars =      acm_tty_flush_chars,
    .write_room =       acm_tty_write_room,
    .ioctl =        acm_tty_ioctl,
    .chars_in_buffer =  acm_tty_chars_in_buffer,
    //.break_ctl =        acm_tty_break_ctl,
    .set_termios =      acm_tty_set_termios,
    .tiocmget =     acm_tty_tiocmget,
    .tiocmset =     acm_tty_tiocmset,
    //.get_icount =       acm_tty_get_icount,
};

/*
 * Init / exit.
 */

static int __init acm_init(void) {
    //printk(KERN_INFO "Arduano Driver: acm_init arduino module\n");
    int retval;
    acm_tty_driver = alloc_tty_driver(ACM_TTY_MINORS);
    if (!acm_tty_driver)
        return -ENOMEM;
    acm_tty_driver->driver_name = "arduano",
    acm_tty_driver->name = "arduano",
    acm_tty_driver->major = ACM_TTY_MAJOR,
    acm_tty_driver->minor_start = 0,
    acm_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
    acm_tty_driver->subtype = SERIAL_TYPE_NORMAL,
    acm_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    acm_tty_driver->init_termios = tty_std_termios;
    acm_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD |
                                           HUPCL | CLOCAL;
    tty_set_operations(acm_tty_driver, &acm_ops);

    retval = tty_register_driver(acm_tty_driver);
    if (retval) {
        put_tty_driver(acm_tty_driver);
        return retval;
    }

    retval = usb_register(&acm_driver);
    if (retval) {
        tty_unregister_driver(acm_tty_driver);
        put_tty_driver(acm_tty_driver);
        return retval;
    }

    //printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_DESC "\n");

    return 0;
}

static void __exit acm_exit(void) {
    //printk(KERN_INFO "Arduano Driver: acm_exit arduino module\n");
    usb_deregister(&acm_driver);
    tty_unregister_driver(acm_tty_driver);
    put_tty_driver(acm_tty_driver);
    idr_destroy(&acm_minors);
}

module_init(acm_init);
module_exit(acm_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(ACM_TTY_MAJOR);