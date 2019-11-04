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

#define SUCCESS 0

/*Local defined macros*/
#define UART_IRQ	4	// UART's Interrupt Request pin
#define UART_BASE	0x03F8	// UART's base I/O port-address
#define INTR_MASK	0x0F	// UART Interrupt Mask
char* DEVICE_NAME  = "uart";

static unsigned RANGE = 0x8; 
static int Device_Open = 0; /* Is device open? */
u8 *addr;



MODULE_LICENSE("GPL v2");

int uart_open(struct inode *inode, struct file *filp);
int uart_release(struct inode *inode, struct file *file);
ssize_t uart_read(struct file *filp, char *buffer,
                   size_t length, loff_t *offset);
ssize_t uart_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
void uart_exit(void);
int uart_init(void);
/* Structure that declares the usual file */
/* access functions */
struct file_operations	uart_fops = {
write:
    uart_write,
read:
    uart_read,
open:
    uart_open,
release:
    uart_release
};
/* Declaration of the init and exit functions */
module_init(uart_init);
module_exit(uart_exit);
/* Global variables of the driver */
/* Major number */
int  uart_major = 84;

enum{
    UART_RX_DATA    = UART_BASE + 0,
    UART_TX_DATA    = UART_BASE + 0,
    UART_DLATCH_LO  = UART_BASE + 0,
    UART_DLATCH_HI  = UART_BASE + 1,
    UART_INTR_EN    = UART_BASE + 1,
    UART_INTR_ID    = UART_BASE + 2,
    UART_FIFO_CTRL  = UART_BASE + 2,
    UART_LINE_CTRL  = UART_BASE + 3,
    UART_MODEM_CTRL = UART_BASE + 4,
    UART_LINE_STAT  = UART_BASE + 5,
    UART_MODEM_STAT = UART_BASE + 6,
    UART_SCRATCH    = UART_BASE + 7,
};

// wait_queue_head_t  wq_recv;
// wait_queue_head_t  wq_xmit;

// irqreturn_t uart_isr( int irq, void *devinfo ) {
//     // static int	rep = 0;
//     int	interrupt_id = inb( UART_INTR_ID ) & 0x0F;
//     if ( interrupt_id & 1 ) return IRQ_NONE;

//     // ++rep;
//     // printk( "UART %d: interrupt_id=%02X \n", rep, interrupt_id );

//     switch ( interrupt_id ) {
//     case 6:	// Receiver Line Status (errors)
//         inb( UART_LINE_STAT );
//         break;
//     case 4:	// Received Data Ready
//     case 12: // Character timeout
//         wake_up_interruptible( &wq_recv );
//         break;
//     case 2: // Transmitter Empty
//         wake_up_interruptible( &wq_xmit );
//         break;
//     case 0:	// Modem Status Changed
//         inb( UART_MODEM_STAT );
//         break;
//     }

//     return	IRQ_HANDLED;
// }

// int my_proc_read( char *buf, char **start, off_t off, int count,
//                   int *eof, void *data ) {
//     int	interrupt_id = inb( UART_INTR_ID );
//     int	line_status = inb( UART_LINE_STAT );
//     int	modem_status = inb( UART_MODEM_STAT );
//     int	len = 0;

//     len += sprintf( buf + len, "\n %02X=modem_status  ", modem_status );
//     len += sprintf( buf + len, "\n %02X=line_status   ", line_status );
//     len += sprintf( buf + len, "\n %02X=interrupt_id  ", interrupt_id );
//     len += sprintf( buf + len, "\n\n" );

//     return	len;
// }

int uart_init(void) {
    
    printk("<1>Configuring uart module... :)\n");
    /*Generate new address required for iowrite and ioread methods*/
    addr = ioremap(UART_BASE, RANGE);

    // configure the UART
    outb(0x00,UART_INTR_EN);
    outb(0xC7,UART_FIFO_CTRL);
    outb(0x83,UART_LINE_CTRL);
    outb(0x01,UART_DLATCH_LO);
    outb(0x00,UART_DLATCH_HI);
    outb(0x03,UART_LINE_CTRL);
    outb(0x03,UART_MODEM_CTRL);
    inb(UART_MODEM_STAT);
    inb(UART_LINE_STAT);
    inb(UART_RX_DATA);
    inb(UART_INTR_ID);

    // if ( request_irq( UART_IRQ, uart_isr, IRQF_SHARED, 
    //             DEVICE_NAME, &DEVICE_NAME) < 0 ) return -EBUSY;
    outb( INTR_MASK, UART_INTR_EN );
    outb( 0x0B, UART_MODEM_CTRL );
    printk( " Interrupt-ID: %02X \n", inb( UART_INTR_ID ) );

    printk("<1>Inserting uart module... :)\n");
    int result;
    /* Registering device */
    result = register_chrdev(uart_major, DEVICE_NAME, &uart_fops);
    if (result < 0) {
        printk("<1>uart: cannot obtain major number %d\n", uart_major);
        return result;
    }
    /*Reserve uart memory region */
    // if(request_mem_region(UART_BASE, RANGE, DEVICE_NAME) == NULL) {
    //     printk("<1>uart: cannot reserve memory \n");
    //     unregister_chrdev(uart_major, DEVICE_NAME);
    //     return -ENODEV;
    // }

    printk("<1>Inserted uart module... :)\n");

    return SUCCESS;
}

void uart_exit(void) {
    /* Freeing the major number and memory space */
    unregister_chrdev(uart_major, DEVICE_NAME);
    //release_mem_region(UART_BASE, RANGE);
    printk("<1>Removing uart module\n");
}

int uart_open(struct inode *inode, struct file *filp) {
    printk("<1>Opening uart module\n");
    /*Avoid reading conflicts*/
    if (Device_Open)
        return -EBUSY;
    Device_Open++;
    return SUCCESS;
}

int uart_release(struct inode *inode, struct file *file) {
    printk("<1>Releasing uart module...\n");
    Device_Open--; //make ready for the next caller*/
    return SUCCESS;
}

ssize_t uart_read( struct file *file, char *buf, size_t len, loff_t *pos ) {
    int	count, i, line_status = inb( UART_LINE_STAT );
    printk("<1>Size being red %ld\n", len);
    if ( (line_status & 1) == 0 ) {
        if ( file->f_flags & O_NONBLOCK ) return 0;
        // if ( wait_event_interruptible( wq_recv,
        //                                (inb( UART_LINE_STAT ) & 1) ) ) return -EINTR;
    }

    printk("<1>Second phase");

    count = 0;
    for (i = 0; i < len; i++) {
        unsigned char	datum = inb( UART_RX_DATA );
        if ( copy_to_user( buf + i, &datum, 1 ) ){

            return -EFAULT;
        }
        ++count;
        printk("<1> rip");
        if ( (inb( UART_LINE_STAT ) & 1) == 0 ) break;
    }
    printk("<1>To return  %d\n",count);
    return	count;
}

ssize_t uart_write( struct file *file, const char *buf, size_t len, loff_t *pos ) {
    int	i, count, modem_status = inb( UART_MODEM_STAT );
    printk("<1>Size being written %ld\n", len);
    if ( (modem_status & 0x10 ) != 0x10 ) {
        if ( file->f_flags & O_NONBLOCK ) return 0;
        // if ( wait_event_interruptible( wq_xmit,
        //                                (inb( UART_MODEM_STAT ) & 0x10) == 0x10 ) ) return -EINTR;
    }

    count = 0;
    for (i = 0; i < len; i++) {
        unsigned char	datum;
        if ( copy_from_user( &datum, buf + i, 1 ) ) return -EFAULT;
        while ( (inb( UART_LINE_STAT ) & 0x20) == 0 );
        printk("<1>Writing %c\n", datum);
        outb( datum, UART_TX_DATA );
        ++count;
        if ( (inb( UART_MODEM_STAT ) & 0x10) != 0x10 ) break;
    }

    return	count;
}
