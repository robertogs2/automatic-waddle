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

// Macros for usb
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x82
#define BUFFER_SIZE 512
#define TIMEOUT 5000

// Driver configuration
#define SUCCESS 0
#define DEVICE_NAME "arduano"
int arduano_major = 245;
static int device_open = 0; /* Is device open? */
static unsigned char read_buffer[BUFFER_SIZE];
static unsigned char write_buffer[BUFFER_SIZE];

// USB configurations
static struct usb_device *arduano_device;
static struct usb_class_driver arduano_class;
static struct usb_device_id arduino_table[] = {{USB_DEVICE(0x2341, 0x0043)}, {}}; //2341:0043

static struct usb_endpoint_descriptor *epread;
static struct usb_endpoint_descriptor *epwrite;

// opening and closing operations
int arduino_open(struct inode *inode, struct file *filp);
int arduino_release(struct inode *inode, struct file *filp);

// reading and writing operations
ssize_t arduino_read(struct file *filp, char *buffer, size_t count, loff_t *offset);
ssize_t arduino_write(struct file *filp, const char *buffer, size_t count, loff_t *offset);

// driver operations
int arduino_init(void);
void arduino_exit(void);

// probe and disconnect for usb
static int arduino_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void arduino_disconnect(struct usb_interface *interface);

struct file_operations arduano_fops = {
read:
    arduino_read,
write:
    arduino_write,
open:
    arduino_open,
release:
    arduino_release
};


static struct usb_driver arduano_driver = {
    .name = "Arduino USB Driver",
    .id_table = arduino_table,// usb id
    .probe = arduino_probe,
    .disconnect = arduino_disconnect,
};


int arduino_open(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Arduano Driver: Opening arduino module\n");
    /*Avoid reading conflicts*/
    if (device_open) return -EBUSY;
    device_open++;
    return SUCCESS;
}
int arduino_release(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Arduano Driver: Releasing arduino module...\n");
    device_open--; //make ready for the next caller*/
    return SUCCESS;
}


static int arduino_probe(struct usb_interface *interface, const struct usb_device_id *id) {
	printk(KERN_INFO "Arduano Driver: Starting probe...\n");
    int retval;
    // Takes interface for the device
    arduano_device = interface_to_usbdev(interface);

    /// THIS SECTION OF CODE I COMPLETELY DON'T KNOW WHAT IT DOES, DON'T TOUCH IT

    //******************************************
    //******************************************
    //******************************************
    //******************************************
    /* normal quirks */
    // unsigned long quirks = (unsigned long)id->driver_info;
    struct usb_interface *control_interface;
    struct usb_interface *data_interface;
    struct usb_endpoint_descriptor *epctrl = NULL;
    epread = NULL;
    epwrite = NULL;
    // if (quirks == IGNORE_DEVICE)
    // 	return -ENODEV;

    // num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ACM_NR;

    // /* handle quirks deadly to normal probing*/
    // if (quirks == NO_UNION_NORMAL) {
    data_interface = usb_ifnum_to_if(arduano_device, 1);
    control_interface = usb_ifnum_to_if(arduano_device, 0);
    /* we would crash */
    if (!data_interface || !control_interface)
        return -ENODEV;
    //goto skip_normal_probe;
    //}


    // I think this sections does define the ep and that stuff

    for (int i = 0; i < data_interface->cur_altsetting->desc.bNumEndpoints; i++) {
        struct usb_endpoint_descriptor *ep;
        ep = &data_interface->cur_altsetting->endpoint[i].desc;

        	if (usb_endpoint_is_int_in(ep))
        		epctrl = ep;
        	else if (usb_endpoint_is_bulk_out(ep))
        		epwrite = ep;
        	else if (usb_endpoint_is_bulk_in(ep))
        		epread = ep;
        	else
        		return -EINVAL;
       
    }
    // if (!epctrl || !epread || !epwrite)
    //     return -ENODEV;
    //******************************************
    //******************************************
    //******************************************
    //******************************************

    arduano_class.name = "arduano%d";
    arduano_class.fops = &arduano_fops;
    printk(KERN_INFO "Arduano Driver: Arduino driver (%04X:%04X) Connected\n", id->idVendor, id->idProduct);
    printk(KERN_INFO "Arduano Driver: Trying to connect arduino driver to a device");
    if ((retval = usb_register_dev(interface, &arduano_class)) < 0) {
        /* Something prevented us from registering this driver */
        printk(KERN_ERR "Arduano Driver: Not able to get a minor for this device.");
    } else {
        printk(KERN_INFO "Arduano Driver: Minor obtained: %d\n", interface->minor);
        printk(KERN_INFO "Arduano Driver: Device is: arduano%d\n", interface->minor);
    }
    return retval;
}
static void arduino_disconnect(struct usb_interface *interface) {
    usb_deregister_dev(interface, &arduano_class);
    printk(KERN_INFO "Arduano Driver: Arduino Disconnected\n");
}


int arduino_init(void) {
    int ret = -1;
    printk(KERN_INFO "Arduano Driver: Arduino Constructor of Driver");
    printk(KERN_INFO "Arduano Driver: Registering Driver with kernel");
    ret = usb_register(&arduano_driver);
    if (ret) printk(KERN_ERR "Arduano Driver: usb_register failed. Error number %d", ret);
    //register_chrdev(arduano_major, "DEVICE_NAME", &arduano_fops);
    printk(KERN_INFO "Arduano Driver: Registration complete");
    return ret;
}
void arduino_exit(void) {
    //deregister
    printk(KERN_INFO "Arduano Driver: Arduino Destructor of Driver");
    usb_deregister(&arduano_driver);
    printk(KERN_INFO "Arduano Driver: UnRegistration complete! \n");
}


ssize_t arduino_read(struct file *filp, char *buffer, size_t count, loff_t *offset) {
    printk(KERN_INFO "Arduano Driver: Reading %ld from device\n", count);
    int retval;
    int read_count;

    /* Read the data from the bulk endpoint */
    retval = usb_bulk_msg(arduano_device, usb_rcvbulkpipe(arduano_device, epread->bEndpointAddress),//BULK_EP_IN),
                          read_buffer, BUFFER_SIZE, &read_count, TIMEOUT);
    if (retval) {
        printk(KERN_ERR "Arduano Driver: Bulk message returned %d\n", retval);
        return retval;
    }
    if (copy_to_user(buffer, read_buffer, MIN(count, read_count))) {
        return -EFAULT;
    }

    return MIN(count, read_count);
}
ssize_t arduino_write(struct file *filp, const char *buffer, size_t count, loff_t *offset) {
    printk(KERN_INFO "Arduano Driver: Writing with size %ld to device\n", count);
    int retval;
    int wrote_cnt = MIN(count, BUFFER_SIZE);
    printk(KERN_INFO "Arduano Driver: Writing with size min %d to device\n", wrote_cnt);
    if (copy_from_user(write_buffer, buffer, MIN(count, BUFFER_SIZE))) {
        return -EFAULT;
    }

    /* Write the data into the bulk endpoint */
    retval = usb_bulk_msg(arduano_device, usb_sndbulkpipe(arduano_device, epwrite->bEndpointAddress),//BULK_EP_OUT),
                          write_buffer, MIN(count, BUFFER_SIZE), &wrote_cnt, TIMEOUT);
    if (retval) {
        printk(KERN_ERR "Arduano Driver: Bulk message returned %d\n", retval);
        return retval;
    }

    return wrote_cnt;
}

module_init(arduino_init);
module_exit(arduino_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ElTetias");
MODULE_DESCRIPTION("USB arduano Registration Driver");