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
 * USB support for TI ACX100 based devices. Many stuff is taken from
 * the PCI driver.
 *
 * Authors:
 *  Martin Wawro <wawro@ls7.cs.uni-dortmund.de>
 *
 * Issues:
 *  - Currently this driver is only able to boot-up the USB device,
 *    no real functionality available yet.
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
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/vmalloc.h>
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
#include <p80211netdev.h>
#include <acx100.h>

/* -------------------------------------------------------------------------
**                             Module Stuff
** ---------------------------------------------------------------------- */

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
MODULE_AUTHOR("Martin Wawro <wawro@ls7.cs.uni-dortmund.de>");
MODULE_DESCRIPTION("TI ACX100 WLAN USB Driver");

MODULE_PARM(firmware_dir, "s");
MODULE_PARM_DESC(firmware_dir, "Directory where to load acx100 firmware file from");


/* -------------------------------------------------------------------------
**                Module Definitions (preprocessor based)
** ---------------------------------------------------------------------- */


#define MAXNETDEVICES 1
#define SHORTNAME "acx100usb"

#define ACX100_VENDOR_ID 0x2001
#define ACX100_PRODUCT_ID_UNBOOTED 0x3B01
#define ACX100_PRODUCT_ID_BOOTED 0x3B00

#define ACX100_USB_UPLOAD_FW 0x10
#define ACX100_USB_ACK_CS 0x11
#define ACX100_USB_UNKNOWN_REQ1 0x12


#define ACX100_USB_MAXCMDLEN 2048

#define ACX100_INIT_SETUPCMD(_rtype,_req,_val,_ind,_len,_data) \
  (_data)->requesttype=_rtype; \
  (_data)->request=_req; \
  (_data)->value=_val; \
  (_data)->index=_ind; \
  (_data)->length=_len


/* -------------------------------------------------------------------------
**                        Module Data Structures
** ---------------------------------------------------------------------- */

typedef struct acx100usb_cmdblock {
  UINT16 cmd __WLAN_ATTRIB_PACK__;
  UINT16 pad __WLAN_ATTRIB_PACK__;
  UINT16 rid __WLAN_ATTRIB_PACK__;
  UINT8 parms[ACX100_USB_MAXCMDLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100usb_cmdblock_t;

typedef struct acx100usb_resblock {
  UINT16 cmd __WLAN_ATTRIB_PACK__;
  UINT16 status __WLAN_ATTRIB_PACK__;
  UINT8  response[ACX100_USB_MAXCMDLEN];
} __WLAN_ATTRIB_PACK__ acx100usb_resblock_t;

struct acx100usb_context {
  struct usb_device *device;                   /* pointer to claimed USB device structure */
  struct net_device *ndevices[MAXNETDEVICES];  /* pointerlist to network devices associated with the supported USB hardware */
  unsigned int remove_pending;                 /* indicator, whether the module should be removed */
  unsigned int numnetdevs;                     /* number of netdevices we have created */
  acx100usb_cmdblock_t usbout;
  acx100usb_resblock_t usbin;
};



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
static int acx100usb_ioctl(struct net_device *);
static void acx100usb_set_rx_mode(struct net_device *);
static void init_network_device(struct net_device *);
static void init_usb_device(struct usb_device *) __attribute__((__unused__));
/* static void acx100usb_printsetup(devrequest *); */
static void acx100usb_complete(struct urb *) __attribute__((__unused__));
static void * acx100usb_read_firmware(const char *,unsigned int *);
static int acx100usb_boot(struct usb_device *);
static int acx100usb_exec_cmd(short,short,int,short parms[],struct acx100usb_context *);
int init_module(void);
void cleanup_module(void);

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *);
#endif

#ifdef ACX_DEBUG
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

static struct usb_device_id acx100usb_ids[] = {
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_BOOTED) },
   { USB_DEVICE(ACX100_VENDOR_ID, ACX100_PRODUCT_ID_UNBOOTED) },
   { }
};


#ifdef ACX_DEBUG
static int debug=L_DEBUG|L_FUNC;
#endif

/* USB driver data structure as required by the kernel's USB core */

static struct usb_driver acx100usb_driver = {
  .name = "acx100_usb",                    /* name of the driver */
  .probe = acx100usb_probe,                /* pointer to probe() procedure */
  .disconnect = acx100usb_disconnect,      /* pointer to disconnect() procedure */
  .id_table = acx100usb_ids
};


char *firmware_dir;

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

static void * acx100usb_probe(struct usb_device *dev,unsigned int ifNum,const struct usb_device_id *devID) {
  struct acx100usb_context *context;
  struct net_device *netdev;
  int i,numconfigs,numfaces,result;
  struct usb_config_descriptor *config;
  /* ---------------------------------------------
  ** first check if this is the "unbooted" hardware
  ** --------------------------------------------- */
  if ((dev->descriptor.idVendor==ACX100_VENDOR_ID)&&(dev->descriptor.idProduct==ACX100_PRODUCT_ID_UNBOOTED)&&(ifNum==0)) {
    /* ---------------------------------------------
    ** Boot the device (i.e. upload the firmware)
    ** --------------------------------------------- */
    acx100usb_boot(dev);
    acxlog(L_DEBUG,"probe done\n");
    /* ---------------------------------------------
    ** OK, we are done with booting. Normally, the
    ** ID for the unbooted device should disappear
    ** and it will not need a driver anyway...so
    ** return a NULL
    ** --------------------------------------------- */
    return(NULL);
  }
  if ((dev->descriptor.idVendor==ACX100_VENDOR_ID)&&(dev->descriptor.idProduct==ACX100_PRODUCT_ID_BOOTED)&&(ifNum==0)) {
    /* ---------------------------------------------
    ** allocate memory for the device driver context
    ** --------------------------------------------- */
    context = (struct acx100usb_context *)kmalloc(sizeof(struct acx100usb_context),GFP_KERNEL);
    if (!context) {
      printk(KERN_WARNING SHORTNAME ": could not allocate %d bytes memory for device driver context, giving up.\n",sizeof(struct acx100usb_context));
      return(NULL);
    }
    context->device=dev;
    context->numnetdevs=0;
    context->remove_pending=0;
    for (i=0;i<MAXNETDEVICES;i++) context->ndevices[i]=0;
    /* ---------------------------------------------
    ** Initialize the device context and also check
    ** if this is really the hardware we know about.
    ** If not sure, atleast notify the user that he
    ** may be in trouble...
    ** --------------------------------------------- */
    numconfigs=(int)(dev->descriptor.bNumConfigurations);
    if (numconfigs!=1) printk(KERN_WARNING SHORTNAME ": number of configurations is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numconfigs);
    config = dev->actconfig;
    numfaces=config->bNumInterfaces;
    if (numfaces!=1) printk(KERN_WARNING SHORTNAME ": number of interfaces is %d, this version of the driver only knows how to handle 1, be prepared for surprises\n",numfaces);
#ifdef ACX_DEBUG
    dump_device(dev);
#endif
    /* ---------------------------------------------
    ** Allocate memory for (currently) one network
    ** device. If succesful, fill up the init()
    ** routine and register the device
    ** --------------------------------------------- */
    if ((netdev = (struct net_device *)kmalloc(sizeof(struct net_device),GFP_ATOMIC))==NULL) {
      printk(KERN_WARNING SHORTNAME ": failed to alloc netdev\n");
      kfree(context);
      return(NULL);
    }
    memset(netdev, 0, sizeof(struct net_device));
    context->ndevices[context->numnetdevs]=netdev;
    context->numnetdevs++;
    netdev->init=(void *)&init_network_device;
    netdev->priv=context;
    /* --------------------------------------
    ** Allocate a network device name
    ** -------------------------------------- */
    acxlog(L_DEBUG,"allocating device name\n");
    result=dev_alloc_name(netdev,"wlan%d");
    if (result<0) {
      printk(KERN_ERR SHORTNAME ": failed to allocate wlan device name (errcode=%d), giving up.\n",result);
      kfree(netdev);
      kfree(context);
      return(NULL);
    }
    /* --------------------------------------
    ** Register the network device
    ** -------------------------------------- */
    acxlog(L_DEBUG,"registering network device\n");
    result=register_netdev(netdev);
    if (result!=0) {
      printk(KERN_ERR SHORTNAME ": failed to register network device for USB WLAN (errcode=%d), giving up.\n",result);
      kfree(netdev);
      kfree(context);
      return(NULL);
    }
    /* --------------------------------------
    ** Everything went OK, we are happy now
    ** -------------------------------------- */
    return(context);
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

static void acx100usb_disconnect(struct usb_device *dev,void *devContext) {
  unsigned int i;
  int result;
  struct acx100usb_context *context;
  context = (struct acx100usb_context *)devContext;
  /* --------------------------------------
  ** Unregister the network devices (ouch!)
  ** -------------------------------------- */
  for (i=0;i<context->numnetdevs;i++) {
    result=unregister_netdevice(context->ndevices[i]);
    acxlog(L_DEBUG,"unregister netdevice on device %d returned %d\n",i,result);
    kfree(context->ndevices[i]);
  }
  /* --------------------------------------
  ** finally free the context...
  ** -------------------------------------- */
  kfree(context);
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

static int acx100usb_boot(struct usb_device *usbdev) {
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
    printk(KERN_ERR "invalid checksum ?\n");
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

static void * acx100usb_read_firmware(const char *filename,unsigned int *size) {
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
        //vfree(res);
        //res = NULL;
      }
    }
    free_page(page);
  } else {
    printk(KERN_ERR "Unable to allocate memory for firmware loading.\n");
  }
  set_fs(orgfs);
  return(res);
}


static void init_usb_device(struct usb_device *dev) {


}



/* ---------------------------------------------------------------------------
** init_network_device():
** Inputs:
**    netdev -> Pointer to network device
** ---------------------------------------------------------------------------
** Returns:
**  <NOTHING>
**
** Description:
**  Basic setup of a network device for use with the WLAN device
*/

static void init_network_device(struct net_device *netdev) {
  /* --------------------------------------
  ** Setup the device and stop the queue
  ** -------------------------------------- */
  ether_setup(netdev);
  netif_stop_queue(netdev);
  /* --------------------------------------
  ** Register the callbacks for the network
  ** device functions.
  ** -------------------------------------- */
  netdev->open = &acx100usb_open;
  netdev->stop = &acx100usb_stop;
  netdev->hard_start_xmit = (void *)&acx100usb_start_xmit;
  netdev->get_stats = (void *)&acx100usb_get_stats;
  netdev->get_wireless_stats = (void *)&acx100usb_get_wireless_stats;
  netdev->do_ioctl = (void *)&acx100usb_ioctl;
  netdev->set_multicast_list = (void *)&acx100usb_set_rx_mode;
#ifdef HAVE_TX_TIMEOUT
  netdev->tx_timeout = &acx100usb_tx_timeout;
  netdev->watchdog_timeo = 4 * HZ;        /* 400 */
#endif
  SET_MODULE_OWNER(netdev);
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
**  currently missing (this function does not do anything useful yet anyway)
*/

static int acx100usb_open(struct net_device *dev) {
  struct acx100usb_context *context;
  short parms[3];
  int result;

  FN_ENTER;
  if (!(dev->priv)) {
    printk(KERN_ERR SHORTNAME ": no pointer to acx100 context in network device, cannot operate.\n");
    return(-ENODEV);
  }
  MOD_INC_USE_COUNT;  /* OK, now this module is used */


  /* LOCK THAT CONTEXT ! */

  /* --------------------------------------
  ** Initialize the USB device
  ** -------------------------------------- */
  context = (struct acx100usb_context *)dev->priv;
  //usbdev = context->device;
  /* --------------------------------------
  ** Get some information from the device
  ** -------------------------------------- */
  parms[0]=0x20;
  result=acx100usb_exec_cmd(ACX100_CMD_INTERROGATE,0xC,1,parms,context); /* reference result: 0x8 */
  if (result<0) {
  }
  /* --------------------------------------
  ** -------------------------------------- */
  parms[0]=0x2;
  result=acx100usb_exec_cmd(ACX100_CMD_INTERROGATE,ACX100_RID_DOT11_CURRENT_REG_DOMAIN,1,parms,context); /* expected result: 0x1 */
  if (result<0) {
  }
  acxlog(L_DEBUG,"REG_DOMAIN result=0x%X\n",result);
  /* --------------------------------------
  ** some memory map stuff...
  ** -------------------------------------- */
  /*
  taken from the USB snoop
  parms[0]=0x28;
  result=acx100usb_exec_cmd(ACX100_CMD_INTERROGATE,ACX100_RID_MEMORY_MAP,1,parms,context);
  if (result<0) {
  }
  acxlog(L_DEBUG,"MEMORY_MAP result=0x%X\n",result);
  */
  /* --------------------------------------
  ** Set WEP options...
  ** -------------------------------------- */
  /*
  taken from the USB snoop
  parms[0]=0x4;
  result=acx100usb_exec_cmd(ACX100_CMD_CONFIGURE,ACX100_RID_WEP_OPTIONS,1,parms,context);
  if (result<0) {
  }
  result=acx100usb_exec_cmd(ACX100_CMD_CONFIGURE,ACX100_RID_DOT11_WEP_KEY,?,parms,context);
  */
  FN_EXIT(1,0);
  return(0);
}





/* ---------------------------------------------------------------------------
** acx100usb_exec_cmd():
** Inputs:
**       cmd -> The command to send to the device
**       rid -> RID to send (if applicable)
**    nparms -> Number of (UINT16) parameters
**     parms -> Pointer to buffer containing parameters
**   context -> Pointer to driver context
** ---------------------------------------------------------------------------
** Returns:
**  (int) Status from device or <0 on failure
**
** Description:
**  This function executes the command specified by the parameters by performing
**  a synchronous control transfer to the device (send) and from the device
**  (ack).
*/

static int acx100usb_exec_cmd(short cmd,short rid,int nparms,short parms[],struct acx100usb_context *context) {
  int i,result,length;
  unsigned int inpipe,outpipe;
  acx100usb_cmdblock_t *cmdreq;
  acx100usb_resblock_t *cmdresp;
  struct usb_device *usbdev;
  /* --------------------------------------
  ** Prepare buffer pointers...
  ** -------------------------------------- */
  cmdreq=(acx100usb_cmdblock_t *)&(context->usbout);
  cmdresp=(acx100usb_resblock_t *)&(context->usbin);
  /* --------------------------------------
  ** Obtain the USB device
  ** -------------------------------------- */
  usbdev = context->device;
  /* --------------------------------------
  ** Obtain the I/O pipes
  ** -------------------------------------- */
  outpipe=usb_sndctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
  inpipe =usb_rcvctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
  /* --------------------------------------
  ** Fill the request block...
  ** -------------------------------------- */
  cmdreq->cmd=cmd;
  cmdreq->rid=rid;
  if (nparms>ACX100_USB_MAXCMDLEN) return(-1);
  for (i=0;i<nparms;i++) ((short *)&(cmdreq->parms))[i]=parms[i];
  /* --------------------------------------
  ** Send the command to the device
  ** -------------------------------------- */
  length=6+(nparms*sizeof(UINT16));
  result=usb_control_msg(usbdev,outpipe,ACX100_USB_UNKNOWN_REQ1,USB_TYPE_VENDOR|USB_DIR_OUT,0,0,&(context->usbout),length,3000);
  if (result<0) return(result);
  /* --------------------------------------
  ** Check for device status...
  ** -------------------------------------- */
  result=usb_control_msg(usbdev,inpipe,ACX100_USB_UNKNOWN_REQ1,USB_TYPE_VENDOR|USB_DIR_IN,0,0,&(context->usbin),sizeof(struct acx100usb_resblock),3000);
  if (result<0) return(result);
  result=cmdresp->status;
  result&=0xFFFF;
  return(result);
}


static void acx100usb_complete(struct urb *request) {
  acxlog(L_DEBUG,"request returned\n");
  //usb_free_urb(request);
}



static int acx100usb_stop(struct net_device *dev) {
  FN_ENTER;
  if (!(dev->priv)) {
    printk(KERN_ERR SHORTNAME ": no pointer to acx100 context in network device, cannot operate.\n");
    return(-ENODEV);
  }
  MOD_DEC_USE_COUNT;
  FN_EXIT(1,0);
  return(0);
}


static int acx100usb_start_xmit(struct net_device *dev) {
  return(0);
}

static struct net_device_stats * acx100usb_get_stats(struct net_device *dev) {
  return(NULL);
}

static void acx100usb_get_wireless_stats(struct net_device *dev) {
}

static int acx100usb_ioctl(struct net_device *dev) {
  return(0);
}

static void acx100usb_set_rx_mode(struct net_device *dev) {
}

#ifdef HAVE_TX_TIMEOUT
static void acx100usb_tx_timeout(struct net_device *dev) {
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
**  MISSING
*/

void cleanup_module() {
  usb_deregister(&acx100usb_driver);
  printk(KERN_INFO "Cleaning up acx100 WLAN USB kernel module\n");
}





/* ---------------------------------------------------------------------------
**                                   DEBUG STUFF
** --------------------------------------------------------------------------- */

#ifdef ACX_DEBUG
static char * acx100usb_pstatus(int val) {
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

#if 0
static void acx100usb_printsetup(devrequest *req) {
  acxlog(L_DEBUG,"setup-packet: type=0x%X req=0x%X val=0x%X ind=0x%X len=0x%X\n",req->requesttype,req->request,req->value,req->index,req->length);
}
#endif

static void acx100usb_printcmdreq(struct acx100_usb_cmdreq *req) {
  acxlog(L_DEBUG,"cmdreq packet: type=0x%X  cmd=0x%X  parm0=0x%X  parm1=0x%X parm2=0x%X\n",req->type,req->cmd,req->parm0,req->parm1,req->parm2);
}


static void dump_device(struct usb_device *dev) {
  int i;
  struct usb_config_descriptor *cd;
  struct usb_interface *ifc;

  printk(KERN_INFO "acx100 device dump:\n");
  printk(KERN_INFO "  devnum: %d\n",dev->devnum);
  printk(KERN_INFO "  speed: %d\n",dev->speed);
  printk(KERN_INFO "  tt: %p\n",dev->tt);
  printk(KERN_INFO "  ttport: %d\n",dev->ttport);
  printk(KERN_INFO "  refcnt: %d\n", dev->refcnt.counter);
  printk(KERN_INFO "  toggle[0]: 0x%X  toggle[1]: 0x%X\n",dev->toggle[0],dev->toggle[1]);
  printk(KERN_INFO "  halted[0]: 0x%X  halted[1]: 0x%X\n",dev->halted[0],dev->halted[1]);
  printk(KERN_INFO "  epmaxpacketin: ");
  for (i=0;i<16;i++) printk("%d ",dev->epmaxpacketin[i]);
  printk("\n");
  printk(KERN_INFO "  epmaxpacketout: ");
  for (i=0;i<16;i++) printk("%d ",dev->epmaxpacketout[i]);
  printk("\n");
  printk("  parent: %p\n",dev->parent);
  printk("  bus: %p\n",dev->bus);
  printk("\n");
  printk("  actconfig: %p\n",dev->actconfig);


  dump_device_descriptor(&(dev->descriptor));
  cd=dev->actconfig;
  dump_config_descriptor(cd);
  ifc=cd->interface;
  if (ifc) {
    printk(KERN_INFO "iface: altsetting=%p act_altsetting=%d  num_altsetting=%d  max_altsetting=%d\n",ifc->altsetting,ifc->act_altsetting,ifc->num_altsetting,ifc->max_altsetting);
    dump_interface_descriptor(ifc->altsetting);
    dump_endpoint_descriptor(ifc->altsetting->endpoint);
  }

}


static void dump_config_descriptor(struct usb_config_descriptor *cd) {
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
  printk(KERN_INFO "  MaxPower: %d (0x%X)\n",cd->MaxPower,cd->MaxPower);
  printk(KERN_INFO "  *interface: %p\n",cd->interface);
  printk(KERN_INFO "  *extra: %p\n",cd->extra);
  printk(KERN_INFO "  extralen: %d (0x%X)\n",cd->extralen,cd->extralen);
}

static void dump_device_descriptor(struct usb_device_descriptor *dd) {
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

static void dump_endpoint_descriptor(struct usb_endpoint_descriptor *ep) {
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

static void dump_interface_descriptor(struct usb_interface_descriptor *id) {
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
  printk(KERN_INFO "  endpoint: %p\n",id->endpoint);
}

static void acx100usb_dump_bytes(void *data,int num) {
  int i,remain=num;
  unsigned char *ptr=(unsigned char *)data;
  while (remain>0) {
    if (remain<8) {
      printk(KERN_INFO);
      for (i=0;i<remain;i++) printk("%02X ",*ptr++);
      printk("\n");
      remain=0;
    } else {
      printk(KERN_INFO);
      for (i=0;i<8;i++) printk("%02X ",*ptr++);
      printk("\n");
      remain-=8;
    }
  }
}

#endif
