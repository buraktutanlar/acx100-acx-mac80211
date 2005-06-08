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

#if (WLAN_HOSTIF==WLAN_USB)
#include <linux/usb.h>
#define USB_CTRL_HARD_TIMEOUT 5500   /* steps in ms */
#endif

#include <linux/if_arp.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif

#include <linux/pci.h>

#include <linux/etherdevice.h>



/*================================================================*/
/* Project Includes */

#include <acx.h>

#if (WLAN_HOSTIF==WLAN_USB)
/* try to make it compile for both 2.4.x and 2.6.x kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

static inline int submit_urb(struct urb *urb, int mem_flags)
{
	return usb_submit_urb(urb, mem_flags);
}

static void acx100usb_control_complete(struct urb *urb)
{
	FN_ENTER;
	FN_EXIT0();
}

#else

/* 2.4.x kernels */
#define USB_24	1

static inline int submit_urb(struct urb *urb, int mem_flags)
{
	return usb_submit_urb(urb);
}

static void acx100usb_control_complete(struct urb *urb, struct pt_regs *regs)
{
	FN_ENTER;
	FN_EXIT0();
}

#endif

#define FILL_SETUP_PACKET(_pack,_rtype,_req,_val,_ind,_len) \
								(_pack)[0]=_rtype; \
								(_pack)[1]=_req; \
								((unsigned short *)(_pack))[1]=_val; \
								((unsigned short *)(_pack))[2]=_ind; \
								((unsigned short *)(_pack))[3]=_len;

#endif

/* FIXME: is 2.4.22 really the first version to do it properly? Didn't test it... */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 22)
#define dump_stack() /* <= 2.4.22 doesn't know about the generic dump_stack() */
#endif


/*****************************************************************************
 *
 * The Really Low Level Stuff (TM)
 *
 ****************************************************************************/

#ifndef INLINE_IO
/* Pull in definitions */
#define INLINE_IO /* defined to nothing. functions will be out-of-line */
#endif

/*****************************************************************************
 * 
 * Intermediate Level
 *
 ****************************************************************************/

#if (WLAN_HOSTIF!=WLAN_USB)
/* Info mailbox format:
2 bytes: type
2 bytes: status
more bytes may follow
    docs say about status:
	0x0000 info available (set by hw)
	0x0001 information received (must be set by host)
	0x1000 info available, mailbox overflowed (messages lost) (set by hw)
    but in practice we've seen:
	0x9000 when we did not set status to 0x0001 on prev message
	0x1001 when we did set it
	0x0000 was never seen
    conclusion: this is really a bitfield:
    0x1000 is 'info available' bit
    'mailbox overflowed' bit is 0x8000, not 0x1000
    value of 0x0000 probably means that there is no message at all
    P.S. I dunno how in hell hw is supposed to notice that messages are lost -
    it does NOT clear bit 0x0001, and this bit will probably stay forever set
    after we set it once. Let's hope this will be fixed in firmware someday
*/

void acx_get_info_state(wlandevice_t *priv)
{
	u32 value;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1);

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR,
		acx_read_reg32(priv, IO_ACX_INFO_MAILBOX_OFFS));

	acx_write_flush(priv); /* make sure we only read the data once all cfg registers are written */
	value = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

	priv->info_type = (u16)value;
	priv->info_status = (value >> 16);

	/* inform hw that we have read this info message */
	acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, priv->info_type | 0x00010000);
	wmb(); /* make sure the flag is already written to memory *before* the ACK */
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_INFOACK); /* now bother hw to notice it */
	acx_write_flush(priv);

	acxlog(L_CTL, "info_type 0x%04x, info_status 0x%04x\n", priv->info_type, priv->info_status);
}

static const char * const cmd_error_strings[] = {
	"Idle",
	"Success",
	"Unknown Command",
	"Invalid Information Element",
	"channel rejected",
	"channel invalid in current regulatory domain",
	"MAC invalid",
	"Command rejected (read-only information element)",
	"Command rejected",
	"Already asleep",
	"Tx in progress",
	"Already awake",
	"Write only",
	"Rx in progress",
	"Invalid parameter",
	"Scan in progress",
	"failed"
};

/*----------------------------------------------------------------
* acx_get_cmd_state
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
void acx_get_cmd_state(wlandevice_t *priv)
{
	u32 value;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1); /* FIXME: why auto increment?? */

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, 
		acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS));

	/* make sure we only read the data once all config registers are written */
	acx_write_flush(priv);
	value = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

	priv->cmd_type = (u16)value;
	priv->cmd_status = (value >> 16);

	acxlog(L_CTL, "cmd_type 0x%04x, cmd_status 0x%04x [%s]\n", priv->cmd_type, priv->cmd_status, priv->cmd_status <= 0x10 ? cmd_error_strings[priv->cmd_status] : "UNKNOWN REASON" );
}

/*----------------------------------------------------------------
* acx_write_cmd_type_or_status
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
void acx_write_cmd_type_or_status(wlandevice_t *priv, u32 val, unsigned int is_status)
{
	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1); /* FIXME: why auto increment?? */

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, 
		acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS));

	if (is_status)
		val <<= 16;
	/* make sure we only write the data once all config registers are written */
	acx_write_flush(priv);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, val);
	acx_write_flush(priv);
}
#endif /* WLAN_HOSTIF!=WLAN_USB */

/*----------------------------------------------------------------
* acx_write_cmd_param
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
static inline void acx_write_cmd_param(wlandevice_t *priv, const acx_ie_generic_t *cmd, unsigned int len)
{
	memcpy(priv->CommandParameters, cmd, len);
}

/*----------------------------------------------------------------
* acx_read_cmd_param
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
static inline void acx_read_cmd_param(const wlandevice_t *priv, acx_ie_generic_t *cmd, unsigned int len)
{
	memcpy(cmd, priv->CommandParameters, len);
}

/*----------------------------------------------------------------
* acx_issue_cmd
* Excecutes a command in the command mailbox 
*
* Arguments:
*   *pcmdparam = an pointer to the data. The data mustn't include
*                the 4 byte command header!
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
#if (WLAN_HOSTIF!=WLAN_USB)
int acx_issue_cmd(wlandevice_t *priv, unsigned int cmd,
			/*@null@*/ void *pcmdparam, unsigned int paramlen, u32 timeout)
{
	unsigned int counter;
	int result = NOT_OK;
	u16 irqtype = 0;
	u16 cmd_status;

	FN_ENTER;
	acxlog(L_CTL, "%s cmd 0x%X timeout %u.\n", __func__, cmd, timeout);

	if (!(priv->dev_state_mask & ACX_STATE_FW_LOADED)) {
		acxlog(L_CTL, "firmware not loaded yet, cannot execute command!!\n");
		goto done;
	}
	
	if ((debug & L_DEBUG) && (cmd != ACX1xx_CMD_INTERROGATE)) {
		acxlog(L_DEBUG,"input pdr (len=%u):\n",paramlen);
		acx_dump_bytes(pcmdparam, paramlen);
	}

	/*** wait for ACX100 to become idle for our command submission ***/
	for (counter = 2000; counter; counter--) {
		/* Auwtsh, busy-waiting!
		 * Hmm, yeah, but this function is probably supposed to be
		 * finished ASAP, so it's probably not such a terribly good
		 * idea to schedule away instead of busy-wait.
		 * Maybe let's just keep it as is. */
		acx_get_cmd_state(priv);
		/* Test for IDLE state */
		if (!priv->cmd_status) 
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) /* FIXME: exact first version? */
		cond_resched();
#endif
	}

	/*** The card doesn't get IDLE, we're in trouble ***/
	if (counter == 0) {
		/* uh oh, cmd status still set, now we're in trouble */
		acxlog((L_BINDEBUG | L_CTL),
		       "Trying to issue a command to the ACX100 but the Command Register is not IDLE (%xh)\n", priv->cmd_status);
		goto done;
	}

	if(priv->irq_status & HOST_INT_CMD_COMPLETE) {
		acxlog((L_BINDEBUG | L_CTL), "irq status var is not empty: 0x%X\n", priv->irq_status);
		/* goto done; */
		priv->irq_status ^= HOST_INT_CMD_COMPLETE;
		acxlog((L_BINDEBUG | L_CTL), "irq status fixed to: 0x%X\n", priv->irq_status);
	}

	/*** now write the parameters of the command if needed ***/
	if (pcmdparam != NULL && paramlen != 0) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
		acx_write_cmd_param(priv, pcmdparam,
			(cmd == ACX1xx_CMD_INTERROGATE) ? 0x4 : paramlen);
	}

	/*** now write the actual command type ***/
	priv->cmd_type = cmd;
	acx_write_cmd_type_or_status(priv, cmd, 0);
	
	/*** execute command ***/
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_CMD);
	acx_write_flush(priv);

	/*** wait for IRQ to occur, then ACK it ***/

	/*** make sure we have at least *some* timeout value ***/
	if (unlikely(timeout == 0)) {
		timeout = 1;
	} else if (unlikely(timeout > 120000)) {
		timeout = 120000;
	}
	timeout *= 20;
	for (counter = 0; counter < timeout; counter++) {
		/* it's a busy wait loop, but we're supposed to be
		 * fast here, so better don't schedule away here?
		 * In theory, yes, but the timeout can be HUGE,
		 * so better schedule away sometimes */
		if (!priv->irqs_active) {
			rmb();
			irqtype = acx_read_reg16(priv, IO_ACX_IRQ_STATUS_NON_DES);
			if (irqtype & HOST_INT_CMD_COMPLETE) {
				acx_write_reg16(priv, IO_ACX_IRQ_ACK, HOST_INT_CMD_COMPLETE);
				break;
			}
		} 
		if (priv->irqs_active) {
			irqtype = priv->irq_status & HOST_INT_CMD_COMPLETE;
			if (irqtype) {
				priv->irq_status ^= HOST_INT_CMD_COMPLETE;
				break;
			}
		}

		/* reschedule after some time has been spent in this loop */
		if (unlikely(counter % 16384 == 8192))
		{
			acx_schedule(HZ / 100);
		}
	}

	/*** Save state for debugging ***/
	acx_get_cmd_state(priv);
	cmd_status = priv->cmd_status;
	
	/*** Put the card in IDLE state ***/
	priv->cmd_status = 0;
	acx_write_cmd_type_or_status(priv, 0, 1);

	if (!(irqtype & HOST_INT_CMD_COMPLETE)) {
		acxlog(0xffff,
			"Polling for an IRQ FAILED with %X, cmd_status %u, irqs_active %u, irq_status %X. Bailing.\n",
			irqtype, cmd_status, priv->irqs_active, priv->irq_status);
		if (debug & 0xffff)
			dump_stack();
		goto done;
	}

	if (1 != cmd_status) {
		acxlog(L_STD | L_CTL, "%s FAILED: %s [%d uSec] Cmd: %Xh, Result: %Xh\n",
				__func__,
				cmd_status <= 0x10 ?
				cmd_error_strings[cmd_status] : "UNKNOWN REASON",
				(timeout - counter) * 50,
				cmd,
				cmd_status);
		if (debug & L_CTL)
			dump_stack();

		/* zero out result buffer */
		if (pcmdparam != NULL && paramlen != 0) {
			memset(pcmdparam, 0, paramlen);
		}
	} else	{
		/*** read in result parameters if needed ***/
		if (pcmdparam != NULL && paramlen != 0) {
			if (cmd == ACX1xx_CMD_INTERROGATE) {
				acx_read_cmd_param(priv, pcmdparam, paramlen);
				if (debug & L_DEBUG) {
        				acxlog(L_DEBUG,"output pdr (len=%u):\n",paramlen);
        				acx_dump_bytes(pcmdparam, paramlen);
				}
			}
		}
		result = OK;
	}

done:
	FN_EXIT1(result);
	return result;
}
#else
int acx_issue_cmd(wlandevice_t *priv,unsigned int cmd,void *pdr,unsigned int paramlen,u32 timeout)
{
	int result,skipridheader,blocklen,inpipe,outpipe,acklen=sizeof(priv->ctrlin);
	int ucode,delcount;
	struct usb_device *usbdev;

	FN_ENTER;
	acxlog(L_CTL, "%s cmd 0x%X timeout %u.\n", __func__, cmd, timeout);
	acxlog(L_CTL,"paramlen=%u type=%d\n",paramlen,(pdr)?le16_to_cpu(((acx_ie_generic_t *)pdr)->type):-1);
	skipridheader=0;
	/* ----------------------------------------------------
	** get context from wlandevice
	** ------------------------------------------------- */
	usbdev=priv->usbdev;
	/* ----------------------------------------------------
	** check which kind of command was issued...
	** ------------------------------------------------- */
	priv->ctrlout.cmd=cpu_to_le16(cmd);
	priv->ctrlout.status=cpu_to_le16(0);
	if (cmd==ACX1xx_CMD_INTERROGATE) {
		/* -----------------------------------------------------
		** setup interrogation command...
		** -------------------------------------------------- */
		priv->ctrlout.u.rridreq.rid=((acx_ie_generic_t *)pdr)->type;
		priv->ctrlout.u.rridreq.frmlen=cpu_to_le16(paramlen-4);  /* -4 bytes because we do not need the USB header in the frame length */
		blocklen=8;
		switch (le16_to_cpu(priv->ctrlout.u.rridreq.rid)) {
			case ACX1xx_IE_SCAN_STATUS:skipridheader=1;break;
		}
		if (skipridheader) acklen=paramlen;
		else acklen=4+paramlen; /* acklen -> expected length of ACK from USB device */
		acxlog(L_CTL,"sending interrogate: cmd=%u status=%u rid=%u frmlen=%u\n",le16_to_cpu(priv->ctrlout.cmd),le16_to_cpu(priv->ctrlout.status),le16_to_cpu(priv->ctrlout.u.rridreq.rid),le16_to_cpu(priv->ctrlout.u.rridreq.frmlen));
	} else if (cmd==ACX1xx_CMD_CONFIGURE) {
		/* -------------------------------------------------
		** setup configure command...
		** --------------------------------------------- */
		priv->ctrlout.u.wridreq.rid=((acx_ie_generic_t *)pdr)->type;
		priv->ctrlout.u.wridreq.frmlen=cpu_to_le16(paramlen);
		memcpy(priv->ctrlout.u.wridreq.data,&(((acx_ie_generic_t *)pdr)->m),paramlen);
		blocklen=paramlen+8;	/* length of parameters + header */
	} else if ((cmd==ACX1xx_CMD_ENABLE_RX)||(cmd==ACX1xx_CMD_ENABLE_TX)||(cmd==ACX1xx_CMD_SLEEP)) {
		priv->ctrlout.u.rxtx.data=1;		/* just for testing */
		blocklen=5;
	} else {
		/* ----------------------------------------------------
		** All other commands (not thoroughly tested)
		** ------------------------------------------------- */
		if ((pdr)&&(paramlen>0)) memcpy(priv->ctrlout.u.wmemreq.data,pdr,paramlen);
		blocklen=paramlen+4;
	}
	/* ----------------------------------------------------
	** Obtain the I/O pipes
	** ------------------------------------------------- */
	outpipe=usb_sndctrlpipe(usbdev,0);
	inpipe =usb_rcvctrlpipe(usbdev,0);
	acxlog(L_CTL,"ctrl inpipe=0x%X outpipe=0x%X\n",inpipe,outpipe);
#if ACX_DEBUG
	acxlog(L_CTL,"sending USB control msg (out) (blocklen=%i)\n",blocklen);
	if (debug&L_DATA) acx_dump_bytes(&(priv->ctrlout),blocklen);
#endif
	/* --------------------------------------
	** fill setup packet and control urb
	** ----------------------------------- */
	FILL_SETUP_PACKET(priv->usb_setup,USB_TYPE_VENDOR|USB_DIR_OUT,ACX100_USB_UNKNOWN_REQ1,0,0,blocklen)
	usb_fill_control_urb(priv->ctrl_urb,usbdev,outpipe,priv->usb_setup,&(priv->ctrlout),blocklen,(usb_complete_t)acx100usb_control_complete,priv);
	/* 2.6.9-rc1: "USB: Remove struct urb->timeout as it does not work" */
	/* priv->ctrl_urb->timeout=timeout; */
	ucode=submit_urb(priv->ctrl_urb, GFP_KERNEL);
	if (ucode!=0) {
		acxlog(L_STD,"WARNING: CTRL MESSAGE FAILED WITH ERRCODE %i\n",ucode);
		FN_EXIT0();
		return NOT_OK;
	}
	/* ---------------------------------
	** wait for request to complete...
	** ------------------------------ */
	delcount=0;
	while (priv->ctrl_urb->status==-EINPROGRESS) {
		udelay(1000);
		delcount++;
		if (unlikely(delcount>USB_CTRL_HARD_TIMEOUT)) {
			acxlog(L_STD,"ERROR, USB device is not responding!\n");
			FN_EXIT0();
			return NOT_OK;
		}
	}
	/* ---------------------------------
	** check the result
	** ------------------------------ */
	result=priv->ctrl_urb->actual_length;
	acxlog(L_CTL,"wrote=%i bytes (status=%u)\n",result,priv->ctrl_urb->status);
	if (result<0) {
		FN_EXIT0();
		return NOT_OK;
	}
	/* --------------------------------------
	** Check for device acknowledge ...
	** -------------------------------------- */
	acxlog(L_CTL,"sending USB control msg (in) (acklen=%i) sizeof(acx100_usbin_t)=%d\n",acklen,sizeof(acx100_usbin_t));
	priv->ctrlin.status=0; /* delete old status flag -> set to fail */
	FILL_SETUP_PACKET(priv->usb_setup,USB_TYPE_VENDOR|USB_DIR_IN,ACX100_USB_UNKNOWN_REQ1,0,0,acklen)
	usb_fill_control_urb(priv->ctrl_urb,usbdev,inpipe,priv->usb_setup,&(priv->ctrlin),acklen,(usb_complete_t)acx100usb_control_complete,priv);
	/* 2.6.9-rc1: "USB: Remove struct urb->timeout as it does not work" */
	/* priv->ctrl_urb->timeout=timeout; */
	ucode=submit_urb(priv->ctrl_urb, GFP_KERNEL);
	if (ucode!=0) {
		acxlog(L_STD,"ctrl message (ack) FAILED with errcode %i\n",ucode);
		FN_EXIT0();
		return NOT_OK;
	}
	/* ---------------------------------
	** wait for request to complete...
	** ------------------------------ */
	delcount=0;
	while (priv->ctrl_urb->status==-EINPROGRESS) {
		udelay(1000);
		delcount++;
		if (unlikely(delcount>USB_CTRL_HARD_TIMEOUT)) {
			acxlog(L_STD,"ERROR, USB device is not responsive!\n");
			FN_EXIT0();
			return NOT_OK;
		}
	}
	/* ---------------------------------
	** check the result
	** ------------------------------ */
	result=priv->ctrl_urb->actual_length;
	acxlog(L_CTL,"read=%i bytes\n",result);
	if (result < 0) {
		FN_EXIT0();
		return NOT_OK;
	}
	if (le16_to_cpu(priv->ctrlin.status)!=1) {
		acxlog(L_DEBUG,"WARNING: COMMAND RETURNED STATUS %u\n",le16_to_cpu(priv->ctrlin.status));
	}
	if (cmd==ACX1xx_CMD_INTERROGATE) {
		if ((pdr)&&(paramlen>0)) {
			if (skipridheader) {
				memcpy(pdr,&(priv->ctrlin.u.rmemresp.data),paramlen-4);
				acxlog(L_CTL,"response frame: cmd=%u status=%u\n",le16_to_cpu(priv->ctrlin.cmd),le16_to_cpu(priv->ctrlin.status));
				acxlog(L_DATA,"incoming bytes (%d):\n",paramlen-4);
				if (debug&L_DATA) 
				    acx_dump_bytes(pdr,paramlen-4);
			}
			else {
				memcpy(pdr,&(priv->ctrlin.u.rridresp.rid),paramlen);
				acxlog(L_CTL,"response frame: cmd=%u status=%u rid=%u frmlen=%u\n",le16_to_cpu(priv->ctrlin.cmd),le16_to_cpu(priv->ctrlin.status),le16_to_cpu(priv->ctrlin.u.rridresp.rid),le16_to_cpu(priv->ctrlin.u.rridresp.frmlen));
				acxlog(L_DATA,"incoming bytes (%u):\n",paramlen);
				if (debug&L_DATA)
				    acx_dump_bytes(pdr,paramlen);
			}
		}
	}
	FN_EXIT0();
	return OK;
}
#endif


/*****************************************************************************
 *
 * The Upper Layer
 *
 ****************************************************************************/

static const u16 CtlLength[] = {
	0,
	ACX100_IE_ACX_TIMER_LEN,
	ACX1xx_IE_POWER_MGMT_LEN,
	ACX1xx_IE_QUEUE_CONFIG_LEN,
	ACX100_IE_BLOCK_SIZE_LEN,
	ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_RATE_FALLBACK_LEN,
	ACX100_IE_WEP_OPTIONS_LEN,
	ACX1xx_IE_MEMORY_MAP_LEN, /*	ACX1xx_IE_SSID_LEN, */
	0,
	ACX1xx_IE_ASSOC_ID_LEN,
	0,
	ACX1xx_IE_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_FWREV_LEN,
	ACX1xx_IE_FCS_ERROR_COUNT_LEN,
	ACX1xx_IE_MEDIUM_USAGE_LEN,
	ACX1xx_IE_RXCONFIG_LEN,
	0,
	0,
	ACX1xx_IE_FIRMWARE_STATISTICS_LEN,
	0,
	ACX1xx_IE_FEATURE_CONFIG_LEN,
	ACX111_IE_KEY_CHOOSE_LEN,
};

static const u16 CtlLengthDot11[] = {
	0,
	ACX1xx_IE_DOT11_STATION_ID_LEN,
	0,
	ACX100_IE_DOT11_BEACON_PERIOD_LEN,
	ACX1xx_IE_DOT11_DTIM_PERIOD_LEN,
	ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_IE_DOT11_WEP_DEFAULT_KEY_LEN,
	ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN,
	ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_IE_DOT11_ED_THRESHOLD_LEN,
	ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0,
};

/*----------------------------------------------------------------
* acx_configure
*
*
* Arguments:
*
* Returns:
*	1 = success
*	0 = failure
* Side effects:
*
* Call context:
*
* STATUS: FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/
int acx_configure(wlandevice_t *priv, void *pdr, short type)
{
	u16 len, offs;

	/* TODO implement and check other acx111 commands */
	if ((priv->chip_type == CHIPTYPE_ACX111) &&
		(type == ACX1xx_IE_DOT11_CURRENT_ANTENNA)) {
		/* acx111 has differing struct size */
		acxlog(L_CTL, "Configure Command 0x%02X not supported under acx111 (yet)\n", type);

		return NOT_OK;
	}

	if (type<0x1000)
		len=CtlLength[type];
	else
		len=CtlLengthDot11[type-0x1000];

	if (unlikely(len==0)) {
		acxlog(L_DEBUG,"WARNING: ENCOUNTERED ZEROLENGTH TYPE (%x)\n",type);
	}
	acxlog(L_XFER,"configuring: type=0x%X len=%u\n",type,len);
	
	((acx_ie_generic_t *)pdr)->type = cpu_to_le16(type);
#if (WLAN_HOSTIF==WLAN_USB)
	((acx_ie_generic_t *)pdr)->len = 0; /* FIXME: is that correct? */
	offs = 0; /* FIXME: really?? */
#else
	((acx_ie_generic_t *)pdr)->len = cpu_to_le16(len);
	offs = 4;
#endif
	return acx_issue_cmd(priv, ACX1xx_CMD_CONFIGURE, pdr, len + offs, ACX_CMD_TIMEOUT_DEFAULT);
}

/*----------------------------------------------------------------
* acx_configure_length
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
inline int acx_configure_length(wlandevice_t *priv, void *pdr, short type, short len)
{
	((acx_ie_generic_t *)pdr)->type = cpu_to_le16(type);
#if (WLAN_HOSTIF==WLAN_USB)
	((acx_ie_generic_t *)pdr)->len = 0; /* FIXME: is that correct? */
#else
	((acx_ie_generic_t *)pdr)->len = cpu_to_le16(len);
#endif
	return acx_issue_cmd(priv, ACX1xx_CMD_CONFIGURE, pdr, 
		len + 4, ACX_CMD_TIMEOUT_DEFAULT);
}

/*----------------------------------------------------------------
* acx_interrogate
*
*
* Arguments:
*
* Returns:
*	OK = success
*	NOT_OK = failure
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
int acx_interrogate(wlandevice_t *priv, void *pdr, short type)
{
	u16 len;

	if (type<0x1000)
		len=CtlLength[type];
	else
		len=CtlLengthDot11[type-0x1000];

	((acx_ie_generic_t *)pdr)->type = cpu_to_le16(type);
#if (WLAN_HOSTIF==WLAN_USB)
	((acx_ie_generic_t *)pdr)->len = 0; /* FIXME: is that correct? */
#else
	((acx_ie_generic_t *)pdr)->len = cpu_to_le16(len);
#endif

	acxlog(L_CTL,"interrogating: type=0x%X len=%u\n",type,len);

	return acx_issue_cmd(priv, ACX1xx_CMD_INTERROGATE, pdr,
		len + 4, ACX_CMD_TIMEOUT_DEFAULT);
}


/*****************************************************************************
 * 
 * MAC Address Stuff
 *
 ****************************************************************************/

/*----------------------------------------------------------------
* acx_power_led
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
void acx_power_led(wlandevice_t *priv, u8 enable)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	u16 gpio_pled =
		(CHIPTYPE_ACX111 == priv->chip_type) ? 0x0040 : 0x0800;
	static int rate_limit = 0;

	if (rate_limit++ < 3)
		acxlog(L_IOCTL, "Please report in case toggling the power LED doesn't work for your card!\n");
	if (enable)
		acx_write_reg16(priv, IO_ACX_GPIO_OUT, 
			acx_read_reg16(priv, IO_ACX_GPIO_OUT) & ~gpio_pled);
	else
		acx_write_reg16(priv, IO_ACX_GPIO_OUT, 
			acx_read_reg16(priv, IO_ACX_GPIO_OUT) | gpio_pled);
#else
	acxlog(L_IOCTL, "no power LED support on ACX100 USB (yet)!\n");
#endif
}

