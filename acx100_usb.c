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
 *  Martin Wawro <wawro@ls7.cs.uni-dortmund.de>
 *  Andreas Mohr <andi AT lisas.de>
 *
 * Issues:
 *  - Currently this driver is only able to boot-up the USB device
 *    and receive data,
 *    full functionality is not available yet, since transmitting hasn't
 *    been implemented correctly yet.
 *
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

/* try to make it compile for both 2.4.x and 2.6.x kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

/* number of endpoints of an interface */
#define NUM_EP(intf) (intf)->altsetting[0].desc.bNumEndpoints
#define EP(intf,nr) (intf)->altsetting[0].endpoint[(nr)].desc
#define GET_DEV(udev) usb_get_dev((udev))
#define PUT_DEV(udev) usb_put_dev((udev))
#define SET_NETDEV_OWNER(ndev,owner) /* not needed anymore ??? */

static inline int submit_urb(struct urb *urb, int mem_flags) {
        return usb_submit_urb(urb, mem_flags);
}
static inline struct urb *alloc_urb(int iso_pk, int mem_flags) {
        return usb_alloc_urb(iso_pk, mem_flags);
}

#else

/* 2.4.x kernels */

#define NUM_EP(intf) (intf)->altsetting[0].bNumEndpoints
#define EP(intf,nr) (intf)->altsetting[0].endpoint[(nr)]

#define GET_DEV(udev) usb_inc_dev_use((udev))
#define PUT_DEV(udev) usb_dec_dev_use((udev))

#define SET_NETDEV_DEV(x,y)
#define SET_NETDEV_OWNER(ndev,owner) ndev->owner = owner

static inline int submit_urb(struct urb *urb, int mem_flags) {
        return usb_submit_urb(urb);
}
static inline struct urb *alloc_urb(int iso_pk, int mem_flags) {
        return usb_alloc_urb(iso_pk);
}

static inline void usb_set_intfdata(struct usb_interface *intf, void *data) {}

#else
#define USB_24	1
#endif //#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

/* -------------------------------------------------------------------------
**                             Module Stuff
** ---------------------------------------------------------------------- */

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
MODULE_AUTHOR("Martin Wawro <wawro@ls7.cs.uni-dortmund.de>");
MODULE_DESCRIPTION("TI ACX100 WLAN USB Driver");

#ifdef ACX_DEBUG
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level mask: 0x0000 - 0x3fff");
#endif

MODULE_PARM(firmware_dir, "s");
MODULE_PARM_DESC(firmware_dir, "Directory where to load acx100 firmware file from");


/* -------------------------------------------------------------------------
**                Module Definitions (preprocessor based)
** ---------------------------------------------------------------------- */


#define SHORTNAME "acx100usb"

#define ACX100_VENDOR_ID 0x2001
#define ACX100_PRODUCT_ID_UNBOOTED 0x3B01
#define ACX100_PRODUCT_ID_BOOTED 0x3B00


/* -------------------------------------------------------------------------
**                        Module Data Structures
** ---------------------------------------------------------------------- */




/* -------------------------------------------------------------------------
**                          Module Prototypes
** ---------------------------------------------------------------------- */

static void * acx100usb_probe(struct usb_device *,unsigned int,const struct usb_device_id *);
static void acx100usb_disconnect(struct usb_device *,void *);
static int acx100usb_open(struct net_device *);
static int acx100usb_stop(struct net_device *);
static int acx100usb_start_xmit(struct net_device *);
static struct net_device_stats * acx100usb_get_stats(struct net_device *);
static void acx100usb_get_wireless_stats(struct net_device *);
static void acx100usb_set_rx_mode(struct net_device *);
static void init_network_device(struct net_device *);
static void init_usb_device(struct usb_device *) __attribute__((__unused__));
/* static void acx100usb_printsetup(devrequest *); */
static void acx100usb_complete(struct urb *) __attribute__((__unused__));
static void * acx100usb_read_firmware(const char *,unsigned int *);
static int acx100usb_boot(struct usb_device *);
static void acx100_usb_complete_rx(struct urb *);

static void acx100_poll_rx(wlandevice_t *);
void acx100_rx(struct rxhostdescriptor *,wlandevice_t *);

static void acx100_usb_complete_send(struct urb *);

int init_module(void);
void cleanup_module(void);

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *);
#endif

#ifdef ACX_DEBUG
/* int debug = L_BIN|L_ASSOC|L_INIT|L_STD; */
int debug = L_ALL;
int acx100_debug_func_indent=0;
static char * acx100usb_pstatus(int);
static void acx100usb_dump_bytes(void *,int) __attribute__((__unused__));
static void dump_interface_descriptor(struct usb_interface_descriptor *);
static void dump_device(struct usb_device *);
static void dump_device_descriptor(struct usb_device_descriptor *);
static void dump_endpoint_descriptor(struct usb_endpoint_descriptor *);
static void dump_config_descriptor(struct usb_config_descriptor *);
/* static void acx100usb_printsetup(devrequest *); */
static void acx100usb_printcmdreq(struct acx100_usb_cmdreq *) __attribute__((__unused__));
#endif

/* -------------------------------------------------------------------------
**                             Module Data
** ---------------------------------------------------------------------- */

char *firmware_dir;

extern const struct iw_handler_def acx100_ioctl_handler_def;

static struct usb_device_id acx100usb_ids[] = {
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_BOOTED) },
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_UNBOOTED) },
   { }
};


/* USB driver data structure as required by the kernel's USB core */

static struct usb_driver acx100usb_driver = {
  .name = "acx100_usb",                    /* name of the driver */
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
*/
/* FIXME: error cleanup paths BROKEN! */
static void * acx100usb_probe(struct usb_device *usbdev,unsigned int ifNum,const struct usb_device_id *devID)
{
  wlandevice_t *priv;
  struct net_device *dev=NULL;
  int numconfigs,numfaces,result;
  struct usb_config_descriptor *config;
  /* ---------------------------------------------
  ** first check if this is the "unbooted" hardware
  ** --------------------------------------------- */
  if ((usbdev->descriptor.idVendor==ACX100_VENDOR_ID)&&(usbdev->descriptor.idProduct==ACX100_PRODUCT_ID_UNBOOTED)&&(ifNum==0)) {
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
    return(NULL);
  }
  if ((usbdev->descriptor.idVendor==ACX100_VENDOR_ID)&&(usbdev->descriptor.idProduct==ACX100_PRODUCT_ID_BOOTED)&&(ifNum==0)) {
    /* ---------------------------------------------
    ** allocate memory for the device driver context
    ** --------------------------------------------- */
    priv = (struct wlandevice *)kmalloc(sizeof(struct wlandevice),GFP_KERNEL);
    if (!priv) {
      printk(KERN_WARNING SHORTNAME ": could not allocate %d bytes memory for device driver context, giving up.\n",sizeof(struct wlandevice));
      return(NULL);
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
    config = usbdev->actconfig;
    numfaces=config->bNumInterfaces;
    if (numfaces!=1) printk(KERN_WARNING SHORTNAME ": number of interfaces is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numfaces);
#ifdef ACX_DEBUG
    dump_device(usbdev);
#endif
    /* ---------------------------------------------
    ** Allocate memory for a network device
    ** --------------------------------------------- */
    if ((dev = (struct net_device *)kmalloc(sizeof(struct net_device),GFP_ATOMIC))==NULL) {
      printk(KERN_WARNING SHORTNAME ": failed to alloc netdev\n");
      kfree(priv);
      return(NULL);
    }
    memset(dev, 0, sizeof(struct net_device));
    dev->init=(void *)&init_network_device;
    dev->priv=priv;
    priv->netdev=dev;

    priv->urb=alloc_urb(0, GFP_KERNEL);
    if (!priv->urb) {
      printk(KERN_WARNING SHORTNAME ": failed to allocate URB\n");
      kfree(dev);
      kfree(priv);
      return(NULL);
    }

    priv->bulkrx_urb=alloc_urb(0, GFP_KERNEL);
    if (!priv->bulkrx_urb) {
      printk(KERN_WARNING SHORTNAME ": failed to allocate input URB\n");
      usb_free_urb(priv->urb);
      kfree(dev);
      kfree(priv);
      return(NULL);
    }
    priv->bulkrx_urb->status=0;

    priv->bulktx_urb=alloc_urb(0, GFP_KERNEL);
    if (!priv->bulktx_urb) {
      printk(KERN_WARNING SHORTNAME ": failed to allocate output URB\n");
      usb_free_urb(priv->urb);
      usb_free_urb(priv->bulkrx_urb);
      kfree(dev);
      kfree(priv);
      return(NULL);
    }
    priv->bulktx_urb->status=0;

    /* --------------------------------------
    ** Allocate a network device name
    ** -------------------------------------- */
    acxlog(L_DEBUG,"allocating device name\n");
    result=dev_alloc_name(dev,"wlan%d");
    if (result<0) {
      printk(KERN_ERR SHORTNAME ": failed to allocate wlan device name (errcode=%d), giving up.\n",result);
      kfree(dev);
      usb_free_urb(priv->urb);
      usb_free_urb(priv->bulkrx_urb);
      usb_free_urb(priv->bulktx_urb);
      kfree(priv);
      return(NULL);
    }
    /* --------------------------------------
    ** Register the network device
    ** -------------------------------------- */
    acxlog(L_DEBUG,"registering network device\n");
    result=register_netdev(dev);
    if (result!=0) {
      printk(KERN_ERR SHORTNAME ": failed to register network device for USB WLAN (errcode=%d), giving up.\n",result);
      kfree(dev);
      usb_free_urb(priv->urb);
      kfree(priv);
      return(NULL);
    }
    /* --------------------------------------
    ** Everything went OK, we are happy now
    ** -------------------------------------- */
    return(priv);
  }
  /* --------------------------------------
  ** no device we could handle, return NULL
  ** -------------------------------------- */
  return(NULL);
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
**  to be freed. Note that there is currently no handling of the user
**  removing a device when the network is still up -> TODO
*/

static void acx100usb_disconnect(struct usb_device *dev,void *devContext)
{
  int result;
  wlandevice_t *priv = (wlandevice_t *)devContext;
  /* --------------------------------------
  ** Unregister the network devices (ouch!)
  ** -------------------------------------- */
	if (priv->netdev) {
		result=unregister_netdevice(priv->netdev);
		acxlog(L_DEBUG,"unregister netdevice returned %d\n",result);
		kfree(priv->netdev);
	}
  if (priv->urb) usb_free_urb(priv->urb);
  if (priv->bulkrx_urb) usb_free_urb(priv->bulkrx_urb);
  /* --------------------------------------
  ** finally free the context...
  ** -------------------------------------- */
  if (priv) kfree(priv);
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
*/

static int acx100usb_boot(struct usb_device *usbdev)
{
  unsigned int offset=8,size,len,inpipe,outpipe;
  int result;
  UINT16 *csptr;
  //struct usb_device *usbdev;
  char filename[128],*firmware,*usbbuf;
  acxlog(L_DEBUG,"booting acx100 USB device...\n");
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
  if (firmware_dir) sprintf(filename,"%s/ACX100.bin",firmware_dir);
  else sprintf(filename,"/etc/acx100/ACX100.bin");
  acxlog(L_STD,"loading firmware %s\n",filename);
  firmware=acx100usb_read_firmware(filename,&size);
  if (!firmware) {
    kfree(usbbuf);
    return(-EIO);
  }
  acxlog(L_DEBUG,"firmware size: %d bytes\n",size);
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
    acxlog(L_DEBUG,"uploading firmware (%d bytes, offset=%d size=%d)\n",len,offset,size);
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
** acx100usb_read_firmware():
** Inputs:
**    filename -> Filename of the firmware file
**        size -> Pointer to an integer to take the size of the file
** ---------------------------------------------------------------------------
** Returns:
**  (void *) Pointer to memory with firmware or NULL on failure
**
** Description:
**  currently missing
*/

static void * acx100usb_read_firmware(const char *filename,unsigned int *size)
{
  char *res = NULL;
  mm_segment_t orgfs;
  unsigned long page;
  char *buffer;
  struct file *inf;
  int retval;
  unsigned int offset = 0;

  orgfs=get_fs(); /* store original fs */
  set_fs(KERNEL_DS);
  /* Read in whole file then check the size */
  page=get_free_page(GFP_KERNEL);
  if (page) {
    buffer=(char*)page;
    inf=(struct file *)filp_open(filename,O_RDONLY,0);
    if (IS_ERR(inf)) {
      printk(KERN_ERR "ERROR %ld trying to open firmware image file '%s'.\n", -PTR_ERR(inf),filename);
    } else {
      if (inf->f_op&&inf->f_op->read) {
        offset = 0;
        do {
          retval=inf->f_op->read(inf,buffer,PAGE_SIZE,&inf->f_pos);
          if (retval < 0) {
            printk(KERN_ERR "ERROR %d reading firmware image file '%s'.\n", -retval, filename);
            if (res) vfree(res);
            res = NULL;
          }
          if (retval > 0) {
            if (!res) {
              res = vmalloc(8+*(UINT32*)(4+buffer));
              acxlog(L_DEBUG,"Allocated %ld bytes for firmware module loading.\n", 8+(*(UINT32*)(4+buffer)));
              *size=8+(*(UINT32*)(buffer+4));
            }
            if (!res) {
              printk(KERN_ERR "Unable to allocate memory for firmware module loading.\n");
              retval=0;
            }
            if (res) {
              memcpy((UINT8*)res+offset, buffer,retval);
              offset += retval;
            }
          }
        } while (retval>0);
      } else {
        printk(KERN_ERR "ERROR: %s does not have a read method\n", filename);
      }
      retval=filp_close(inf,NULL);
      if (retval) printk(KERN_ERR "ERROR %d closing %s\n", -retval, filename);
      if ((res) && ((*size) != offset)) {
        printk(KERN_INFO "Firmware is reporting a different size 0x%08x to read 0x%08x\n", (int)(*size), offset);
        /*
        vfree(res);
        res = NULL;
        */
      }
    }
    free_page(page);
  } else {
    printk(KERN_ERR "Unable to allocate memory for firmware loading.\n");
  }
  set_fs(orgfs);
  return(res);
}


static void init_usb_device(struct usb_device *dev)
{
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
**  Basic setup of a network device for use with the WLAN device
** ------------------------------------------------------------------------- */

static void init_network_device(struct net_device *dev)
{
  int result=0;
  /* --------------------------------------
  ** Setup the device and stop the queue
  ** -------------------------------------- */
  ether_setup(dev);
  netif_stop_queue(dev);
  /* --------------------------------------
  ** Register the callbacks for the network
  ** device functions.
  ** -------------------------------------- */
  dev->open = &acx100usb_open;
  dev->stop = &acx100usb_stop;
  dev->hard_start_xmit = (void *)&acx100usb_start_xmit;
#if 0
  dev->get_stats = (void *)&acx100usb_get_stats;
  dev->get_wireless_stats = (void *)&acx100usb_get_wireless_stats;
#else
  dev->get_stats = NULL;
  dev->get_wireless_stats = NULL;
#endif
#if WIRELESS_EXT >= 13
        dev->wireless_handlers = (struct iw_handler_def *)&acx100_ioctl_handler_def;
#endif
  dev->do_ioctl = (void *)&acx100_ioctl_main;
  dev->set_multicast_list = (void *)&acx100usb_set_rx_mode;
#ifdef HAVE_TX_TIMEOUT
  dev->tx_timeout = &acx100usb_tx_timeout;
  dev->watchdog_timeo = 4 * HZ;        /* 400 */
#endif
  result=acx100_init_mac(dev);
  if (!result) {
    SET_MODULE_OWNER(dev);
  }
  //return(result);
}



/* ---------------------------------------------------------------------------
** acx100usb_open():
** Inputs:
**    dev -> Pointer to network device
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**
*/

static int acx100usb_open(struct net_device *dev)
{
  wlandevice_t *priv = (wlandevice_t *)dev->priv;
  unsigned long flags;

  acx100_lock(priv, &flags);

  init_timer(&(priv->mgmt_timer));
  priv->mgmt_timer.function=acx100_timer;
  priv->mgmt_timer.data=(unsigned long)priv;

  /* set ifup to 1, since acx100_start needs it (FIXME: ugly) */

  priv->dev_state_mask |= ACX_STATE_IFACE_UP;
  acx100_unlock(priv,&flags);
  acx100_start(priv);
  /* --- */
  //dev->state |= (1<<WLAN_STATE_INUSE_BIT);
  WLAN_MOD_INC_USE_COUNT;
  return(0);
}




/*----------------------------------------------------------------
* acx100_rx():
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

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

static void acx100_poll_rx(wlandevice_t *priv)
{
	acx100_usbin_t *inbuf;
	struct usb_device *usbdev;
	unsigned int inpipe;

	FN_ENTER;
	inbuf=&(priv->bulkin);
	usbdev=priv->usbdev;

	inpipe=usb_rcvbulkpipe(usbdev,1);      /* default endpoint for bulk-transfers: 1 */

 	if (priv->bulkrx_urb->status==-EINPROGRESS) {
		usb_unlink_urb(priv->bulkrx_urb);
	}
	usb_fill_bulk_urb(priv->bulkrx_urb,usbdev,inpipe,inbuf,sizeof(acx100_usbin_t),acx100_usb_complete_rx,priv);
	priv->bulkrx_urb->transfer_flags=USB_ASYNC_UNLINK;
	priv->bulkrx_urb->timeout=5*HZ;
	submit_urb(priv->bulkrx_urb, GFP_ATOMIC);
	FN_EXIT(0,0);
}


static void acx100_usb_complete_rx(struct urb *urb)
{
	wlandevice_t *priv;
	int size;
	char *ptr;
	struct rxhostdescriptor *rxdesc;
	TIWLAN_DC *ticontext;

	FN_ENTER;
	priv=(wlandevice_t *)urb->context;
	/* ----------------------------
	 * grab the TI device context
	 * -------------------------- */
	ticontext=&(priv->dc);
	/* ---------------------------------------------
	 * check if the receive buffer is the right one
	 * --------------------------------------------- */
	if (urb->transfer_buffer==&(priv->bulkin)) {
		/* ------------------------------------------------------------------
		 * now fill the data into the rxhostdescriptor for further processing
		 * ---------------------------------------------------------------- */
		if (!(ticontext->pRxHostDescQPool)) {
			acxlog(L_STD,"acx100usb: ERROR rxhostdescriptor pool is NULL\n");
			return; /* bail out before something bad happens */
		}
		rxdesc=&(ticontext->pRxHostDescQPool[ticontext->rx_tail]);
		acxlog(L_DEBUG,"bulk recv completed (len=%d, rxdesc=%p) rx_pool_count=%d rx_tail=%d\n",urb->actual_length,rxdesc,ticontext->rx_pool_count,ticontext->rx_tail);
		ptr=(char *)&(priv->bulkin);
		rxdesc->Ctl|=ACX100_CTL_OWN;
		rxdesc->val0x14=0xF0000000;	// set the MSB
		size=urb->actual_length;
		if (size>sizeof(struct rxbuffer)) {
			acxlog(L_STD,"acx100usb: ERROR, # of received bytes (%d) higher than capacity of buffer (%d bytes)\n",size,sizeof(struct rxbuffer));
			/* -------------------------
			 * prevent buffer overflow
			 * -----------------------*/
			size=sizeof(struct rxbuffer);
		}
		memcpy(rxdesc->data,ptr,size);
#ifdef ACX_DEBUG
		if (debug&L_DATA) {
			if ((urb->actual_length>0)&&(urb->actual_length<1024)) {
				acx100usb_dump_bytes(ptr,urb->actual_length);
			}
		}
#endif
		/* ---------------------------------------------
		* now handle the received data....
		* --------------------------------------------- */
		if (size>0) {	/* >0 ain't enough */
			acx100_process_rx_desc(priv);
		}
	}
	FN_EXIT(0,0);
}


void acx100_dma_tx_data(wlandevice_t *priv, struct txdescriptor *tx_desc)
{
	acx100_usbout_t *outbuf;
	struct usb_device *usbdev;
	unsigned int outpipe;
	int res;
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;

	FN_ENTER;
	/* header and payload are located in adjacent descriptors */
	header = tx_desc->host_desc;
	payload = tx_desc->host_desc + 1;

	tx_desc->tx_time = jiffies;

	tx_desc->Ctl |= priv->preamble_flag;

	/* set autodma and reclaim and 1st mpdu */
	tx_desc->Ctl |= ACX100_CTL_AUTODMA | ACX100_CTL_RECLAIM | ACX100_CTL_FIRSTFRAG;

	if (tx_desc->total_length > priv->rts_threshold)
		tx_desc->Ctl2 |= DESC_CTL2_RTS;

	if (WLAN_GET_FC_FTYPE(((p80211_hdr_t*)header->data)->a3.fc) == WLAN_FTYPE_MGMT) {
		tx_desc->rate = 20;     /* 2Mbps for MGMT pkt compatibility */
	} else {
		tx_desc->rate = priv->bitrateval;
	}
        acxlog(L_XFER | L_DATA,
                "Tx pkt (%s): len %i, hdr_len %i, pyld_len %i, mode %d, status %d\n",
                acx100_get_packet_type_string(((p80211_hdr_t*)header->data)->a3.fc),
                tx_desc->total_length,
                header->length,
                payload->length,
                priv->mode,
                priv->status);

	/* sets Ctl DESC_CTL_FREE to zero telling that the descriptors are now owned by the acx100 */
	header->Ctl &= (UINT16) ~ACX100_CTL_OWN;
	payload->Ctl &= (UINT16) ~ACX100_CTL_OWN;
	tx_desc->Ctl &= (UINT16) ~ACX100_CTL_OWN;

	/* FIXME: how to trigger USB Tx?? */
	/* acx100_write_reg16(priv, priv->io[IO_ACX_INT_TRIG], 0x4); */

	usbdev = priv->usbdev;
	outbuf = &(priv->bulkout);
	outpipe = usb_sndbulkpipe(usbdev, 2);      /* FIXME: hopefully endpoint 2 is correct for ACX100 */
	usb_fill_bulk_urb(priv->bulktx_urb, usbdev, outpipe, outbuf, sizeof(acx100_usbout_t), acx100_usb_complete_send, priv);
	priv->bulktx_urb->transfer_flags |= USB_QUEUE_BULK;
	if ((res = submit_urb(priv->bulktx_urb, GFP_ATOMIC))) {
		acxlog(L_STD, "failed Tx urb %d\n", res);
		priv->stats.tx_errors++;
	} else {
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += tx_desc->total_length; /* FIXME: correct? */
		priv->netdev->trans_start = jiffies;
	}
	FN_EXIT(0, 0);
}

static void acx100_usb_complete_send(struct urb *urb)
{
	wlandevice_t *priv;
	struct txhostdescriptor *txdesc;
	TIWLAN_DC *ticontext;

	FN_ENTER;
	priv=(wlandevice_t *)urb->context;
	/* ----------------------------
	 * grab the TI device context
	 * -------------------------- */
	ticontext=&(priv->dc);
	/* ---------------------------------------------
	 * check if the transmit buffer is the right one
	 * --------------------------------------------- */
	if (urb->transfer_buffer==&(priv->bulkout)) {
		txdesc=&(ticontext->pTxHostDescQPool[ticontext->tx_tail]);
		acxlog(L_DEBUG,"bulk xmt completed (status=%d, len=%d, txdesc=%p) tx_pool_count=%d tx_tail=%d\n",urb->status,urb->actual_length,txdesc,ticontext->tx_pool_count,ticontext->tx_tail);
	}
	/* FIXME: are we freeing the skb correctly in the "other" driver
	 * code in USB mode?? */
	FN_EXIT(0,0);
}

static int acx100usb_stop(struct net_device *dev)
{
  FN_ENTER;
  if (!(dev->priv)) {
    printk(KERN_ERR SHORTNAME ": no pointer to acx100 context in network device, cannot operate.\n");
    return(-ENODEV);
  }
  WLAN_MOD_DEC_USE_COUNT;
  FN_EXIT(1,0);
  return(0);
}


static int acx100usb_start_xmit(struct net_device *dev)
{
  return(0);
}

static struct net_device_stats *acx100usb_get_stats(struct net_device *dev)
{
  return(NULL);
}

static void acx100usb_get_wireless_stats(struct net_device *dev)
{
}

static void acx100usb_set_rx_mode(struct net_device *dev)
{
}

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *dev)
{
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
**  MISSING
*/

int init_module()
{
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
**  MISSING
*/

void cleanup_module()
{
  usb_deregister(&acx100usb_driver);
  printk(KERN_INFO "Cleaning up acx100 WLAN USB kernel module\n");
}





/* ---------------------------------------------------------------------------
**                                   DEBUG STUFF
** --------------------------------------------------------------------------- */

#ifdef ACX_DEBUG
static char *acx100usb_pstatus(int val)
{
  switch (val) {
    case USB_ST_NOERROR:return("USB_ST_NOERROR");
    case USB_ST_CRC:return("USB_ST_CRC");
    case USB_ST_BITSTUFF:return("USB_ST_BITSTUFF");
    case USB_ST_DATAOVERRUN:return("USB_ST_DATAOVERRUN");
    case USB_ST_BUFFEROVERRUN:return("USB_ST_BUFFEROVERRUN");
    case USB_ST_BUFFERUNDERRUN:return("USB_ST_BUFFERUNDERRUN");
    case USB_ST_SHORT_PACKET:return("USB_ST_SHORT_PACKET");
    case USB_ST_URB_KILLED:return("USB_ST_URB_KILLED");
    case USB_ST_URB_PENDING:return("USB_ST_URB_PENDING");
    case USB_ST_REMOVED:return("USB_ST_REMOVED");
    case USB_ST_TIMEOUT:return("USB_ST_TIMEOUT");
    case USB_ST_NOTSUPPORTED:return("USB_ST_NOTSUPPORTED");
    case USB_ST_BANDWIDTH_ERROR:return("USB_ST_BANDWIDTH_ERROR");
    case USB_ST_URB_INVALID_ERROR:return("USB_ST_URB_INVALID_ERROR");
    case USB_ST_URB_REQUEST_ERROR:return("USB_ST_URB_REQUEST_ERROR");
    case USB_ST_STALL:return("USB_ST_STALL");
  default:
    return("UNKNOWN");
  }
}

static void dump_usbblock(char *block,int bytes)
{
  int i;
  for (i=0;i<bytes;i++) {
    if ((i&0xF)==0) {
      if (i!=0) printk("\n");
      printk(KERN_INFO);
    }
    printk("%02X ",(unsigned char )(block[i]));
  }
}

static void dump_device(struct usb_device *dev)
{
  int i;
  struct usb_config_descriptor *cd;
  struct usb_interface *ifc;

  printk(KERN_INFO "acx100 device dump:\n");
  printk(KERN_INFO "  devnum: %d\n",dev->devnum);
  printk(KERN_INFO "  speed: %d\n",dev->speed);
  printk(KERN_INFO "  tt: 0x%X\n",(unsigned int)(dev->tt));
  printk(KERN_INFO "  ttport: %d\n",(unsigned int)(dev->ttport));
#ifdef USB_24
  printk(KERN_INFO "  refcnt: %d\n",dev->refcnt);
#endif
  printk(KERN_INFO "  toggle[0]: 0x%X  toggle[1]: 0x%X\n",(unsigned int)(dev->toggle[0]),(unsigned int)(dev->toggle[1]));
  printk(KERN_INFO "  halted[0]: 0x%X  halted[1]: 0x%X\n",dev->halted[0],dev->halted[1]);
  printk(KERN_INFO "  epmaxpacketin: ");
  for (i=0;i<16;i++) printk("%d ",dev->epmaxpacketin[i]);
  printk("\n");
  printk(KERN_INFO "  epmaxpacketout: ");
  for (i=0;i<16;i++) printk("%d ",dev->epmaxpacketout[i]);
  printk("\n");
  printk(KERN_INFO "  parent: 0x%X\n",(unsigned int)(dev->parent));
  printk(KERN_INFO "  bus: 0x%X\n",(unsigned int)(dev->bus));
#if NO_DATATYPE
  printk(KERN_INFO "  configs: ");
  for (i=0;i<dev->descriptor.bNumConfigurations;i++) printk("0x%X ",dev->config[i]);
  printk("\n");
#endif
  printk(KERN_INFO "  actconfig: %p\n",dev->actconfig);
  dump_device_descriptor(&(dev->descriptor));
  cd=dev->actconfig;
  dump_config_descriptor(cd);
#if USB_24
  ifc=cd->interface;
  if (ifc) {
    printk(KERN_INFO "iface: altsetting=%p act_altsetting=%d  num_altsetting=%d  max_altsetting=%d\n",ifc->altsetting,ifc->act_altsetting,ifc->num_altsetting,ifc->max_altsetting);
    dump_interface_descriptor(ifc->altsetting);
    dump_endpoint_descriptor(ifc->altsetting->endpoint);
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
  printk(KERN_INFO "  MaxPower: %d (0x%X)\n",cd->bMaxPower,cd->bMaxPower);
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

static void acx100usb_dump_bytes(void *data,int num)
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
