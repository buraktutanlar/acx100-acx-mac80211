/* src/ihw.c - low level control functions
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
 */

#if (WLAN_HOSTIF==WLAN_USB)

#ifdef ACX_DEBUG
extern void acx100usb_dump_bytes(void *,int);
#endif

#include <linux/config.h>
#define WLAN_DBVAR	prism2_debug
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/usb.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <asm/pci.h>
#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <version.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100_conv.h>
#include <p80211msg.h>
//#include <p80211ioctl.h>
#include <acx100.h>
//#include <p80211req.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
//#include <acx100mgmt.h>
#include <acx100.h>


int acx100_issue_cmd(wlandevice_t *hw,UINT cmd,void *pdr,int paramlen,UINT32 timeout) {
  int result,blocklen,inpipe,outpipe,acklen=sizeof(hw->usbin);
  struct usb_device *usbdev;
  FN_ENTER;
  /* ----------------------------------------------------
  ** get context from wlandevice
  ** ------------------------------------------------- */
  usbdev=hw->usbdev;
  acxlog(L_DEBUG,"hw=%p usbdev=%p cmd=%d paramlen=%d timeout=%ld\n",hw,usbdev,cmd,paramlen,timeout);
  /* ----------------------------------------------------
  **
  ** ------------------------------------------------- */
  if (cmd==ACX100_CMD_INTERROGATE) {
    hw->usbout.rridreq.cmd=cmd;
    hw->usbout.rridreq.status=0;
    hw->usbout.rridreq.rid=((memmap_t *)pdr)->type;
    hw->usbout.rridreq.frmlen=paramlen;
    blocklen=8;
    acklen=paramlen+8;
    acxlog(L_DEBUG,"sending interrogate: cmd=%d status=%d rid=%d frmlen=%d\n",hw->usbout.rridreq.cmd,hw->usbout.rridreq.status,hw->usbout.rridreq.rid,hw->usbout.rridreq.frmlen);
  } else if (cmd==ACX100_CMD_CONFIGURE) {
    hw->usbout.wridreq.cmd=cmd;
    hw->usbout.wridreq.status=0;
    hw->usbout.wridreq.rid=((memmap_t *)pdr)->type;
    hw->usbout.wridreq.frmlen=paramlen;
    memcpy(hw->usbout.wridreq.data,&(((memmap_t *)pdr)->m),paramlen);
    blocklen=8+paramlen;
  } else if ((cmd==ACX100_CMD_ENABLE_RX)||(cmd==ACX100_CMD_ENABLE_TX)) {
 	hw->usbout.rxtx.cmd=cmd;
	hw->usbout.rxtx.status=0;
	hw->usbout.rxtx.data=1;		/* just for testing */
	blocklen=5;
  } else {
    /* ----------------------------------------------------
    ** All other commands (not thoroughly tested)
    ** ------------------------------------------------- */
    hw->usbout.wmemreq.cmd=cmd;
    hw->usbout.wmemreq.status=0;
    if ((pdr)&&(paramlen>0)) memcpy(hw->usbout.wmemreq.data,pdr,paramlen);
    blocklen=4+paramlen;
  }
  /* ----------------------------------------------------
  ** Obtain the I/O pipes
  ** ------------------------------------------------- */
  outpipe=usb_sndctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
  inpipe =usb_rcvctrlpipe(usbdev,0);      /* default endpoint for ctrl-transfers: 0 */
  /* ----------------------------------------------------
  ** Copy parameters..
  ** ------------------------------------------------- */
  acxlog(L_DEBUG,"sending USB control msg (out) (blocklen=%d)\n",blocklen);
  acx100usb_dump_bytes(&(hw->usbout),blocklen);
  result=usb_control_msg(usbdev,outpipe,ACX100_USB_UNKNOWN_REQ1,USB_TYPE_VENDOR|USB_DIR_OUT,0,0,&(hw->usbout),blocklen,timeout);
  acxlog(L_DEBUG,"wrote=%d bytes\n",result);
  if (result<0) {
    return(0);
  }
  /* --------------------------------------
  ** Check for device acknowledge ...
  ** -------------------------------------- */
  acxlog(L_DEBUG,"sending USB control msg (in) (acklen=%d)\n",acklen);
  hw->usbin.rridresp.status=0; /* delete old status flag -> set to fail */
  result=usb_control_msg(usbdev,inpipe,ACX100_USB_UNKNOWN_REQ1,USB_TYPE_VENDOR|USB_DIR_IN,0,0,&(hw->usbin),acklen,timeout);
  acxlog(L_DEBUG,"read=%d bytes\n",result);
  if (result<0) {
    FN_EXIT(0,result);
    return(0);
  }
  acxlog(L_DEBUG,"status=%d\n",hw->usbin.rridresp.status);
  if (cmd==ACX100_CMD_INTERROGATE) {
    if ((pdr)&&(paramlen>0)) {
      memcpy(pdr,&(hw->usbin.rridresp.rid),paramlen+4);
      acxlog(L_DEBUG,"response frame: cmd=%d status=%d rid=%d frmlen=%d\n",hw->usbin.rridresp.cmd,hw->usbin.rridresp.status,hw->usbin.rridresp.rid,hw->usbin.rridresp.frmlen);
      acxlog(L_DEBUG,"incoming bytes (%d):\n",paramlen+4);
      acx100usb_dump_bytes(pdr,paramlen+4);
    }
  }
  FN_EXIT(0,1);
  return(1);
}



/*****************************************************************************
 *
 * The Upper Layer
 *
 ****************************************************************************/

static short CtlLength[0x14] = {
	0,
	ACX100_RID_ACX_TIMER_LEN,
	ACX100_RID_POWER_MGMT_LEN,
	ACX100_RID_QUEUE_CONFIG_LEN,
	ACX100_RID_BLOCK_SIZE_LEN,
	ACX100_RID_MEMORY_CONFIG_OPTIONS_LEN,
	0,
	ACX100_RID_WEP_OPTIONS_LEN,
	ACX100_RID_MEMORY_MAP_LEN, //	ACX100_RID_SSID_LEN,
	0,
	ACX100_RID_ASSOC_ID_LEN,
	0,
	0,
	ACX100_RID_FWREV_LEN,
	ACX100_RID_FCS_ERROR_COUNT_LEN,
	ACX100_RID_MEDIUM_USAGE_LEN,
	ACX100_RID_RXCONFIG_LEN,
	0,
	0,
	ACX100_RID_FIRMWARE_STATISTICS_LEN
};


static short CtlLengthDot11[0x14] = {
	0,
	ACX100_RID_DOT11_STATION_ID_LEN,
	0,
	ACX100_RID_DOT11_BEACON_PERIOD_LEN,
	ACX100_RID_DOT11_DTIM_PERIOD_LEN,
	ACX100_RID_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX100_RID_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_RID_DOT11_WEP_DEFAULT_KEY_LEN, //ACX100_RID_DOT11_WEP_KEY_LEN,
	ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX100_RID_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX100_RID_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX100_RID_DOT11_TX_POWER_LEVEL_LEN,
	ACX100_RID_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_RID_DOT11_ED_THRESHOLD_LEN,
	ACX100_RID_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0
};




/*----------------------------------------------------------------
** acx100_configure():
**
**  Inputs:
**     hw -> Pointer to wlandevice structure
**    pdr -> Field for parameter data
**   type -> Type of request (Request-ID)
** ---------------------------------------------------------------
** Returns:
**  (int) Success indicator (1 on success, 0 on failure)
**
** Description:
*----------------------------------------------------------------*/

int acx100_configure(wlandevice_t *hw,void *pdr,short type) {
  UINT16 len;
  /* ----------------------------------------------------
  ** check if ACX100 control command or if 802.11 command
  ** ------------------------------------------------- */
  if (type<0x1000) len=CtlLength[type];
  else len=CtlLengthDot11[type-0x1000];
	if (len==0) {
		acxlog(L_DEBUG,"WARNING: ENCOUNTERED ZEROLENGTH RID (%x)\n",type);
	}
  acxlog(L_DEBUG,"configuring: type(rid)=0x%X len=%d\n",type,len);
  ((memmap_t *)pdr)->type=type;
  return(acx100_issue_cmd(hw,ACX100_CMD_CONFIGURE,pdr,len,5000));
}



/*----------------------------------------------------------------
** acx100_configure_length():
**
**  Inputs:
**     hw -> Pointer to wlandevice structure
**    pdr -> Field for parameter data
**   type -> Type of request (Request-ID)
** ---------------------------------------------------------------
** Returns:
**  (int) Success indicator (0 on failure, 1 on success)
**
** Description:
** ----------------------------------------------------------------*/

int acx100_configure_length(wlandevice_t *hw, void *pdr,short type,short len) {
  acxlog(L_DEBUG,"configuring: type(rid)=0x%X len=%d\n",type,len);
  ((memmap_t *)pdr)->type = type;
  return(acx100_issue_cmd(hw,ACX100_CMD_CONFIGURE,pdr,len,5000));
}



/*----------------------------------------------------------------
** acx100_interrogate():
**  Inputs:
**     hw -> Pointer to wlandevice structure
**    pdr -> Field for parameter data
**   type -> Type of request (Request-ID)
** ---------------------------------------------------------------
** Returns:
**  (int) Errorcode (0 if OK)
**
** Description:
**--------------------------------------------------------------*/

int acx100_interrogate(wlandevice_t *hw,void *pdr,short type) {
  UINT16 len;
  /* ----------------------------------------------------
  ** check if ACX100 control command or if 802.11 command
  ** ------------------------------------------------- */
  if (type<0x1000) len=CtlLength[type];
  else len=CtlLengthDot11[type-0x1000];
	if (len==0) {
		acxlog(L_DEBUG,"WARNING: ENCOUNTERED ZEROLENGTH RID (%x)\n",type);
	}
  acxlog(L_DEBUG,"interrogating: type(rid)=0x%X len=%d pdr=%p\n",type,len,pdr);
  ((memmap_t *)pdr)->type=type;
  return(acx100_issue_cmd(hw,ACX100_CMD_INTERROGATE,pdr,len,5000));
}



/*****************************************************************************
 *
 * MAC Address Stuff
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* acx100_is_mac_address_zero
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/

int acx100_is_mac_address_zero(mac_t * mac) {
  if ((mac->vala == 0) && (mac->valb == 0)) {
    return(1);
  }
  return 0;
}


/*----------------------------------------------------------------
* acx100_clear_mac_address
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
void acx100_clear_mac_address(mac_t *m) {
  m->vala = 0;
  m->valb = 0;
}


/*----------------------------------------------------------------
* acx100_is_mac_address_equal
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
int acx100_is_mac_address_equal(UINT8 * one, UINT8 * two)
{
	if (memcmp(one, two, WLAN_ADDR_LEN))
		return 0; /* no match */
	else
		return 1; /* matched */
}


/*----------------------------------------------------------------
* acx100_copy_mac_address
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
void acx100_copy_mac_address(UINT8 * to, const UINT8 * const from)
{
	memcpy(to, from, ETH_ALEN);
}

/*----------------------------------------------------------------
* acx100_is_mac_address_group
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
UINT8 acx100_is_mac_address_group(mac_t * mac)
{
	return mac->vala & 1;
}

/*----------------------------------------------------------------
* acx100_is_mac_address_directed
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
UINT8 acx100_is_mac_address_directed(mac_t * mac)
{
	if (mac->vala & 1) {
		return 0;
	}
	return 1;
}

/*----------------------------------------------------------------
* acx100_set_mac_address_broadcast
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
void acx100_set_mac_address_broadcast(UINT8 *mac)
{
	memset(mac, 0xff, ETH_ALEN);
}

/*----------------------------------------------------------------
* acx100_is_mac_address_broadcast
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
int acx100_is_mac_address_broadcast(const UINT8 * const address)
{
	static const unsigned char bcast[ETH_ALEN] ={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	return !memcmp(address, bcast, ETH_ALEN);
}

/*----------------------------------------------------------------
* acx100_is_mac_address_multicast
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
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
int acx100_is_mac_address_multicast(mac_t * mac)
{
	if (mac->vala & 1) {
		if ((mac->vala == 0xffffffff) && (mac->valb == 0xffff))
			return 0;
		else
			return 1;
	}
	return 0;
}

/*----------------------------------------------------------------
* acx100_log_mac_address
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
* STATUS: NEW
*
* Comment:
*
*----------------------------------------------------------------*/

void acx100_log_mac_address(int level, UINT8 * mac) {
	acxlog(level, "%02X.%02X.%02X.%02X.%02X.%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


/*----------------------------------------------------------------
* acx100_power_led
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
* STATUS: NEW
*
* Comment:
*
*----------------------------------------------------------------*/
void acx100_power_led(wlandevice_t *wlandev, int enable) {
  /*
  if (enable)
    acx100_write_reg16(wlandev, 0x290, acx100_read_reg16(wlandev, 0x290) & ~0x0800);
  else
    acx100_write_reg16(wlandev, 0x290, acx100_read_reg16(wlandev, 0x290) | 0x0800);
  */
}

#endif
