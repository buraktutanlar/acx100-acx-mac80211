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
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/if_arp.h>
#include <linux/wireless.h>

#include <wlan_compat.h>

#include <linux/pci.h>

#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <p80211hdr.h>
#include <acx100.h>
#include <acx100_helper.h>
#include <ihw.h>

void acx100_dump_bytes(void *,int);


/*****************************************************************************
 *
 * The Really Low Level Stuff (TM)
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* acx100_read_reg32
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
inline UINT32 acx100_read_reg32(wlandevice_t *priv, UINT offset)
{
#if ACX_IO_WIDTH == 32
	return readl(priv->iobase + offset);
#else 
	return readw(priv->iobase + offset)
	    + (readw(priv->iobase + offset + 2) << 16);
#endif
}

/*----------------------------------------------------------------
* acx100_read_reg16
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
inline UINT16 acx100_read_reg16(wlandevice_t *priv, UINT offset)
{
	return readw(priv->iobase + offset);
}

/*----------------------------------------------------------------
* acx100_read_reg8
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
inline UINT8 acx100_read_reg8(wlandevice_t *priv, UINT offset)
{
	return readb(priv->iobase + offset);
}

/*----------------------------------------------------------------
* acx100_write_reg32
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
inline void acx100_write_reg32(wlandevice_t *priv, UINT offset, UINT valb)
{
#if ACX_IO_WIDTH == 32
	writel(valb, priv->iobase + offset);
#else 
	writew(valb & 0xffff, priv->iobase + offset);
	writew(valb >> 16, priv->iobase + offset + 2);
#endif
}

/*----------------------------------------------------------------
* acx100_write_reg16
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
inline void acx100_write_reg16(wlandevice_t *priv, UINT offset, UINT16 valb)
{
	writew(valb, priv->iobase + offset);
}

/*----------------------------------------------------------------
* acx100_write_reg8
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
inline void acx100_write_reg8(wlandevice_t *priv, UINT offset, UINT valb)
{
	writeb(valb, priv->iobase + offset);
}

/*****************************************************************************
 * 
 * Intermediate Level
 *
 ****************************************************************************/

void acx100_get_info_state(wlandevice_t *priv)
{
	UINT32 value;

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_END_CTL], 0x0);
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_CTL], cpu_to_le32(0x1));

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_ADDR], 
		acx100_read_reg32(priv, priv->io[IO_ACX_INFO_MAILBOX_OFFS]));

	value = le32_to_cpu(acx100_read_reg32(priv, priv->io[IO_ACX_SLV_MEM_DATA]));

	priv->info_type = value & 0xffff;
	priv->info_status = value >> 16;

	acxlog(L_CTL, "info_type 0x%04x, info_status 0x%04x\n", priv->info_type, priv->info_status);
}

/*----------------------------------------------------------------
* acx100_get_cmd_state
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
void acx100_get_cmd_state(wlandevice_t *priv)
{
	UINT32 value;

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_END_CTL], cpu_to_le32(0x0));
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_CTL], cpu_to_le32(0x1)); /* why auto increment ?? */

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_ADDR], 
		acx100_read_reg32(priv, priv->io[IO_ACX_CMD_MAILBOX_OFFS]));

	value = acx100_read_reg32(priv, priv->io[IO_ACX_SLV_MEM_DATA]);
	priv->cmd_type = (UINT16)value;
	priv->cmd_status = (UINT16)(value >> 16);

	acxlog(L_CTL, "cmd_type 0x%04x, cmd_status 0x%04x\n", priv->cmd_type, priv->cmd_status);

}

/*----------------------------------------------------------------
* acx100_write_cmd_type
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
void acx100_write_cmd_type(wlandevice_t *priv, UINT16 vala)
{
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_END_CTL], cpu_to_le32(0x0));
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_CTL], cpu_to_le32(0x1));

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_ADDR], 
		acx100_read_reg32(priv, priv->io[IO_ACX_CMD_MAILBOX_OFFS]));

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_DATA], cpu_to_le32(vala));
}

/*----------------------------------------------------------------
* acx100_write_cmd_status
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
void acx100_write_cmd_status(wlandevice_t *priv, UINT vala)
{
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_END_CTL], cpu_to_le32(0x0));
	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_CTL], cpu_to_le32(0x1));

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_ADDR], 
		acx100_read_reg32(priv, priv->io[IO_ACX_CMD_MAILBOX_OFFS]));

	acx100_write_reg32(priv, priv->io[IO_ACX_SLV_MEM_DATA], cpu_to_le32(vala));
}

/*----------------------------------------------------------------
* acx100_write_cmd_param
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
static inline void acx100_write_cmd_param(wlandevice_t *priv, memmap_t *cmd, int len)
{
	memcpy((UINT32 *) priv->CommandParameters, cmd, len);
}

/*----------------------------------------------------------------
* acx100_read_cmd_param
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
static inline void acx100_read_cmd_param(wlandevice_t *priv, memmap_t *cmd, int len)
{
	memcpy(cmd, (UINT *) priv->CommandParameters, len);
}

static const char * const cmd_error_strings[] = {
	"Idle[TIMEOUT]",
	"[SUCCESS]",
	"Unknown Command",
	"Invalid Information Element",
	"unknown_FIXME",
	"channel invalid in current regulatory domain",
	"unknown_FIXME2",
	"Command rejected (read-only information element)",
	"Command rejected",
	"Already asleep",
	"Tx in progress",
	"Already awake",
	"Write only",
	"Rx in progress",
	"Invalid parameter",
	"Scan in progress"
};

/*----------------------------------------------------------------
* acx100_issue_cmd
* Excecutes a command in the command mailbox 
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
int acx100_issue_cmd(wlandevice_t *priv, UINT cmd,
			/*@null@*/ void *pcmdparam, int paramlen, UINT32 timeout)
{
	int counter;
	int result = 0;
	UINT16 irqtype = 0;
	UINT16 cmd_status;

	FN_ENTER;
	acxlog(L_CTL, "%s cmd 0x%X timeout %ld.\n", __func__, cmd, timeout);

	if (!(priv->dev_state_mask & ACX_STATE_FW_LOADED))
	{
		acxlog(L_CTL, "firmware not loaded yet, cannot execute command!!\n");
		goto done;
	}
	
	if (cmd != ACX100_CMD_INTERROGATE) {
		acxlog(L_DEBUG,"input pdr (len=%d):\n",paramlen);
		acx100_dump_bytes(pcmdparam, paramlen);
	}

	/*** make sure we have at least *some* timeout value ***/
	if (timeout == 0) {
		timeout = 1;
	}

	/*** wait for ACX100 to become idle for our command submission ***/
	for (counter = 2000; counter > 0; counter--) {
		/* Auwtsh, busy-waiting!
		 * Hmm, yeah, but this function is probably supposed to be
		 * finished ASAP, so it's probably not such a terribly good
		 * idea to schedule away instead of busy-wait.
		 * Maybe let's just keep it as is. */
		acx100_get_cmd_state(priv);
		/* Test for IDLE state */
		if (!priv->cmd_status) 
			break;
	}

	/*** The card doesn't get IDLE, we're in trouble ***/
	if (counter == 0) {
		/* uh oh, cmd status still set, now we're in trouble */
		acxlog((L_BINDEBUG | L_CTL),
		       "Trying to issue a command to the ACX100 but the Command Register is not IDLE (%xh)\n", priv->cmd_status);
		goto done;
	}

	/*** now write the parameters of the command if needed ***/
	if (pcmdparam != NULL && paramlen != 0) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
		acx100_write_cmd_param(priv, pcmdparam,
			(cmd == ACX100_CMD_INTERROGATE) ? 0x4 : paramlen);
	}

	/*** now write the actual command type ***/
	priv->cmd_type = cmd;
	acx100_write_cmd_type(priv, cmd);
	
	/*** execute command ***/
	acx100_write_reg16(priv, priv->io[IO_ACX_INT_TRIG],
			  acx100_read_reg16(priv, priv->io[IO_ACX_INT_TRIG]) | 0x01);

	/*** wait for IRQ to occur, then ACK it? ***/
	if (timeout > 120000) {
		timeout = 120000;
	}
	timeout *= 20;
	if (timeout) {
		for (counter = timeout; counter > 0; counter--) {
			/* it's a busy wait loop, but we're supposed to be
			 * fast here, so better don't schedule away here?
			 * In theory, yes, but the timeout can be HUGE,
			 * so better schedule away sometimes */
			if ((irqtype =
			     acx100_read_reg16(priv, priv->io[IO_ACX_IRQ_STATUS_NON_DES])) & 0x200) {

				acx100_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], 0x200);
				break;
			} 

			if (counter % 30000 == 0)
			{
				acx100_schedule(HZ / 50);
			}
		}
	}

	/*** Save state for debugging ***/
	acx100_get_cmd_state(priv);
	cmd_status = priv->cmd_status;
	
	/*** Put the card in IDLE state ***/
	priv->cmd_status = 0;
	acx100_write_cmd_status(priv, 0);

	if ((irqtype | 0xfdff) == 0xfdff) {
		acxlog(L_CTL,
			"Polling for an IRQ failed with %X, cmd_status %d. Bailing.\n",
			irqtype, cmd_status);
		goto done;
	}

	if (1 != cmd_status) {
		acxlog(L_STD | L_CTL, "%s failed: %s [%ld uSec] Cmd: %Xh, Result: %Xh\n",
				__func__,
				cmd_status <= 0x0f ?
				cmd_error_strings[cmd_status] : "UNKNOWN REASON",
				(timeout - counter) * 50,
				cmd,
				cmd_status);
	} else	{
		/*** read in result parameters if needed ***/
		if (pcmdparam != NULL && paramlen != 0) {
			if (cmd == ACX100_CMD_INTERROGATE) {
				acx100_read_cmd_param(priv, pcmdparam, paramlen);
        			acxlog(L_DEBUG,"output pdr (len=%d):\n",paramlen);
        			acx100_dump_bytes(pcmdparam, paramlen);
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
	ACX100_RID_RATE_FALLBACK_LEN,
	ACX100_RID_WEP_OPTIONS_LEN,
	ACX100_RID_MEMORY_MAP_LEN, /*	ACX100_RID_SSID_LEN, */
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
	ACX100_RID_DOT11_WEP_DEFAULT_KEY_LEN, /* ACX100_RID_DOT11_WEP_KEY_LEN, */
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
* acx100_configure
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
int acx100_configure(wlandevice_t *priv, void *pdr, short type)
{
	((memmap_t *)pdr)->type = cpu_to_le16(type);
	if (type < 0x1000) {
		((memmap_t *)pdr)->length = cpu_to_le16(CtlLength[type]);
		return acx100_issue_cmd(priv, ACX100_CMD_CONFIGURE, pdr, 
			CtlLength[type] + 4, 5000);
	} else {
		((memmap_t *)pdr)->length = cpu_to_le16(CtlLengthDot11[type-0x1000]);
		return acx100_issue_cmd(priv, ACX100_CMD_CONFIGURE, pdr, 
			CtlLengthDot11[type-0x1000] + 4, 5000);
	}
}

/*----------------------------------------------------------------
* acx100_configure_length
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
inline int acx100_configure_length(wlandevice_t *priv, void *pdr, short type, short length)
{
	((memmap_t *)pdr)->type = cpu_to_le16(type);
	((memmap_t *)pdr)->length = cpu_to_le16(length);
	return acx100_issue_cmd(priv, ACX100_CMD_CONFIGURE, pdr, 
		length + 4, 5000);
}

/*----------------------------------------------------------------
* acx100_interrogate
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
int acx100_interrogate(wlandevice_t *priv, void *pdr, short type)
{
	((memmap_t *)pdr)->type = cpu_to_le16(type);
	if (type < 0x1000) {
		((memmap_t *)pdr)->length = cpu_to_le16(CtlLength[type]);
		return acx100_issue_cmd(priv, ACX100_CMD_INTERROGATE, pdr,
			CtlLength[type] + 4, 5000);
	} else {
		((memmap_t *)pdr)->length = cpu_to_le16(CtlLengthDot11[type-0x1000]);
		return acx100_issue_cmd(priv, ACX100_CMD_INTERROGATE, pdr,
			CtlLengthDot11[type-0x1000] + 4, 5000);
	}
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
inline int acx100_is_mac_address_zero(mac_t * mac)
{
	if ((mac->vala == 0) && (mac->valb == 0)) {
		return 1;
	}
	return 0;
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
inline int acx100_is_mac_address_equal(UINT8 * one, UINT8 * two)
{
	if (memcmp(one, two, ETH_ALEN))
		return 0; /* no match */
	else
		return 1; /* matched */
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
inline UINT8 acx100_is_mac_address_group(mac_t * mac)
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
inline UINT8 acx100_is_mac_address_directed(mac_t *mac)
{
	if (mac->vala & 1) {
		return 0;
	}
	return 1;
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
inline int acx100_is_mac_address_broadcast(const UINT8 * const address)
{
	unsigned char bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (memcmp(address, bcast_addr, ETH_ALEN) == 0)
		return 1;

	/* IPv6 broadcast address */
	if ((address[0] == 0x33) && (address[1] == 0x33))
		return 1;

	return 0;
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
inline int acx100_is_mac_address_multicast(mac_t * mac)
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
void acx100_log_mac_address(int level, UINT8 * mac)
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
* STATUS: NEW
*
* Comment:
*
*----------------------------------------------------------------*/
void acx100_power_led(wlandevice_t *priv, UINT8 enable)
{
	if (enable)
		acx100_write_reg16(priv, priv->io[IO_ACX_GPIO_OE], 
			acx100_read_reg16(priv, priv->io[IO_ACX_GPIO_OE]) & ~0x0800);
	else
		acx100_write_reg16(priv, priv->io[IO_ACX_GPIO_OE], 
			acx100_read_reg16(priv, priv->io[IO_ACX_GPIO_OE]) | 0x0800);
}


void acx100_dump_bytes(void *data,int num) {
  int i,remain=num;
  unsigned char *ptr=(unsigned char *)data;

  if (!(debug & L_DEBUG))
	  return;

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
