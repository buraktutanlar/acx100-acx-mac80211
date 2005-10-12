/***********************************************************************
** Copyright (C) 2003  ACX100 Open Source Project
**
** The contents of this file are subject to the Mozilla Public
** License Version 1.1 (the "License"); you may not use this file
** except in compliance with the License. You may obtain a copy of
** the License at http://www.mozilla.org/MPL/
**
** Software distributed under the License is distributed on an "AS
** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
** implied. See the License for the specific language governing
** rights and limitations under the License.
**
** Alternatively, the contents of this file may be used under the
** terms of the GNU Public License version 2 (the "GPL"), in which
** case the provisions of the GPL are applicable instead of the
** above.  If you wish to allow the use of your version of this file
** only under the terms of the GPL and not to allow others to use
** your version of this file under the MPL, indicate your decision
** by deleting the provisions above and replace them with the notice
** and other provisions required by the GPL.  If you do not delete
** the provisions above, a recipient may use your version of this
** file under either the MPL or the GPL.
** ---------------------------------------------------------------------
** Inquiries regarding the ACX100 Open Source Project can be
** made directly to:
**
** acx100-users@lists.sf.net
** http://acx100.sf.net
** ---------------------------------------------------------------------
*/

/***********************************************************************
** USB support for TI ACX100 based devices. Many parts are taken from
** the PCI driver.
**
** Authors:
**  Martin Wawro <martin.wawro AT uni-dortmund.de>
**  Andreas Mohr <andi AT lisas.de>
**
** Issues:
**  - Note that this driver relies on a native little-endian byteformat
**    at some points
**
** LOCKING
** callback functions called by USB core are running in interrupt context
** and thus have names with _i_.
*/
#define ACX_USB 1

#include <linux/version.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif

#include "acx.h"


/***********************************************************************
*/
/* number of endpoints of an interface */
#define NUM_EP(intf) (intf)->altsetting[0].desc.bNumEndpoints
#define EP(intf, nr) (intf)->altsetting[0].endpoint[(nr)].desc
#define GET_DEV(udev) usb_get_dev((udev))
#define PUT_DEV(udev) usb_put_dev((udev))
#define SET_NETDEV_OWNER(ndev, owner) /* not needed anymore ??? */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
/* removed in 2.6.14. We will use fake value for now */
#define URB_ASYNC_UNLINK 0
#endif


/***********************************************************************
*/
#define ACX100_VENDOR_ID 0x2001
#define ACX100_PRODUCT_ID_UNBOOTED 0x3B01
#define ACX100_PRODUCT_ID_BOOTED 0x3B00

/* RX-Timeout: NONE (request waits forever) */
#define ACX100_USB_RX_TIMEOUT (0)

#define ACX100_USB_TX_TIMEOUT (4*HZ)

#define USB_CTRL_HARD_TIMEOUT 5500   /* steps in ms */


/***********************************************************************
** Prototypes
*/
static int acx100usb_e_probe(struct usb_interface *, const struct usb_device_id *);
static void acx100usb_e_disconnect(struct usb_interface *);
static void acx100usb_i_complete_tx(struct urb *, struct pt_regs *);
static void acx100usb_i_complete_rx(struct urb *, struct pt_regs *);
static int acx100usb_e_open(struct net_device *);
static int acx100usb_e_close(struct net_device *);
static void acx100usb_i_set_rx_mode(struct net_device *);
static int acx100usb_e_init_network_device(struct net_device *);
static int acx100usb_boot(struct usb_device *);

static struct net_device_stats * acx_e_get_stats(struct net_device *);
static struct iw_statistics *acx_e_get_wireless_stats(struct net_device *);

static void acx100usb_l_poll_rx(wlandevice_t *, usb_rx_t* rx);

static void acx100usb_i_tx_timeout(struct net_device *);

/* static void dump_device(struct usb_device *); */
/* static void dump_device_descriptor(struct usb_device_descriptor *); */
/* static void dump_config_descriptor(struct usb_config_descriptor *); */

/***********************************************************************
** Module Data
*/
#define TXBUFSIZE sizeof(usb_txbuffer_t)
//// Bogus! We CANNOT pretend that rxbuffer_t is larger than it is.
/* make it a multiply of 64 */
/* #define RXBUFSIZE ((sizeof(rxbuffer_t)+63) & ~63) */
#define RXBUFSIZE sizeof(rxbuffer_t)

static const struct usb_device_id
acx100usb_ids[] = {
	{ USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_BOOTED) },
	{ USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_UNBOOTED) },
	{}
};


/* USB driver data structure as required by the kernel's USB core */
static struct usb_driver
acx100usb_driver = {
	.name = "acx_usb",
	.owner = THIS_MODULE,
	.probe = acx100usb_e_probe,
	.disconnect = acx100usb_e_disconnect,
	.id_table = acx100usb_ids
};


/***********************************************************************
** USB helper
**
** ldd3 ch13 says:
** When the function is usb_kill_urb, the urb lifecycle is stopped. This
** function is usually used when the device is disconnected from the system,
** in the disconnect callback. For some drivers, the usb_unlink_urb function
** should be used to tell the USB core to stop an urb. This function does not
** wait for the urb to be fully stopped before returning to the caller.
** This is useful for stoppingthe urb while in an interrupt handler or when
** a spinlock is held, as waiting for a urb to fully stop requires the ability
** for the USB core to put the calling process to sleep. This function requires
** that the URB_ASYNC_UNLINK flag value be set in the urb that is being asked
** to be stopped in order to work properly.
**
** (URB_ASYNC_UNLINK is obsolete, usb_unlink_urb will always be
** asynchronous while usb_kill_urb is synchronous and should be called
** directly (drivers/usb/core/urb.c))
**
** In light of this, timeout is just for paranoid reasons...
*
* Actually, it's useful for debugging. If we reach timeout, we're doing
* something wrong with the urbs.
*/
static void
acx_unlink_urb(struct urb* urb)
{
	if (!urb)
		return;

	if (urb->status == -EINPROGRESS) {
		int timeout = 10;

		usb_unlink_urb(urb);
		while (--timeout && urb->status == -EINPROGRESS) {
			mdelay(1);
		}
		if (!timeout) {
			printk("acx_usb: urb unlink timeout!\n");
		}
	}
}


/***********************************************************************
** EEPROM and PHY read/write helpers
*/
/***********************************************************************
** acxusb_s_read_phy_reg
*/
int
acxusb_s_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf)
{
	mem_read_write_t mem;

	FN_ENTER;

	mem.addr = cpu_to_le16(reg);
	mem.type = cpu_to_le16(0x82);
	mem.len = cpu_to_le32(4);
	acx_s_issue_cmd(priv, ACX1xx_CMD_MEM_READ, &mem, sizeof(mem));
	*charbuf = mem.data;
	acxlog(L_DEBUG, "read radio PHY[0x%04X]=0x%02X\n", reg, *charbuf);

	FN_EXIT1(OK);
	return OK;
}


/***********************************************************************
*/
int
acxusb_s_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value)
{
	mem_read_write_t mem;

	FN_ENTER;

	mem.addr = cpu_to_le16(reg);
	mem.type = cpu_to_le16(0x82);
	mem.len = cpu_to_le32(4);
	mem.data = value;
	acx_s_issue_cmd(priv, ACX1xx_CMD_MEM_WRITE, &mem, sizeof(mem));
	acxlog(L_DEBUG, "write radio PHY[0x%04X]=0x%02X\n", reg, value);

	FN_EXIT1(OK);
	return OK;
}


/***********************************************************************
** acx_s_issue_cmd_timeo
** Excecutes a command in the command mailbox
**
** buffer = a pointer to the data.
** The data must not include 4 byte command header
*/

/* TODO: ideally we shall always know how much we need
** and this shall be 0 */
#define BOGUS_SAFETY_PADDING 0x40

#undef FUNC
#define FUNC "issue_cmd"

#if !ACX_DEBUG
int
acxusb_s_issue_cmd_timeo(
	wlandevice_t *priv,
	unsigned cmd,
	void *buffer,
	unsigned buflen,
	unsigned timeout)
{
#else
int
acxusb_s_issue_cmd_timeo_debug(
	wlandevice_t *priv,
	unsigned cmd,
	void *buffer,
	unsigned buflen,
	unsigned timeout,
	const char* cmdstr)
{
#endif
	/* USB ignores timeout param */

	struct usb_device *usbdev;
	struct {
		u16	cmd ACX_PACKED;
		u16	status ACX_PACKED;
		u8	data[1] ACX_PACKED;
	} *loc;
	const char *devname;
	int acklen, blocklen, inpipe, outpipe;
	int cmd_status;
	int result;

	FN_ENTER;

	devname = priv->netdev->name;
	if (!devname || !devname[0])
		devname = "acx";

	acxlog(L_CTL, FUNC"(cmd:%s,buflen:%u,type:0x%04X)\n",
		cmdstr, buflen,
		buffer ? le16_to_cpu(((acx_ie_generic_t *)buffer)->type) : -1);

	loc = kmalloc(buflen + 4 + BOGUS_SAFETY_PADDING, GFP_KERNEL);
	if (!loc) {
		printk("%s: "FUNC"(): no memory for data buffer\n", devname);
		goto bad;
	}

	/* get context from wlandevice */
	usbdev = priv->usbdev;

	/* check which kind of command was issued */
	loc->cmd = cpu_to_le16(cmd);
	loc->status = 0;

/* NB: buflen == frmlen + 4
**
** Interrogate: write 8 bytes: (cmd,status,rid,frmlen), then
**		read (cmd,status,rid,frmlen,data[frmlen]) back
**
** Configure: write (cmd,status,rid,frmlen,data[frmlen])
**
** Possibly bogus special handling of ACX1xx_IE_SCAN_STATUS removed
*/

	/* now write the parameters of the command if needed */
	acklen = buflen + 4 + BOGUS_SAFETY_PADDING;
	blocklen = buflen;
	if (buffer && buflen) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
		if (cmd == ACX1xx_CMD_INTERROGATE) {
			blocklen = 4;
			acklen = buflen + 4;
		}
		memcpy(loc->data, buffer, blocklen);
	}
	blocklen += 4; /* account for cmd,status */

	/* obtain the I/O pipes */
	outpipe = usb_sndctrlpipe(usbdev, 0);
	inpipe = usb_rcvctrlpipe(usbdev, 0);
	acxlog(L_CTL, "ctrl inpipe=0x%X outpipe=0x%X\n", inpipe, outpipe);
	acxlog(L_CTL, "sending USB control msg (out) (blocklen=%d)\n", blocklen);
	if (acx_debug & L_DATA)
		acx_dump_bytes(loc, blocklen);

	result = usb_control_msg(usbdev, outpipe,
		ACX_USB_REQ_CMD, /* request */
		USB_TYPE_VENDOR|USB_DIR_OUT, /* requesttype */
		0, /* value */
		0, /* index */
		loc, /* dataptr */
		blocklen, /* size */
		USB_CTRL_HARD_TIMEOUT /* timeout in ms */
	);

	if (result == -ENODEV) {
		acxlog(L_CTL, "no device present (unplug?)\n");
		goto good;
	}

	acxlog(L_CTL, "wrote %d bytes\n", result);
	if (result < 0) {
		goto bad;
	}

	/* check for device acknowledge */
	acxlog(L_CTL, "sending USB control msg (in) (acklen=%d)\n", acklen);
	loc->status = 0; /* delete old status flag -> set to IDLE */
//shall we zero out the rest?
	result = usb_control_msg(usbdev, inpipe,
		ACX_USB_REQ_CMD, /* request */
		USB_TYPE_VENDOR|USB_DIR_IN, /* requesttype */
		0, /* value */
		0, /* index */
		loc, /* dataptr */
		acklen, /* size */
		USB_CTRL_HARD_TIMEOUT /* timeout in ms */
	);
	if (result < 0) {
		printk("%s: "FUNC"(): USB read error %d\n", devname, result);
		goto bad;
	}
	if (acx_debug & L_CTL) {
		printk("read %d bytes: ", result);
		acx_dump_bytes(loc, result);
	}

//check for result==buflen+4? Was seen:
/*
interrogate(type:ACX100_IE_DOT11_ED_THRESHOLD,len:4)
issue_cmd(cmd:ACX1xx_CMD_INTERROGATE,buflen:8,type:4111)
ctrl inpipe=0x80000280 outpipe=0x80000200
sending USB control msg (out) (blocklen=8)
01 00 00 00 0F 10 04 00
wrote 8 bytes
sending USB control msg (in) (acklen=12) sizeof(loc->data
read 4 bytes <==== MUST BE 12!!
*/

	cmd_status = le16_to_cpu(loc->status);
	if (cmd_status != 1) {
		printk("%s: "FUNC"(): cmd_status is not SUCCESS: %d (%s)\n",
			devname, cmd_status, acx_cmd_status_str(cmd_status));
		/* TODO: goto bad; ? */
	}
	if ((cmd == ACX1xx_CMD_INTERROGATE) && buffer && buflen) {
		memcpy(buffer, loc->data, buflen);
		acxlog(L_CTL, "response frame: cmd=0x%04X status=%d\n",
			le16_to_cpu(loc->cmd),
			cmd_status);
	}
good:
	kfree(loc);
	FN_EXIT1(OK);
	return OK;
bad:
	/* Give enough info so that callers can avoid
	** printing their own diagnostic messages */
#if ACX_DEBUG
	printk("%s: "FUNC"(cmd:%s) FAILED\n", devname, cmdstr);
#else
	printk("%s: "FUNC"(cmd:0x%04X) FAILED\n", devname, cmd);
#endif
	dump_stack();
	kfree(loc);
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***********************************************************************
** acx100usb_e_probe()
**
** Inputs:
**    dev -> Pointer to usb_device structure that may or may not be claimed
**  ifNum -> Interface number
**  devID -> Device ID (vendor and product specific stuff)
************************************************************************
** Returns:
**  (void *) Pointer to (custom) driver context or NULL if we are not interested
**           or unable to handle the offered device.
**
** Description:
**  This function is invoked by the kernel's USB core whenever a new device is
**  attached to the system or the module is loaded. It is presented a usb_device
**  structure from which information regarding the device is obtained and evaluated.
**  In case this driver is able to handle one of the offered devices, it returns
**  a non-null pointer to a driver context and thereby claims the device.
*/
static void
acx_netdev_init(struct net_device *dev) {}

static int
acx100usb_e_probe(struct usb_interface *intf, const struct usb_device_id *devID)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	wlandevice_t *priv = NULL;
	struct net_device *dev = NULL;
	struct usb_config_descriptor *config;
	struct usb_endpoint_descriptor *epdesc;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
	struct usb_host_endpoint *ep;
#endif
	struct usb_interface_descriptor *ifdesc;
	const char* msg;
	int numconfigs, numfaces, numep;
	int result = OK;
	int i;

	FN_ENTER;

	/* First check if this is the "unbooted" hardware */
	if ((usbdev->descriptor.idVendor == ACX100_VENDOR_ID)
	 && (usbdev->descriptor.idProduct == ACX100_PRODUCT_ID_UNBOOTED)) {
		/* Boot the device (i.e. upload the firmware) */
		acx100usb_boot(usbdev);

		/* OK, we are done with booting. Normally, the
		** ID for the unbooted device should disappear
		** and it will not need a driver anyway...so
		** return a NULL
		*/
		acxlog(L_INIT, "finished booting, returning from probe()\n");
		result = OK; /* success */
		goto end;
	}

	if ((usbdev->descriptor.idVendor != ACX100_VENDOR_ID)
	 || (usbdev->descriptor.idProduct != ACX100_PRODUCT_ID_BOOTED)) {
		goto end_nodev;
	}

	/* Ok, so it's our device and it is already booted */

	/* Allocate memory for a network device */
	dev = alloc_netdev(sizeof(wlandevice_t), "wlan%d", acx_netdev_init);
	/* (NB: memsets to 0 entire area) */
	if (!dev) {
		msg = "acx: no memory for netdev\n";
		goto end_nomem;
	}
	dev->init = (void *)&acx100usb_e_init_network_device;

	/* Setup private driver context */
	priv = netdev_priv(dev);
	priv->netdev = dev;
	priv->dev_type = DEVTYPE_USB;
	priv->chip_type = CHIPTYPE_ACX100;
	/* FIXME: should be read from register (via firmware) using standard ACX code */
	priv->radio_type = RADIO_MAXIM_0D;
	priv->usbdev = usbdev;

	spin_lock_init(&priv->lock);    /* initial state: unlocked */
	sema_init(&priv->sem, 1);       /* initial state: 1 (upped) */

	/* Initialize the device context and also check
	** if this is really the hardware we know about.
	** If not sure, at least notify the user that he
	** may be in trouble...
	*/
	numconfigs = (int)usbdev->descriptor.bNumConfigurations;
	if (numconfigs != 1)
		printk("acx: number of configurations is %d, "
			"this driver only knows how to handle 1, "
			"be prepared for surprises\n", numconfigs);

	config = &usbdev->config->desc;
	numfaces = config->bNumInterfaces;
	if (numfaces != 1)
		printk("acx: number of interfaces is %d, "
			"this driver only knows how to handle 1, "
			"be prepared for surprises\n", numfaces);

	ifdesc = &intf->altsetting->desc;
	numep = ifdesc->bNumEndpoints;
	acxlog(L_DEBUG, "# of endpoints: %d\n", numep);

	/* obtain information about the endpoint
	** addresses, begin with some default values
	*/
	priv->bulkoutep = 1;
	priv->bulkinep = 1;
	for (i = 0; i < numep; i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
		ep = usbdev->ep_in[i];
		if (!ep)
			continue;
		epdesc = &ep->desc;
#else
		epdesc = usb_epnum_to_ep_desc(usbdev, i);
		if (!epdesc)
			continue;
#endif
		if (epdesc->bmAttributes & USB_ENDPOINT_XFER_BULK) {
			if (epdesc->bEndpointAddress & 0x80)
				priv->bulkinep = epdesc->bEndpointAddress & 0xF;
			else
				priv->bulkoutep = epdesc->bEndpointAddress & 0xF;
		}
	}
	acxlog(L_DEBUG, "bulkout ep: 0x%X\n", priv->bulkoutep);
	acxlog(L_DEBUG, "bulkin ep: 0x%X\n", priv->bulkinep);

	/* Set the packet-size equivalent to the buffer size */
	/* already done by memset: priv->rxtruncsize = 0; */
	acxlog(L_DEBUG, "TXBUFSIZE=%d RXBUFSIZE=%d\n",
				(int) TXBUFSIZE, (int) RXBUFSIZE);

	priv->tx_free = ACX100_USB_NUM_BULK_URBS;

	/* Allocate the RX/TX containers. */
	priv->usb_tx = kmalloc(sizeof(usb_tx_t) * ACX100_USB_NUM_BULK_URBS,
			       GFP_KERNEL);
	if (!priv->usb_tx) {
		msg = "acx: no memory for tx container";
		goto end_nomem;
	}
	priv->usb_rx = kmalloc(sizeof(usb_rx_t) * ACX100_USB_NUM_BULK_URBS,
			       GFP_KERNEL);
	if (!priv->usb_rx) {
		msg = "acx: no memory for rx container";
		goto end_nomem;
	}

	/* Setup URBs for bulk-in/out messages */
	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++) {
		priv->usb_rx[i].urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!priv->usb_rx[i].urb) {
			msg = "acx: no memory for input URB\n";
			goto end_nomem;
		}
		priv->usb_rx[i].urb->status = 0;
		priv->usb_rx[i].priv = priv;

		priv->usb_tx[i].urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!priv->usb_tx[i].urb) {
			msg = "acx: no memory for output URB\n";
			goto end_nomem;
		}
		priv->usb_tx[i].urb->status = 0;
		priv->usb_tx[i].priv = priv;
	}

	usb_set_intfdata(intf, priv);
	SET_NETDEV_DEV(dev, &intf->dev);

	/* Register the network device */
	acxlog(L_INIT, "registering network device\n");
	result = register_netdev(dev);
	if (result != 0) {
		msg = "acx: failed to register network device "
			"for USB WLAN (errcode=%d)\n";
		goto end_nomem;
	}

	if (OK != acx_proc_register_entries(dev)) {
		printk("acx: /proc registration failed\n");
	}

	printk("acx: USB module " WLAN_RELEASE " loaded successfully\n");

#if CMD_DISCOVERY
	great_inquisitor(priv);
#endif

	/* Everything went OK, we are happy now	*/
	result = OK;
	goto end;

end_nomem:
	printk(msg, result);

	if (dev) {
		if (priv->usb_rx) {
			for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++)
				usb_free_urb(priv->usb_rx[i].urb);
			kfree(priv->usb_rx);
		}
		if (priv->usb_tx) {
			for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++)
				usb_free_urb(priv->usb_tx[i].urb);
			kfree(priv->usb_tx);
		}
		free_netdev(dev);
	}

	result = -ENOMEM;
	goto end;

end_nodev:

	/* no device we could handle, return error. */
	result = -EIO;

end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acx100usb_e_disconnect()
**
** This function is invoked whenever the user pulls the plug from the USB
** device or the module is removed from the kernel. In these cases, the
** network devices have to be taken down and all allocated memory has
** to be freed.
*/
static void
acx100usb_e_disconnect(struct usb_interface *intf)
{
	wlandevice_t *priv = usb_get_intfdata(intf);
	int i;
	unsigned long flags;

	FN_ENTER;

	/* No WLAN device... no sense */
	if (!priv)
		goto end;

	/* Unregister network device
	 *
	 * If the interface is up, unregister_netdev() will take
	 * care of calling our close() function, which takes
	 * care of unlinking the urbs, sending the device to
	 * sleep, etc...
	 * This can't be called with sem or lock held because
	 * _close() will try to grab it as well if it's called,
	 * deadlocking the machine.
	 */
	unregister_netdev(priv->netdev);

	acx_sem_lock(priv);
	acx_lock(priv, flags);
	/* This device exists no more */
	usb_set_intfdata(intf, NULL);
	acx_proc_unregister_entries(priv->netdev);

	/*
	 * Here we only free them. _close() takes care of
	 * unlinking them.
	 */
	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; ++i) {
		usb_free_urb(priv->usb_rx[i].urb);
		usb_free_urb(priv->usb_tx[i].urb);
	}

	/* The the containers. */
	kfree(priv->usb_rx);
	kfree(priv->usb_tx);

	acx_unlock(priv, flags);
	acx_sem_unlock(priv);

	free_netdev(priv->netdev);
end:
	FN_EXIT0;
}


/***********************************************************************
** acx100usb_boot():
** Inputs:
**    usbdev -> Pointer to kernel's usb_device structure
**  endpoint -> Address of the endpoint for control transfers
************************************************************************
** Returns:
**  (int) Errorcode or 0 on success
**
** Description:
**  This function triggers the loading of the firmware image from harddisk
**  and then uploads the firmware to the USB device. After uploading the
**  firmware and transmitting the checksum, the device resets and appears
**  as a new device on the USB bus (the device we can finally deal with)
*/
static int
acx100usb_boot(struct usb_device *usbdev)
{
	static const char filename[] = "tiacx100usb";

	char *firmware = NULL;
	char *usbbuf;
	unsigned int offset;
	unsigned int len, inpipe, outpipe;
	u32 checksum;
	u32 size;
	int result;

	FN_ENTER;

	usbbuf = kmalloc(ACX100_USB_RWMEM_MAXLEN, GFP_KERNEL);
	if (!usbbuf) {
		printk(KERN_ERR "acx: no memory for USB transfer buffer ("
			STRING(ACX100_USB_RWMEM_MAXLEN)" bytes)\n");
		result = -ENOMEM;
		goto end;
	}
	firmware = (char *)acx_s_read_fw(&usbdev->dev, filename, &size);
	if (!firmware) {
		result = -EIO;
		goto end;
	}
	acxlog(L_INIT, "firmware size: %d bytes\n", size);

	/* Obtain the I/O pipes */
	outpipe = usb_sndctrlpipe(usbdev, 0);
	inpipe = usb_rcvctrlpipe(usbdev, 0);

	/* now upload the firmware, slice the data into blocks */
	offset = 8;
	while (offset < size) {
		len = size - offset;
		if (len >= ACX100_USB_RWMEM_MAXLEN) {
			len = ACX100_USB_RWMEM_MAXLEN;
		}
		acxlog(L_INIT, "uploading firmware (%d bytes, offset=%d)\n",
						len, offset);
		result = 0;
		memcpy(usbbuf, firmware + offset, len);
		result = usb_control_msg(usbdev, outpipe,
			ACX_USB_REQ_UPLOAD_FW,
			USB_TYPE_VENDOR|USB_DIR_OUT,
			size - 8, /* value */
			0, /* index */
			usbbuf, /* dataptr */
			len, /* size */
			3000 /* timeout in ms */
		);
		offset += len;
		if (result < 0) {
			printk(KERN_ERR "acx: error %d during upload "
				"of firmware, aborting\n", result);
			goto end;
		}
	}

	/* finally, send the checksum and reboot the device */
	checksum = le32_to_cpu(*(u32 *)firmware);
	/* is this triggers the reboot? */
	result = usb_control_msg(usbdev, outpipe,
		ACX_USB_REQ_UPLOAD_FW,
		USB_TYPE_VENDOR|USB_DIR_OUT,
		checksum & 0xffff, /* value */
		checksum >> 16, /* index */
		NULL, /* dataptr */
		0, /* size */
		3000 /* timeout in ms */
	);
	if (result < 0) {
		printk(KERN_ERR "acx: error %d during tx of checksum, "
				"aborting\n", result);
		goto end;
	}
	result = usb_control_msg(usbdev, inpipe,
		ACX_USB_REQ_ACK_CS,
		USB_TYPE_VENDOR|USB_DIR_IN,
		checksum & 0xffff, /* value */
		checksum >> 16, /* index */
		usbbuf, /* dataptr */
		8, /* size */
		3000 /* timeout in ms */
	);
	if (result < 0) {
		printk(KERN_ERR "acx: error %d during ACK of checksum, "
				"aborting\n", result);
		goto end;
	}
	if (*usbbuf != 0x10) {
		kfree(usbbuf);
		printk(KERN_ERR "acx: invalid checksum?\n");
		result = -EINVAL;
		goto end;
	}
	result = 0;
end:
	vfree(firmware);
	kfree(usbbuf);

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acx100usb_e_init_network_device():
** Inputs:
**    dev -> Pointer to network device
************************************************************************
** Description:
**  Basic setup of a network device for use with the WLAN device.
*/
static int
acx100usb_e_init_network_device(struct net_device *dev)
{
	wlandevice_t *priv;
	int result = 0;

	FN_ENTER;

	/* Setup the device and stop the queue */
	ether_setup(dev);
	acx_stop_queue(dev, "on init");

	priv = netdev_priv(dev);

	acx_sem_lock(priv);

	/* put the ACX100 out of sleep mode */
	acx_s_issue_cmd(priv, ACX1xx_CMD_WAKE, NULL, 0);

	/* Register the callbacks for the network device functions */
	dev->open = &acx100usb_e_open;
	dev->stop = &acx100usb_e_close;
	dev->hard_start_xmit = (void *)&acx_i_start_xmit;
	dev->get_stats = (void *)&acx_e_get_stats;
	dev->get_wireless_stats = (void *)&acx_e_get_wireless_stats;
#if WIRELESS_EXT >= 13
	dev->wireless_handlers = (struct iw_handler_def *)&acx_ioctl_handler_def;
#else
	dev->do_ioctl = (void *)&acx_e_ioctl_old;
#endif
	dev->set_multicast_list = (void *)&acx100usb_i_set_rx_mode;
#ifdef HAVE_TX_TIMEOUT
	dev->tx_timeout = &acx100usb_i_tx_timeout;
	dev->watchdog_timeo = 4 * HZ;
#endif
	result = acx_s_init_mac(dev);
	if (OK != result)
		goto end;
	result = acx_s_set_defaults(priv);
	if (OK != result) {
		printk("%s: acx_set_defaults FAILED\n", dev->name);
		goto end;
	}

	SET_MODULE_OWNER(dev);
end:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acx100usb_e_open
** This function is called when the user sets up the network interface.
** It initializes a management timer, sets up the USB card and starts
** the network tx queue and USB receive.
*/
static int
acx100usb_e_open(struct net_device *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	int i;

	FN_ENTER;

	acx_sem_lock(priv);

	/* put the ACX100 out of sleep mode */
	acx_s_issue_cmd(priv, ACX1xx_CMD_WAKE, NULL, 0);

	acx_init_task_scheduler(priv);

	init_timer(&priv->mgmt_timer);
	priv->mgmt_timer.function = acx_i_timer;
	priv->mgmt_timer.data = (unsigned long)priv;

	/* acx_s_start needs it */
	SET_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	acx_s_start(priv);

	acx_start_queue(dev, "on open");

	acx_lock(priv, flags);
	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++) {
		acx100usb_l_poll_rx(priv, &priv->usb_rx[i]);
	}
	acx_unlock(priv, flags);

	WLAN_MOD_INC_USE_COUNT;

	acx_sem_unlock(priv);

	FN_EXIT0;
	return 0;
}


/***********************************************************************
** acx100usb_l_poll_rx
** This function (re)initiates a bulk-in USB transfer on given urb
*/
static void
acx100usb_l_poll_rx(wlandevice_t *priv, usb_rx_t* rx)
{
	struct usb_device *usbdev;
	struct urb *rxurb;
	int errcode, rxnum;
	unsigned int inpipe;

	FN_ENTER;

	rxurb = rx->urb;
	usbdev = priv->usbdev;

	rxnum = rx - priv->usb_rx;

	inpipe = usb_rcvbulkpipe(usbdev, priv->bulkinep);
	if (rxurb->status == -EINPROGRESS) {
		printk(KERN_ERR "acx: error, rx triggered while rx urb in progress\n");
		/* FIXME: this is nasty, receive is being cancelled by this code
		 * on the other hand, this should not happen anyway...
		 */
		usb_unlink_urb(rxurb);
	} else if (rxurb->status == -ECONNRESET) {
		acxlog(L_USBRXTX, "acx_usb: _poll_rx: connection reset\n");
		goto end;
	}
	rxurb->actual_length = 0;
	usb_fill_bulk_urb(rxurb, usbdev, inpipe,
		&rx->bulkin, /* dataptr */
		RXBUFSIZE, /* size */
		acx100usb_i_complete_rx, /* handler */
		rx /* handler param */
	);
	rxurb->transfer_flags = URB_ASYNC_UNLINK;

	/* ATOMIC: we may be called from complete_rx() usb callback */
	errcode = usb_submit_urb(rxurb, GFP_ATOMIC);
	/* FIXME: evaluate the error code! */
	acxlog(L_USBRXTX, "SUBMIT RX (%d) inpipe=0x%X size=%d errcode=%d\n",
			rxnum, inpipe, (int) RXBUFSIZE, errcode);
end:
	FN_EXIT0;
}


/***********************************************************************
** acx100usb_i_complete_rx():
** Inputs:
**     urb -> Pointer to USB request block
**    regs -> Pointer to register-buffer for syscalls (see asm/ptrace.h)
************************************************************************
** Description:
**  This function is invoked by USB subsystem whenever a bulk receive
**  request returns.
**  The received data is then committed to the network stack and the next
**  USB receive is triggered.
*/
static void
acx100usb_i_complete_rx(struct urb *urb, struct pt_regs *regs)
{
	wlandevice_t *priv;
	rxbuffer_t *ptr;
	rxbuffer_t *inbuf;
	usb_rx_t *rx;
	unsigned long flags;
	int size, remsize, packetsize, rxnum;

	FN_ENTER;

	BUG_ON(!urb->context);

	rx = (usb_rx_t *)urb->context;
	priv = rx->priv;

	acx_lock(priv, flags);

	/*
	 * Happens on disconnect or close. Don't play with the urb.
	 * Don't resubmit it. It will get unlinked by close()
	 */
	if (!(priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		acxlog(L_USBRXTX, "not doing anything.\n");
		goto end_unlock;
	}

	inbuf = &rx->bulkin;
	size = urb->actual_length;
	remsize = size;
	rxnum = rx - priv->usb_rx;

	acxlog(L_USBRXTX, "RETURN RX (%d) status=%d size=%d\n",
				rxnum, urb->status, size);

	if(size > sizeof(rxbuffer_t))
		printk("acx_usb: rx too large: %d bytes, lease report\n", size);

	/* check if the transfer was aborted */
	switch (urb->status) {
	case 0: /* No error */
		break;
	case -EOVERFLOW:
		printk(KERN_ERR "acx: error in rx, data overrun -> emergency stop\n");
		/* LOCKING BUG! acx100usb_e_close(priv->netdev); */
		goto end_unlock;
	case -ECONNRESET:
		goto end_unlock;
	case -ESHUTDOWN: /* rmmod */
		goto end_unlock;
	default:
		priv->stats.rx_errors++;
		printk("acx: rx error (urb status=%d)\n", urb->status);
		goto do_poll_rx;
	}

	if (!size)
		printk("acx: warning, encountered zerolength rx packet\n");

	if (urb->transfer_buffer != inbuf)
		goto do_poll_rx;

	/* check if previous frame was truncated
	** FIXME: this code can only handle truncation
	** of consecutive packets!
	*/
	ptr = inbuf;
	if (priv->rxtruncsize) {
		int tail_size;

		ptr = &priv->rxtruncbuf;
		packetsize = RXBUF_BYTES_USED(ptr);
		if (acx_debug & L_USBRXTX) {
			printk("handling truncated frame (truncsize=%d size=%d "
					"packetsize(from trunc)=%d)\n",
					priv->rxtruncsize, size, packetsize);
			acx_dump_bytes(ptr, RXBUF_HDRSIZE);
			acx_dump_bytes(inbuf, RXBUF_HDRSIZE);
		}

		/* bytes needed for rxtruncbuf completion: */
		tail_size = packetsize - priv->rxtruncsize;

		if (size < tail_size) {
			/* there is not enough data to complete this packet,
			** simply append the stuff to the truncation buffer
			*/
			memcpy(((char *)ptr) + priv->rxtruncsize, inbuf, size);
			priv->rxtruncsize += size;
			remsize = 0;
		} else {
			/* ok, this data completes the previously
			** truncated packet. copy it into a descriptor
			** and give it to the rest of the stack	*/

			/* append tail to previously truncated part
			** NB: priv->rxtruncbuf (pointed to by ptr) can't
			** overflow because this is already checked before
			** truncation buffer was filled. See below,
			** "if (packetsize > sizeof(rxbuffer_t))..." code */
			memcpy(((char *)ptr) + priv->rxtruncsize, inbuf, tail_size);

			if (acx_debug & L_USBRXTX) {
				printk("full trailing packet + 12 bytes:\n");
				acx_dump_bytes(inbuf, tail_size + RXBUF_HDRSIZE);
			}
			acx_l_process_rxbuf(priv, ptr);
			priv->rxtruncsize = 0;
			ptr = (rxbuffer_t *) (((char *)inbuf) + tail_size);
			remsize -= tail_size;
		}
		acxlog(L_USBRXTX, "post-merge size=%d remsize=%d\n",
						size, remsize);
	}

	/* size = USB data block size
	** remsize = unprocessed USB bytes left
	** ptr = current pos in USB data block
	*/
	while (remsize) {
		if (remsize < RXBUF_HDRSIZE) {
			printk("acx: truncated rx header (%d bytes)!\n",
				remsize);
			break;
		}
		packetsize = RXBUF_BYTES_USED(ptr);
		acxlog(L_USBRXTX, "packet with packetsize=%d\n", packetsize);
		if (packetsize > sizeof(rxbuffer_t)) {
			printk("acx: packet exceeds max wlan "
				"frame size (%d > %d). size=%d\n",
				packetsize, (int) sizeof(rxbuffer_t), size);
			/* FIXME: put some real error-handling in here! */
			break;
		}

		/* skip null packets (does this really happen?!) */
		if (packetsize == RXBUF_HDRSIZE) {
			remsize -= RXBUF_HDRSIZE;
			if (acx_debug & L_USBRXTX) {
				printk("acx: null packet, new remsize=%d. "
					"header follows:\n", remsize);
				acx_dump_bytes(ptr, RXBUF_HDRSIZE);
			}
			ptr = (rxbuffer_t *)(((char *)ptr) + RXBUF_HDRSIZE);
			continue;
		}

		if (packetsize > remsize) {
			/* frame truncation handling */
			if (acx_debug & L_USBRXTX) {
				printk("need to truncate packet, "
					"packetsize=%d remsize=%d "
					"size=%d\n",
					packetsize, remsize, size);
				acx_dump_bytes(ptr, RXBUF_HDRSIZE);
			}
			memcpy(&priv->rxtruncbuf, ptr, remsize);
			priv->rxtruncsize = remsize;
			break;
		} else { /* packetsize <= remsize */
			/* now handle the received data */
			acx_l_process_rxbuf(priv, ptr);

			ptr = (rxbuffer_t *)(((char *)ptr) + packetsize);
			remsize -= packetsize;
			if ((acx_debug & L_USBRXTX) && remsize) {
				printk("more than one packet in buffer, "
						"second packet hdr follows\n");
				acx_dump_bytes(ptr, RXBUF_HDRSIZE);
			}
		}
	}

do_poll_rx:
	/* receive of frame completed, now look for the next one */
	acx100usb_l_poll_rx(priv, rx);
end_unlock:
	acx_unlock(priv, flags);
/* end: */
	FN_EXIT0;
}


/***********************************************************************
** acx100usb_i_complete_tx():
** Inputs:
**     urb -> Pointer to USB request block
**    regs -> Pointer to register-buffer for syscalls (see asm/ptrace.h)
************************************************************************
** Description:
**   This function is invoked upon termination of a USB transfer. As the
**   USB device is only capable of sending a limited amount of bytes per
**   transfer to the bulk-out endpoint, this routine checks if there are
**   more bytes to send and triggers subsequent transfers. In case the
**   transfer size exactly matches the maximum bulk-out size, it triggers
**   a transfer of a null-frame, telling the card that this is it. Upon
**   completion of a frame, it checks whether the Tx ringbuffer contains
**   more data to send and invokes the Tx routines if this is the case.
**   If there are no more occupied Tx descriptors, the Tx Mutex is unlocked
**   and the network queue is switched back to life again.
**
** FIXME: unlike PCI code, we do not analyze tx rate used, retries, etc...
** Thus we have no automatic rate control in USB!
*/
static void
acx100usb_i_complete_tx(struct urb *urb, struct pt_regs *regs)
{
	wlandevice_t *priv;
	usb_tx_t *tx;
	unsigned long flags;
	int txnum;

	FN_ENTER;

	BUG_ON(!urb->context);

	tx = (usb_tx_t *)urb->context;
	priv = tx->priv;

	txnum = tx - priv->usb_tx;

	acx_lock(priv, flags);

	/*
	 * If the iface isn't up, we don't have any right
	 * to play with them. The urb may get unlinked.
	 */
	if (!(priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		acxlog(L_USBRXTX, "not doing anything\n");
		goto end_unlock;
		/*FIXME: Do the priv->tx_free++? */
	}

	acxlog(L_USBRXTX, "RETURN TX (%d): status=%d size=%d\n",
				txnum, urb->status, urb->actual_length);

	/* handle USB transfer errors */
	switch (urb->status) {
	case 0:	/* No error */
		break;
	case -ESHUTDOWN:
		goto end_unlock;
		break;
	case -ECONNRESET:
		goto end_unlock;
		break;
		/* FIXME: real error-handling code here please */
	default:
		printk(KERN_ERR "acx: tx error, urb status=%d\n", urb->status);
		/* FIXME: real error-handling code here please */
	}

	/* free the URB and check for more data	*/
	priv->tx_free++;
	tx->busy = 0;

end_unlock:
	acx_unlock(priv, flags);
/* end: */
	FN_EXIT0;
}


/***********************************************************************
** acx100usb_e_close()
**
** This function stops the network functionality of the interface (invoked
** when the user calls ifconfig <wlan> down). The tx queue is halted and
** the device is marked as down. In case there were any pending USB bulk
** transfers, these are unlinked (asynchronously). The module in-use count
** is also decreased in this function.
*/
static int
acx100usb_e_close(struct net_device *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	int i;

	FN_ENTER;

#ifdef WE_STILL_DONT_CARE_ABOUT_IT
	/* Transmit a disassociate frame */
	lock
	acx_l_transmit_disassoc(priv, &client);
	unlock
#endif

	acx_sem_lock(priv);

	/* Make sure we don't get any more rx requests */
	acx_s_issue_cmd(priv, ACX1xx_CMD_DISABLE_RX, NULL, 0);
	acx_s_issue_cmd(priv, ACX1xx_CMD_DISABLE_TX, NULL, 0);

	/*
	 * We must do FLUSH *without* holding sem to avoid a deadlock.
	 * See pci.c:acx_s_down() for deails.
	 */
	acx_sem_unlock(priv);
	FLUSH_SCHEDULED_WORK();
	acx_sem_lock(priv);

	/* Power down the device */
	acx_s_issue_cmd(priv, ACX1xx_CMD_SLEEP, NULL, 0);

	/* Stop the transmit queue, mark the device as DOWN */
	acx_lock(priv, flags);
	acx_stop_queue(dev, "on iface stop");
	CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	/* stop pending rx/tx urb transfers */
	/* Make sure you don't free them! */
	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++) {
		acx_unlink_urb(priv->usb_rx[i].urb);
		acx_unlink_urb(priv->usb_tx[i].urb);
	}

	del_timer_sync(&priv->mgmt_timer);

	priv->tx_free = ACX100_USB_NUM_BULK_URBS;

	for (i = 0; i< ACX100_USB_NUM_BULK_URBS; ++i) {
		priv->usb_tx[i].busy = 0;
		priv->usb_rx[i].busy = 0;
	}

	acx_unlock(priv, flags);

	acx_sem_unlock(priv);

	/* Decrease module-in-use count (if necessary) */

	WLAN_MOD_DEC_USE_COUNT;

	FN_EXIT0;
	return 0;
}


/***************************************************************
** acxusb_l_alloc_tx
** Actually returns a usb_tx_t* ptr
*/
tx_t*
acxusb_l_alloc_tx(wlandevice_t* priv)
{
	int i;
	usb_tx_t *tx = NULL;

	FN_ENTER;

	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++) {
		if (!priv->usb_tx[i].busy) {
			acxlog(L_USBRXTX, "allocated tx %d\n", i);
			tx = &priv->usb_tx[i];
			tx->busy = 1;
			break;
		}
	}
	if (i >= ACX100_USB_NUM_BULK_URBS) {
		printk_ratelimited("acx: tx buffers full\n");
	}

	FN_EXIT0;

	return (tx_t*)tx;
}


/***************************************************************
*/
void*
acxusb_l_get_txbuf(wlandevice_t *priv, tx_t* tx_opaque)
{
	usb_tx_t* tx = (usb_tx_t*)tx_opaque;
	return &tx->bulkout.data;
}


/***************************************************************
** acxusb_l_tx_data
**
** Can be called from IRQ (rx -> (AP bridging or mgmt response) -> tx).
** Can be called from acx_i_start_xmit (data frames from net core).
*/
void
acxusb_l_tx_data(wlandevice_t *priv, tx_t* tx_opaque, int wlanpkt_len)
{
	struct usb_device *usbdev;
	struct urb* txurb;
	usb_tx_t* tx;
	usb_txbuffer_t* txbuf;
	client_t *clt;
	wlan_hdr_t* whdr;
	unsigned int outpipe;
	int ucode;
	u8 rate100;

	FN_ENTER;

	tx = ((usb_tx_t *)tx_opaque);
	txurb = tx->urb;
	txbuf = &tx->bulkout;
	whdr = (wlan_hdr_t *)txbuf->data;

	priv->tx_free--;
	acxlog(L_DEBUG, "using buf#%d free=%d len=%d\n",
			(int)(tx - priv->usb_tx),
			priv->tx_free, wlanpkt_len);

	switch (priv->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_3_AP:
		clt = acx_l_sta_list_get(priv, whdr->a1);
		break;
	case ACX_MODE_2_STA:
		clt = priv->ap_client;
		break;
	default: /* ACX_MODE_OFF, ACX_MODE_MONITOR */
		clt = NULL;
		break;
	}

	if (unlikely(clt && !clt->rate_cur)) {
		printk("acx: driver bug! bad ratemask\n");
		goto end;
	}

	/* used in tx cleanup routine for auto rate and accounting: */
//TODO: currently unused - fix that
	tx->txc = clt;

	rate100 = clt ? clt->rate_100 : priv->rate_bcast100;

	/* fill the USB transfer header */
	txbuf->desc = cpu_to_le16(USB_TXBUF_TXDESC);
	txbuf->MPDUlen = cpu_to_le16(wlanpkt_len);
	txbuf->ctrl1 = 0;
	txbuf->ctrl2 = 0;
	txbuf->hostData = cpu_to_le32(wlanpkt_len | (rate100 << 24));
	if (1 == priv->preamble_cur)
		SET_BIT(txbuf->ctrl1, DESC_CTL_SHORT_PREAMBLE);
	SET_BIT(txbuf->ctrl1, DESC_CTL_FIRSTFRAG);
	txbuf->txRate = rate100;
	txbuf->index = 1;
	txbuf->dataLength = cpu_to_le16(wlanpkt_len);

	if ( (WF_FC_FTYPEi & whdr->fc) == WF_FTYPE_DATAi )
		SET_BIT(txbuf->hostData, cpu_to_le32(USB_TXBUF_HD_ISDATA));
	if (mac_is_directed(whdr->a1))
		SET_BIT(txbuf->hostData, cpu_to_le32(USB_TXBUF_HD_DIRECTED));
	else if (mac_is_bcast(whdr->a1))
		SET_BIT(txbuf->hostData, cpu_to_le32(USB_TXBUF_HD_BROADCAST));

	if (acx_debug & L_DATA) {
		printk("dump of bulk out urb:\n");
		acx_dump_bytes(txbuf, wlanpkt_len + USB_TXBUF_HDRSIZE);
	}

	if (txurb->status == -EINPROGRESS) {
		printk("acx: trying to submit tx urb while already in progress\n");
	}

	/* now schedule the USB transfer */
	usbdev = priv->usbdev;
	outpipe = usb_sndbulkpipe(usbdev, priv->bulkoutep);

	usb_fill_bulk_urb(txurb, usbdev, outpipe,
		txbuf, /* dataptr */
		wlanpkt_len + USB_TXBUF_HDRSIZE, /* size */
		acx100usb_i_complete_tx, /* handler */
		tx /* handler param */
	);

	txurb->transfer_flags = URB_ASYNC_UNLINK|URB_ZERO_PACKET;
	ucode = usb_submit_urb(txurb, GFP_ATOMIC);
	acxlog(L_USBRXTX, "SUBMIT TX (%p): outpipe=0x%X buf=%p txsize=%d "
		"errcode=%d\n", tx, outpipe, txbuf,
		wlanpkt_len + USB_TXBUF_HDRSIZE, ucode);

	if (ucode) {
		printk(KERN_ERR "acx: submit_urb() error=%d txsize=%d\n",
			ucode, wlanpkt_len + USB_TXBUF_HDRSIZE);

		/* on error, just mark the frame as done and update
		** the statistics
		*/
		priv->stats.tx_errors++;
		tx->busy = 0;
		priv->tx_free++;
	}
end:
	FN_EXIT0;
}


/***********************************************************************
*/
static struct net_device_stats*
acx_e_get_stats(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	return &priv->stats;
}


/***********************************************************************
*/
static struct iw_statistics*
acx_e_get_wireless_stats(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	FN_ENTER;
	FN_EXIT0;
	return &priv->wstats;
}


/***********************************************************************
*/
static void
acx100usb_i_set_rx_mode(struct net_device *dev)
{
}


/***********************************************************************
*/
#ifdef HAVE_TX_TIMEOUT
static void
acx100usb_i_tx_timeout(struct net_device *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	int i;

	FN_ENTER;

	acx_lock(priv, flags);
	/* unlink the URBs */
	for (i = 0; i < ACX100_USB_NUM_BULK_URBS; i++) {
		acx_unlink_urb(priv->usb_tx[i].urb);
	}
	/* TODO: stats update */
	acx_unlock(priv, flags);

	FN_EXIT0;
}
#endif


/***********************************************************************
** init_module():
**
** This function is invoked upon loading of the kernel module.
** It registers itself at the kernel's USB subsystem.
**
** Returns: Errorcode on failure, 0 on success
*/
int __init
acxusb_e_init_module(void)
{
	acxlog(L_INIT, "USB module " WLAN_RELEASE " initialized, "
		"probing for devices...\n");
	return usb_register(&acx100usb_driver);
}



/***********************************************************************
** cleanup_module():
**
** This function is invoked as last step of the module unloading. It simply
** deregisters this module at the kernel's USB subsystem.
*/
void __exit
acxusb_e_cleanup_module()
{
	usb_deregister(&acx100usb_driver);
}


/***********************************************************************
** DEBUG STUFF
*/
#if ACX_DEBUG

#ifdef UNUSED
static void
dump_device(struct usb_device *usbdev)
{
	int i;
	struct usb_config_descriptor *cd;

	printk("acx device dump:\n");
	printk("  devnum: %d\n", usbdev->devnum);
	printk("  speed: %d\n", usbdev->speed);
	printk("  tt: 0x%X\n", (unsigned int)(usbdev->tt));
	printk("  ttport: %d\n", (unsigned int)(usbdev->ttport));
	printk("  toggle[0]: 0x%X  toggle[1]: 0x%X\n", (unsigned int)(usbdev->toggle[0]), (unsigned int)(usbdev->toggle[1]));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 8)
	/* halted removed in 2.6.9-rc1 */
	/* DOH, Canbreak... err... Mandrake decided to do their very own very
	 * special version "2.6.8.1" which already includes this change, so we
	 * need to blacklist that version already (i.e. 2.6.8) */
	printk("  halted[0]: 0x%X  halted[1]: 0x%X\n", usbdev->halted[0], usbdev->halted[1]);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
	/* This saw a change after 2.6.10 */
	printk("  ep_in wMaxPacketSize: ");
	for (i = 0; i < 16; ++i)
		printk("%d ", usbdev->ep_in[i]->desc.wMaxPacketSize);
	printk("\n");
	printk("  ep_out wMaxPacketSize: ");
	for (i = 0; i < 15; ++i)
		printk("%d ", usbdev->ep_out[i]->desc.wMaxPacketSize);
	printk("\n");
#else
	printk("  epmaxpacketin: ");
	for (i = 0; i < 16; i++)
		printk("%d ", usbdev->epmaxpacketin[i]);
	printk("\n");
	printk("  epmaxpacketout: ");
	for (i = 0; i < 16; i++)
		printk("%d ", usbdev->epmaxpacketout[i]);
	printk("\n");
#endif
	printk("  parent: 0x%X\n", (unsigned int)usbdev->parent);
	printk("  bus: 0x%X\n", (unsigned int)usbdev->bus);
#if NO_DATATYPE
	printk("  configs: ");
	for (i = 0; i < usbdev->descriptor.bNumConfigurations; i++)
		printk("0x%X ", usbdev->config[i]);
	printk("\n");
#endif
	printk("  actconfig: %p\n", usbdev->actconfig);
	dump_device_descriptor(&usbdev->descriptor);

	cd = &usbdev->config->desc;
	dump_config_descriptor(cd);
}


/***********************************************************************
*/
static void
dump_config_descriptor(struct usb_config_descriptor *cd)
{
	printk("Configuration Descriptor:\n");
	if (!cd) {
		printk("NULL\n");
		return;
	}
	printk("  bLength: %d (0x%X)\n", cd->bLength, cd->bLength);
	printk("  bDescriptorType: %d (0x%X)\n", cd->bDescriptorType, cd->bDescriptorType);
	printk("  bNumInterfaces: %d (0x%X)\n", cd->bNumInterfaces, cd->bNumInterfaces);
	printk("  bConfigurationValue: %d (0x%X)\n", cd->bConfigurationValue, cd->bConfigurationValue);
	printk("  iConfiguration: %d (0x%X)\n", cd->iConfiguration, cd->iConfiguration);
	printk("  bmAttributes: %d (0x%X)\n", cd->bmAttributes, cd->bmAttributes);
	/* printk("  MaxPower: %d (0x%X)\n", cd->bMaxPower, cd->bMaxPower); */
}


static void
dump_device_descriptor(struct usb_device_descriptor *dd)
{
	printk("Device Descriptor:\n");
	if (!dd) {
		printk("NULL\n");
		return;
	}
	printk("  bLength: %d (0x%X)\n", dd->bLength, dd->bLength);
	printk("  bDescriptortype: %d (0x%X)\n", dd->bDescriptorType, dd->bDescriptorType);
	printk("  bcdUSB: %d (0x%X)\n", dd->bcdUSB, dd->bcdUSB);
	printk("  bDeviceClass: %d (0x%X)\n", dd->bDeviceClass, dd->bDeviceClass);
	printk("  bDeviceSubClass: %d (0x%X)\n", dd->bDeviceSubClass, dd->bDeviceSubClass);
	printk("  bDeviceProtocol: %d (0x%X)\n", dd->bDeviceProtocol, dd->bDeviceProtocol);
	printk("  bMaxPacketSize0: %d (0x%X)\n", dd->bMaxPacketSize0, dd->bMaxPacketSize0);
	printk("  idVendor: %d (0x%X)\n", dd->idVendor, dd->idVendor);
	printk("  idProduct: %d (0x%X)\n", dd->idProduct, dd->idProduct);
	printk("  bcdDevice: %d (0x%X)\n", dd->bcdDevice, dd->bcdDevice);
	printk("  iManufacturer: %d (0x%X)\n", dd->iManufacturer, dd->iManufacturer);
	printk("  iProduct: %d (0x%X)\n", dd->iProduct, dd->iProduct);
	printk("  iSerialNumber: %d (0x%X)\n", dd->iSerialNumber, dd->iSerialNumber);
	printk("  bNumConfigurations: %d (0x%X)\n", dd->bNumConfigurations, dd->bNumConfigurations);
}
#endif /* UNUSED */

#endif /* ACX_DEBUG */
