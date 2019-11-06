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

#define SUCCESS 0
#define DEVICE_NAME "arduano"
int arduano_major = 245;
static int Device_Open = 0; /* Is device open? */

int arduino_open(struct inode *inode, struct file *filp);
int arduino_release(struct inode *inode, struct file *filp);
ssize_t arduino_read(struct file *filp, char *buf, size_t count, loff_t *offset);
ssize_t arduino_write(struct file *filp, const char *buf, size_t count, loff_t *offset);
int arduino_init(void);
void arudino_exit(void);

struct file_operations arduino_fops = {
read:
    arduino_read,
write:
    arduino_write,
open:
    arduino_open,
release:
    arduino_release
};

int arduino_open(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "Arduano Driver: Opening arduino module\n");
    /*Avoid reading conflicts*/
    if (Device_Open)
        return -EBUSY;
    Device_Open++;
    return SUCCESS;
}


int arduino_release(struct inode *inode, struct file *filp) {
    /* Success */
    return 0;
}

static struct usb_device_id arduino_table[] = {
    //2341:0043
    {USB_DEVICE(0x2341, 0x0043) },
    {} //TELOS EGRAFHS
};


static int arduino_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    printk(KERN_INFO "Arduano Driver: arduino driver (%04X:%04X) Connected\n", id->idVendor, id->idProduct);
    return 0;
}


static void arduino_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "Arduano Driver: arduino Disconnected\n");
}


static struct usb_driver arduino_driver = {
    .name = "Arduino USB Driver",
    .id_table = arduino_table,// usb id
    .probe = arduino_probe,
    .disconnect = arduino_disconnect,
};


int arduino_init(void) {
    int ret = -1;
    printk(KERN_INFO "Arduano Driver: Arduino Constructor of Driver");
    printk(KERN_INFO "Arduano Driver: Registering Driver with kernel");
    ret = usb_register(&arduino_driver);
    register_chrdev(arduano_major, "DEVICE_NAME", &arduino_fops);
    printk(KERN_INFO "Arduano Driver: Registration complete");
    return ret;
}

void arduino_exit(void) {
    //deregister
    printk(KERN_INFO "Arduano Driver: Arduino Destructor of Driver");
    usb_deregister(&arduino_driver);
    printk(KERN_INFO "Arduano Driver: UnRegistration complete! \n");
}

/******************************************************************************************************
*************************************   1st Problem**************************************************
******************************************************************************************************/
ssize_t arduino_read(struct file *filp, char *bufStoreData, size_t bufCount, loff_t *curOffset) {

    //printk(KERN_INFO "Arduino: Reading from device");
    //ret = copy_to_user(bufStoreData, arduino.data, bufCount);
    int count = 0;
    return count;
}

/******************************************************************************************************
*************************************   2nd Problem**************************************************
******************************************************************************************************/
ssize_t arduino_write(struct file *filp, const char *bufSourceData, size_t bufCount, loff_t *curOffset) {

    //printk(KERN_INFO "Arduino: writing to device");
    //ret = copy_from_user( arduino.data, bufSourceData, bufCount);
    int count = 0;
    return count;
}


module_init(arduino_init);
module_exit(arduino_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ElTetias");
MODULE_DESCRIPTION("USB arduino Registration Driver");