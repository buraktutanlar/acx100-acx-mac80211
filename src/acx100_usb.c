/*  - USB main module functions
 *
 * --------------------------------------------------------------------
 *
 * Copyright (C) 2003  ACX100 Open Source Project
 *
 *   The contents of this file are subject to the Mozilla Public
 *   License Version 1.1 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.mozilla.org/MPL/
 *
 *   Software distributed under the License is distributed on an "AS
 *   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *   implied. See the License for the specific language governing
 *   rights and limitations under the License.
 *
 *   Alternatively, the contents of this file may be used under the
 *   terms of the GNU Public License version 2 (the "GPL"), in which
 *   case the provisions of the GPL are applicable instead of the
 *   above.  If you wish to allow the use of your version of this file
 *   only under the terms of the GPL and not to allow others to use
 *   your version of this file under the MPL, indicate your decision
 *   by deleting the provisions above and replace them with the notice
 *   and other provisions required by the GPL.  If you do not delete
 *   the provisions above, a recipient may use your version of this
 *   file under either the MPL or the GPL.
 *
 * --------------------------------------------------------------------
 *
 * Inquiries regarding the ACX100 Open Source Project can be
 * made directly to:
 *
 * acx100-users@lists.sf.net
 * http://acx100.sf.net
 *
 * --------------------------------------------------------------------
 * USB support for TI ACX100 based devices. Many parts are taken from
 * the PCI driver.
 *
 * Authors:
 *  Martin Wawro <martin.wawro AT uni-dortmund.de>
 *  Andreas Mohr <andi AT lisas.de>
 *
 * Issues:
 *  - Note that this driver relies on a native little-endian byteformat
 *    at some points
 *
 *
*/

/* -------------------------------------------------------------------------
**                      Linux Kernel Header Files
** ---------------------------------------------------------------------- */

#include <version.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

/* -------------------------------------------------------------------------
**                         Project Header Files
** ---------------------------------------------------------------------- */

/* FIXME: integrate nicely into src/Makefile at the time it is clear which
 * other source files from the PCI / CardBus driver have to be linked with this
 * one and therefore _also_ need this define - until then this hack is ok */
#undef WLAN_HOSTIF
#define WLAN_HOSTIF WLAN_USB

#include <wlan_compat.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <acx100_conv.h>
#include <idma.h>
#include <ihw.h>

/* try to make it compile for both 2.4.x and 2.6.x kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

/* number of endpoints of an interface */
#define NUM_EP(intf) (intf)->altsetting[0].desc.bNumEndpoints
#define EP(intf,nr) (intf)->altsetting[0].endpoint[(nr)].desc
#define GET_DEV(udev) usb_get_dev((udev))
#define PUT_DEV(udev) usb_put_dev((udev))
#define SET_NETDEV_OWNER(ndev,owner) /* not needed anymore ??? */

#define ASYNC_UNLINK	URB_ASYNC_UNLINK

/* For GFP_KERNEL vs. GFP_ATOMIC, see
 * http://groups.google.de/groups?th=6cd2e5f77e799a23&seekm=linux.scsi.OF9C38FD78.07E54601-ON87256C24.0056B509%40boulder.ibm.com
 * Basically GFP_KERNEL waits until an alloc succeeds, while
 * GFP_ATOMIC should be used in situations where we need to know
 * the result immediately, since we cannot wait (in case we're within lock
 * or so) */
static inline int submit_urb(struct urb *urb, int mem_flags) {
        return usb_submit_urb(urb, mem_flags);
}
static inline struct urb *alloc_urb(int iso_pk, int mem_flags) {
        return usb_alloc_urb(iso_pk, mem_flags);
}

#else

/* 2.4.x kernels */
#define USB_24	1

#define NUM_EP(intf) (intf)->altsetting[0].bNumEndpoints
#define EP(intf,nr) (intf)->altsetting[0].endpoint[(nr)]

#define GET_DEV(udev) usb_inc_dev_use((udev))
#define PUT_DEV(udev) usb_dec_dev_use((udev))

#define SET_NETDEV_DEV(x,y)
#define SET_NETDEV_OWNER(ndev,owner) ndev->owner = owner

#define ASYNC_UNLINK	USB_ASYNC_UNLINK

static inline int submit_urb(struct urb *urb, int mem_flags) {
        return usb_submit_urb(urb);
}
static inline struct urb *alloc_urb(int iso_pk, int mem_flags) {
        return usb_alloc_urb(iso_pk);
}

static inline void usb_set_intfdata(struct usb_interface *intf, void *data) {}

#endif //#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

/* -------------------------------------------------------------------------
**                             Module Stuff
** ---------------------------------------------------------------------- */

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>");
MODULE_DESCRIPTION("TI ACX100 WLAN USB Driver");

#ifdef ACX_DEBUG
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level mask: 0x0000 - 0x3fff");
#endif

MODULE_PARM(firmware_dir, "s");
MODULE_PARM_DESC(firmware_dir, "Directory to load acx100 firmware file from");


/* -------------------------------------------------------------------------
**                Module Definitions (preprocessor based)
** ---------------------------------------------------------------------- */


#define SHORTNAME "acx_usb"

#define ACX100_VENDOR_ID 0x2001
#define ACX100_PRODUCT_ID_UNBOOTED 0x3B01
#define ACX100_PRODUCT_ID_BOOTED 0x3B00

/* RX-Timeout: NONE (request waits forever) */
#define ACX100_USB_RX_TIMEOUT (0)    

#define ACX100_USB_TX_TIMEOUT (4*HZ)


/* -------------------------------------------------------------------------
**                        Module Data Structures
** ---------------------------------------------------------------------- */




/* -------------------------------------------------------------------------
**                          Module Prototypes
** ---------------------------------------------------------------------- */

#if USB_24
static void * acx100usb_probe(struct usb_device *, unsigned int, const struct usb_device_id *);
static void acx100usb_disconnect(struct usb_device *,void *);
static void acx100usb_complete_tx(struct urb *);
static void acx100usb_complete_rx(struct urb *);
#else
static int acx100usb_probe(struct usb_interface *, const struct usb_device_id *);
static void acx100usb_disconnect(struct usb_interface *);
static void acx100usb_complete_tx(struct urb *, struct pt_regs *);
static void acx100usb_complete_rx(struct urb *, struct pt_regs *);
#endif
static int acx100usb_open(struct net_device *);
static int acx100usb_stop(struct net_device *);
static int acx100usb_start_xmit(struct sk_buff *,struct net_device *);
static void acx100usb_set_rx_mode(struct net_device *);
static void init_network_device(struct net_device *);
static void acx100usb_send_tx_frags(wlandevice_t *);
static int acx100usb_boot(struct usb_device *);
void acx100usb_tx_data(wlandevice_t *,struct txdescriptor *);
static void acx100usb_prepare_tx(wlandevice_t *,struct txdescriptor *);
static void acx100usb_flush_tx(wlandevice_t *);
static void acx100usb_trigger_next_tx(wlandevice_t *);

static struct net_device_stats * acx100_get_stats(struct net_device *);
static struct iw_statistics *acx100_get_wireless_stats(struct net_device *);


static void acx100usb_poll_rx(wlandevice_t *);
void acx100_rx(struct rxhostdescriptor *,wlandevice_t *);

int init_module(void);
void cleanup_module(void);

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *);
#endif

#ifdef ACX_DEBUG
//int debug = L_DEBUG|L_ASSOC|L_INIT|L_STD;
//int debug = L_ALL;
int debug = L_STD;
int acx100_debug_func_indent=0;
static char * acx100usb_pstatus(int);
void acx100usb_dump_bytes(void *,int) __attribute__((__unused__));
static void dump_device(struct usb_device *);
static void dump_device_descriptor(struct usb_device_descriptor *);
#if USB_24
static void dump_endpoint_descriptor(struct usb_endpoint_descriptor *);
static void dump_interface_descriptor(struct usb_interface_descriptor *);
#endif
static void dump_config_descriptor(struct usb_config_descriptor *);
/* static void acx100usb_printsetup(devrequest *); */
/* static void acx100usb_printcmdreq(struct acx100_usb_cmdreq *) __attribute__((__unused__)); */
#endif

/* -------------------------------------------------------------------------
**                             Module Data
** ---------------------------------------------------------------------- */

static int disconnected=0;

char *firmware_dir;

extern const struct iw_handler_def acx100_ioctl_handler_def;

static struct usb_device_id acx100usb_ids[] = {
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_BOOTED) },
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_UNBOOTED) },
   { }
};


/* USB driver data structure as required by the kernel's USB core */

static struct usb_driver acx100usb_driver = {
  .name = "acx_usb",                       /* name of the driver */
  .probe = acx100usb_probe,                /* pointer to probe() procedure */
  .disconnect = acx100usb_disconnect,      /* pointer to disconnect() procedure */
  .id_table = acx100usb_ids
};



/* ---------------------------------------------------------------------------
** acx100usb_probe():
** Inputs:
**    dev -> Pointer to usb_device structure that may or may not be claimed
**  ifNum -> Interface number
**  devID -> Device ID (vendor and product specific stuff)
** ---------------------------------------------------------------------------
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
** ------------------------------------------------------------------------ */

#if USB_24
#define OUTOFMEM	NULL
static void * acx100usb_probe(struct usb_device *usbdev,unsigned int ifNum,const struct usb_device_id *devID)
{
#else
#define OUTOFMEM	-ENOMEM
static int acx100usb_probe(struct usb_interface *intf, const struct usb_device_id *devID)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
#endif
	wlandevice_t *priv;
	struct net_device *dev=NULL;
	int numconfigs,numfaces,result;
	struct usb_config_descriptor *config;
	struct usb_endpoint_descriptor *epdesc;
	FN_ENTER;
	dump_device(usbdev);
	/* ---------------------------------------------
	** first check if this is the "unbooted" hardware
	** --------------------------------------------- */
	if ((usbdev->descriptor.idVendor==ACX100_VENDOR_ID)&&(usbdev->descriptor.idProduct==ACX100_PRODUCT_ID_UNBOOTED)) {
		/* ---------------------------------------------
		** Boot the device (i.e. upload the firmware)
		** --------------------------------------------- */
		acx100usb_boot(usbdev);
		/* ---------------------------------------------
		** OK, we are done with booting. Normally, the
		** ID for the unbooted device should disappear
		** and it will not need a driver anyway...so
		** return a NULL
		** --------------------------------------------- */
		acxlog(L_INIT, "Finished booting, returning from probe().\n");
#if USB_24
		return NULL;
#else
		return 0; /* is that ok?? */
#endif
	}
	if ((usbdev->descriptor.idVendor==ACX100_VENDOR_ID)&&(usbdev->descriptor.idProduct==ACX100_PRODUCT_ID_BOOTED)) {
		/* ---------------------------------------------
		** allocate memory for the device driver context
		** --------------------------------------------- */
		priv = (struct wlandevice *)kmalloc(sizeof(struct wlandevice),GFP_KERNEL);
		if (!priv) {
			printk(KERN_WARNING SHORTNAME ": could not allocate %d bytes memory for device driver context, giving up.\n",sizeof(struct wlandevice));
			return OUTOFMEM;
		}
		memset(priv,0,sizeof(wlandevice_t));
		priv->chip_type = CHIPTYPE_ACX100;
		priv->usbdev=usbdev;
		/* ---------------------------------------------
		** Initialize the device context and also check
		** if this is really the hardware we know about.
		** If not sure, atleast notify the user that he
		** may be in trouble...
		** --------------------------------------------- */
		numconfigs=(int)(usbdev->descriptor.bNumConfigurations);
		if (numconfigs!=1) printk(KERN_WARNING SHORTNAME ": number of configurations is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numconfigs);
#if USB_24
		config = usbdev->actconfig;
#else
		config = &usbdev->config->desc;
#endif
		numfaces=config->bNumInterfaces;
		if (numfaces!=1) printk(KERN_WARNING SHORTNAME ": number of interfaces is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numfaces);
		/* ---------------------------------------------
		** initialize custom spinlocks...
		** --------------------------------------------- */
		spin_lock_init(&(priv->usb_ctrl_lock));
		spin_lock_init(&(priv->usb_tx_lock));
		priv->usb_tx_mutex=0;
		priv->currentdesc=0;
		/* ---------------------------------------------
		** Allocate memory for a network device
		** --------------------------------------------- */
		if ((dev = (struct net_device *)kmalloc(sizeof(struct net_device),GFP_ATOMIC))==NULL) {
			printk(KERN_WARNING SHORTNAME ": failed to alloc netdev\n");
			kfree(priv);
			return OUTOFMEM;
		}
		/* ---------------------------------------------
		** Setup network device
		** --------------------------------------------- */
		memset(dev, 0, sizeof(struct net_device));
		dev->init=(void *)&init_network_device;
		dev->priv=priv;
		priv->netdev=dev;
		/* ---------------------------------------------
		** Setup URB for control messages
		** --------------------------------------------- */
		priv->ctrl_urb=alloc_urb(0, GFP_KERNEL);
		if (!priv->ctrl_urb) {
			printk(KERN_WARNING SHORTNAME ": failed to allocate URB\n");
			kfree(dev);
			kfree(priv);
			return OUTOFMEM;
		}
		/* ---------------------------------------------
		** Setup URB for bulk-in messages
		** --------------------------------------------- */
		priv->bulkrx_urb=alloc_urb(0, GFP_KERNEL);
		if (!priv->bulkrx_urb) {
			printk(KERN_WARNING SHORTNAME ": failed to allocate input URB\n");
			usb_free_urb(priv->ctrl_urb);
			kfree(dev);
			kfree(priv);
			return OUTOFMEM;
		}
		priv->bulkrx_urb->status=0;
		/* ---------------------------------------------
		** Setup URB for bulk-out messages
		** --------------------------------------------- */
		priv->bulktx_urb=alloc_urb(0, GFP_KERNEL);
		if (!priv->bulktx_urb) {
			printk(KERN_WARNING SHORTNAME ": failed to allocate output URB\n");
			usb_free_urb(priv->ctrl_urb);
			usb_free_urb(priv->bulkrx_urb);
			kfree(dev);
			kfree(priv);
			return OUTOFMEM;
		}
		priv->bulktx_urb->status=0;
		/* --------------------------------------
		** Allocate a network device name
		** -------------------------------------- */
		acxlog(L_INIT,"allocating device name\n");
		result=dev_alloc_name(dev,"wlan%d");
		if (result<0) {
			printk(KERN_ERR SHORTNAME ": failed to allocate wlan device name (errcode=%d), giving up.\n",result);
			kfree(dev);
			usb_free_urb(priv->ctrl_urb);
			usb_free_urb(priv->bulkrx_urb);
			usb_free_urb(priv->bulktx_urb);
			kfree(priv);
			return OUTOFMEM;
		}
#if USB_24 == 0
		usb_set_intfdata(intf, priv);
		SET_NETDEV_DEV(dev, &intf->dev);
#endif
		/* --------------------------------------
		** Register the network device
		** -------------------------------------- */
		acxlog(L_INIT,"registering network device\n");
		result=register_netdev(dev);
		if (result!=0) {
			printk(KERN_ERR SHORTNAME ": failed to register network device for USB WLAN (errcode=%d), giving up.\n",result);
			kfree(dev);
			usb_free_urb(priv->ctrl_urb);
			usb_free_urb(priv->bulkrx_urb);
			usb_free_urb(priv->bulktx_urb);
			kfree(priv);
			return OUTOFMEM;
		}
		/* --------------------------------------
		** Check the max. tx size of the endpoint
		** -------------------------------------- */
		epdesc = usb_epnum_to_ep_desc(usbdev,1);   /* get the descriptor of the bulk endpoint */
		if (epdesc) {
			priv->usb_max_bulkout=epdesc->wMaxPacketSize;
		} else {
			priv->usb_max_bulkout=64; /* guess a default value */
		}
		/* --------------------------------------
		** Everything went OK, we are happy now
		** ----------------------------------- */
#if USB_24
		return priv;
#else
		return 0;
#endif
	}
	/* --------------------------------------
	** no device we could handle, return NULL
	** -------------------------------------- */
#if USB_24
	return NULL;
#else
	return -EIO;
#endif
}




/* ---------------------------------------------------------------------------
** acx100usb_disconnect():
** Inputs:
**         dev -> Pointer to usb_device structure handled by this module
**  devContext -> Pointer to own device context (acx100usb_context)
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is invoked whenever the user pulls the plug from the USB
**  device or the module is removed from the kernel. In these cases, the
**  network devices have to be taken down and all allocated memory has
**  to be freed.
** ------------------------------------------------------------------------ */

#if USB_24
static void acx100usb_disconnect(struct usb_device *usbdev, void *devContext)
{
	wlandevice_t *priv = (wlandevice_t *)devContext;
#else
static void acx100usb_disconnect(struct usb_interface *intf)
{
	wlandevice_t *priv = usb_get_intfdata(intf);
#endif
	int result;

	if (disconnected) return;
	/* --------------------------------------
	** No WLAN device...no sense
	** ----------------------------------- */
	if (NULL == priv)
		return;
	/* --------------------------------------
	**  stop the transmit queue...
	**  ----------------------------------- */
	if (priv->netdev) {
		rtnl_lock();
		if (!netif_queue_stopped(priv->netdev)) {
			netif_stop_queue(priv->netdev);
	        }
		rtnl_unlock();
	}
	/* --------------------------------------
	 * now abort pending URBs and free them
	 * ------------------------------------ */
	if (priv->ctrl_urb) {
		acxlog(L_DEBUG,"freeing ctrl urb\n");
		if (priv->ctrl_urb->status==-EINPROGRESS) usb_unlink_urb(priv->ctrl_urb);
		usb_free_urb(priv->ctrl_urb);
	}
	if (priv->bulkrx_urb) {
		acxlog(L_DEBUG,"freeing rx urb\n");
		if (priv->bulkrx_urb->status==-EINPROGRESS) usb_unlink_urb(priv->bulkrx_urb);
		usb_free_urb(priv->bulkrx_urb);
	}
	if (priv->bulktx_urb) {
		acxlog(L_DEBUG,"freeing tx urb\n");
		if (priv->bulktx_urb->status==-EINPROGRESS) usb_unlink_urb(priv->bulktx_urb);
		usb_free_urb(priv->bulktx_urb);
	}
	/* --------------------------------------
	** Unregister the network devices
	** -------------------------------------- */
	if (priv->netdev) {
		rtnl_lock();
		result=unregister_netdevice(priv->netdev);
		rtnl_unlock();
		kfree(priv->netdev);
	}
	/* --------------------------------------
	** finally free the WLAN device...
	** -------------------------------------- */
	if (priv) kfree(priv);
	disconnected=1;
}




/* ---------------------------------------------------------------------------
** acx100usb_boot():
** Inputs:
**  usbdev -> Pointer to kernel's usb_device structure
** ---------------------------------------------------------------------------
** Returns:
**  (int) Errorcode or 0 on success
**
** Description:
**  This function triggers the loading of the firmware image from harddisk
**  and then uploads the firmware to the USB device. After uploading the
**  firmware and transmitting the checksum, the device resets and appears
**  as a new device on the USB bus (the device we can finally deal with)
** ----------------------------------------------------------------------- */

static int acx100usb_boot(struct usb_device *usbdev)
{
	unsigned int offset=8,len,inpipe,outpipe;
	UINT32 size;
	int result;
	UINT16 *csptr;
	char filename[128],*firmware,*usbbuf;
	FN_ENTER;
	usbbuf = (char *)kmalloc(ACX100_USB_RWMEM_MAXLEN,GFP_KERNEL);
	if (!usbbuf) {
		printk(KERN_ERR SHORTNAME ": not enough memory for allocating USB transfer buffer (req=%d bytes)\n",ACX100_USB_RWMEM_MAXLEN);
		return(-ENOMEM);
	}
	if ((firmware_dir)&&(strlen(firmware_dir)>114)) {
		printk(KERN_ERR "path in firmware_dir is too long (max. 114 chars)\n");
		kfree(usbbuf);
		return(-EINVAL);
	}
	if (firmware_dir) sprintf(filename,"%s/ACX100_USB.bin",firmware_dir);
	else sprintf(filename,"/usr/share/acx/ACX100_USB.bin");
	acxlog(L_INIT,"loading firmware %s\n",filename);
	firmware=(char *)acx_read_fw(filename, &size);
	if (!firmware) {
		kfree(usbbuf);
		return(-EIO);
	}
	acxlog(L_INIT,"firmware size: %d bytes\n",size);
	/* --------------------------------------
	** Obtain the I/O pipes
	** -------------------------------------- */
	outpipe=usb_sndctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
	inpipe =usb_rcvctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
	/* --------------------------------------
	** now upload the firmware, slice the data
	** into blocks
	** -------------------------------------- */
	while (offset<size) {
		if ((size-offset)>=(ACX100_USB_RWMEM_MAXLEN)) len=ACX100_USB_RWMEM_MAXLEN;
		else len=size-offset;
		acxlog(L_INIT,"uploading firmware (%d bytes, offset=%d size=%d)\n",len,offset,size);
		result=0;
		memcpy(usbbuf,firmware+offset,len);
		result=usb_control_msg(usbdev,outpipe,ACX100_USB_UPLOAD_FW,USB_TYPE_VENDOR|USB_DIR_OUT,(UINT16)(size-8),0,usbbuf,len,3000);
		offset+=len;
		if (result<0) {
#ifdef ACX_DEBUG
			printk(KERN_ERR "error %d (%s) during upload of firmware, aborting\n",result,acx100usb_pstatus(result));
#else
			printk(KERN_ERR "error %d during upload of firmware, aborting\n", result);
#endif
			kfree(usbbuf);
			vfree(firmware);
			return(result);
		}
	}
	/* --------------------------------------
	** finally, send the checksum and reboot
	** the device...
	** -------------------------------------- */
	csptr=(UINT16 *)firmware;
	result=usb_control_msg(usbdev,outpipe,ACX100_USB_UPLOAD_FW,USB_TYPE_VENDOR|USB_DIR_OUT,csptr[0],csptr[1],NULL,0,3000); /* this triggers the reboot ? */
	if (result<0) {
		printk(KERN_ERR "error %d during tx of checksum, aborting\n",result);
		kfree(usbbuf);
		vfree(firmware);
		return(result);
	}
	result=usb_control_msg(usbdev,inpipe,ACX100_USB_ACK_CS,USB_TYPE_VENDOR|USB_DIR_IN,csptr[0],csptr[1],usbbuf,8,3000);
	if (result<0) {
		printk(KERN_ERR "error %d during ACK of checksum, aborting\n",result);
		kfree(usbbuf);
		vfree(firmware);
		return(result);
	}
	vfree(firmware);
	if (((UINT16 *)usbbuf)[0]!=0x10) {
		kfree(usbbuf);
		printk(KERN_ERR "invalid checksum?\n");
		return(-EINVAL);
	}
	kfree(usbbuf);
	return(0);
}




/* ---------------------------------------------------------------------------
** init_network_device():
** Inputs:
**    dev -> Pointer to network device
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  Basic setup of a network device for use with the WLAN device.
** ------------------------------------------------------------------------- */

static void init_network_device(struct net_device *dev) {
	int result=0;
	wlandevice_t *priv;
	/* --------------------------------------
	** Setup the device and stop the queue
	** -------------------------------------- */
	ether_setup(dev);
	netif_stop_queue(dev);
	/* --------------------------------------
	** put the ACX100 out of sleep mode
	** ----------------------------------- */
	priv=dev->priv;
	acx100_issue_cmd(priv,ACX100_CMD_WAKE,NULL,0,5000);
	/* --------------------------------------
	** Register the callbacks for the network
	** device functions.
	** -------------------------------------- */
	dev->open = &acx100usb_open;
	dev->stop = &acx100usb_stop;
	dev->hard_start_xmit = (void *)&acx100usb_start_xmit;
	dev->get_stats = (void *)&acx100_get_stats;
	dev->get_wireless_stats = (void *)&acx100_get_wireless_stats;
#if WIRELESS_EXT >= 13
	dev->wireless_handlers = (struct iw_handler_def *)&acx100_ioctl_handler_def;
#else
	dev->do_ioctl = (void *)&acx_ioctl_old;
#endif
	dev->set_multicast_list = (void *)&acx100usb_set_rx_mode;
#ifdef HAVE_TX_TIMEOUT
	dev->tx_timeout = &acx100usb_tx_timeout;
	dev->watchdog_timeo = 4 * HZ;        /* 400 */
#endif
	result=acx100_init_mac(dev, 1);
	if (!result) {
	  SET_MODULE_OWNER(dev);
	}
}





/* --------------------------------------------------------------------------
** acx100usb_open():
** Inputs:
**    dev -> Pointer to network device
** --------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is called when the user sets up the network interface.
**  It initializes a management timer, sets up the USB card and starts
**  the network tx queue and USB receive.
** ---------------------------------------------------------------------- */

static int acx100usb_open(struct net_device *dev)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;

	FN_ENTER;

	/* ---------------------------------
	** put the ACX100 out of sleep mode
	** ------------------------------ */
	acx100_issue_cmd(priv,ACX100_CMD_WAKE,NULL,0,5000);

	init_timer(&(priv->mgmt_timer));
	priv->mgmt_timer.function=acx100_timer;
	priv->mgmt_timer.data=(unsigned long)priv;

	/* set ifup to 1, since acx100_start needs it (FIXME: ugly) */

	priv->dev_state_mask |= ACX_STATE_IFACE_UP;
	acx100_start(priv);


	netif_start_queue(dev);
	acx100usb_poll_rx(priv);
	/* --- */
	WLAN_MOD_INC_USE_COUNT;
	return 0;
}




/* ---------------------------------------------------------------------------
** acx100_rx():
** Inputs:
**    dev -> Pointer to network device
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is invoked when a packet has been received by the USB
**  part of the code. It converts the rxdescriptor to an ethernet frame and
**  then commits the data to the network stack.
** ------------------------------------------------------------------------ */

void acx100_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	netdevice_t *dev = priv->netdev;
	struct sk_buff *skb;
	FN_ENTER;
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) {
		if ((skb = acx100_rxdesc_to_ether(priv, rxdesc))) {
			skb->dev = dev;
			skb->protocol = eth_type_trans(skb, dev);
			dev->last_rx = jiffies;

			netif_rx(skb);

			priv->stats.rx_packets++;
			priv->stats.rx_bytes += skb->len;
		}
	}
	FN_EXIT(0, 0);
}



/* ---------------------------------------------------------------------------
** acx100usb_poll_rx():
** Inputs:
**    priv -> Pointer to wlandevice structure
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function initiates a bulk-in USB transfer (in case the interface
**  is up).
** ------------------------------------------------------------------------- */

static void acx100usb_poll_rx(wlandevice_t *priv) {
	acx100_usbin_t *inbuf;
	struct usb_device *usbdev;
	unsigned int inpipe;

	FN_ENTER;
        acxlog(L_DEBUG,"X");
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) {
		inbuf=&(priv->bulkin);
		usbdev=priv->usbdev;

		inpipe=usb_rcvbulkpipe(usbdev,1);      /* default endpoint for bulk-transfers: 1 */

	 	if (priv->bulkrx_urb->status==-EINPROGRESS) {
			printk(KERN_ERR SHORTNAME ": ERROR, RX TRIGGERED WHILE RX-URB IN PROGRESS\n");
			/* FIXME: this is nasty, receive is being cancelled by this code
			 * on the other hand, this should not happen anyway...
			 */
			usb_unlink_urb(priv->bulkrx_urb);
		}
		priv->bulkrx_urb->actual_length=0;
		usb_fill_bulk_urb(priv->bulkrx_urb, usbdev, inpipe, inbuf, sizeof(acx100_usbin_t), acx100usb_complete_rx, priv);
		priv->bulkrx_urb->transfer_flags=ASYNC_UNLINK;
		priv->bulkrx_urb->timeout=ACX100_USB_RX_TIMEOUT;
		priv->bulkrx_urb->status=0;
		submit_urb(priv->bulkrx_urb, GFP_KERNEL);
	}
	FN_EXIT(0,0);
}




/* ---------------------------------------------------------------------------
** acx100usb_complete_rx():
** Inputs:
**     urb -> Pointer to USB request block
**    regs -> Pointer to register-buffer for syscalls (see asm/ptrace.h)
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is invoked whenever a bulk receive request returns. The
**  received data is then committed to the network stack and the next
**  USB receive is triggered.
** ------------------------------------------------------------------------- */

#if USB_24
static void acx100usb_complete_rx(struct urb *urb)
#else
static void acx100usb_complete_rx(struct urb *urb, struct pt_regs *regs)
#endif
{
	wlandevice_t *priv;
	int offset,size,packetsize;
	rxbuffer_t *ptr;
	struct rxhostdescriptor *rxdesc;
	TIWLAN_DC *ticontext;

	FN_ENTER;
	acxlog(L_DEBUG,"Y");
	priv=(wlandevice_t *)urb->context;
	/* ----------------------------
	** grab the TI device context
	** -------------------------- */
	ticontext=&(priv->dc);
	size=urb->actual_length;
	/* ---------------------------------------------
	** check if the transfer was aborted...
	** ------------------------------------------ */
	if (urb->status!=0) {
		priv->stats.rx_errors++;
		if (priv->dev_state_mask & ACX_STATE_IFACE_UP) acx100usb_poll_rx(priv);
		return;
	}
	/* ---------------------------------------------
	** check if the receive buffer is the right one
	** --------------------------------------------- */
	if (size==0) acxlog(L_STD,"acx_usb: warning, encountered zerolength rx packet\n");
	acxlog(L_XFER,"bulk rx completed (urb->actual_length=%d)\n",size);
	if ((size>0)&&(urb->transfer_buffer==&(priv->bulkin))) {
		/* ------------------------------------------------------------------
		** now fill the data into the rxhostdescriptor for further processing
		** ---------------------------------------------------------------- */
		if (!(ticontext->pRxHostDescQPool)) {
			acxlog(L_STD,"acx100usb: ERROR rxhostdescriptor pool is NULL\n");
			return; /* bail out before something bad happens */
		}
		ptr=(rxbuffer_t *)&(priv->bulkin);
		offset=0;
		while (offset<size) {
			rxdesc=&(ticontext->pRxHostDescQPool[ticontext->rx_tail]);
			rxdesc->Ctl|=ACX100_CTL_OWN;
			rxdesc->Status=0xF0000000;	/* set the MSB */
			packetsize=(ptr->mac_cnt_rcvd & 0xfff)+ACX100_RXBUF_HDRSIZE; /* packetsize is limited to 12 bits */
			acxlog(L_DEBUG,"packetsize: %d\n",packetsize);
			if (packetsize>sizeof(struct rxbuffer)) {
				acxlog(L_STD,"acx100usb: ERROR, # of received bytes (%d) higher than capacity of buffer (%d bytes)\n",size,sizeof(struct rxbuffer));
				/* -------------------------
				** prevent buffer overflow
				** -----------------------*/
				packetsize=sizeof(struct rxbuffer);
			}
			memcpy(rxdesc->data,ptr,packetsize);
#ifdef ACX_DEBUG
			if (debug&L_DATA) {
				if ((packetsize>0)&&(packetsize<1024)) {
					acxlog(L_DATA,"received data:\n");
					acx100usb_dump_bytes(ptr,packetsize);
				}
			}
#endif
			/* ---------------------------------------------
			** now handle the received data....
			** ------------------------------------------ */
			acx100_process_rx_desc(priv);
			ptr=(rxbuffer_t *)(((char *)ptr)+packetsize);
			offset+=packetsize;
		}
	}
	/* -------------------------------
	** look for the next rx ...
	** ---------------------------- */
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) acx100usb_poll_rx(priv); /* receive of frame completed, now look for the next one */
	FN_EXIT(0,0);
}




/* ---------------------------------------------------------------------------
** acx100usb_tx_data():
** Inputs:
**    priv -> Pointer to wlandevice structure
**    desc -> Pointer to TX descriptor
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is called by acx100_dma_tx_data() and is responsible for
**  sending out the data within the given txdescriptor to the USB device.
**  In order to avoid inconsistency on SMP systems, a Mutex is checked
**  that forbids other packets to disturb the USB queue when there is
**  more than 1 packet within the Tx queue at a time. In case there is
**  a transfer in progress, this function immediately returns. It is
**  within the responsibility of the acx100usb_complete_tx() function to
**  ensure that these transfers are completed after the current transfer
**  was finished. In case there are no transfers in progress, the Mutex
**  is set and the transfer is triggered.
** ------------------------------------------------------------------------- */

void acx100usb_tx_data(wlandevice_t *priv,struct txdescriptor *desc)
{
	unsigned long flags;
	FN_ENTER;
	/* ------------------------------------
	** some sanity checks...
	** --------------------------------- */

	acxlog(L_DEBUG,"tx data\n");
	if ((!priv)||(!desc)) return;
	/*-----------------------------------------------
	** check if we are still not done sending the
	** last frames...
	** ------------------------------------------- */
	spin_lock_irqsave(&(priv->usb_tx_lock),flags);
	if (priv->usb_tx_mutex) {
		spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
		return;
	}
	/*-----------------------------------------------
	** indicate a tx frame in progress...
	** ------------------------------------------- */
	priv->usb_tx_mutex=1;
	spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
	acx100usb_prepare_tx(priv,desc);
	FN_EXIT(0,0);
}



/* ---------------------------------------------------------------------------
** acx100usb_prepare_tx():
** Inputs:
**    priv -> Pointer to wlandevice structure
**    desc -> Pointer to TX descriptor
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function inserts the given txdescriptor into the USB output buffer
**  and initiates the USB data transfer of the packet.
** ------------------------------------------------------------------------- */

static void acx100usb_prepare_tx(wlandevice_t *priv,struct txdescriptor *desc) {
	int txsize,ucode,size;
	acx100_usb_txfrm_t *buf;
	UINT8 *addr;
	struct usb_device *usbdev;
	unsigned int outpipe;
	struct txhostdescriptor *header,*payload;
	TIWLAN_DC *ticontext;
	FN_ENTER;
	priv->currentdesc = desc;
	ticontext=&(priv->dc);
	/* ------------------------------------------
	** extract header and payload from descriptor
	** --------------------------------------- */
	header = desc->host_desc;
	payload = desc->host_desc+1;
	/* ----------------------------------------------
	** concatenate header and payload into USB buffer
	** ------------------------------------------- */
	acxlog(L_XFER,"tx_data: headerlen=%d  payloadlen=%d\n",header->length,payload->length);
	buf=&(priv->bulkout.txfrm);
	size=header->length+payload->length;
	if (size>WLAN_DATA_MAXLEN) {
		acxlog(L_STD,"acx100usb: ERROR, USB buffer smaller than total data to send (%d vs. %d)\n",size,WLAN_DATA_MAXLEN);
		return;
	}
	memcpy(&(buf->data),header->data,header->length);
	memcpy(((char *)&(buf->data))+header->length,payload->data,payload->length);
	/* ----------------------------------------------
	** fill the USB transfer header
	** ------------------------------------------- */
	buf->hdr.desc=ACX100_USB_TX_DESC;
	buf->hdr.MPDUlen=cpu_to_le16(size);
	buf->hdr.ctrl1=0;
	buf->hdr.ctrl2=0;
	buf->hdr.hostData=cpu_to_le32(size|(desc->rate)<<24);
	if (1 == priv->preamble_flag)
		buf->hdr.ctrl1|=DESC_CTL_SHORT_PREAMBLE;
	buf->hdr.ctrl1|=DESC_CTL_FIRST_MPDU;
	buf->hdr.txRate=desc->rate;
	buf->hdr.index=1;
	buf->hdr.dataLength=cpu_to_le16(size|((buf->hdr.txRate)<<24));
	if (WLAN_GET_FC_FTYPE(((p80211_hdr_t *)header->data)->a3.fc)==WLAN_FTYPE_DATA) {
		buf->hdr.hostData|=(ACX100_USB_TXHI_ISDATA<<16);
	}
	addr=(((p80211_hdr_t *)(header->data))->a3.a3);
	if (acx100_is_mac_address_directed((mac_t *)addr)) buf->hdr.hostData|=cpu_to_le32((ACX100_USB_TXHI_DIRECTED<<16));
	if (acx100_is_mac_address_broadcast(addr)) buf->hdr.hostData|=cpu_to_le32((ACX100_USB_TXHI_BROADCAST<<16));
	acxlog(L_DATA,"Dump of bulk out urb:\n");
	if (debug&L_DATA) acx100usb_dump_bytes(buf,size+sizeof(acx100_usb_txhdr_t));
	/* ---------------------------------------------
	** check whether we need to send the data in
	** fragments (block larger than max bulkout
	** size)...
	** ------------------------------------------ */
	acxlog(L_XFER,"USB xfer size: %d\n",size+sizeof(acx100_usb_txhdr_t));
	priv->usb_txsize=size+sizeof(acx100_usb_txhdr_t);
	if (priv->usb_txsize<priv->usb_max_bulkout) {
		priv->usb_txoffset=priv->usb_txsize;
		txsize=priv->usb_txsize;
	} else {
		priv->usb_txoffset=priv->usb_max_bulkout;
		txsize=priv->usb_max_bulkout;
	}
	/* ---------------------------------------------
	** now schedule the USB transfer...
	** ------------------------------------------ */
	usbdev=priv->usbdev;
	outpipe=usb_sndbulkpipe(usbdev,1);      /* default endpoint for bulk-transfers: 1 */
	acxlog(L_XFER,"sending initial %d bytes (remain: %d)\n",txsize,priv->usb_txsize-priv->usb_txoffset);
	usb_fill_bulk_urb(priv->bulktx_urb,usbdev,outpipe,buf,txsize,acx100usb_complete_tx,priv);
	priv->bulktx_urb->transfer_flags=ASYNC_UNLINK;
	priv->bulktx_urb->timeout=ACX100_USB_TX_TIMEOUT;
	ucode=submit_urb(priv->bulktx_urb, GFP_KERNEL);
	acxlog(L_XFER,"dump: outpipe=%X buf=%p txsize=%d\n",outpipe,buf,txsize);
	if (ucode!=0) {
		acxlog(L_STD,"submit_urb() return code: %d (%s:%d) txsize=%d\n",ucode,__FILE__,__LINE__,txsize);
		/* -------------------------------------------------
		** on error, just mark the frame as done and update
		** the statistics...
		** ---------------------------------------------- */
		priv->stats.tx_errors++;
		acx100usb_trigger_next_tx(priv);
	}
	FN_EXIT(0,0);
}




/* ---------------------------------------------------------------------------
** acx100usb_send_tx_frags():
** Inputs:
**    priv -> Pointer to WLAN device structure
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function sends out the remaining bytes of a USB transfer buffer to
**  the endpoint, due to the restrictions on the max. transfer size of bulk
**  USB transfers.
** ------------------------------------------------------------------------- */

static void acx100usb_send_tx_frags(wlandevice_t *priv) {
	struct usb_device *usbdev;
	unsigned int outpipe;
	int ucode,diff,txsize;
	char *buf;
	FN_ENTER;

	buf=((char *)&(priv->bulkout))+priv->usb_txoffset;
	diff=priv->usb_txsize-priv->usb_txoffset;
	if (diff<=0) return;
	if (diff<priv->usb_max_bulkout) {
		priv->usb_txoffset+=diff;
		txsize=diff;
	} else {
		priv->usb_txoffset+=priv->usb_max_bulkout;
		txsize=priv->usb_max_bulkout;
	}
	acxlog(L_XFER,"sending %d bytes (remain: %d)\n",txsize,priv->usb_txsize-priv->usb_txoffset);
	usbdev=priv->usbdev;
	outpipe=usb_sndbulkpipe(usbdev,1);      /* default endpoint for bulk-transfers: 1 */
	usb_fill_bulk_urb(priv->bulktx_urb,usbdev,outpipe,buf,txsize,acx100usb_complete_tx,priv);
	priv->bulktx_urb->transfer_flags=ASYNC_UNLINK;
	priv->bulktx_urb->timeout=ACX100_USB_TX_TIMEOUT;
	ucode=submit_urb(priv->bulktx_urb, GFP_KERNEL);
	if (ucode!=0) {
		acxlog(L_STD,"submit_urb() return code: %d (%s:%d)\n",ucode,__FILE__,__LINE__);
		/* -------------------------------------------------
		** on error, just mark the frame as done and update
		** the statistics...
		** ---------------------------------------------- */
		priv->stats.tx_errors++;
		acx100usb_trigger_next_tx(priv);
	}
	FN_EXIT(0,0);
}


/* ---------------------------------------------------------------------------
** acx100usb_flush_tx():
** Inputs:
**    priv -> Pointer to WLAN device structure
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function sends out a null packet to the USB device. Because the
**  device has an internal data buffer, transfers of packets whose size exactly
**  divides the max. size of bulk transfers are not recognized as finished by
**  the card. 
** ------------------------------------------------------------------------- */

static void acx100usb_flush_tx(wlandevice_t *priv)
{
	struct usb_device *usbdev;
	unsigned int outpipe;
	int ucode;
	char *buf;
	FN_ENTER;
	buf=((char *)&(priv->bulkout))+priv->usb_txoffset;
	priv->usb_txoffset++;  /* just to make sure that this function is invoked only ONCE */
	usbdev=priv->usbdev;
	outpipe=usb_sndbulkpipe(usbdev,1);      /* default endpoint for bulk-transfers: 1 */
	usb_fill_bulk_urb(priv->bulktx_urb,usbdev,outpipe,buf,0,&(acx100usb_complete_tx),priv);
	priv->bulktx_urb->transfer_flags=ASYNC_UNLINK;
	priv->bulktx_urb->timeout=ACX100_USB_TX_TIMEOUT;
	ucode=submit_urb(priv->bulktx_urb, GFP_KERNEL);
        if (ucode!=0) {
		acxlog(L_STD,"submit_urb() return code: %d (%s:%d)\n",ucode,__FILE__,__LINE__);
		/* -------------------------------------------------
		** on error, just mark the frame as done and update
		** the statistics...
		** ---------------------------------------------- */
		priv->stats.tx_errors++;
		acx100usb_trigger_next_tx(priv);
	}
	FN_EXIT(0,0);
}




/* ---------------------------------------------------------------------------
** acx100usb_complete_tx():
** Inputs:
**     urb -> Pointer to USB request block
**    regs -> Pointer to register-buffer for syscalls (see asm/ptrace.h)
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
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
** ------------------------------------------------------------------------- */

#if USB_24
static void acx100usb_complete_tx(struct urb *urb)
#else
static void acx100usb_complete_tx(struct urb *urb, struct pt_regs *regs)
#endif
{
	wlandevice_t *priv;

	FN_ENTER;

	priv=(wlandevice_t *)urb->context;
	if (!priv) acxlog(L_STD,"ERROR, encountered NULL WLAN device in URB\n");
	/* ------------------------------
	** handle USB transfer errors...
	** --------------------------- */
	if (urb->status!=0) {
		acx100usb_trigger_next_tx(priv);
	}
	/* ---------------------------------------------
	** check if there is more data to transmit...
	** --------------------------------------------- */
	acxlog(L_XFER,"transfer_buffer: %p  priv->bulkout: %p\n",urb->transfer_buffer,&(priv->bulkout));
	acxlog(L_XFER,"actual length: %d  status: %d\n",urb->actual_length,urb->status);
	acxlog(L_XFER,"bulk xmt completed (status=%d, len=%d) TxQueueFree=%d\n",urb->status,urb->actual_length,priv->TxQueueFree);
	if (priv->usb_txoffset<priv->usb_txsize) {
		acx100usb_send_tx_frags(priv);
	} else {
		if ((priv->usb_txoffset==priv->usb_txsize)&&((priv->usb_txoffset%priv->usb_max_bulkout)==0)) {
			/* ---------------------------------------------
			** in case the block was exactly the maximum tx
			** size of the bulk endpoint, send out a null
			** message so that the card sends out the block
			** from its internal memory...
			** --------------------------------------------- */
			acx100usb_flush_tx(priv);
		}
		acx100usb_trigger_next_tx(priv);
	}
	FN_EXIT(0,0);
}


/* ---------------------------------------------------------------------------
** acx100usb_trigger_next_tx():
** Inputs:
**     priv -> Pointer to WLAN device structure
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is invoked when the transfer of the current txdescriptor
**  to send is completed OR if there went something wrong during the transfer
**  of the descriptor. In either case, this function checks if there are more
**  descriptors to send and handles the descriptors over to the 
**  acx100usb_prepare_tx() function. If there are no more descriptors to send,
**  the transfer-in-progress Mutex is released and the network tx queue is
**  kicked back to life.
** ------------------------------------------------------------------------ */

static void acx100usb_trigger_next_tx(wlandevice_t *priv) {
	struct txdescriptor *txdesc;
	struct txhostdescriptor *header,*payload;
	int descnum;
	unsigned long flags;
	struct TIWLAN_DC *ticontext;
	/* ----------------------------
	** grab the TI device context
	** -------------------------- */
	ticontext=&(priv->dc);
	/* ----------------------------------------------
	** free the txdescriptor...
	** ------------------------------------------- */
	txdesc=priv->currentdesc;
	header = txdesc->host_desc;
	payload = txdesc->host_desc+1;
	header->Ctl|=DESC_CTL_DONE;
	payload->Ctl|=DESC_CTL_DONE;
	txdesc->Ctl|=DESC_CTL_DONE;
	acx100_clean_tx_desc(priv);
	/* ----------------------------------------------
	** check if there are still descriptors that have
	** to be sent. acx100_clean_tx_desc() should
	** have set the next non-free descriptor position
	** in tx_tail...
	** ------------------------------------------- */
	descnum=ticontext->tx_tail;
	txdesc=&(ticontext->pTxDescQPool[descnum]);
	if (!(txdesc->Ctl & ACX100_CTL_OWN)) {
		acx100usb_prepare_tx(priv,txdesc);
	} else {
		/* ----------------------------------------------
		** now release the mutex and wake the output
		** queue...
		** ------------------------------------------- */
		spin_lock_irqsave(&(priv->usb_tx_lock),flags);
		priv->usb_tx_mutex=0;
		spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
		if (priv->dev_state_mask&ACX_STATE_IFACE_UP) netif_wake_queue(priv->netdev);
	}
}


/* ---------------------------------------------------------------------------
** acx100usb_stop():
** Inputs:
**    dev -> Pointer to network device structure
** ---------------------------------------------------------------------------
** Returns:
**  (int) 0 on success, or error-code 
**
** Description:
**  This function stops the network functionality of the interface (invoked
**  when the user calls ifconfig <wlan> down). The tx queue is halted and
**  the device is markes as down. In case there were any pending USB bulk
**  transfers, these are unlinked (asynchronously). The module in-use count
**  is also decreased in this function.
** ------------------------------------------------------------------------- */

static int acx100usb_stop(struct net_device *dev)
{
	wlandevice_t *priv;
	client_t client;
	int already_down;
	if (!(dev->priv)) {
		printk(KERN_ERR SHORTNAME ": no pointer to acx100 context in network device, cannot operate.\n");
		return -ENODEV;
	}
	priv=dev->priv;
	FN_ENTER;
	/* ------------------------------
	** Transmit a disassociate frame
	** --------------------------- */
	acx100_transmit_disassoc(&client,priv);
	/* --------------------------------
	* stop the transmit queue...
	* ------------------------------ */
	if (!netif_queue_stopped(dev)) {
		netif_stop_queue(dev);
	}
	/* --------------------------------
	** mark the device as DOWN
	** ----------------------------- */
	if (priv->dev_state_mask&ACX_STATE_IFACE_UP) already_down=0; else already_down=1;
	priv->dev_state_mask&=~ACX_STATE_IFACE_UP;
	/* ----------------------------------------
	* interrupt pending bulk rx transfers ....
	* ------------------------------------- */
	acxlog(L_DEBUG,"bulkrx status=%d  bulktx status=%d\n",priv->bulkrx_urb->status,priv->bulktx_urb->status);
	if (priv->bulkrx_urb->status==-EINPROGRESS) usb_unlink_urb(priv->bulkrx_urb);
	/* -----------------------------------------------------
	** wait for the disassoc packet to leave the building...
	** FIXME: there must be a better way
	** -------------------------------------------------- */
 	mdelay(250); 
	/* ------------------------
	** disable rx and tx ...
	** --------------------- */
	acx100_issue_cmd(priv,ACX100_CMD_DISABLE_TX,NULL,0,5000);
	acx100_issue_cmd(priv,ACX100_CMD_DISABLE_RX,NULL,0,5000);
	/* -------------------------
	** power down the device...
	** ---------------------- */
	acx100_issue_cmd(priv,ACX100_CMD_SLEEP,NULL,0,5000);
	/* --------------------------------------------
	** decrease module-in-use count (if necessary)
	** ----------------------------------------- */
	if (!already_down) WLAN_MOD_DEC_USE_COUNT;
	FN_EXIT(1,0);
	return 0;
}



/* ---------------------------------------------------------------------------
** acx100usb_start_xmit():
** Inputs:
**    skb -> Pointer to sk_buffer that contains the data to send 
**    dev -> Pointer to the network device this transfer is directed to
** ---------------------------------------------------------------------------
** Returns:
**  (int) 0 on success, or error-code
**
** Description:
** ------------------------------------------------------------------------- */

static int acx100usb_start_xmit(struct sk_buff *skb, netdevice_t * dev) {
	int txresult = 0;
	unsigned long flags;
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	struct txdescriptor *tx_desc;
	int templen;

	FN_ENTER;

	if (!skb) {
		return 0;
	}
	if (!priv) {
		return 1;
	}
  /*
  if (!(priv->open)) {
		return 1;
	}
  */
	/* --------------------------------------
	** if the device is otherwise locked,
	** bail-out...
	** ----------------------------------- */
	if (acx100_lock(priv, &flags))
		return 1;
	/* --------------------------------------
	** if the queue is halted, there is no point
	** in sending out data...
	** ----------------------------------- */
	if (netif_queue_stopped(dev)) {
		acxlog(L_STD, "%s: called when queue stopped\n", __func__);
		txresult = 1;
		goto end;
	}
	/* --------------------------------------
	** there is no one to talk to...
	** ----------------------------------- */
	if (priv->status != ISTATUS_4_ASSOCIATED) {
		acxlog(L_STD, "Trying to xmit, but not associated yet: aborting...\n");
		/* silently drop the packet, since we're not connected yet */
		dev_kfree_skb(skb);
		priv->stats.tx_errors++;
		txresult = 0;
		goto end;
	}
#if 0
	/* we're going to transmit now, so stop another packet from entering.
	 * FIXME: most likely we shouldn't do it like that, but instead:
	 * stop the queue during card init, then wake the queue once
	 * we're associated to the network, then stop the queue whenever
	 * we don't have any free Tx buffers left, and wake it again once a
	 * Tx buffer becomes free again. And of course also stop the
	 * queue once we lose association to the network (since it
	 * doesn't make sense to allow more user packets if we can't
	 * forward them to a network).
	 * FIXME: Hmm, seems this is all wrong. We SHOULD leave the
	 * queue open from the beginning (as long as we're not full,
	 * and also even before we're even associated),
	 * otherwise we'll get NETDEV WATCHDOG transmit timeouts... */
	acxlog(L_XFER, "stop queue during Tx.\n");
	netif_stop_queue(dev);
#endif
#if UNUSED
	if (acx100_lock(priv,&flags)) ...

	memset(pb, 0, sizeof(wlan_pb_t) /*0x14*4 */ );

	pb->ethhostbuf = skb;
	pb->ethbuf = skb->data;
#endif
	templen = skb->len;
	/* ------------------------------------
	** get the next free tx descriptor...
	** -------------------------------- */
	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		acxlog(L_BINSTD,"BUG: txdesc ring full\n");
		txresult = 1;
		goto end;
	}
	/* ------------------------------------
	** convert the ethernet frame into our
	** own tx descriptor type...
	** -------------------------------- */
	acx100_ether_to_txdesc(priv,tx_desc,skb);
	/* -------------------------
	** free the skb
	** ---------------------- */
	dev_kfree_skb(skb);
	dev->trans_start = jiffies;
	/* -----------------------------------
	** until the packages are flushed out,
	** stop the queue...
	** -------------------------------- */
	netif_stop_queue(dev);
	/* -----------------------------------
	** transmit the data...
	** -------------------------------- */
	acx100_dma_tx_data(priv, tx_desc);  /* this function finally calls acx100usb_tx_data() */
	txresult=0;
	/* -----------------------------------
	** statistical bookkeeping...
	** -------------------------------- */
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += templen;
end:
	acx100_unlock(priv, &flags);
	FN_EXIT(1, txresult);
	return txresult;
}



static struct net_device_stats *acx100_get_stats(netdevice_t *dev) {
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	FN_ENTER;
	FN_EXIT(1, (int)&priv->stats);
	return &priv->stats;
}



static struct iw_statistics *acx100_get_wireless_stats(netdevice_t *dev) {
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	FN_ENTER;
	FN_EXIT(1, (int)&priv->stats);
	return &priv->wstats;
}



static void acx100usb_set_rx_mode(struct net_device *dev)
{
}



#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *dev) {
	wlandevice_t *priv;
	FN_ENTER;
	priv=dev->priv;
	/* ------------------------------------
	** unlink the URB....
	** --------------------------------- */
	usb_unlink_urb(priv->bulktx_urb);
	/* ------------------------------------
	** TODO: stats update
	** --------------------------------- */
	FN_EXIT(0,0);
}
#endif





/* ---------------------------------------------------------------------------
** init_module():
** Inputs:
**  <NONE>
** ---------------------------------------------------------------------------
** Returns:
**  (int) Errorcode on failure, 0 on success
**
** Description:
**  This function is invoked upon loading of the kernel module. It registers
**  itself at the Kernel's USB subsystem.
** ------------------------------------------------------------------------ */

int init_module() {
	int err;
	printk(KERN_INFO "Initializing acx100 WLAN USB kernel module\n");
	/* ------------------------------------------------------
	** Register this driver to the USB subsystem
	** --------------------------------------------------- */
	err=usb_register(&acx100usb_driver);
	if (!err) {
		return(err);
	}
	return(0);
}



/* ---------------------------------------------------------------------------
** cleanup_module():
** Inputs:
**  <NONE>
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  This function is invoked as last step of the module unloading. It simply
**  deregisters this module at the Kernel's USB subsystem.
** ------------------------------------------------------------------------- */

void cleanup_module() {
	usb_deregister(&acx100usb_driver);
	printk(KERN_INFO "Cleaning up acx100 WLAN USB kernel module\n");
}



/* ---------------------------------------------------------------------------
**                                   DEBUG STUFF
** --------------------------------------------------------------------------- */

#ifdef ACX_DEBUG
#if USB_24
static char *acx100usb_pstatus(int val)
{
#define CASE(status)	case status: return ""#status""
	switch (val) {
		CASE(USB_ST_NOERROR);
		CASE(USB_ST_CRC);
		CASE(USB_ST_BITSTUFF);
		CASE(USB_ST_DATAOVERRUN);
		CASE(USB_ST_BUFFEROVERRUN);
		CASE(USB_ST_BUFFERUNDERRUN);
		CASE(USB_ST_SHORT_PACKET);
		CASE(USB_ST_URB_KILLED);
		CASE(USB_ST_URB_PENDING);
		CASE(USB_ST_REMOVED);
		CASE(USB_ST_TIMEOUT);
		CASE(USB_ST_NOTSUPPORTED);
		CASE(USB_ST_BANDWIDTH_ERROR);
		CASE(USB_ST_URB_INVALID_ERROR);
		CASE(USB_ST_URB_REQUEST_ERROR);
		CASE(USB_ST_STALL);
	default:
		return "UNKNOWN";
	}
}
#else
static char *acx100usb_pstatus(int val)
{
	static char status[80];

	if (val < 0)
		sprintf(status, "errno %d\n", -val);
	else
		sprintf(status, "length %d\n", val);

	return status;
}
#endif

static void dump_device(struct usb_device *usbdev)
{
  int i;
  struct usb_config_descriptor *cd;

  printk(KERN_INFO "acx100 device dump:\n");
  printk(KERN_INFO "  devnum: %d\n",usbdev->devnum);
  printk(KERN_INFO "  speed: %d\n",usbdev->speed);
  printk(KERN_INFO "  tt: 0x%X\n",(unsigned int)(usbdev->tt));
  printk(KERN_INFO "  ttport: %d\n",(unsigned int)(usbdev->ttport));
  printk(KERN_INFO "  toggle[0]: 0x%X  toggle[1]: 0x%X\n",(unsigned int)(usbdev->toggle[0]),(unsigned int)(usbdev->toggle[1]));
  printk(KERN_INFO "  halted[0]: 0x%X  halted[1]: 0x%X\n",usbdev->halted[0],usbdev->halted[1]);
  printk(KERN_INFO "  epmaxpacketin: ");
  for (i=0;i<16;i++) printk("%d ",usbdev->epmaxpacketin[i]);
  printk("\n");
  printk(KERN_INFO "  epmaxpacketout: ");
  for (i=0;i<16;i++) printk("%d ",usbdev->epmaxpacketout[i]);
  printk("\n");
  printk(KERN_INFO "  parent: 0x%X\n",(unsigned int)(usbdev->parent));
  printk(KERN_INFO "  bus: 0x%X\n",(unsigned int)(usbdev->bus));
#if NO_DATATYPE
  printk(KERN_INFO "  configs: ");
  for (i=0;i<usbdev->descriptor.bNumConfigurations;i++) printk("0x%X ",usbdev->config[i]);
  printk("\n");
#endif
  printk(KERN_INFO "  actconfig: %p\n",usbdev->actconfig);
  dump_device_descriptor(&(usbdev->descriptor));
#if USB_24
  cd = usbdev->actconfig;
#else
  cd = &usbdev->config->desc;
#endif
  dump_config_descriptor(cd);
#if USB_24
  {
    struct usb_interface *ifc;
    ifc=cd->interface;
    if (ifc) {
      printk(KERN_INFO "iface: altsetting=%p act_altsetting=%d  num_altsetting=%d  max_altsetting=%d\n",ifc->altsetting,ifc->act_altsetting,ifc->num_altsetting,ifc->max_altsetting);
      dump_interface_descriptor(ifc->altsetting);
      dump_endpoint_descriptor(ifc->altsetting->endpoint);
    }
  }
#endif
}


static void dump_config_descriptor(struct usb_config_descriptor *cd)
{
  printk(KERN_INFO "Configuration Descriptor:\n");
  if (!cd) {
    printk(KERN_INFO "NULL\n");
    return;
  }
  printk(KERN_INFO "  bLength: %d (0x%X)\n",cd->bLength,cd->bLength);
  printk(KERN_INFO "  bDescriptorType: %d (0x%X)\n",cd->bDescriptorType,cd->bDescriptorType);
  printk(KERN_INFO "  bNumInterfaces: %d (0x%X)\n",cd->bNumInterfaces,cd->bNumInterfaces);
  printk(KERN_INFO "  bConfigurationValue: %d (0x%X)\n",cd->bConfigurationValue,cd->bConfigurationValue);
  printk(KERN_INFO "  iConfiguration: %d (0x%X)\n",cd->iConfiguration,cd->iConfiguration);
  printk(KERN_INFO "  bmAttributes: %d (0x%X)\n",cd->bmAttributes,cd->bmAttributes);
  //printk(KERN_INFO "  MaxPower: %d (0x%X)\n",cd->bMaxPower,cd->bMaxPower);
}

static void dump_device_descriptor(struct usb_device_descriptor *dd)
{
  printk(KERN_INFO "Device Descriptor:\n");
  if (!dd) {
    printk(KERN_INFO "NULL\n");
    return;
  }
  printk(KERN_INFO "  bLength: %d (0x%X)\n",dd->bLength,dd->bLength);
  printk(KERN_INFO "  bDescriptortype: %d (0x%X)\n",dd->bDescriptorType,dd->bDescriptorType);
  printk(KERN_INFO "  bcdUSB: %d (0x%X)\n",dd->bcdUSB,dd->bcdUSB);
  printk(KERN_INFO "  bDeviceClass: %d (0x%X)\n",dd->bDeviceClass,dd->bDeviceClass);
  printk(KERN_INFO "  bDeviceSubClass: %d (0x%X)\n",dd->bDeviceSubClass,dd->bDeviceSubClass);
  printk(KERN_INFO "  bDeviceProtocol: %d (0x%X)\n",dd->bDeviceProtocol,dd->bDeviceProtocol);
  printk(KERN_INFO "  bMaxPacketSize0: %d (0x%X)\n",dd->bMaxPacketSize0,dd->bMaxPacketSize0);
  printk(KERN_INFO "  idVendor: %d (0x%X)\n",dd->idVendor,dd->idVendor);
  printk(KERN_INFO "  idProduct: %d (0x%X)\n",dd->idProduct,dd->idProduct);
  printk(KERN_INFO "  bcdDevice: %d (0x%X)\n",dd->bcdDevice,dd->bcdDevice);
  printk(KERN_INFO "  iManufacturer: %d (0x%X)\n",dd->iManufacturer,dd->iManufacturer);
  printk(KERN_INFO "  iProduct: %d (0x%X)\n",dd->iProduct,dd->iProduct);
  printk(KERN_INFO "  iSerialNumber: %d (0x%X)\n",dd->iSerialNumber,dd->iSerialNumber);
  printk(KERN_INFO "  bNumConfigurations: %d (0x%X)\n",dd->bNumConfigurations,dd->bNumConfigurations);
}

#if USB_24
static void dump_usbblock(char *block,int bytes)
{
  int i;
  for (i=0;i<bytes;i++) {
    if ((i&0xF)==0) {
      if (i!=0) printk("\n");
      printk(KERN_INFO);
    }
    printk("%02X ",(unsigned char)(block[i]));
  }
}

static void dump_endpoint_descriptor(struct usb_endpoint_descriptor *ep)
{
  printk(KERN_INFO "Endpoint Descriptor:\n");
  if (!ep) {
    printk(KERN_INFO "NULL\n");
    return;
  }
  printk(KERN_INFO "  bLength: %d (0x%X)\n",ep->bLength,ep->bLength);
  printk(KERN_INFO "  bDescriptorType: %d (0x%X)\n",ep->bDescriptorType,ep->bDescriptorType);
  printk(KERN_INFO "  bEndpointAddress: %d (0x%X)\n",ep->bEndpointAddress,ep->bEndpointAddress);
  printk(KERN_INFO "  bmAttributes: 0x%X\n",ep->bmAttributes);
  printk(KERN_INFO "  wMaxPacketSize: %d (0x%X)\n",ep->wMaxPacketSize,ep->wMaxPacketSize);
  printk(KERN_INFO "  bInterval: %d (0x%X)\n",ep->bInterval,ep->bInterval);
  printk(KERN_INFO "  bRefresh: %d (0x%X)\n",ep->bRefresh,ep->bRefresh);
  printk(KERN_INFO "  bSyncAdrress: %d (0x%X)\n",ep->bSynchAddress,ep->bSynchAddress);
}

static void dump_interface_descriptor(struct usb_interface_descriptor *id)
{
  printk(KERN_INFO "Interface Descriptor:\n");
  if (!id) {
    printk(KERN_INFO "NULL\n");
    return;
  }
  printk(KERN_INFO "  bLength: %d (0x%X)\n",id->bLength,id->bLength);
  printk(KERN_INFO "  bDescriptorType: %d (0x%X)\n",id->bDescriptorType,id->bDescriptorType);
  printk(KERN_INFO "  bInterfaceNumber: %d (0x%X)\n",id->bInterfaceNumber,id->bInterfaceNumber);
  printk(KERN_INFO "  bAlternateSetting: %d (0x%X)\n",id->bAlternateSetting,id->bAlternateSetting);
  printk(KERN_INFO "  bNumEndpoints: %d (0x%X)\n",id->bNumEndpoints,id->bNumEndpoints);
  printk(KERN_INFO "  bInterfaceClass: %d (0x%X)\n",id->bInterfaceClass,id->bInterfaceClass);
  printk(KERN_INFO "  bInterfaceSubClass: %d (0x%X)\n",id->bInterfaceSubClass,id->bInterfaceSubClass);
  printk(KERN_INFO "  bInterfaceProtocol: %d (0x%X)\n",id->bInterfaceProtocol,id->bInterfaceProtocol);
  printk(KERN_INFO "  iInterface: %d (0x%X)\n",id->iInterface,id->iInterface);
#if USB_24
  printk(KERN_INFO "  endpoint: 0x%X\n",(unsigned int)(id->endpoint));
#endif
}
#endif

void acx100usb_dump_bytes(void *data,int num)
{
  int i,remain=num;
  unsigned char *ptr=(unsigned char *)data;
  while (remain>0) {
    if (remain<16) {
      printk(KERN_WARNING);
      for (i=0;i<remain;i++) printk("%02X ",*ptr++);
      printk("\n");
      remain=0;
    } else {
      printk(KERN_WARNING);
      for (i=0;i<16;i++) printk("%02X ",*ptr++);
      printk("\n");
      remain-=16;
    }
  }
}

#endif
