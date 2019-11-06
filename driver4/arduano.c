/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */
#include <linux/ioport.h>
#include <linux/poll.h>     // for poll_wait()
#include <linux/interrupt.h>    // for request_irq()
#include <asm/io.h>
#include <linux/usb.h>

#include <arduano.h>

// Macros for usb
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x82
#define BUFFER_SIZE 512
#define TIMEOUT 5000

// Driver configuration
#define SUCCESS 0

static int device_open = 0; /* Is device open? */
static unsigned char read_buffer[BUFFER_SIZE];
static unsigned char write_buffer[BUFFER_SIZE];

// USB configurations
static struct tty_driver *arduano_tty_driver;
static struct usb_driver arduano_usb_driver;

static struct usb_device *arduano_device;
static struct usb_class_driver arduano_class;
static struct usb_device_id arduino_table[] = {{USB_DEVICE(0x2341, 0x0043)}, {}}; //2341:0043

static struct usb_endpoint_descriptor *epread;
static struct usb_endpoint_descriptor *epwrite;

// driver operations
int arduino_init(void);
void arduino_exit(void);

static const struct tty_operations arduano_tty_ops = {
    .open =         arduano_tty_open,
    .close =        arduano_tty_close,
    .cleanup =      arduano_tty_cleanup,
    .hangup =       arduano_tty_hangup,
    .write =        arduano_tty_write,
};

static int __init arduino_init(void) {
    int retval;
    printk(KERN_INFO "Arduano Driver: Arduino driver init");

    // Initialize tty for arduano
    arduano_tty_driver = alloc_tty_driver(ACM_TTY_MINORS);
    if (!arduano_tty_driver)
        return -ENOMEM;
    arduano_tty_driver->driver_name = "arduano_driver",
    arduano_tty_driver->name = DEVICE_NAME,
    arduano_tty_driver->major = ACM_TTY_MAJOR,
    arduano_tty_driver->minor_start = 0,
    arduano_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
    arduano_tty_driver->subtype = SERIAL_TYPE_NORMAL,
    arduano_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    arduano_tty_driver->init_termios = tty_std_termios;
    arduano_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    tty_set_operations(arduano_tty_driver, &arduano_ops);
    retval = tty_register_driver(arduano_tty_driver);
    if (retval) {
        printk(KERN_ERR "Arduano Driver: tty_register_driver failed. Error number %d", retval);
        put_tty_driver(arduano_tty_driver);
        return retval;
    }

    retval = usb_register(&arduano_usb_driver);
    if (retval){
        tty_unregister_driver(arduano_tty_driver);
        put_tty_driver(arduano_tty_driver);
        printk(KERN_ERR "Arduano Driver: usb_register failed. Error number %d", retval);
        return retval;
    }
    printk(KERN_INFO "Arduano Driver: Registration complete");
    return retval;
}
static void __exit arduino_exit(void) {
    //deregister
    printk(KERN_INFO "Arduano Driver: Arduino Destructor of Driver");
    usb_deregister(&arduano_usb_driver);
    tty_unregister_driver(arduano_tty_driver);
    put_tty_driver(arduano_tty_driver);
    printk(KERN_INFO "Arduano Driver: UnRegistration complete! \n");
}

module_init(arduino_init);
module_exit(arduino_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ElTetias");
MODULE_DESCRIPTION("USB arduano Registration Driver");