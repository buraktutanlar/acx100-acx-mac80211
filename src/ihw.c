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

#include <linux/config.h>
#define WLAN_DBVAR	prism2_debug
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

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
#include <p80211conv.h>
#include <p80211msg.h>
#include <p80211ioctl.h>
#include <acx100.h>
#include <p80211netdev.h>
#include <p80211req.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <acx100mgmt.h>

/*****************************************************************************
 * 
 * The Really Low Level Stuff (TM)
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* hwReadRegister32
* rename to acx100_read_reg32
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

/* hwReadRegister32()
 * STATUS: ok
 */
UINT32 hwReadRegister32(wlandevice_t * hw, UINT offset)
{
	return readl(hw->iobase + offset);
}
/*----------------------------------------------------------------
* hwReadRegister16
* FIXME: rename to acx100_read_reg16
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

/* hwReadRegister16()
 * STATUS: should be ok.
 */
UINT16 hwReadRegister16(wlandevice_t * hw, UINT offset)
{
	return readw(hw->iobase + offset);
}

/*----------------------------------------------------------------
* hwReadRegister8
* FIXME: rename to acx100_read_reg8
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

/* hwReadRegister8()
 * STATUS: ok
 */
UINT8 hwReadRegister8(wlandevice_t * hw, UINT offset)
{
	return readb(hw->iobase + offset);
}

/*----------------------------------------------------------------
* hwWriteRegister32
* FIXME: rename to acx100_write_reg32
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

/* hwWriteRegister32()
 * STATUS: should be ok.
 */
void hwWriteRegister32(wlandevice_t * hw, UINT offset, UINT valb)
{
	writel(valb, hw->iobase + offset);
}

/*----------------------------------------------------------------
* hwWriteRegister16
* FIXME: rename to acx100_write_reg16
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

/* hwWriteRegister16()
 * STATUS: should be ok.
 */
void hwWriteRegister16(wlandevice_t * hw, UINT offset, UINT16 valb)
{
	writew(valb, hw->iobase + offset);
}

/*----------------------------------------------------------------
* hwWriteRegister8
* FIXME: rename to acx100_write_reg8
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

/* hwWriteRegister8()
 * STATUS: should be ok.
 */
void hwWriteRegister8(wlandevice_t * hw, UINT offset, UINT valb)
{
	writeb(valb, hw->iobase + offset);
}

/*****************************************************************************
 * 
 * Intermediate Level
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* get_cmd_state
* FIXME: rename to acx100_get_cmd_state
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

/* get_cmd_state()
 * STATUS: wow, lots of errors, but now it should be ok.
 */
void get_cmd_state(wlandevice_t * hw)
{
	hwWriteRegister16(hw, ACX100_FW_4, 0x0);
	hwWriteRegister16(hw, ACX100_FW_5, 0x0);
	hwWriteRegister16(hw, ACX100_FW_2, 0x0);
	hwWriteRegister16(hw, ACX100_FW_3, 0x1);
	hwWriteRegister16(hw, ACX100_FW_0,
			  hwReadRegister16(hw, ACX100_CMD_MAILBOX_OFFS));
	hwWriteRegister16(hw, ACX100_FW_1, 0x0);
	hw->cmd_type = hwReadRegister16(hw, ACX100_DATA_LO);
	hw->cmd_status = hwReadRegister16(hw, ACX100_DATA_HI);
}
/*----------------------------------------------------------------
* write_cmd_type
* FIXME: rename to acx100_write_cmd_type
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

/* write_cmd_type()
 * STATUS: should really be ok.
 */
void write_cmd_type(wlandevice_t * hw, UINT16 vala)
{
	hwWriteRegister16(hw, ACX100_FW_4, 0x0);
	hwWriteRegister16(hw, ACX100_FW_5, 0x0);
	hwWriteRegister16(hw, ACX100_FW_2, 0x0);
	hwWriteRegister16(hw, ACX100_FW_3, 0x1);
	hwWriteRegister16(hw, ACX100_FW_0,
			  hwReadRegister16(hw, ACX100_CMD_MAILBOX_OFFS));
	hwWriteRegister16(hw, ACX100_FW_1, 0x0);
	hwWriteRegister16(hw, ACX100_DATA_LO, vala);
	hwWriteRegister16(hw, ACX100_DATA_HI, 0x0);
}

/*----------------------------------------------------------------
* write_cmd_status
* FIXME: rename to acx100_write_cmd_status
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

/* write_cmd_status()
 * STATUS: should really be ok.
 */
void write_cmd_status(wlandevice_t * act, UINT vala)
{
	hwWriteRegister16(act, ACX100_FW_4, 0x0);
	hwWriteRegister16(act, ACX100_FW_5, 0x0);
	hwWriteRegister16(act, ACX100_FW_2, 0x0);
	hwWriteRegister16(act, ACX100_FW_3, 0x1);
	hwWriteRegister16(act, ACX100_FW_0,
			  hwReadRegister16(act, ACX100_CMD_MAILBOX_OFFS));
	hwWriteRegister16(act, ACX100_FW_1, 0x0);
	hwWriteRegister16(act, ACX100_DATA_LO, 0x0);
	hwWriteRegister16(act, ACX100_DATA_HI, vala);
}

/*----------------------------------------------------------------
* write_cmd_Parameters
* FIXME: rename to acx100_write_cmd_parameters
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

/* write_cmd_Parameters()
 * STATUS: should be ok.
 */
int write_cmd_Parameters(wlandevice_t * hw, memmap_t * cmd, int len)
{
	memcpy((UINT *) hw->CommandParameters, cmd, len);
	return 0;
}

/*----------------------------------------------------------------
* read_cmd_Parameters
* FIXME: rename to acx100_write_cmd_parameters
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

/* read_cmd_Parameters()
 * STATUS: should be ok.
 */
int read_cmd_Parameters(wlandevice_t * hw, memmap_t * cmd, int len)
{
	memcpy(cmd, (UINT *) hw->CommandParameters, len);
	return 0;
}

/*----------------------------------------------------------------
* ctlIssueCommand
* FIXME: rename to acx100_issue_cmd
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

/* ctlIssueCommand()
 * STATUS: FINISHED, UNVERIFIED.
 */
int ctlIssueCommand(wlandevice_t * hw, UINT cmd,
		    void *pcmdparam, int paramlen, UINT32 timeout)
{
	int counter;
	int result = 0;
	UINT16 irqtype = 0;
	UINT16 cmd_status;

	FN_ENTER;
	acxlog(L_CTL, "%s cmd %X timeout %ld: UNVERIFIED.\n", __func__,
	       cmd, timeout);

	/*** make sure we have at least *some* timeout value ***/
	if (timeout == 0) {
		timeout = 1;
	}

	/* 
	   cmd_state can be one of the following:
		0x0: IDLE
		0x1: ??
		0x2: Unknown Command
		0x3: INvalid InfoEle
		0xe: "value out of range"
		default: Unknown
	*/

	/*** wait for ACX100 to become idle for our command submission ***/
	for (counter = 2000; counter > 0; counter--) {
		/* FIXME: auwtsh, busy waiting.
		 * Hmm, yeah, but this function is probably supposed to be
		 * finished ASAP, so it's probably not such a terribly good
		 * idea to schedule away instead of busy wait.
		 * Maybe let's just keep it as is. */
		get_cmd_state(hw);
		/* Test for IDLE state */
		if (!hw->cmd_status)
			break;
	}

	/*** The card doesn't get IDLE, we're in trouble ***/
	if (counter == 0) {
		/* uh oh, cmd status still set, now we're in trouble */
		acxlog((L_BINDEBUG | L_CTL),
		       "Trying to issue a command to the ACX100 but the Command Register is not IDLE\n");
		goto done;
	}

	/*** now write the parameters of the command if needed ***/
	if (pcmdparam != NULL && paramlen != 0) {
		if (cmd == ACX100_CMD_INTERROGATE) {
			/* it's an INTERROGATE command, so just pass the length
			 * of parameters to read, as data */
			write_cmd_Parameters(hw, pcmdparam, 0x4);
		} else {
			write_cmd_Parameters(hw, pcmdparam, paramlen);
		}
	}

	/*** now write the actual command type ***/
	hw->cmd_type = cmd;
	write_cmd_type(hw, cmd);

	/*** is this "execute command" operation? ***/
	hwWriteRegister16(hw, ACX100_REG_7C,
			  hwReadRegister16(hw, ACX100_REG_7C) | 0x01);

	/*** wait for IRQ to occur, then ACK it? ***/
	if (timeout > 120000) {
		timeout = 120000;
	}
	timeout *= 20;
	if (timeout) {
		for (counter = timeout; counter > 0; counter--) {
			/* it's a busy wait loop, but we're supposed to be
			 * fast here, so better don't schedule away here? */
			if ((irqtype =
			     hwReadRegister16(hw, ACX100_STATUS)) & 0x200) {
				hwWriteRegister16(hw, ACX100_IRQ_ACK, 0x200);
				break;
			}
		}
	}

	/*** Save state for debugging ***/
	get_cmd_state(hw);
	cmd_status = hw->cmd_status;

	/*** Put the card in IDLE state ***/
	hw->cmd_status = 0;
	write_cmd_status(hw, 0);

	if ((irqtype | 0xfdff) == 0xfdff) {
		acxlog(L_CTL,
		       "Polling for an IRQ failed with %X, cmd_status %d. Bailing.\n",
		       irqtype, cmd_status);
		goto done;
	}

	if (cmd_status != 1) {
		switch (cmd_status) {
		case 0x0:
			acxlog((L_BINDEBUG | L_CTL),
			       "ctlIssueCommand failed: Idle[TIMEOUT] [%ld uSec]\n",
			       (timeout - counter) * 50);
			break;

		case 0x2:
			acxlog((L_BINDEBUG | L_CTL),
			       "ctlIssueCommand failed: Unknown Command [%ld uSec]\n",
			       (timeout - counter) * 50);
			break;

		case 0x3:
			acxlog((L_BINDEBUG | L_CTL),
			       "ctlIssueCommand failed: INvalid InfoEle [%ld uSec]\n",
			       (timeout - counter) * 50);
			break;

		default:
			acxlog((L_BINDEBUG | L_CTL),
			       "ctlIssueCommand failed: Unknown(0x%x) [%ld uSec]\n",
			       cmd_status, (timeout - counter) * 50);
			/* cmd_status 0xe might be "value out of range": happens if you
			 * write a value < 1 or > 2 at ctlDot11CurrentTxPowerLevelWrite */
			break;
		}
	} else	{
		/*** read in result parameters if needed ***/
		if (pcmdparam != NULL && paramlen != 0) {
			if (cmd == ACX100_CMD_INTERROGATE) {
				read_cmd_Parameters(hw, pcmdparam,
						    paramlen);
			}
		}
		result = 1;
	}

      done:
	FN_EXIT(1, result);
	return result;
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
	ACX100_RID_RATE_LEN,
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
	ACX100_RID_DOT11_WEP_DEFAULT_KEY_LEN,
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
* ctlConfigure
* FIXME: acx100_configure
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

/* ctlConfigure()
 * STATUS: should be ok. FINISHED.
 */
int ctlConfigure(wlandevice_t * hw, void * pdr, short type)
{
	((memmap_t *)pdr)->type = type;
	if (type < 0x1000) {
		((memmap_t *)pdr)->length = CtlLength[type];
		return ctlIssueCommand(hw, ACX100_CMD_CONFIGURE, pdr, 
			CtlLength[type] + 4, 5000);
	} else {
		((memmap_t *)pdr)->length = CtlLengthDot11[type-0x1000];
		return ctlIssueCommand(hw, ACX100_CMD_CONFIGURE, pdr, 
			CtlLengthDot11[type-0x1000] + 4, 5000);
	}
}

/*----------------------------------------------------------------
* ctlConfigureLength
* FIXME: rename to acx100_configure_length
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

/* ctlConfigureLength()
 */
int ctlConfigureLength(wlandevice_t * hw, void * pdr, short type, short length)
{
	((memmap_t *)pdr)->type = type;
	((memmap_t *)pdr)->length = length;
	return ctlIssueCommand(hw, ACX100_CMD_CONFIGURE, pdr, 
		length + 4, 5000);
}

/*----------------------------------------------------------------
* ctlInterrogate
* FIXME: rename to acx100_interrogate
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

/* ctlInterrogate()
 * STATUS: should be ok. FINISHED.
 */
int ctlInterrogate(wlandevice_t * hw, void * pdr, short type)
{
	((memmap_t *)pdr)->type = type;
	if (type < 0x1000) {
		((memmap_t *)pdr)->length = CtlLength[type];
		return ctlIssueCommand(hw, ACX100_CMD_INTERROGATE, pdr,
			CtlLength[type] + 4, 5000);
	} else {
		((memmap_t *)pdr)->length = CtlLengthDot11[type-0x1000];
		return ctlIssueCommand(hw, ACX100_CMD_INTERROGATE, pdr,
			CtlLengthDot11[type-0x1000] + 4, 5000);
	}
}


/*****************************************************************************
 * 
 * MAC Address Stuff
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* IsMacAddressZero
* FIXME: rename to acx100_is_mac_address_zero
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

/* IsMacAddressZero()
 * STATUS: should be ok. FINISHED.
 */
int IsMacAddressZero(mac_t * mac)
{
	if ((mac->vala == 0) && (mac->valb == 0)) {
		return 1;
	}
	return 0;
}
/*----------------------------------------------------------------
* ClearMacAddress
* FIXME: rename to acx100_clear_mac_address
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

/* ClearMacAddress()
 * STATUS: should be ok. FINISHED.
 */
void ClearMacAddress(mac_t * m)
{
	m->vala = 0;
	m->valb = 0;
}
/*----------------------------------------------------------------
* IsMacAddressEqual
* FIXME: rename to acx100_is_mac_address_equal
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

/* IsMacAddressEqual()
 * STATUS: should be ok. FINISHED.
 */
int IsMacAddressEqual(UINT8 * one, UINT8 * two)
{
	if (memcmp(one, two, WLAN_ADDR_LEN))
		return 0; /* no match */
	else
		return 1; /* matched */
}

/*----------------------------------------------------------------
* CopyMacAddress
* FIXME: rename to acx100_copy_mac_address
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

/* CopyMacAddress()
 * STATUS: should be ok. FINISHED.
 */
void CopyMacAddress(UINT8 * to, UINT8 * from)
{
	memcpy(to, from, WLAN_ADDR_LEN);
}

/*----------------------------------------------------------------
* IsMacAddressGroup
* FIXME: rename to acx100_is_mac_address_group
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

/* IsMacAddressGroup()
 * STATUS: should be ok. FINISHED.
 */
UINT8 IsMacAddressGroup(mac_t * mac)
{
	return mac->vala & 1;
}
/*----------------------------------------------------------------
* IsMacAddressDirected
* FIXME: rename to acx100_is_mac_address_directed
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

/* IsMacAddressDirected()
 * STATUS: should be ok. FINISHED.
 */
UINT8 IsMacAddressDirected(mac_t * mac)
{
	if (mac->vala & 1) {
		return 0;
	}
	return 1;
}

/*----------------------------------------------------------------
* SetMacAddressBroadcast
* FIXME: rename to acx100_set_mac_address_broadcast
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

/* SetMacAddressBroadcast()
 * STATUS: should be ok. FINISHED.
 */
void SetMacAddressBroadcast(char *mac)
{
	memset(mac, 0xff, WLAN_ADDR_LEN);
}

/*----------------------------------------------------------------
* IsMacAddressBroadcast
* FIXME: rename to acx100_is_mac_address_broadcast
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

/* IsMacAddressBroadcast()
 * STATUS: should be ok. FINISHED.
 */
int IsMacAddressBroadcast(mac_t * mac)
{
	if ((mac->vala == 0xffffffff) && (mac->valb == 0xffff)) {
		return 1;
	}
	return 0;
}

/*----------------------------------------------------------------
* IsMacAddressMulticast
* FIXME: rename to acx100_is_mac_address_multicast
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

/* IsMacAddressMulticast()
 * STATUS: should be ok. FINISHED.
 */
int IsMacAddressMulticast(mac_t * mac)
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
* LogMacAddress
* FIXME: rename to acx100_log_mac_address
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

/* LogMacAddress()
 * STATUS: NEW
 */
void LogMacAddress(int level, UINT8 * mac)
{
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
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

void acx100_power_led(wlandevice_t *wlandev, int enable)
{
	if (enable)
		hwWriteRegister16(wlandev, 0x290, hwReadRegister16(wlandev, 0x290) & ~0x0800);
	else
		hwWriteRegister16(wlandev, 0x290, hwReadRegister16(wlandev, 0x290) | 0x0800);
}
