// /*
//  * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
//  *
//  * Driver based on the 2.6.3 version of drivers/usb/usb-skeleton.c
//  */

// #include <linux/kernel.h>
// #include <linux/errno.h>
// #include <linux/slab.h>
// #include <linux/module.h>
// #include <linux/kref.h>
// #include <linux/uaccess.h>
// #include <linux/usb.h>
// #include <linux/mutex.h>


// #define USB_SKEL_VENDOR_ID	0x2341

// static const struct usb_device_id skel_table[] = {
// 	{ USB_DEVICE(USB_SKEL_VENDOR_ID,0x0042) },
// 	{ USB_DEVICE(USB_SKEL_VENDOR_ID, 0x0043) },
// 	{ }					
// };
// MODULE_DEVICE_TABLE(usb, skel_table);


// #define USB_SKEL_MINOR_BASE	192

// #define MAX_TRANSFER		(PAGE_SIZE - 512)
// #define WRITES_IN_FLIGHT	8

// struct usb_skel {
// 	struct usb_device	*udev;			
// 	struct usb_interface	*interface;	
// 	struct semaphore	limit_sem;		
// 	struct usb_anchor	submitted;		
// 	struct urb		*bulk_in_urb;
// 	unsigned char           *bulk_in_buffer;	
// 	size_t			bulk_in_size;		
// 	size_t			bulk_in_filled;		
// 	size_t			bulk_in_copied;		
// 	__u8			bulk_in_endpointAddr;	
// 	__u8			bulk_out_endpointAddr;	
// 	int			errors;			
// 	bool			ongoing_read;		
// 	spinlock_t		err_lock;		
// 	struct kref		kref;
// 	struct mutex		io_mutex;		
// 	wait_queue_head_t	bulk_in_wait;		
// };
// #define to_skel_dev(d) container_of(d, struct usb_skel, kref)

// static struct usb_driver skel_driver;
// static void skel_draw_down(struct usb_skel *dev);

// static void skel_delete(struct kref *kref){
// 	printk(KERN_INFO "Arduano Driver: Trying to delete");
// 	struct usb_skel *dev = to_skel_dev(kref);

// 	usb_free_urb(dev->bulk_in_urb);
// 	usb_put_dev(dev->udev);
// 	kfree(dev->bulk_in_buffer);
// 	kfree(dev);
// }

// static int skel_open(struct inode *inode, struct file *file){
// 	printk(KERN_INFO "Arduano Driver: Trying to open");
// 	struct usb_skel *dev;
// 	struct usb_interface *interface;
// 	int subminor;
// 	int retval = 0;

// 	subminor = iminor(inode);

// 	interface = usb_find_interface(&skel_driver, subminor);
// 	if (!interface) {
// 		pr_err("%s - error, can't find device for minor %d\n",
// 			__func__, subminor);
// 		retval = -ENODEV;
// 		goto exit;
// 	}

// 	dev = usb_get_intfdata(interface);
// 	if (!dev) {
// 		retval = -ENODEV;
// 		goto exit;
// 	}

// 	retval = usb_autopm_get_interface(interface);
// 	if (retval)
// 		goto exit;

// 	kref_get(&dev->kref);

// 	file->private_data = dev;

// exit:
// 	return retval;
// }

// static int skel_release(struct inode *inode, struct file *file){
// 	printk(KERN_INFO "Arduano Driver: Trying to release");
// 	struct usb_skel *dev;

// 	dev = file->private_data;
// 	if (dev == NULL)
// 		return -ENODEV;

// 	mutex_lock(&dev->io_mutex);
// 	if (dev->interface)
// 		usb_autopm_put_interface(dev->interface);
// 	mutex_unlock(&dev->io_mutex);

// 	kref_put(&dev->kref, skel_delete);
// 	return 0;
// }

// static int skel_flush(struct file *file, fl_owner_t id){
// 	printk(KERN_INFO "Arduano Driver: Trying to flush");
// 	struct usb_skel *dev;
// 	int res;

// 	dev = file->private_data;
// 	if (dev == NULL)
// 		return -ENODEV;

// 	mutex_lock(&dev->io_mutex);
// 	skel_draw_down(dev);

// 	spin_lock_irq(&dev->err_lock);
// 	res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
// 	dev->errors = 0;
// 	spin_unlock_irq(&dev->err_lock);

// 	mutex_unlock(&dev->io_mutex);

// 	return res;
// }

// static void skel_read_bulk_callback(struct urb *urb){
// 	printk(KERN_INFO "Arduano Driver: Trying to read bulk callback");
// 	struct usb_skel *dev;
// 	unsigned long flags;

// 	dev = urb->context;

// 	spin_lock_irqsave(&dev->err_lock, flags);
// 	if (urb->status) {
// 		if (!(urb->status == -ENOENT ||
// 		    urb->status == -ECONNRESET ||
// 		    urb->status == -ESHUTDOWN))
// 			dev_err(&dev->interface->dev,
// 				"%s - nonzero write bulk status received: %d\n",
// 				__func__, urb->status);

// 		dev->errors = urb->status;
// 	} else {
// 		dev->bulk_in_filled = urb->actual_length;
// 	}
// 	dev->ongoing_read = 0;
// 	spin_unlock_irqrestore(&dev->err_lock, flags);

// 	wake_up_interruptible(&dev->bulk_in_wait);
// }

// static int skel_do_read_io(struct usb_skel *dev, size_t count){
// 	printk(KERN_INFO "Arduano Driver: Trying to write");
// 	int rv;

// 	usb_fill_bulk_urb(dev->bulk_in_urb,
// 			dev->udev,
// 			usb_rcvbulkpipe(dev->udev,
// 				dev->bulk_in_endpointAddr),
// 			dev->bulk_in_buffer,
// 			min(dev->bulk_in_size, count),
// 			skel_read_bulk_callback,
// 			dev);
// 	spin_lock_irq(&dev->err_lock);
// 	dev->ongoing_read = 1;
// 	spin_unlock_irq(&dev->err_lock);

// 	dev->bulk_in_filled = 0;
// 	dev->bulk_in_copied = 0;

// 	rv = usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
// 	if (rv < 0) {
// 		dev_err(&dev->interface->dev,
// 			"%s - failed submitting read urb, error %d\n",
// 			__func__, rv);
// 		rv = (rv == -ENOMEM) ? rv : -EIO;
// 		spin_lock_irq(&dev->err_lock);
// 		dev->ongoing_read = 0;
// 		spin_unlock_irq(&dev->err_lock);
// 	}

// 	return rv;
// }

// static ssize_t skel_read(struct file *file, char *buffer, size_t count,
// 			 loff_t *ppos){
// 	printk(KERN_INFO "Arduano Driver: Trying to read");

// 	struct usb_skel *dev;
// 	int rv;
// 	bool ongoing_io;

// 	dev = file->private_data;

// 	if (!dev->bulk_in_urb || !count)
// 		return 0;

// 	rv = mutex_lock_interruptible(&dev->io_mutex);
// 	if (rv < 0)
// 		return rv;

// 	if (!dev->interface) {		
// 		rv = -ENODEV;
// 		goto exit;
// 	}

// retry:
// 	spin_lock_irq(&dev->err_lock);
// 	ongoing_io = dev->ongoing_read;
// 	spin_unlock_irq(&dev->err_lock);

// 	if (ongoing_io) {
// 		if (file->f_flags & O_NONBLOCK) {
// 			rv = -EAGAIN;
// 			goto exit;
// 		}
// 		rv = wait_event_interruptible(dev->bulk_in_wait, (!dev->ongoing_read));
// 		if (rv < 0)
// 			goto exit;
// 	}

// 	rv = dev->errors;
// 	if (rv < 0) {
// 		dev->errors = 0;
// 		rv = (rv == -EPIPE) ? rv : -EIO;
// 		goto exit;
// 	}

// 	if (dev->bulk_in_filled) {
// 		size_t available = dev->bulk_in_filled - dev->bulk_in_copied;
// 		size_t chunk = min(available, count);

// 		if (!available) {
			
// 			rv = skel_do_read_io(dev, count);
// 			if (rv < 0)
// 				goto exit;
// 			else
// 				goto retry;
// 		}

// 		if (copy_to_user(buffer,
// 				 dev->bulk_in_buffer + dev->bulk_in_copied,
// 				 chunk))
// 			rv = -EFAULT;
// 		else
// 			rv = chunk;

// 		dev->bulk_in_copied += chunk;

// 		if (available < count)
// 			skel_do_read_io(dev, count - chunk);
// 	} else {
// 		rv = skel_do_read_io(dev, count);
// 		if (rv < 0)
// 			goto exit;
// 		else
// 			goto retry;
// 	}
// exit:
// 	mutex_unlock(&dev->io_mutex);
// 	return rv;
// }

// static void skel_write_bulk_callback(struct urb *urb){
// 	printk(KERN_INFO "Arduano Driver: Trying to write bulk callback");
// 	struct usb_skel *dev;
// 	unsigned long flags;

// 	dev = urb->context;

// 	if (urb->status) {
// 		if (!(urb->status == -ENOENT ||
// 		    urb->status == -ECONNRESET ||
// 		    urb->status == -ESHUTDOWN))
// 			dev_err(&dev->interface->dev,
// 				"%s - nonzero write bulk status received: %d\n",
// 				__func__, urb->status);

// 		spin_lock_irqsave(&dev->err_lock, flags);
// 		dev->errors = urb->status;
// 		spin_unlock_irqrestore(&dev->err_lock, flags);
// 	}

// 	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
// 			  urb->transfer_buffer, urb->transfer_dma);
// 	up(&dev->limit_sem);
// }

// static ssize_t skel_write(struct file *file, const char *user_buffer,
// 			  size_t count, loff_t *ppos){
// 	printk(KERN_INFO "Arduano Driver: Trying to write");
// 	struct usb_skel *dev;
// 	int retval = 0;
// 	struct urb *urb = NULL;
// 	char *buf = NULL;
// 	size_t writesize = min(count, (size_t)MAX_TRANSFER);

// 	dev = file->private_data;

// 	if (count == 0)
// 		goto exit;

	
// 	if (!(file->f_flags & O_NONBLOCK)) {
// 		if (down_interruptible(&dev->limit_sem)) {
// 			retval = -ERESTARTSYS;
// 			goto exit;
// 		}
// 	} else {
// 		if (down_trylock(&dev->limit_sem)) {
// 			retval = -EAGAIN;
// 			goto exit;
// 		}
// 	}

// 	spin_lock_irq(&dev->err_lock);
// 	retval = dev->errors;
// 	if (retval < 0) {
// 		dev->errors = 0;
// 		retval = (retval == -EPIPE) ? retval : -EIO;
// 	}
// 	spin_unlock_irq(&dev->err_lock);
// 	if (retval < 0)
// 		goto error;

// 	urb = usb_alloc_urb(0, GFP_KERNEL);
// 	if (!urb) {
// 		retval = -ENOMEM;
// 		goto error;
// 	}

// 	buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
// 				 &urb->transfer_dma);
// 	if (!buf) {
// 		retval = -ENOMEM;
// 		goto error;
// 	}

// 	if (copy_from_user(buf, user_buffer, writesize)) {
// 		retval = -EFAULT;
// 		goto error;
// 	}

// 	mutex_lock(&dev->io_mutex);
// 	if (!dev->interface) {		
// 		mutex_unlock(&dev->io_mutex);
// 		retval = -ENODEV;
// 		goto error;
// 	}

// 	usb_fill_bulk_urb(urb, dev->udev,
// 			  usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
// 			  buf, writesize, skel_write_bulk_callback, dev);
// 	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
// 	usb_anchor_urb(urb, &dev->submitted);

// 	retval = usb_submit_urb(urb, GFP_KERNEL);
// 	mutex_unlock(&dev->io_mutex);
// 	if (retval) {
// 		dev_err(&dev->interface->dev,
// 			"%s - failed submitting write urb, error %d\n",
// 			__func__, retval);
// 		goto error_unanchor;
// 	}
// 	usb_free_urb(urb);


// 	return writesize;

// error_unanchor:
// 	usb_unanchor_urb(urb);
// error:
// 	if (urb) {
// 		usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
// 		usb_free_urb(urb);
// 	}
// 	up(&dev->limit_sem);

// exit:
// 	return retval;
// }

// static const struct file_operations skel_fops = {
// 	.owner =	THIS_MODULE,
// 	.read =		skel_read,
// 	.write =	skel_write,
// 	.open =		skel_open,
// 	.release =	skel_release,
// 	.flush =	skel_flush,
// 	.llseek =	noop_llseek,
// };

// static struct usb_class_driver skel_class = {
// 	.name =		"arduano%d",
// 	.fops =		&skel_fops,
// 	.minor_base =	USB_SKEL_MINOR_BASE,
// };

// static int skel_probe(struct usb_interface *interface,
// 		      const struct usb_device_id *id){
// 	printk(KERN_INFO "Arduano Driver: Trying to probe");
// 	struct usb_skel *dev;
// 	struct usb_endpoint_descriptor *bulk_in, *bulk_out;
// 	int retval;

// 	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
// 	if (!dev)
// 		return -ENOMEM;

// 	kref_init(&dev->kref);
// 	sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
// 	mutex_init(&dev->io_mutex);
// 	spin_lock_init(&dev->err_lock);
// 	init_usb_anchor(&dev->submitted);
// 	init_waitqueue_head(&dev->bulk_in_wait);

// 	dev->udev = usb_get_dev(interface_to_usbdev(interface));
// 	dev->interface = interface;



// 	// retval = usb_find_common_endpoints(interface->cur_altsetting,
// 	// 		&bulk_in, &bulk_out, NULL, NULL);
// 	// if (retval) {
// 	// 	dev_err(&interface->dev,
// 	// 		"Could not find both bulk-in and bulk-out endpoints\n");
// 	// 	goto error;
// 	// }

// 	// dev->bulk_in_size = usb_endpoint_maxp(bulk_in);
// 	// dev->bulk_in_endpointAddr = bulk_in->bEndpointAddress;
// 	// dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
// 	// if (!dev->bulk_in_buffer) {
// 	// 	retval = -ENOMEM;
// 	// 	goto error;
// 	// }
// 	// dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
// 	// if (!dev->bulk_in_urb) {
// 	// 	retval = -ENOMEM;
// 	// 	goto error;
// 	// }

// 	// dev->bulk_out_endpointAddr = bulk_out->bEndpointAddress;



// 	// Recyling code

//     struct usb_interface *data_interface;
//     struct usb_endpoint_descriptor *epctrl = NULL;
//     struct usb_endpoint_descriptor *epread = NULL;
//     struct usb_endpoint_descriptor *epwrite = NULL;
//     // if (quirks == IGNORE_DEVICE)
//     // 	return -ENODEV;

//     // num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ACM_NR;

//     // /* handle quirks deadly to normal probing*/
//     // if (quirks == NO_UNION_NORMAL) {
//     data_interface = usb_ifnum_to_if(dev->udev, 1);
//     /* we would crash */
//     if (!data_interface)
//         return -ENODEV;
//     //goto skip_normal_probe;
//     //}


//     // I think this sections does define the ep and that stuff

//     for (int i = 0; i < data_interface->cur_altsetting->desc.bNumEndpoints; i++) {
//         struct usb_endpoint_descriptor *ep;
//         ep = &data_interface->cur_altsetting->endpoint[i].desc;

//         	// if (usb_endpoint_is_int_in(ep))
//         	// 	epctrl = ep;
//         	// else 
//         	if (usb_endpoint_is_bulk_out(ep))
//         		epwrite = ep;
//         	else if (usb_endpoint_is_bulk_in(ep))
//         		epread = ep;
//         	// else
//         	// 	return -EINVAL;
       
//     }

//     // End of recycling
//     printk(KERN_INFO "Arduano Driver: Endpoints set");
//     dev->bulk_in_size = usb_endpoint_maxp(epread);
// 	dev->bulk_in_endpointAddr = epread->bEndpointAddress;
// 	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
// 	if (!dev->bulk_in_buffer) {
// 		retval = -ENOMEM;
// 	}
// 	dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
// 	if (!dev->bulk_in_urb) {
// 		retval = -ENOMEM;
// 	}

// 	dev->bulk_out_endpointAddr = epwrite->bEndpointAddress;



// 	usb_set_intfdata(interface, dev);

// 	retval = usb_register_dev(interface, &skel_class);
// 	if (retval) {
// 		dev_err(&interface->dev,
// 			"Not able to get a minor for this device.\n");
// 		usb_set_intfdata(interface, NULL);
// 		goto error;
// 	}

// 	dev_info(&interface->dev,
// 		 "USB Skeleton device now attached to USBSkel-%d",
// 		 interface->minor);
// 	return 0;

// error:
// 	kref_put(&dev->kref, skel_delete);

// 	return retval;
// }

// static void skel_disconnect(struct usb_interface *interface){
// 	printk(KERN_INFO "Arduano Driver: Trying to disconnect");
// 	struct usb_skel *dev;
// 	int minor = interface->minor;

// 	dev = usb_get_intfdata(interface);
// 	usb_set_intfdata(interface, NULL);

// 	usb_deregister_dev(interface, &skel_class);

// 	mutex_lock(&dev->io_mutex);
// 	dev->interface = NULL;
// 	mutex_unlock(&dev->io_mutex);

// 	usb_kill_anchored_urbs(&dev->submitted);

// 	kref_put(&dev->kref, skel_delete);

// 	dev_info(&interface->dev, "USB Skeleton #%d now disconnected", minor);
// }

// static void skel_draw_down(struct usb_skel *dev){
// 	printk(KERN_INFO "Arduano Driver: Trying to draw down");
// 	int time;

// 	time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
// 	if (!time)
// 		usb_kill_anchored_urbs(&dev->submitted);
// 	usb_kill_urb(dev->bulk_in_urb);
// }

// static int skel_suspend(struct usb_interface *intf, pm_message_t message){
// 	printk(KERN_INFO "Arduano Driver: Trying to suspend");
// 	struct usb_skel *dev = usb_get_intfdata(intf);

// 	if (!dev)
// 		return 0;
// 	skel_draw_down(dev);
// 	return 0;
// }

// static int skel_resume(struct usb_interface *intf){
// 	printk(KERN_INFO "Arduano Driver: Trying to resume");
// 	return 0;
// }

// static int skel_pre_reset(struct usb_interface *intf){
// 	printk(KERN_INFO "Arduano Driver: Trying to pre reset");
// 	struct usb_skel *dev = usb_get_intfdata(intf);

// 	mutex_lock(&dev->io_mutex);
// 	skel_draw_down(dev);

// 	return 0;
// }

// static int skel_post_reset(struct usb_interface *intf){
// 	printk(KERN_INFO "Arduano Driver: Trying to post reset");
// 	struct usb_skel *dev = usb_get_intfdata(intf);

// 	dev->errors = -EPIPE;
// 	mutex_unlock(&dev->io_mutex);

// 	return 0;
// }

// static struct usb_driver skel_driver = {
// 	.name =		"skeleton",
// 	.probe =	skel_probe,
// 	.disconnect =	skel_disconnect,
// 	.suspend =	skel_suspend,
// 	.resume =	skel_resume,
// 	.pre_reset =	skel_pre_reset,
// 	.post_reset =	skel_post_reset,
// 	.id_table =	skel_table,
// 	.supports_autosuspend = 1,
// };

// module_usb_driver(skel_driver);

// MODULE_LICENSE("GPL v2");
