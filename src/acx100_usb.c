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
#define QUEUE_BULK	0
#define ZERO_PACKET	URB_ZERO_PACKET

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

#ifndef SET_NETDEV_DEV
#define SET_NETDEV_DEV(x,y)
#endif
#define SET_NETDEV_OWNER(ndev,owner) ndev->owner = owner

#define ASYNC_UNLINK	USB_ASYNC_UNLINK
#define QUEUE_BULK	USB_QUEUE_BULK
#define ZERO_PACKET	USB_ZERO_PACKET

static inline int submit_urb(struct urb *urb, int mem_flags) {
        return usb_submit_urb(urb);
}
static inline struct urb *alloc_urb(int iso_pk, int mem_flags) {
        return usb_alloc_urb(iso_pk);
}

static inline void usb_set_intfdata(struct usb_interface *intf, void *data) {}

#endif /* #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) */

/* -------------------------------------------------------------------------
**                             Module Stuff
** ---------------------------------------------------------------------- */

char *firmware_dir;

/* unsigned int debug = L_DEBUG|L_ASSOC|L_INIT|L_STD; */
unsigned int debug = L_STD; 

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>");
MODULE_DESCRIPTION("TI ACX100 WLAN USB Driver");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#ifdef ACX_DEBUG
module_param(debug, uint, 0);
#endif
module_param(firmware_dir, charp, 0);
#else
#ifdef ACX_DEBUG
MODULE_PARM(debug, "i");
#endif
MODULE_PARM(firmware_dir, "s");
#endif

MODULE_PARM_DESC(debug, "Debug level mask: 0x0000 - 0x3fff");
MODULE_PARM_DESC(firmware_dir, "Directory to load acx100 firmware files from");

/* -------------------------------------------------------------------------
**                Module Definitions (preprocessor based)
** ---------------------------------------------------------------------- */


#define SHORTNAME "acx_usb: "

#define ACX100_VENDOR_ID 0x2001
#define ACX100_PRODUCT_ID_UNBOOTED 0x3B01
#define ACX100_PRODUCT_ID_BOOTED 0x3B00

/* RX-Timeout: NONE (request waits forever) */
#define ACX100_USB_RX_TIMEOUT (0)    

#define ACX100_USB_TX_TIMEOUT (4*HZ)


/* -------------------------------------------------------------------------
**                        Module Data Structures
** ---------------------------------------------------------------------- */

typedef struct {
	void *device;
	int number;
} acx_usb_bulk_context_t;


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
static int acx100usb_close(struct net_device *);
static int acx100usb_start_xmit(struct sk_buff *,struct net_device *);
static void acx100usb_set_rx_mode(struct net_device *);
static int init_network_device(struct net_device *);
static int acx100usb_boot(struct usb_device *);
void acx100usb_tx_data(wlandevice_t *,struct txdescriptor *);
static void acx100usb_prepare_tx(wlandevice_t *,struct txdescriptor *);
static void acx100usb_trigger_next_tx(wlandevice_t *);

static struct net_device_stats * acx_get_stats(struct net_device *);
static struct iw_statistics *acx_get_wireless_stats(struct net_device *);


static void acx100usb_poll_rx(wlandevice_t *,int number);
void acx_rx(struct rxhostdescriptor *,wlandevice_t *);

int init_module(void);
void cleanup_module(void);

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *);
#endif

#ifdef ACX_DEBUG
int txbufsize;
int rxbufsize;
static char * acx100usb_pstatus(int);
extern void acx_dump_bytes(void *,int);
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

/* FIXME: static variable, big no-no!! might disrupt operation of two USB
 * adapters at the same time! */
static int disconnected=0;

extern const struct iw_handler_def acx_ioctl_handler_def;

static const struct usb_device_id acx100usb_ids[] = {
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


acx_usb_bulk_context_t rxcons[ACX100_USB_NUM_BULK_URBS];
acx_usb_bulk_context_t txcons[ACX100_USB_NUM_BULK_URBS];


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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
	struct usb_host_endpoint *ep;
#endif
	struct usb_interface_descriptor *ifdesc;
	int i,j,numep;
#if USB_24
	void *res = NULL;
#else
	int res = 0;
#endif

	FN_ENTER;
	/* ---------------------------------------------
	** First check if this is the "unbooted" hardware
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
		res = NULL;
#else
		res = 0; /* is that ok?? */
#endif
		goto end;
	}
	if ((usbdev->descriptor.idVendor==ACX100_VENDOR_ID)&&(usbdev->descriptor.idProduct==ACX100_PRODUCT_ID_BOOTED)) {
		/* ---------------------------------------------
		** allocate memory for the device driver context
		** --------------------------------------------- */
		priv = kmalloc(sizeof(struct wlandevice),GFP_KERNEL);
		if (!priv) {
			printk(KERN_WARNING SHORTNAME ": could not allocate %d bytes memory for device driver context, giving up.\n",sizeof(struct wlandevice));
			res = OUTOFMEM;
			goto end;
		}
		memset(priv,0,sizeof(wlandevice_t));
		priv->chip_type = CHIPTYPE_ACX100;
		priv->radio_type = RADIO_MAXIM_0D; /* FIXME: should be read from register (via firmware) using standard ACX code */
		priv->usbdev=usbdev;
		/* ---------------------------------------------
		** Initialize the device context and also check
		** if this is really the hardware we know about.
		** If not sure, at least notify the user that he
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
		if (numfaces!=1) printk(KERN_WARNING SHORTNAME "number of interfaces is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numfaces);
		/* --------------------------------------------
		 * ----------------------------------------- */
#ifdef USB_24
		ifdesc = config->interface->altsetting;
#else		
		ifdesc = &(intf->altsetting->desc);
#endif
		numep = ifdesc->bNumEndpoints;
		acxlog(L_STD,"# of endpoints: %d\n",numep);
		/* ------------------------------------------
		 * obtain information about the endpoint
		 * addresses, begin with some default values
		 * --------------------------------------- */
		priv->bulkoutep=1;
		priv->bulkinep=1;
		for (i=0;i<numep;i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
			ep = usbdev->ep_in[i];
			if(!ep) continue;
			epdesc = &ep->desc;
#else
			epdesc=usb_epnum_to_ep_desc(usbdev,i);
			if (!epdesc) continue;
#endif
			if ((epdesc->bmAttributes)&USB_ENDPOINT_XFER_BULK) {
				if ((epdesc->bEndpointAddress)&0x80) priv->bulkinep=(epdesc->bEndpointAddress)&0xF;
				else priv->bulkoutep=(epdesc->bEndpointAddress)&0xF;
			}
		}
		acxlog(L_STD,"bulkout ep: 0x%X\n",priv->bulkoutep);
		acxlog(L_STD,"bulkin ep: 0x%X\n",priv->bulkinep);
		/* --------------------------------------------
		** Set the packet-size equivalent to the
		** buffer size...
		** ------------------------------------------ */
		txbufsize=sizeof(acx100_usb_txfrm_t);
		rxbufsize=sizeof(acx100_usbin_t);
		rxbufsize&=~0x3f; /* make it a multiply of 6ty fucking 4 ! */
		priv->rxtruncsize=0;
		priv->rxtruncation=0;
		printk(KERN_INFO SHORTNAME "txbufsize=%d rxbufsize=%d\n",txbufsize,rxbufsize);
		/* ---------------------------------------------
		** initialize custom spinlocks...
		** --------------------------------------------- */
		spin_lock_init(&(priv->usb_ctrl_lock));
		spin_lock_init(&(priv->usb_tx_lock));
		priv->currentdesc=0;
		priv->usb_free_tx=ACX100_USB_NUM_BULK_URBS;
		for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
			priv->bulktx_states[i]=0;
		}
		/* ---------------------------------------------
		** Allocate memory for a network device
		** --------------------------------------------- */
		if ((dev = kmalloc(sizeof(struct net_device),GFP_ATOMIC))==NULL) {
			printk(KERN_WARNING SHORTNAME ": failed to alloc netdev\n");
			kfree(priv);
			res = OUTOFMEM;
			goto end;
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
			res = OUTOFMEM;
			goto end;
		}
		/* ---------------------------------------------
		** Setup URBs for bulk-in messages
		** --------------------------------------------- */
		for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
			priv->bulkrx_urbs[i]=alloc_urb(0, GFP_KERNEL);
			if (!priv->bulkrx_urbs[i]) {
				printk(KERN_WARNING SHORTNAME ": failed to allocate input URB\n");
				for (j=0;j<i;j++) usb_free_urb(priv->bulkrx_urbs[j]);
				usb_free_urb(priv->ctrl_urb);
				kfree(dev);
				kfree(priv);
				res = OUTOFMEM;
				goto end;
			}
			priv->bulkrx_urbs[i]->status=0;
		}
		/* ---------------------------------------------
		** Setup URBs for bulk-out messages
		** --------------------------------------------- */
		for(i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
			priv->bulktx_urbs[i]=alloc_urb(0, GFP_KERNEL);
			if (!priv->bulktx_urbs[i]) {
				printk(KERN_WARNING SHORTNAME ": failed to allocate output URB\n");
				usb_free_urb(priv->ctrl_urb);
				for (j=0;j<i;j++) usb_free_urb(priv->bulktx_urbs[j]);
				for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) usb_free_urb(priv->bulkrx_urbs[i]);
				kfree(dev);
				kfree(priv);
				res = OUTOFMEM;
				goto end;
			}
			priv->bulktx_urbs[i]->status=0;
		}
		/* --------------------------------------
		** Allocate a network device name
		** -------------------------------------- */
		acxlog(L_INIT,"allocating device name\n");
		result=dev_alloc_name(dev,"wlan%d");
		if (result<0) {
			printk(KERN_ERR SHORTNAME ": failed to allocate wlan device name (errcode=%d), giving up.\n",result);
			kfree(dev);
			usb_free_urb(priv->ctrl_urb);
			for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
				usb_free_urb(priv->bulkrx_urbs[i]);
				usb_free_urb(priv->bulktx_urbs[i]);
			}
			kfree(priv);
			res = OUTOFMEM;
			goto end;
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
			printk(KERN_ERR SHORTNAME "failed to register network device for USB WLAN (errcode=%d), giving up.\n",result);
			kfree(dev);
			usb_free_urb(priv->ctrl_urb);
			for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
				usb_free_urb(priv->bulkrx_urbs[i]);
				usb_free_urb(priv->bulktx_urbs[i]);
			}	
			kfree(priv);
			res = OUTOFMEM;
			goto end;
		}
#ifdef CONFIG_PROC_FS
		if (OK != acx_proc_register_entries(dev)) {
			acxlog(L_INIT, "/proc registration failed\n");
		}
#endif

		/* --------------------------------------
		** Everything went OK, we are happy now
		** ----------------------------------- */
#if USB_24
		res = priv;
#else
		res = 0;
#endif
		goto end;
	}
	/* --------------------------------------
	** no device we could handle, return NULL
	** -------------------------------------- */
#if USB_24
	res = NULL;
#else
	res = -EIO;
#endif
end:
	FN_EXIT1((int)res);
	return res;
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
	int i,result;

	if (disconnected) return;
	/* --------------------------------------
	** No WLAN device...no sense
	** ----------------------------------- */
	if (NULL == priv) 
		return;
	/* -------------------------------------
	** stop the transmit queue...
	** ---------------------------------- */
	if (priv->netdev) {
		rtnl_lock();
		if (!acx_queue_stopped(priv->netdev)) {
			acx_stop_queue(priv->netdev,"on USB disconnect");
		}
		rtnl_unlock();
#ifdef CONFIG_PROC_FS
		acx_proc_unregister_entries(priv->netdev);
#endif

	}	
	/* ---------------------------------------
	** now abort pending URBs and free them...
	** ------------------------------------ */
	if (priv->ctrl_urb) {
		if (priv->ctrl_urb->status==-EINPROGRESS) usb_unlink_urb(priv->ctrl_urb);
		while (priv->ctrl_urb->status==-EINPROGRESS) {
			mdelay(2);
		}
		usb_free_urb(priv->ctrl_urb);
	}
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		if (priv->bulkrx_urbs[i]) {
			if (priv->bulkrx_urbs[i]->status==-EINPROGRESS) usb_unlink_urb(priv->bulkrx_urbs[i]);
			while (priv->bulkrx_urbs[i]->status==-EINPROGRESS) {
				mdelay(2);
			}
			usb_free_urb(priv->bulkrx_urbs[i]);
		}
	}
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		if (priv->bulktx_urbs[i]) {
			if (priv->bulktx_urbs[i]->status==-EINPROGRESS) usb_unlink_urb(priv->bulktx_urbs[i]);
			while (priv->bulktx_urbs[i]->status==-EINPROGRESS) {
				mdelay(2);
			}
			usb_free_urb(priv->bulktx_urbs[i]);
		}
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
**    usbdev -> Pointer to kernel's usb_device structure
**  endpoint -> Address of the endpoint for control transfers
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
	u32 size;
	int result;
	u16 *csptr;
	char filename[128],*firmware,*usbbuf;
	FN_ENTER;
	usbbuf = kmalloc(ACX100_USB_RWMEM_MAXLEN,GFP_KERNEL);
	if (!usbbuf) {
		printk(KERN_ERR SHORTNAME "not enough memory for allocating USB transfer buffer (req=%d bytes)\n",ACX100_USB_RWMEM_MAXLEN);
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
	outpipe=usb_sndctrlpipe(usbdev,0);
	inpipe =usb_rcvctrlpipe(usbdev,0);
	/* --------------------------------------
	** now upload the firmware, slice the data
	** into blocks
	** -------------------------------------- */
	while (offset<size) {
		if ((size-offset)>=(ACX100_USB_RWMEM_MAXLEN)) len=ACX100_USB_RWMEM_MAXLEN;
		else len=size-offset;
		acxlog(L_INIT,"uploading firmware (%d bytes, offset=%d)\n",len,offset);
		result=0;
		memcpy(usbbuf,firmware+offset,len);
		result=usb_control_msg(usbdev,outpipe,ACX100_USB_UPLOAD_FW,USB_TYPE_VENDOR|USB_DIR_OUT,(u16)(size-8),0,usbbuf,len,3000);
		offset+=len;
		if (result<0) {
#ifdef ACX_DEBUG
			printk(KERN_ERR SHORTNAME "error %d (%s) during upload of firmware, aborting\n",result,acx100usb_pstatus(result));
#else
			printk(KERN_ERR SHORTNAME "error %d during upload of firmware, aborting\n", result);
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
	csptr=(u16 *)firmware;
	result=usb_control_msg(usbdev,outpipe,ACX100_USB_UPLOAD_FW,USB_TYPE_VENDOR|USB_DIR_OUT,csptr[0],csptr[1],NULL,0,3000); /* this triggers the reboot ? */
	if (result<0) {
		printk(KERN_ERR SHORTNAME "error %d during tx of checksum, aborting\n",result);
		kfree(usbbuf);
		vfree(firmware);
		return(result);
	}
	result=usb_control_msg(usbdev,inpipe,ACX100_USB_ACK_CS,USB_TYPE_VENDOR|USB_DIR_IN,csptr[0],csptr[1],usbbuf,8,3000);
	if (result<0) {
		printk(KERN_ERR SHORTNAME "error %d during ACK of checksum, aborting\n",result);
		kfree(usbbuf);
		vfree(firmware);
		return(result);
	}
	vfree(firmware);
	if (((u16 *)usbbuf)[0]!=0x10) {
		kfree(usbbuf);
		printk(KERN_ERR SHORTNAME "invalid checksum?\n");
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

static int init_network_device(struct net_device *dev) {
	int result=0;
	wlandevice_t *priv;
	/* --------------------------------------
	** Setup the device and stop the queue
	** -------------------------------------- */
	ether_setup(dev);
	acx_stop_queue(dev, "on init");
	/* --------------------------------------
	** put the ACX100 out of sleep mode
	** ----------------------------------- */
	priv=dev->priv;
	acx_issue_cmd(priv,ACX1xx_CMD_WAKE,NULL,0,ACX_CMD_TIMEOUT_DEFAULT);
	/* --------------------------------------
	** Register the callbacks for the network
	** device functions.
	** -------------------------------------- */
	dev->open = &acx100usb_open;
	dev->stop = &acx100usb_close;
	dev->hard_start_xmit = (void *)&acx100usb_start_xmit;
	dev->get_stats = (void *)&acx_get_stats;
	dev->get_wireless_stats = (void *)&acx_get_wireless_stats;
#if WIRELESS_EXT >= 13
	dev->wireless_handlers = (struct iw_handler_def *)&acx_ioctl_handler_def;
#else
	dev->do_ioctl = (void *)&acx_ioctl_old;
#endif
	dev->set_multicast_list = (void *)&acx100usb_set_rx_mode;
#ifdef HAVE_TX_TIMEOUT
	dev->tx_timeout = &acx100usb_tx_timeout;
	dev->watchdog_timeo = 4 * HZ;        /* 400 */
#endif
	result=acx_init_mac(dev, 1);
	if (OK == result) {
	  SET_MODULE_OWNER(dev);
	}
	return result;
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
	int i;

	FN_ENTER;

	/* ---------------------------------
	** put the ACX100 out of sleep mode
	** ------------------------------ */
	acx_issue_cmd(priv,ACX1xx_CMD_WAKE,NULL,0,ACX_CMD_TIMEOUT_DEFAULT);

	acx_init_task_scheduler(priv);

	init_timer(&(priv->mgmt_timer));
	priv->mgmt_timer.function=acx_timer;
	priv->mgmt_timer.data=(unsigned long)priv;

	/* set ifup to 1, since acx_start needs it (FIXME: ugly) */

	SET_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	acx_start(priv);


	acx_start_queue(dev, "on open");
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		acx100usb_poll_rx(priv,i);
	}
	/* --- */
	WLAN_MOD_INC_USE_COUNT;
	return 0;
}




/* ---------------------------------------------------------------------------
** acx_rx():
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

void acx_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	netdevice_t *dev = priv->netdev;
	struct sk_buff *skb;

	FN_ENTER;
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) {
		if ((skb = acx_rxdesc_to_ether(priv, rxdesc))) {
			netif_rx(skb);
			dev->last_rx = jiffies;
			priv->stats.rx_packets++;
			priv->stats.rx_bytes += skb->len;
		}
	}
	FN_EXIT0();
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

static void acx100usb_poll_rx(wlandevice_t *priv,int number) {
	acx100_usbin_t *inbuf;
	struct usb_device *usbdev;
	int errcode;
	unsigned int inpipe;

	FN_ENTER;
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) {
		inbuf=&(priv->bulkins[number]);
		usbdev=priv->usbdev;
		rxcons[number].device=priv;
		rxcons[number].number=number;
		inpipe=usb_rcvbulkpipe(usbdev,priv->bulkinep);
	 	if (priv->bulkrx_urbs[number]->status==-EINPROGRESS) {
			printk(KERN_ERR SHORTNAME "error, rx triggered while rx urb in progress\n");
			/* FIXME: this is nasty, receive is being cancelled by this code
			 * on the other hand, this should not happen anyway...
			 */
			usb_unlink_urb(priv->bulkrx_urbs[number]);
		}
		priv->bulkrx_urbs[number]->actual_length=0;
		usb_fill_bulk_urb(priv->bulkrx_urbs[number], usbdev, inpipe, inbuf, rxbufsize, acx100usb_complete_rx, &(rxcons[number]));
		priv->bulkrx_urbs[number]->transfer_flags=ASYNC_UNLINK|QUEUE_BULK;
#ifdef USB_24
		priv->bulkrx_urbs[number]->timeout=0;
		priv->bulkrx_urbs[number]->status=0;
#endif
		errcode=submit_urb(priv->bulkrx_urbs[number], GFP_KERNEL);
		/* FIXME: evaluate the error code ! */
		acxlog(L_USBRXTX,"SUBMIT RX (%d) inpipe=0x%X size=%d errcode=%d\n",number,inpipe,rxbufsize,errcode);
	}
	FN_EXIT0();
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
	int offset,size,number,remsize,packetsize;
	rxbuffer_t *ptr;
	struct rxhostdescriptor *rxdesc;
	TIWLAN_DC *ticontext;
	acx_usb_bulk_context_t *context;

	FN_ENTER;
	if (!urb->context) {
		printk(KERN_ERR SHORTNAME "error, urb context was NULL\n");
		return; /* at least try to prevent the worst */
	}
	context=(acx_usb_bulk_context_t *)(urb->context);
	priv=context->device;
	number=context->number;
	/* ----------------------------
	** grab the TI device context
	** -------------------------- */
	ticontext=&(priv->dc);
	size=urb->actual_length;
	/* ---------------------------------------------
	** check if the transfer was aborted...
	** ------------------------------------------ */
	acxlog(L_USBRXTX,"RETURN RX (%d) status=%d size=%d\n",number,urb->status,size);
	if (urb->status!=0) {
		switch (urb->status) {
			case -ECONNRESET:
				break;
			case -EOVERFLOW:
				printk(KERN_ERR SHORTNAME "error in rx, data overrun -> emergency stop\n");
				acx100usb_close(priv->netdev);
				return;
			default:
				priv->stats.rx_errors++;
				printk(KERN_WARNING SHORTNAME "rx error (urb status=%d)\n",urb->status);
		}
		if (priv->dev_state_mask & ACX_STATE_IFACE_UP) acx100usb_poll_rx(priv,number);
		return;
	}
	/* ---------------------------------------------
	** work the receive buffer...
	** --------------------------------------------- */
	if (size==0) acxlog(L_STD,"warning, encountered zerolength rx packet\n");
	if ((size>0)&&(urb->transfer_buffer==&(priv->bulkins[number]))) {
		/* ------------------------------------------------------------------
		** now fill the data into the rxhostdescriptor for further processing
		** ---------------------------------------------------------------- */
		if (!(ticontext->pRxHostDescQPool)) {
			printk(KERN_ERR SHORTNAME "error, rxhostdescriptor pool is NULL\n");
			return; /* bail out before something bad happens */
		}
		ptr=(rxbuffer_t *)&(priv->bulkins[number]);
		offset=0;
		remsize=size;
		/* -------------------------------------------
		 * check if previous frame was truncated...
		 * FIXME: this code can only handle truncation
		 * into TWO parts, NOT MORE !
		 * ---------------------------------------- */
		if (priv->rxtruncation) {
			ptr=(rxbuffer_t *)&(priv->rxtruncbuf);
			packetsize=MAC_CNT_RCVD(ptr)+ACX100_RXBUF_HDRSIZE;
			rxdesc=&(ticontext->pRxHostDescQPool[ticontext->rx_tail]);
			SET_BIT(rxdesc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
			rxdesc->Status=cpu_to_le32(0xF0000000);	/* set the MSB, FIXME: shouldn't that be MSBit instead??? (BIT31) */
			acxlog(L_USBRXTX,"handling truncated frame (truncsize=%d usbsize=%d packetsize(from trunc)=%d)\n",priv->rxtruncsize,size,packetsize);
#ifdef ACX_DEBUG
			if (debug&L_USBRXTX) acx_dump_bytes(ptr,12);
			if (debug&L_USBRXTX) acx_dump_bytes(&(priv->bulkins[number]),12);
#endif
			if (size<(packetsize-priv->rxtruncsize)) {
				/* -------------------------------------------------
				 * there is not enough data to complete this packet
				 * simply append the stuff to the truncation buffer
				 * ----------------------------------------------- */
				memcpy(((char *)ptr)+priv->rxtruncsize,&(priv->bulkins[number]),size);
				priv->rxtruncsize+=size;
				offset=size;
				remsize=0;
			} else {
				/* ------------------------------------------
				 * ok, this data completes the previously
				 * truncated packet. copy it into a descriptor
				 * and give it to the rest of the stack...
				 * ----------------------------------------- */
				memcpy(rxdesc->data,ptr,priv->rxtruncsize);	/* first copy the previously truncated part */
				ptr=(rxbuffer_t *)&(priv->bulkins[number]);
				memcpy(((char *)(rxdesc->data))+priv->rxtruncsize,(char *)ptr,packetsize-priv->rxtruncsize);
#ifdef ACX_DEBUG			
				acxlog(L_USBRXTX,"full trailing packet + 12 bytes:\n");
				if (debug&L_USBRXTX) acx_dump_bytes(ptr,(packetsize-priv->rxtruncsize)+ACX100_RXBUF_HDRSIZE);
#endif
				priv->rxtruncation=0;
				offset=(packetsize-priv->rxtruncsize);
				ptr=(rxbuffer_t *)(((char *)ptr)+offset);
				remsize-=offset;
				acx_process_rx_desc(priv);
			}
			acxlog(L_USBRXTX,"post-merge offset: %d usbsize: %d remsize=%d\n",offset,size,remsize);
		}
		while (offset<size) {
			packetsize=MAC_CNT_RCVD(ptr)+ACX100_RXBUF_HDRSIZE;
			acxlog(L_USBRXTX,"packet with packetsize=%d\n",packetsize);
			if (packetsize>sizeof(rxbuffer_t)) {
				printk(KERN_ERR "packetsize exceeded (got %d , max %d, usbsize=%d)\n",packetsize,sizeof(rxbuffer_t),size);
				/* FIXME: put some real error-handling in here ! */
			}
			/* --------------------------------
			 * skip zero-length packets...
			 * ----------------------------- */
			if (packetsize==0) {
				offset+=ACX100_RXBUF_HDRSIZE;
				remsize-=ACX100_RXBUF_HDRSIZE;
				acxlog(L_USBRXTX,"packetsize=0, new offs=%d new rem=%d header follows:\n",offset,remsize);
#ifdef ACX_DEBUG
				if (debug&L_USBRXTX) acx_dump_bytes(ptr,12);
#endif
				ptr=(rxbuffer_t *)(((char *)ptr)+ACX100_RXBUF_HDRSIZE);
				continue;
			}
			/* -------------------------------
			 * if packet has no information,
			 * skip it..
			 * ---------------------------- */
			if (remsize<=ACX100_RXBUF_HDRSIZE) {
			}
			if (packetsize>remsize) {
				/* -----------------------------------
				 * frame truncation handling...
				 * --------------------------------- */
				acxlog(L_USBRXTX,"need to truncate this packet , packetsize=%d remain=%d offset=%d usbsize=%d\n",packetsize,remsize,offset,size);
#ifdef ACX_DEBUG				
				if (debug&L_USBRXTX) acx_dump_bytes(ptr,12);
#endif				
				priv->rxtruncation=1;
				memcpy(&(priv->rxtruncbuf),ptr,remsize);
				priv->rxtruncsize=remsize;
				offset=size;
			} else {
				rxdesc=&(ticontext->pRxHostDescQPool[ticontext->rx_tail]);
				SET_BIT(rxdesc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
				rxdesc->Status=cpu_to_le32(0xF0000000);	/* set the MSB, FIXME: shouldn't that be MSBit instead??? (BIT31) */
				memcpy(rxdesc->data,ptr,packetsize);
				/* ---------------------------------------------
				** now handle the received data....
				** ------------------------------------------ */
				acx_process_rx_desc(priv);
				ptr=(rxbuffer_t *)(((char *)ptr)+packetsize);
				offset+=packetsize;
				remsize-=packetsize;
#ifdef ACX_DEBUG
				if ((remsize)&&(debug&L_USBRXTX)) {
					acxlog(L_USBRXTX,"more than one packet in buffer, second packet hdr follows\n");
					if (debug&L_USBRXTX) acx_dump_bytes(ptr,12);
				}
#endif				
			}
		}
	}
	/* -------------------------------
	** look for the next rx ...
	** ---------------------------- */
	if (priv->dev_state_mask & ACX_STATE_IFACE_UP) acx100usb_poll_rx(priv,number); /* receive of frame completed, now look for the next one */
	FN_EXIT0();
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
**  This function is called by acx_dma_tx_data() and is responsible for
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
	FN_ENTER;
	/* ------------------------------------
	** some sanity checks...
	** --------------------------------- */
	if ((!priv)||(!desc)) return;
	/*-----------------------------------------------
	** check if there are free buffers to use...
	** ------------------------------------------- */
	if (!(priv->usb_free_tx)) return;
	/*-----------------------------------------------
	** transmit the frame...
	** ------------------------------------------- */
	acx100usb_prepare_tx(priv,desc);
	FN_EXIT0();
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
	int bufindex,txsize,ucode,size;
	unsigned long flags;
	acx100_usb_txfrm_t *buf;
	const u8 *addr;
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
	header = desc->fixed_size.s.host_desc;
	payload = desc->fixed_size.s.host_desc+1;
	/* ---------------------------------------------
	** look for a free tx buffer.....
	** ------------------------------------------ */
	spin_lock_irqsave(&(priv->usb_tx_lock),flags);
	for (bufindex=0;bufindex<ACX100_USB_NUM_BULK_URBS;bufindex++) {
		if (priv->bulktx_states[bufindex]==0) break;
	}
	if (bufindex>=ACX100_USB_NUM_BULK_URBS) {
		printk(KERN_WARNING SHORTNAME "tx buffers full\n");
		spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
		return;
	}
	priv->bulktx_states[bufindex]=1;
	priv->usb_free_tx--;
	acxlog(L_DEBUG,"using buf #%d (free=%d) len=%d\n",bufindex,priv->usb_free_tx,le16_to_cpu(header->length)+le16_to_cpu(payload->length));
	/* ----------------------------------------------
	** concatenate header and payload into USB buffer
	** ------------------------------------------- */
	acxlog(L_XFER,"tx_data: headerlen=%d  payloadlen=%d\n",le16_to_cpu(header->length),le16_to_cpu(payload->length));
	buf=&(priv->bulkouts[bufindex].txfrm);
	size=le16_to_cpu(header->length) + le16_to_cpu(payload->length);
	if (size>txbufsize) {
		printk(KERN_WARNING SHORTNAME "error, USB buffer smaller than total data to send (%d vs. %d) -> frame dropped\n",size,txbufsize);
		priv->bulktx_states[bufindex]=0;
		priv->usb_free_tx++;
		spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
		return;
	}
	memcpy(&(buf->data),header->data,le16_to_cpu(header->length));
	memcpy(((char *)&(buf->data))+le16_to_cpu(header->length),payload->data,le16_to_cpu(payload->length));
	/* ----------------------------------------------
	** fill the USB transfer header
	** ------------------------------------------- */
	buf->hdr.desc=cpu_to_le16(ACX100_USB_TX_DESC);
	buf->hdr.MPDUlen=cpu_to_le16(size);
	buf->hdr.ctrl1=0;
	buf->hdr.ctrl2=0;
	buf->hdr.hostData=cpu_to_le32(size|(desc->u.r1.rate)<<24);
	if (1 == priv->defpeer.shortpre) /* vda: TODO: when to use ap_peer? */
		SET_BIT(buf->hdr.ctrl1, DESC_CTL_SHORT_PREAMBLE);
	SET_BIT(buf->hdr.ctrl1, DESC_CTL_FIRSTFRAG);
	buf->hdr.txRate=desc->u.r1.rate;
	buf->hdr.index=1;
	buf->hdr.dataLength=cpu_to_le16(size|((buf->hdr.txRate)<<24));
	if (WLAN_GET_FC_FTYPE(ieee2host16(((p80211_hdr_t *)header->data)->a3.fc))==WLAN_FTYPE_DATA) {
		SET_BIT(buf->hdr.hostData, (ACX100_USB_TXHI_ISDATA<<16));
	}
	addr=(((p80211_hdr_t *)(header->data))->a3.a3);
	if (OK == acx_is_mac_address_directed((mac_t *)addr)) 
	    SET_BIT(buf->hdr.hostData, cpu_to_le32((ACX100_USB_TXHI_DIRECTED<<16)));
	if (OK == acx_is_mac_address_broadcast(addr)) 
	    SET_BIT(buf->hdr.hostData, cpu_to_le32((ACX100_USB_TXHI_BROADCAST<<16)));
	acxlog(L_DATA,"Dump of bulk out urb:\n");
	if (debug&L_DATA) acx_dump_bytes(buf,size+sizeof(acx100_usb_txhdr_t));


	if (priv->bulktx_urbs[bufindex]->status==-EINPROGRESS) {
		printk(KERN_WARNING SHORTNAME "trying to subma a tx urb while already in progress\n");
	}
	
	priv->usb_txsize=size+sizeof(acx100_usb_txhdr_t);
	priv->usb_txoffset=priv->usb_txsize;
	txsize=priv->usb_txsize;
	/* ---------------------------------------------
	** now schedule the USB transfer...
	** ------------------------------------------ */
	usbdev=priv->usbdev;
	outpipe=usb_sndbulkpipe(usbdev,priv->bulkoutep); 
	txcons[bufindex].device=priv;
	txcons[bufindex].number=bufindex;
	usb_fill_bulk_urb(priv->bulktx_urbs[bufindex],usbdev,outpipe,buf,txsize,(usb_complete_t)acx100usb_complete_tx,&(txcons[bufindex]));
	priv->bulktx_urbs[bufindex]->transfer_flags=ASYNC_UNLINK|QUEUE_BULK|ZERO_PACKET;
#ifdef USB_24	
	priv->bulktx_urbs[bufindex]->status=0;
	priv->bulktx_urbs[bufindex]->timeout=ACX100_USB_TX_TIMEOUT;
#endif	
	ucode=submit_urb(priv->bulktx_urbs[bufindex], GFP_KERNEL);
	acxlog(L_USBRXTX,"SUBMIT TX (%d): outpipe=0x%X buf=%p txsize=%d errcode=%d\n",bufindex,outpipe,buf,txsize,ucode);
	if (ucode!=0) {
		printk(KERN_ERR SHORTNAME "submit_urb() return code: %d (%s:%d) txsize=%d\n",ucode,__FILE__,__LINE__,txsize);
		/* -------------------------------------------------
		** on error, just mark the frame as done and update
		** the statistics...
		** ---------------------------------------------- */
		priv->stats.tx_errors++;
		priv->bulktx_states[bufindex]=0;
		priv->usb_free_tx++;
		spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
		acx100usb_trigger_next_tx(priv);
		return;
	}
	spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
	FN_EXIT0();
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
	acx_usb_bulk_context_t *context;
	int index;
	unsigned long flags;

	FN_ENTER;
	
	context=(acx_usb_bulk_context_t *)(urb->context);
	if (!context) {
		printk(KERN_ERR SHORTNAME "error, enountered NULL context in tx completion callback\n");
		/* FIXME: real error-handling code must go here ! */
		return;
	}
	priv=context->device;
	index=context->number;
	acxlog(L_USBRXTX,"RETURN TX (%d): status=%d size=%d\n",index,urb->status,urb->actual_length);
	/* ------------------------------
	** handle USB transfer errors...
	** --------------------------- */
	if (urb->status!=0) {
		switch (urb->status) {
			case -ECONNRESET:
				break;
			default:
				printk(KERN_ERR SHORTNAME "tx error, urb status=%d\n",urb->status);
		}
		/* FIXME: real error-handling code here please */
	}
	/* ---------------------------------------------
	** free the URB and check for more data...
	** --------------------------------------------- */
	spin_lock_irqsave(&(priv->usb_tx_lock),flags);
	priv->usb_free_tx++;
	priv->bulktx_states[index]=0;
	spin_unlock_irqrestore(&(priv->usb_tx_lock),flags);
	if (priv->dev_state_mask&ACX_STATE_IFACE_UP) acx100usb_trigger_next_tx(priv);
	FN_EXIT0();
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
	struct TIWLAN_DC *ticontext;
	/* ----------------------------
	** grab the TI device context
	** -------------------------- */
	ticontext=&(priv->dc);
	/* ----------------------------------------------
	** free the txdescriptor...
	** ------------------------------------------- */
	txdesc=priv->currentdesc;
	header = txdesc->fixed_size.s.host_desc;
	payload = txdesc->fixed_size.s.host_desc+1;
	SET_BIT(header->Ctl_16, cpu_to_le16(DESC_CTL_DONE));
	SET_BIT(payload->Ctl_16, cpu_to_le16(DESC_CTL_DONE));
	SET_BIT(txdesc->Ctl_8, DESC_CTL_DONE);
	acx_clean_tx_desc(priv);
	/* ----------------------------------------------
	** check if there are still descriptors that have
	** to be sent. acx_clean_tx_desc() should
	** have set the next non-free descriptor position
	** in tx_tail...
	** ------------------------------------------- */
	descnum=ticontext->tx_tail;
	txdesc=&(ticontext->pTxDescQPool[descnum]);
	if (!(txdesc->Ctl_8 & DESC_CTL_HOSTOWN)) {
		acx100usb_prepare_tx(priv,txdesc);
	} else {
		/* ----------------------------------------------
		** now wake the output queue...
		** ------------------------------------------- */
		if (priv->dev_state_mask&ACX_STATE_IFACE_UP) acx_wake_queue(priv->netdev, "for next Tx");
	}
}


/* ---------------------------------------------------------------------------
** acx100usb_close():
** Inputs:
**    dev -> Pointer to network device structure
** ---------------------------------------------------------------------------
** Returns:
**  (int) 0 on success, or error-code 
**
** Description:
**  This function stops the network functionality of the interface (invoked
**  when the user calls ifconfig <wlan> down). The tx queue is halted and
**  the device is marked as down. In case there were any pending USB bulk
**  transfers, these are unlinked (asynchronously). The module in-use count
**  is also decreased in this function.
** ------------------------------------------------------------------------- */

static int acx100usb_close(struct net_device *dev)
{
	wlandevice_t *priv;
	client_t client;
	int i,already_down;
	if (!(dev->priv)) {
		printk(KERN_ERR SHORTNAME "dev->priv empty, FAILED.\n");
		return -ENODEV;
	}
	priv=dev->priv;
	FN_ENTER;
	/* ------------------------------
	** Transmit a disassociate frame
	** --------------------------- */
	acx_transmit_disassoc(&client,priv);
	/* --------------------------------
	* stop the transmit queue...
	* ------------------------------ */
	if (!acx_queue_stopped(dev)) {
		acx_stop_queue(dev, "on iface stop");
	}
	acx_flush_task_scheduler();
	/* --------------------------------
	** mark the device as DOWN
	** ----------------------------- */
	if (priv->dev_state_mask&ACX_STATE_IFACE_UP) {
		already_down=0;
		CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	} else
		already_down=1;
	/* --------------------------------------
	 * wait until all tx are out...
	 * ----------------------------------- */
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		while (priv->bulktx_urbs[i]->status==-EINPROGRESS) {
			mdelay(5);
		}
	}
	/* ----------------------------------------
	* interrupt pending bulk rx transfers ....
	* ------------------------------------- */
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		if (priv->bulkrx_urbs[i]->status==-EINPROGRESS) usb_unlink_urb(priv->bulkrx_urbs[i]);
		while (priv->bulkrx_urbs[i]->status==-EINPROGRESS) { 
			mdelay(5);
		}
	}
	/* ----------------------------------------------------
	 * stop any pending tx URBs...
	 * ------------------------------------------------- */
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {
		if (priv->bulktx_urbs[i]->status==-EINPROGRESS) usb_unlink_urb(priv->bulktx_urbs[i]);
		while (priv->bulktx_urbs[i]->status==-EINPROGRESS) { 
			mdelay(20);
		}
	}
	/* ------------------------
	** disable rx and tx ...
	** --------------------- */
	acx_issue_cmd(priv,ACX1xx_CMD_DISABLE_TX,NULL,0,ACX_CMD_TIMEOUT_DEFAULT);
	acx_issue_cmd(priv,ACX1xx_CMD_DISABLE_RX,NULL,0,ACX_CMD_TIMEOUT_DEFAULT);
	/* -------------------------
	** power down the device...
	** ---------------------- */
	acx_issue_cmd(priv,ACX1xx_CMD_SLEEP,NULL,0,ACX_CMD_TIMEOUT_DEFAULT);
	/* --------------------------------------------
	** decrease module-in-use count (if necessary)
	** ----------------------------------------- */
	if (!already_down) WLAN_MOD_DEC_USE_COUNT;
	FN_EXIT1(0);
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
	if (acx_lock(priv, &flags))
		return 1;
	/* --------------------------------------
	** if the queue is halted, there is no point
	** in sending out data...
	** ----------------------------------- */
	if (acx_queue_stopped(dev)) {
		acxlog(L_STD, "%s: called when queue stopped\n", __func__);
		txresult = 1;
		goto end;
	}
	/* --------------------------------------
	** there is no one to talk to...
	** ----------------------------------- */
	if (priv->status != ISTATUS_4_ASSOCIATED) {
		printk(KERN_INFO SHORTNAME "trying to xmit, but not associated yet: aborting.\n");
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
	acx_stop_queue(dev, "during Tx");
#endif
#if UNUSED
	if (acx_lock(priv,&flags)) ...

	memset(pb, 0, sizeof(wlan_pb_t) /*0x14*4 */ );

	pb->ethhostbuf = skb;
	pb->ethbuf = skb->data;
#endif
	templen = skb->len;
	/* ------------------------------------
	** get the next free tx descriptor...
	** -------------------------------- */
	if ((tx_desc = acx_get_tx_desc(priv)) == NULL) {
		acxlog(L_BINSTD,"BUG: txdesc ring full\n");
		txresult = 1;
		goto end;
	}
	/* ------------------------------------
	** convert the ethernet frame into our
	** own tx descriptor type...
	** -------------------------------- */
	acx_ether_to_txdesc(priv,tx_desc,skb);
	/* -------------------------
	** free the skb
	** ---------------------- */
	dev_kfree_skb(skb);
	dev->trans_start = jiffies;
	/* -----------------------------------
	** until the packages are flushed out,
	** stop the queue...
	** -------------------------------- */
	acx_stop_queue(dev, "after Tx");
	/* -----------------------------------
	** transmit the data...
	** -------------------------------- */
	acx_dma_tx_data(priv, tx_desc);  /* this function finally calls acx100usb_tx_data() */
	txresult=0;
	/* -----------------------------------
	** statistical bookkeeping...
	** -------------------------------- */
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += templen;
end:
	acx_unlock(priv, &flags);
	FN_EXIT1(txresult);
	return txresult;
}



static struct net_device_stats *acx_get_stats(netdevice_t *dev) {
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	FN_ENTER;
	FN_EXIT1((int)&priv->stats);
	return &priv->stats;
}



static struct iw_statistics *acx_get_wireless_stats(netdevice_t *dev) {
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	FN_ENTER;
	FN_EXIT1((int)&priv->stats);
	return &priv->wstats;
}



static void acx100usb_set_rx_mode(struct net_device *dev)
{
}



#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *dev) {
	wlandevice_t *priv;
	int i;
	FN_ENTER;
	priv=dev->priv;
	/* ------------------------------------
	** unlink the URBs....
	** --------------------------------- */
	for (i=0;i<ACX100_USB_NUM_BULK_URBS;i++) {	
		if (priv->bulktx_urbs[i]->status==-EINPROGRESS) usb_unlink_urb(priv->bulktx_urbs[i]);
	}
	/* ------------------------------------
	** TODO: stats update
	** --------------------------------- */
	FN_EXIT0();
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
  /* halted removed in 2.6.9-rc1 */
  printk(KERN_INFO "  halted[0]: 0x%X  halted[1]: 0x%X\n",usbdev->halted[0],usbdev->halted[1]);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
  /* This saw a change after 2.6.10 */
  printk(KERN_INFO "  ep_in wMaxPacketSize: ");
  for(i = 0; i < 16; ++i) printk("%d ", usbdev->ep_in[i]->desc.wMaxPacketSize);
  printk("\n");
  printk(KERN_INFO "  ep_out wMaxPacketSize: ");
  for(i = 0; i < 15; ++i) printk("%d ", usbdev->ep_out[i]->desc.wMaxPacketSize);
  printk("\n");
#else
  printk(KERN_INFO "  epmaxpacketin: ");
  for (i=0;i<16;i++) printk("%d ",usbdev->epmaxpacketin[i]);
  printk("\n");
  printk(KERN_INFO "  epmaxpacketout: ");
  for (i=0;i<16;i++) printk("%d ",usbdev->epmaxpacketout[i]);
  printk("\n");
#endif
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
  /* printk(KERN_INFO "  MaxPower: %d (0x%X)\n",cd->bMaxPower,cd->bMaxPower); */
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

#endif
