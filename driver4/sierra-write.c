/* Write */
static int sierra_write(struct tty_struct *tty, 
			struct usb_serial_port *port,
			const unsigned char *buf, int count)
{
	struct sierra_port_private *portdata = usb_get_serial_port_data(port);
	struct sierra_intf_private *intfdata;
	struct usb_serial *serial = port->serial;
	unsigned long flags;
	unsigned char *buffer;
	struct urb *urb;
	size_t writesize = min((size_t)count, (size_t)MAX_TRANSFER);
	int retval = 0;

	/* verify that we actually have some data to write */
	if (count == 0)
		return 0;

	dev_dbg(&port->dev, "%s: write (%zu bytes)\n", __func__, writesize);

	intfdata = serial->private;

	spin_lock_irqsave(&portdata->lock, flags);
	if (portdata->outstanding_urbs > portdata->num_out_urbs) {
		spin_unlock_irqrestore(&portdata->lock, flags);
		dev_dbg(&port->dev, "%s - write limit hit\n", __func__);
		return 0;
	}
	portdata->outstanding_urbs++;
	spin_unlock_irqrestore(&portdata->lock, flags);

	retval = usb_autopm_get_interface_async(serial->interface);
	if (unlikely(retval < 0)) {
		spin_lock_irqsave(&portdata->lock, flags);
		portdata->outstanding_urbs--;
		spin_unlock_irqrestore(&portdata->lock, flags);
		return retval;
	}

	buffer = kmalloc(writesize, GFP_ATOMIC);
	if (!buffer) {
		dev_err(&port->dev, "out of memory\n");
		spin_lock_irqsave(&portdata->lock, flags);
		--portdata->outstanding_urbs;
		spin_unlock_irqrestore(&portdata->lock, flags);
		usb_autopm_put_interface_async(serial->interface);
		return -ENOMEM;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		dev_err(&port->dev, "no more free urbs\n");
		kfree(buffer);
		spin_lock_irqsave(&portdata->lock, flags);
		--portdata->outstanding_urbs;
		spin_unlock_irqrestore(&portdata->lock, flags);
		usb_autopm_put_interface_async(serial->interface);
		return -ENOMEM;
	}

	memcpy(buffer, buf, writesize); 

	usb_serial_debug_data(debug, &port->dev, __func__, writesize, buffer);

	usb_fill_bulk_urb(urb, serial->dev,
			  usb_sndbulkpipe(serial->dev,
					  port->bulk_out_endpointAddress),
			  buffer, writesize, sierra_outdat_callback, port);

	/* Handle the need to send a zero length packet and release the
	 * transfer buffer
	 */
	urb->transfer_flags |= (URB_ZERO_PACKET | URB_FREE_BUFFER);

	spin_lock_irqsave(&intfdata->susp_lock, flags);

	if (intfdata->suspended) {
		usb_anchor_urb(urb, &portdata->delayed);
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);
		/* release our reference to this urb, the USB core will 
		 * eventually free it entirely */
		usb_free_urb(urb);
		return writesize;
	}
	usb_anchor_urb(urb, &portdata->active);

	/* send it down the pipe */
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval) {
		usb_unanchor_urb(urb);
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);

		dev_err(&port->dev, "%s - usb_submit_urb(write bulk) failed "
			"with status = %d\n", __func__, retval);
		usb_free_urb(urb);
		spin_lock_irqsave(&portdata->lock, flags);
		--portdata->outstanding_urbs;
		spin_unlock_irqrestore(&portdata->lock, flags);
		usb_autopm_put_interface_async(serial->interface);
		atomic_inc(&intfdata->stats.write_err);
		return retval;
	} else {
		intfdata->in_flight++;
		spin_unlock_irqrestore(&intfdata->susp_lock, flags);
		atomic_inc(&intfdata->stats.write_cnt);
		atomic_add(writesize, &intfdata->stats.tx_bytes);
	}
	/* release our reference to this urb, the USB core will eventually
	 * free it entirely */
	usb_free_urb(urb); 

	return writesize;
}
