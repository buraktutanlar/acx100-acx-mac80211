/* src/idma.c - low level rx and tx management
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
#include <monitor.h>

#define RXBUFFERNO 32
#define TXBUFFERNO 32
#define MINFREE_TX 3
spinlock_t tx_lock;
spinlock_t rx_lock;

/* HandleInfoInterrupt()
 * STATUS: FINISHED.
 */
void HandleInfoInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleDTIMInterrupt()
 * STATUS: FINISHED.
 */
void HandleDTIMInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleBeaconInterrupt()
 * STATUS: FINISHED.
 */
void HandleBeaconInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleTickInterrupt()
 * STATUS: FINISHED.
 */
void HandleTickInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleKeyNotFoundInterrupt()
 * STATUS: FINISHED.
 */
void HandleKeyNotFoundInterrupt(wlandevice_t * hw)
{
	FN_ENTER;

	/* hw->val0x1204++; Unused, so removed */
	hw->stats.rx_errors++;

	FN_EXIT(0, 0);
}

/* HandleIvIcvFailureInterrupt()
 * STATUS: FINISHED.
 */
void HandleIvIcvFailureInterrupt(wlandevice_t * hw)
{
	FN_ENTER;

	hw->stats.rx_crc_errors++;
	hw->stats.rx_errors++;

	FN_EXIT(0, 0);
}

/* HandleCommandCompleteInterrupt()
 * STATUS: FINISHED.
 */
void HandleCommandCompleteInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleOverflowInterrupt()
 * STATUS: FINISHED.
 */
void HandleOverflowInterrupt(wlandevice_t * hw)
{
	FN_ENTER;

	hw->overflow_errors++;
	hw->stats.rx_over_errors++;

	FN_EXIT(0, 0);
}

/* HandleProcessErrorInterrupt()
 * STATUS: FINISHED.
 */
void HandleProcessErrorInterrupt(void)
{
	FN_ENTER;
	FN_EXIT(0, 0);
}

/* HandleFCSThresholdInterrupt()
 * 3D
 * STATUS: FINISHED.
 */
void HandleFCSThresholdInterrupt(wlandevice_t * hw)
{
	FN_ENTER;

	/* hw->val0x1180 += hw->val0x2408; Unused, so removed */

	FN_EXIT(0, 0);
}


/*----------------------------------------------------------------
* hwEnableISR
* FIXME: rename to acx100_enable_irq
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
/* hwEnableISR()
 * STATUS: ok.
 */
void hwEnableISR(wlandevice_t * hw)
{
	/* if (hw->val0x240c != 0) { */
		hwWriteRegister16(hw, ACX100_IRQ_MASK, hw->irq_mask);
	/* } */
	hwWriteRegister16(hw, ACX100_IRQ_34, 0x8000);
}

/*----------------------------------------------------------------
* hwDisableISR
* FIXME: rename to acx100_disable_irq
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

/* hwDisableISR()
 * STATUS: ok.
 */
void hwDisableISR(wlandevice_t * hw)
{
	/* if (hw->val0x240c != 0) { */
		hwWriteRegister16(hw, ACX100_IRQ_MASK, 0x7fff);
	/* } */
	hwWriteRegister16(hw, ACX100_IRQ_34, 0x0);
}

/*----------------------------------------------------------------
* dmaCreateDC
* FIXME: rename to acx100_create_dma_regions
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

/* dmaCreateDC()
 * STATUS: should be ok. UNVERIFIED
 *  This is the V3 version.
 */
int dmaCreateDC(wlandevice_t * hw)
{
	QueueConfig_t CW;
	struct {
		UINT16 val0x0[0x1c / 2];
		UINT val0x1c;
		UINT val0x20;
		UINT val0x24;
		UINT val0x28;
		UINT val0x2c;
	} MemMap;
	struct TIWLAN_DC *tdc;

	acxlog(L_FUNC, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;

	hw->val0x24e0 = TXBUFFERNO*2; /* 0x40 */
	hw->RxHostDescNo = RXBUFFERNO;
	spin_lock_init(&rx_lock);
	spin_lock_init(&tx_lock);
	tdc = &hw->dc;

#if BOGUS
	/* if we base TIWLAN_DC on wlandevice_t memory, then surely we do have
	 * memory?? ;-) */
	if (!tdc) {
		acxlog(L_BINSTD,
		       "Failed to allocate memory for struct TIWLAN_DC\n");

		return 2;
	}
#endif

	tdc->wldev = hw;
	/* which memmap is read here, acxInitMemoryPools did not get called yet ? */
	if (!ctlInterrogate(hw, &MemMap, ACX100_RID_MEMORY_MAP)) {
		acxlog(L_BINSTD, "ctlMemoryMapRead returns error\n");
		tdc->val0x7c = 1;
		return 2;
	} else {
		/* Nr of items in Rx and Tx queues */
		hw->TxQueueNo = TXBUFFERNO;
		hw->RxQueueNo = RXBUFFERNO;

		CW.AreaSize = ( sizeof(struct txdescriptor) * TXBUFFERNO +
		                sizeof(struct rxdescriptor) * RXBUFFERNO + 0x08);
				//area size
		CW.vale = 1;

		tdc->ui32ACXTxQueueStart = MemMap.val0x1c;
		/* sets the beginning of the tx descriptor queue */
		CW.TxQueueStart = MemMap.val0x1c;
		CW.valj = 0;

		tdc->ui32ACXRxQueueStart =
		    hw->TxQueueNo *
		    sizeof(struct txdescriptor) /* 0x30 */ +
		    MemMap.val0x1c;
		/* sets the beginning of the rx descriptor queue */
		CW.RxQueueStart = tdc->ui32ACXRxQueueStart;
		CW.vald = 1;

		/* sets the end of the rx descriptor queue */
		CW.QueueEnd = hw->RxQueueNo * sizeof(struct rxdescriptor) /* 0x34 */ +
		    tdc->ui32ACXRxQueueStart;
		/* sets the beginning of the next queue */
		CW.QueueEnd2 = CW.QueueEnd + 8;

		hw->val0x244c = (uint *)((UINT32)hw->pvMemBaseAddr2 + (UINT32)CW.QueueEnd );
		hw->val0x2450 = (uint *)((UINT32)hw->pvMemBaseAddr2 + (UINT32)CW.QueueEnd2 + 8);

		hw->val0x244c[0] = 0;
		hw->val0x244c[1] = 0;
		hw->val0x2450[0] = 0;
		hw->val0x2450[1] = 0;

		acxlog(L_BINDEBUG, "<== Initialize the Queue Indicator\n");

		if (!ctlConfigureLength(hw, &CW, ACX100_RID_QUEUE_CONFIG, 0x14 + (CW.vale * 0x08))) {
			acxlog(L_BINSTD,
			       "ctlQueueConfigurationWrite returns error\n");

			tdc->val0x7c = 1;
			return 1;
		}
		if (dmaCreateTxHostDescQ(tdc)) {
			acxlog(L_BINSTD,
			       "dmaCreateTxHostDescQ returns error\n");

			tdc->val0x7c = 1;
			return 2;
		}
		if (dmaCreateTxDescQ(tdc)) {
			acxlog(L_BINSTD,
			       "dmaCreateTxDescQ returns error\n");

			dmaFreeTxHostDescQ(tdc);

			tdc->val0x7c = 1;
			return 2;
		}
		if (dmaCreateRxHostDescQ(tdc)) {
			acxlog(L_BINSTD,
			       "dmaCreateRxHostDescQ returns error\n");

			dmaFreeTxHostDescQ(tdc);
			dmaFreeTxDescQ(tdc);

			tdc->val0x7c = 1;
			return 2;
		}
		if (dmaCreateRxDescQ(tdc)) {
			acxlog(L_BINSTD,
			       "dmaCreateRxDescQ returns error\n");

			dmaFreeRxHostDescQ(tdc);
			dmaFreeTxHostDescQ(tdc);
			dmaFreeTxDescQ(tdc);

			tdc->val0x7c = 1;
			return 2;
		}
		if (!ctlInterrogate(hw, &MemMap, ACX100_RID_MEMORY_MAP)) {
			acxlog(L_BINSTD, "Failed to read memory map\n");

			dmaFreeRxHostDescQ(tdc);
			dmaFreeRxDescQ(tdc);
			dmaFreeTxHostDescQ(tdc);
			dmaFreeTxDescQ(tdc);

			tdc->val0x7c = 1;
			return 1;
		}
		/* start at least 4 bytes away from the end of the last pool */
		MemMap.val0x24 = (MemMap.val0x20 + 0x1F + 4) & 0xffffffe0;

		if (!ctlConfigure(hw, &MemMap, ACX100_RID_MEMORY_MAP)) {
			acxlog(L_BINSTD,
			       "ctlMemoryMapWrite returns error\n");

			dmaFreeRxHostDescQ(tdc);
			dmaFreeRxDescQ(tdc);
			dmaFreeTxHostDescQ(tdc);
			dmaFreeTxDescQ(tdc);

			tdc->val0x7c = 1;
			return 1;
		}
		if (!acxInitMemoryPools(hw, (memmap_t *) &MemMap)) {
			acxlog(L_BINSTD,
			       "acxInitMemoryPools returns error\n");

			dmaFreeRxHostDescQ(tdc);
			dmaFreeRxDescQ(tdc);
			dmaFreeTxHostDescQ(tdc);
			dmaFreeTxDescQ(tdc);

			tdc->val0x7c = 1;
			return 1;
		}

		acx100_set_defaults(hw);

		FN_EXIT(0, 0);

		return 0;
	}
}
/*----------------------------------------------------------------
* dmaDeleteDC
* FIXME: rename to acx100_delete_dma_region
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

/* dmaDeleteDC()
 * STATUS: ok
 */
int dmaDeleteDC(wlandevice_t * hw)
{
	FN_ENTER;

	hw->dc.val0x7c = 1;
	hwWriteRegister16(hw, ACX100_REG_104, 0);

	/* used to be a for loop 1000, do scheduled delay instead */
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ / 10);

	dmaFreeTxHostDescQ(&hw->dc);
	dmaFreeTxDescQ(&hw->dc);
	dmaFreeRxDescQ(&hw->dc);
	dmaFreeRxHostDescQ(&hw->dc);

	FN_EXIT(0, 0);
	return 0;
}
/*----------------------------------------------------------------
* dma_tx_data
* FIXME: rename to acx100_dma_tx_data
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

void dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *tx_desc)
{
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;
	int flags;
	int i;

	header = tx_desc->val0x1c;
	payload = tx_desc->val0x1c+1;
	tx_desc->val0xc = jiffies;

	tx_desc->Ctl |= wlandev->preamble_flag; /* set Preamble */
	/* It seems as if the Preamble setting was actually REVERSED:
	 * bit 0 should most likely actually be ACTIVATED
	 * for Short Preamble, not the other way around as before!
	 * This caused many Tx error 0x20 errors with APs
	 * that don't support Long Preamble, since we were
	 * thinking we are setting Long Preamble, when in fact
	 * it was Short Preamble.
	 * The flag reversal theory has been sort of confirmed
	 * by throughput measurements:
	 * ~ 680K/s with flag disabled
	 * ~ 760K/s with flag enabled
	 */

	/* set autodma and reclaim and 1st mpdu */
	tx_desc->Ctl |= 0x4 | 0x8 | 0x2;

	/* set rate */
	if (WLAN_GET_FC_FTYPE(((p80211_hdr_t*)header->data)->a3.fc) ==
	    WLAN_FTYPE_MGMT) {
		/* 2Mbps for MGMT pkt compatibility */
		tx_desc->rate = 20;
	} else {
		tx_desc->rate = wlandev->bitrateval;
	}

	/* sets Ctl 0x80 to zero telling that the descriptor is now owned by the acx100 */


	acxlog(L_XFER | L_DATA,
		"Tx packet (%s): len %i, hdr_len %i, pyld_len %i, mode %lX, iStatus %X\n",
		GetPacketTypeString(((p80211_hdr_t*)header->data)->a3.fc),
		tx_desc->total_length,
		header->length,
		payload->length,
		wlandev->mode,
		wlandev->iStatus);

	acxlog(L_DATA, "802.11 header[%d]: ", header->length);
	for (i = 0; i < header->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) header->data)[i]);
	acxlog(L_DATA, "\n");
	acxlog(L_DATA, "802.11 payload[%d]: ", payload->length);
	for (i = 0; i < payload->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) payload->data)[i]);
	acxlog(L_DATA, "\n");

	/* what is this ? */
	spin_lock_irqsave(&tx_lock, flags);
	header->Ctl &= (UINT16) ~0x80;
	payload->Ctl &= (UINT16) ~0x80;
	tx_desc->Ctl &= (UINT16) ~0x80;

	wlandev->val0x244c[0] = 0x1;
	hwWriteRegister16(wlandev, ACX100_REG_7C, 0x4);
	spin_unlock_irqrestore(&tx_lock, flags);
}
/*----------------------------------------------------------------
* log_txbuffer
* FIXME: rename to acx100_log_txbuffer
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

void log_txbuffer(TIWLAN_DC *pDc)
{
	int i;
	txdesc_t *pTxDesc;

	for (i=0; i < pDc->pool_count; i++)
	{
		pTxDesc = &pDc->pTxDescQPool[i];

		if ((pTxDesc->Ctl & 0xc0) == 0xc0)
			acxlog(L_BUF, "txbuf %d done!\n", i);
	}
}

/*----------------------------------------------------------------
* dmaTxDataISR
* FIXME: rename to acx100_clean_tx_desc
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

/* dmaTxDataISR()
 * V3 version!
 *
 * This function probably resets the txdescs' status when the ACX100
 * signals the TX done IRQ (txdescs have been processed), starting with
 * the pool index of the descriptor which we would use next,
 * in order to make sure that we can be as fast as possible
 * in filling new txdescs.
 * Oops, now we have our own index, so everytime we get called we know
 * where the next packet to be cleaned is.
 * Hmm, still need to loop through the whole ring buffer now,
 * since we lost sync for some reason when ping flooding or so...
 * (somehow we don't get the IRQ for dmaTxDataISR any more when
 * too many packets are being sent!)
 * FIXME: currently we only process one packet, but this gets out of
 * sync for some reason when ping flooding, so we need to loop,
 * but the previous smart loop implementation causes the ping latency
 * to rise dramatically (~3000 ms), at least on CardBus PheeNet WL-0022.
 * Dunno what to do :-\
 *
 * STATUS: good. FINISHED.
 */
void dmaTxDataISR(wlandevice_t *wlandev)
{
	TIWLAN_DC *pDc = &wlandev->dc;
	txdesc_t *pTxDesc;
	UINT finger, watch;
	int flags;

	FN_ENTER;

	log_txbuffer(pDc);
	acxlog(L_BUF, "cleaning up Tx bufs from %d.\n", pDc->pool_idx2);

	spin_lock_irqsave(&tx_lock, flags);
	finger = pDc->pool_idx2;
	watch = finger;

	do {
		pTxDesc = &pDc->pTxDescQPool[finger];

		/* check if txdesc is marked as "Tx finished" and "owned" */
		if ((pTxDesc->Ctl & 0xc0) == 0xc0) {

			acxlog(L_BUF, "cleaning %d.\n", finger);
			if (pTxDesc->val0x26 != 0) {
				acxlog(L_STD, "Tx error occurred (error 0x%02X)!! (maybe distance too high?)\n", pTxDesc->val0x26);
				wlandev->stats.tx_carrier_errors++;
				wlandev->stats.tx_errors++;
			}
			/* initialise it */
			pTxDesc->Ctl = 0x80; /* This can be just 0x80: the host now owns this descriptor. */
			wlandev->TxQueueFree++;
			if ((wlandev->TxQueueFree >= MINFREE_TX)
			&& (wlandev->iStatus == ISTATUS_4_ASSOCIATED)
			&& (netif_queue_stopped(wlandev->netdev)))
			{
				/* FIXME: this should maybe be optimized
				 * to only wake queue after some 5 or 10
				 * packets have been freed again.
				 * And besides, the if construct is ugly:
				 * should have functions acx100_stop_queue
				 * etc. which set flag wlandev->tx_stopped
				 * to be checked here. */
				acxlog(L_XFER, "wake queue (avail. Tx desc %d).\n", wlandev->TxQueueFree);
				netif_wake_queue(wlandev->netdev);
			}
		}
		else
			break;

		/* update pointer for descr to be cleaned next */
		finger = (finger + 1) % pDc->pool_count;
	} while (watch != finger);
	/* remember last position */
	pDc->pool_idx2 = finger;
	spin_unlock_irqrestore(&tx_lock, flags);

	FN_EXIT(0, 0);
	return;
}
/*----------------------------------------------------------------
* acx100_rxmonitor
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

void acx100_rxmonitor(wlandevice_t * hw, struct rxbuffer *buf)
{
	unsigned long *data = (unsigned long *)buf;
	unsigned int packet_len = data[0] & 0xFFF;
	int sig_strength = (data[1] >> 16) & 0xFF;
	int sig_quality  = (data[1] >> 24) & 0xFF;
	p80211msg_lnxind_wlansniffrm_t *msg;

	int payload_offset = 0;
	unsigned int skb_len;
	struct sk_buff *skb;
	void *datap;
	
	if (!(hw->rx_config_1 & 0x2000))
	{
		printk("rx_config_1 misses 0x2000\n");
		return;
	}
	
	if (hw->rx_config_1 & 0x2000)
	{
		payload_offset += 3*4; /* status words         */
		packet_len     += 3*4; /* length is w/o status */
	}
		
	if (hw->rx_config_1 & 2)
		payload_offset += 4;   /* phy header   */

	/* we are in big luck: the acx100 doesn't modify any of the fields */
	/* in the 802.11-frame. just pass this packet into the PF_PACKET-  */
	/* subsystem. yeah. */
	
	skb_len = packet_len - payload_offset;

	if (hw->netdev->type == ARPHRD_IEEE80211_PRISM)
		skb_len += sizeof(p80211msg_lnxind_wlansniffrm_t);

		/* sanity check */
	if (skb_len > (WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN + WLAN_CRC_LEN))
	{
		printk("monitor mode panic: oversized frame!\n");
		return;
	}
	
		/* allocate skb */
	if ( (skb = dev_alloc_skb(skb_len)) == NULL)
	{
		printk("alloc_skb failed trying to allocate %d bytes\n", skb_len);
		return;
	}
	
	skb_put(skb, skb_len);
	
		/* when in raw 802.11 mode, just copy frame as-is */
	if (hw->netdev->type == ARPHRD_IEEE80211)
		datap = skb->data;
	else  /* otherwise, emulate prism header */
	{
		msg = (p80211msg_lnxind_wlansniffrm_t*)skb->data;
		datap = msg + 1;
		
		msg->msgcode = DIDmsg_lnxind_wlansniffrm;
		msg->msglen = sizeof(p80211msg_lnxind_wlansniffrm_t);
		strcpy(msg->devname, hw->netdev->name);
		
		msg->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
		msg->hosttime.status = 0;
		msg->hosttime.len = 4;
		msg->hosttime.data = jiffies;
		
		msg->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
		msg->mactime.status = 0;
		msg->mactime.len = 4;
		msg->mactime.data = data[2];
		
		msg->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
		msg->channel.status = P80211ENUM_msgitem_status_no_value;
		msg->channel.len = 4;
		msg->channel.data = 0;
		
		msg->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
		msg->rssi.status = P80211ENUM_msgitem_status_no_value;
		msg->rssi.len = 4;
		msg->rssi.data = 0;

		msg->sq.did = DIDmsg_lnxind_wlansniffrm_sq;
		msg->sq.status = P80211ENUM_msgitem_status_no_value;
		msg->sq.len = 4;
		msg->sq.data = 0;

		msg->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
		msg->signal.status = 0;
		msg->signal.len = 4;
		msg->signal.data = sig_quality;

		msg->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
		msg->noise.status = 0;
		msg->noise.len = 4;
		msg->noise.data = sig_strength;

		msg->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
		msg->rate.status = P80211ENUM_msgitem_status_no_value;
		msg->rate.len = 4;
		msg->rate.data = 0;

		msg->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
		msg->istx.status = 0;
		msg->istx.len = 4;
		msg->istx.data = P80211ENUM_truth_false;

		skb_len -= sizeof(p80211msg_lnxind_wlansniffrm_t);

		msg->frmlen.did = DIDmsg_lnxind_wlansniffrm_signal;
		msg->frmlen.status = 0;
		msg->frmlen.len = 4;
		msg->frmlen.data = skb_len;
	}

	memcpy(datap, ((unsigned char*)buf)+payload_offset, skb_len);
	
	skb->dev = hw->netdev;
	skb->dev->last_rx = jiffies;
	
	skb->mac.raw = skb->data;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_80211_RAW);

	hw->stats.rx_packets++;
	hw->stats.rx_bytes += skb->len;
	
	netif_rx(skb);
}
/*----------------------------------------------------------------
* log_rxbuffer
* FIXME: rename to acx100_log_rxbuffer
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

void log_rxbuffer(TIWLAN_DC *pDc)
{
	int i;
	struct rxhostdescriptor *pDesc;

	for (i=0; i < pDc->iPoolSize; i++)
	{
		pDesc = &pDc->pRxHostDescQPool[i];

		if ((pDesc->Ctl & 0x80) && (pDesc->val0x14 < 0))
			acxlog(L_BUF, "rxbuf %d full!\n", i);
	}
}

/*----------------------------------------------------------------
* dmaRxXfrISR
* FIXME: rename to acx100_process_rx_desc
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

/*
 * STATUS: FINISHED, UNVERIFIED, FISHY (structs and pool index updating).
 */
void dmaRxXfrISR(wlandevice_t * hw)
{
	struct rxhostdescriptor *RxPool;
	TIWLAN_DC *pDc;
	struct rxhostdescriptor *pDesc;
	UINT16 buf_len;
	p80211_hdr_t *buf;
	int flags;
	int curr_idx;
	int count = 0;
	static int entry_count = 0;

	acxlog(L_STATE, "%s: UNVERIFIED, FISHY!\n", __func__);
	pDc = &hw->dc;

	log_rxbuffer(pDc);
	entry_count++;
	acxlog(L_IRQ, "%s entry_count %d\n", __func__, entry_count);
	RxPool = pDc->pRxHostDescQPool;

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * iPoolStart and the full descriptor we're supposed to handle. */
	spin_lock_irqsave(&rx_lock, flags);
	do {
		curr_idx = pDc->iPoolStart;
		pDesc = &RxPool[pDc->iPoolStart];
		pDc->iPoolStart = (pDc->iPoolStart + 1) % pDc->iPoolSize;
		count++;
		if (count > pDc->iPoolSize)
		{ /* hmm, no luck: all descriptors empty, bail out */
			spin_unlock_irqrestore(&rx_lock, flags);
			goto end;
		}
	}
	/* "pDesc->val0x14 < 0" is there to check whether MSB
	 * is set or not */
	while (!((pDesc->Ctl & 0x80) && (pDesc->val0x14 < 0))); /* check whether descriptor full, advance to next one if not */
	spin_unlock_irqrestore(&rx_lock, flags);

	while (1)
	{
		acxlog(L_BUF, "%s: using curr_idx %d, iPoolStart is now %d.\n", __func__, curr_idx, pDc->iPoolStart);

		buf = (p80211_hdr_t *)&pDesc->pThisBuf->buf;
		if (hw->rx_config_1 & 0x2) {
			/* take into account additional field */
			buf = (p80211_hdr_t*)((UINT8*)buf + 4);
		}
	
		buf_len = pDesc->pThisBuf->status & 0xfff;      /* somelength */
		acxlog(L_XFER|L_DATA, "Rx packet %02d (%s): time %lu, len %i, SIR %d, SNR %d, mode %lX, iStatus %X\n",
			curr_idx,
			GetPacketTypeString(buf->a3.fc),
			pDesc->pThisBuf->time,
			buf_len,
			pDesc->pThisBuf->silence,
			pDesc->pThisBuf->signal,
			hw->mode,
			hw->iStatus);

		hw->wstats.qual.level = pDesc->pThisBuf->silence;
		hw->wstats.qual.noise = pDesc->pThisBuf->signal;
		hw->wstats.qual.qual =
			(hw->wstats.qual.level > hw->wstats.qual.noise) ?
			(hw->wstats.qual.level - hw->wstats.qual.noise) : 0;
		hw->wstats.qual.updated = 7;
	
		if (hw->monitor)
			acx100_rxmonitor(hw, pDesc->pThisBuf);
		else
		{
			if (buf_len >= 14) {
				acx80211_rx(pDesc, hw);
			} else
			{
				acxlog(L_DEBUG | L_XFER | L_DATA,
				"NOT receiving packet (%s): size too small (%d)\n", GetPacketTypeString(buf->a3.fc), buf_len);
	                }
		}
	
		if (pDesc) {
			pDesc->Ctl &= ~0x80; //Host no longer owns this
			pDesc->val0x14 = 0;
		} else {
			acxlog(L_BINDEBUG, "pDesc is NULL\n");
		}

		/* ok, descriptor is handled, now check the next descriptor */
		spin_lock_irqsave(&rx_lock, flags);
		curr_idx = pDc->iPoolStart;
		pDesc = &RxPool[pDc->iPoolStart];

		/* if next descriptor is empty, then bail out */
		if (!((pDesc->Ctl & 0x80) && (pDesc->val0x14 < 0)))
		{
			spin_unlock_irqrestore(&rx_lock, flags);
			break;
		}
		else
			pDc->iPoolStart = (pDc->iPoolStart + 1) % pDc->iPoolSize;
		spin_unlock_irqrestore(&rx_lock, flags);
	}
end:
	entry_count--;
}

/*----------------------------------------------------------------
* dmaFreeTxHostDescQ
* FIXME: rename to acx100_free_tx_host_desc_queue
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

/* dmaFreeTxHostDescQ()
 * STATUS: ok.
 */
int dmaFreeTxHostDescQ(TIWLAN_DC * pDc)
{
	FN_ENTER;

	if (pDc->pRxHostDescQPool) {
		pci_free_consistent(0,
				    pDc->TxHostDescQPoolSize,
				    pDc->pTxHostDescQPool,
				    pDc->TxHostDescQPoolPhyAddr);
		pDc->pTxHostDescQPool = NULL;
		pDc->TxHostDescQPoolSize = 0;
	}
	if (pDc->pFrameHdrQPool) {
		pci_free_consistent(0, pDc->FrameHdrQPoolSize,
				    pDc->pFrameHdrQPool,
				    pDc->FrameHdrQPoolPhyAddr);
		pDc->pFrameHdrQPool = NULL;
		pDc->FrameHdrQPoolSize = 0;
	}
	if (pDc->pTxBufferPool) {
		pci_free_consistent(0, pDc->TxBufferPoolSize,
				    pDc->pTxBufferPool,
				    pDc->TxBufferPoolPhyAddr);
		pDc->pTxBufferPool = NULL;
		pDc->TxBufferPoolSize = 0;
	}
	FN_EXIT(0, 0);
	return 0;
}
/*----------------------------------------------------------------
* dmaFreeTxDescQ
* FIXME: rename to acx100_free_tx_desc_queue
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

/* dmaFreeTxDescQ()
 * STATUS: ok.
 */
int dmaFreeTxDescQ(TIWLAN_DC * pDc)
{
	FN_ENTER;

	pDc->pTxDescQPool = NULL;

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* dmaFreeRxHostDescQ
* FIXME: rename to acx100_free_rx_host_desc_queue
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

/* dmaFreeRxHostDescQ()
 * STATUS: almost ok, but UNVERIFIED.
 */
int dmaFreeRxHostDescQ(TIWLAN_DC * pDc)
{
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;
	if (pDc->pRxHostDescQPool != 0) {
		pci_free_consistent(0, pDc->RxHostDescQPoolSize,
				    pDc->pRxHostDescQPool,
				    pDc->RxHostDescQPoolPhyAddr);
		pDc->pRxHostDescQPool = NULL;
		pDc->RxHostDescQPoolSize = 0;
	}
	if (pDc->pRxBufferPool != 0) {	//pRxBufferPool
		pci_free_consistent(0, pDc->RxBufferPoolSize,
				    pDc->pRxBufferPool,
				    pDc->RxBufferPoolPhyAddr);
		pDc->pRxBufferPool = NULL;
		pDc->RxBufferPoolSize = 0;
	}
	FN_EXIT(0, 0);
	return 0;
}
/*----------------------------------------------------------------
* dmaFreeRxDescQ
* FIXME: rename to acx100_free_rx_desc_queue
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

/* dmaFreeRxDescQ()
 * STATUS: FINISHED
 */
int dmaFreeRxDescQ(TIWLAN_DC * pDc)
{
	FN_ENTER;
	pDc->rxdescq1 = 0;
	pDc->rxdescq2 = 0;
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* dmaCreateTxHostDescQ
* FIXME: rename to acx100_create_tx_host_desc_queue
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

/* dmaCreateTxHostDescQ()
 *
 * WARNING!!! If you change any variable types in here, make *DAMN SURE*
 * the pointer arithmetic is still correct!!!!!!!!!!!!!!!!!!
 *
 * STATUS: working on it. UNVERIFIED.
 */
int dmaCreateTxHostDescQ(TIWLAN_DC * pDc)
{
	wlandevice_t *hw;
	UINT32 ibuf;
	struct framehdr *frame_hdr;
	dma_addr_t frame_hdr_phy;
	UINT32 align_offs;

	dma_addr_t frame_payload_phy;
	struct txhostdescriptor *pDesc;
	dma_addr_t host_desc_phy;
	UINT32 alignment;
	UINT8 *frame_payload;

	FN_ENTER;

	hw = pDc->wldev;

	/*** first allocate frame header queue pool ***/
	pDc->FrameHdrQPoolSize = ((hw->val0x24e0 >> 1) * sizeof(struct framehdr));	/* 0x26 == (9 * 2 + 1) * 2 */
	if (!(pDc->pFrameHdrQPool =
	      pci_alloc_consistent(0, pDc->FrameHdrQPoolSize,
				   &pDc->FrameHdrQPoolPhyAddr))) {
		acxlog(L_BINSTD,
		       "pDc->pFrameHdrQPool memory allocation error\n");
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->pFrameHdrQPool = 0x%8x\n",
	       (UINT) pDc->pFrameHdrQPool);
	acxlog(L_BINDEBUG, "pDc->pFrameHdrQPoolPhyAddr = 0x%8x\n",
	       (UINT) pDc->FrameHdrQPoolPhyAddr);

	/*** now allocate TX buffer pool ***/
	pDc->TxBufferPoolSize = hw->val0x24e0 * (WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN);

	if (!(pDc->pTxBufferPool =
	      pci_alloc_consistent(0, pDc->TxBufferPoolSize,
				   &pDc->TxBufferPoolPhyAddr))) {
		acxlog(L_BINSTD,
		       "pDc->pTxBufferPool memory allocation error\n");
		pci_free_consistent(0, pDc->FrameHdrQPoolSize,
				    pDc->pFrameHdrQPool,
				    pDc->FrameHdrQPoolPhyAddr);
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->TxBufferPool = 0x%8x\n",
	       (UINT) pDc->pTxBufferPool);
	acxlog(L_BINDEBUG, "pDc->TxBufferPoolPhyAddr = 0x%8x\n",
	       (UINT) pDc->TxBufferPoolPhyAddr);

	/*** now allocate the TX host descriptor queue pool ***/
	pDc->TxHostDescQPoolSize =  hw->val0x24e0 * sizeof(struct txhostdescriptor) + 3;
		/* 0xb03 == 64 entries * 0x2c struct size + 3 (for reserved alignment space??) */

	if (!(pDc->pTxHostDescQPool =
	      pci_alloc_consistent(0, pDc->TxHostDescQPoolSize,
				   &pDc->TxHostDescQPoolPhyAddr))) {
		acxlog(L_BINSTD,
		       "Failed to allocate shared memory for TxHostDesc que\n");
		pci_free_consistent(0, pDc->FrameHdrQPoolSize,
				    pDc->pFrameHdrQPool,
				    pDc->FrameHdrQPoolPhyAddr);
		pci_free_consistent(0, pDc->TxBufferPoolSize,
				    pDc->pTxBufferPool,
				    pDc->TxBufferPoolPhyAddr);
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->pTxHostDescQPool = 0x%8x\n",
	       (UINT) pDc->pTxHostDescQPool);
	acxlog(L_BINDEBUG, "pDc->TxHostDescQPoolPhyAddr = 0x%8x\n",
	       pDc->TxHostDescQPoolPhyAddr);

	/*** now check for proper alignment of TX host descriptor pool ***/
	alignment = (UINT) pDc->pTxHostDescQPool & 3;
	if (alignment) {
		acxlog(L_BINSTD,
		       "dmaCreateTxHostDescQ: TxHostDescQPool not aligned properly\n");
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	pDesc =
	    (struct txhostdescriptor *) ((UINT8 *) pDc->pTxHostDescQPool) +
	    align_offs;
	host_desc_phy = pDc->TxHostDescQPoolPhyAddr + align_offs;
	frame_hdr = pDc->pFrameHdrQPool;
	frame_hdr_phy = pDc->FrameHdrQPoolPhyAddr;
	frame_payload = pDc->pTxBufferPool;
	frame_payload_phy = pDc->TxBufferPoolPhyAddr;

	if (hw->val0x24e0 != 1) {
		for (ibuf = 0; ibuf < hw->val0x24e0 - 1; ibuf++)
		{
			if (!(ibuf & 1)) {
				pDesc->data_phy = frame_hdr_phy;
				pDesc->data = (UINT8 *) frame_hdr;
				frame_hdr_phy += sizeof(struct framehdr);	/* += 0x26 */
				frame_hdr++;	/* += 0x26 */
			} else {
				pDesc->data_phy = frame_payload_phy;
				pDesc->data = (UINT8 *) frame_payload;
				frame_payload_phy += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;	/* 0x5dc == 1500 */
				frame_payload += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;	/* 0x5dc == 1500 */
			}

			pDesc->desc_phy_next = host_desc_phy + sizeof(struct txhostdescriptor);	/* + 0x2c */
			pDesc->val0x10 =
			    (((ibuf & 1) !=
			      0) - 1) & (host_desc_phy +
					 sizeof(struct txhostdescriptor)
					 /* 0x2c */
			    );
			pDesc->Ctl |= 0x80;
			pDesc->desc_phy = host_desc_phy;

			pDesc++;
			host_desc_phy += sizeof(struct txhostdescriptor);	/* += 0x2c */
		}
	}
	pDesc->data_phy = frame_payload_phy;
	pDesc->data = frame_payload;
	pDesc->desc_phy_next = pDc->TxHostDescQPoolPhyAddr + align_offs;
	pDesc->Ctl |= 0x80;
	pDesc->desc_phy = host_desc_phy;

	FN_EXIT(0, 0);

	return 0;
}

/*----------------------------------------------------------------
* dmaCreateTxDescQ
* FIXME: rename to acx100_create_tx_desc_queue
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

/* dmaCreateTxDescQ
 * STATUS: good. V3.
 */
int dmaCreateTxDescQ(TIWLAN_DC * pDc)
{
	wlandevice_t *pAdapter;

	UINT32 mem_offs;
	struct txhostdescriptor *tx_hostdesc;
	UINT32 i;
	struct txdescriptor *tx_desc;
	UINT hostmemptr;

	FN_ENTER;
	pAdapter = pDc->wldev;

	pDc->pTxDescQPool =
	    (struct txdescriptor *) (pAdapter->pvMemBaseAddr2 +
				     pDc->ui32ACXTxQueueStart);

	acxlog(L_BINDEBUG, "pAdapter->rHWInfo.pvMemBaseAddr2 = 0x%08x\n",
	       pAdapter->pvMemBaseAddr2);
	acxlog(L_BINDEBUG, "pDc->ui32ACXTxQueueStart = 0x%08x\n",
	       pDc->ui32ACXTxQueueStart);
	acxlog(L_BINDEBUG, "pDc->pTxDescQPool = 0x%08x\n",
	       (UINT) pDc->pTxDescQPool);

	pDc->pool_count = pAdapter->TxQueueNo;
	pAdapter->TxQueueFree = pAdapter->TxQueueNo;
	mem_offs = pDc->ui32ACXTxQueueStart;
	tx_hostdesc = (struct txhostdescriptor *) pDc->pTxHostDescQPool;
	hostmemptr = pDc->TxHostDescQPoolPhyAddr;
	pDc->pool_idx = 0;
	pDc->pool_idx2 = 0;
	tx_desc = pDc->pTxDescQPool;

	/* loop over complete send pool */
	for (i = 0; i < pDc->pool_count; i++) {
		/* clear em */
		memset(tx_desc, 0, sizeof(struct txdescriptor));	// size = 4*0xC
		/* pointer to hostdesc memory */
		tx_desc->HostMemPtr = hostmemptr;
		/* initialise ctl */
		tx_desc->Ctl = 0x8e;
		tx_desc->something2 = 0x0;
		/* point to next txdesc */
		tx_desc->pNextDesc = mem_offs + sizeof(struct txdescriptor);	// size = 0x30
		/* pointer to first txhostdesc */
		tx_desc->val0x1c = tx_hostdesc;

//		tx_desc->val0x2b = 0x0;

		/* reserve two (hdr desc and payload desc) */
		tx_hostdesc += 2;
		hostmemptr += 2 * sizeof(struct txhostdescriptor);	/* 2 * 0x2c == 0x58 */
		/* go to the next */
		mem_offs += sizeof(struct txdescriptor);
		tx_desc++;	// actually add 0x30
	}
	/* go to the last one */
	tx_desc--;		// actually sub 0x30
	/* and point to the first making it a ring buffer */
	tx_desc->pNextDesc = pDc->ui32ACXTxQueueStart;

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* dmaCreateRxHostDescQ
* FIXME: rename to acx100_create_rx_host_desc_queue
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

/*
 * STATUS: UNVERIFIED.
 */
int dmaCreateRxHostDescQ(TIWLAN_DC * pDc)
{
	wlandevice_t *hw;
	struct rxhostdescriptor *pDesc;
	UINT32 alignment;
	UINT32 bufphy;
	struct rxbuffer *esi;
	UINT32 edx;
	UINT32 var_4;
	UINT32 align_offs;
	int result = 0;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;
	hw = pDc->wldev;
	pDc->iPoolSize = hw->RxHostDescNo;
	pDc->iPoolStart = 0;
	pDc->RxHostDescQPoolSize =
	    (hw->RxHostDescNo * sizeof(struct rxhostdescriptor)) + 0x3;

	if ((pDc->pRxHostDescQPool =
	     pci_alloc_consistent(0, pDc->RxHostDescQPoolSize,
				  &pDc->RxHostDescQPoolPhyAddr)) == NULL) {
		acxlog(L_BINSTD,
		       "Failed to allocate shared memory for RxHostDesc que\n");
		result = 2;
		goto fail;
	}
	pDc->RxBufferPoolSize = (hw->RxHostDescNo * 2390);
	if ((pDc->pRxBufferPool =
	     pci_alloc_consistent(0, pDc->RxBufferPoolSize,
				  &pDc->RxBufferPoolPhyAddr)) == NULL) {
		acxlog(L_BINSTD,
		       "Failed to allocate shared memory for Rx buffer\n");
		pci_free_consistent(0, pDc->RxHostDescQPoolSize,
				    pDc->pRxHostDescQPool,
				    pDc->RxHostDescQPoolPhyAddr);
		result = 2;
		goto fail;
	}
	acxlog(L_BINDEBUG, "pDc->pRxHostDescQPool = 0x%8x\n",
	       (UINT) pDc->pRxHostDescQPool);
	acxlog(L_BINDEBUG, "pDc->RxHostDescQPoolPhyAddr = 0x%8x\n",
	       (UINT) pDc->RxHostDescQPoolPhyAddr);
	acxlog(L_BINDEBUG, "pDc->pRxBufferPool = 0x%8x\n",
	       (UINT) pDc->pRxBufferPool);
	acxlog(L_BINDEBUG, "pDc->RxBufferPoolPhyAddr = 0x%8x\n",
	       (UINT) pDc->RxBufferPoolPhyAddr);

	if ((alignment = ((UINT) pDc->pRxHostDescQPool) & 0x3)) {
		acxlog(L_BINSTD,
		       "dmaCreateRxHostDescQ: RxHostDescQPool not aligned properly\n");
		align_offs = 0x4 - alignment;
	} else {
		align_offs = 0;
	}
	pDesc =
	    (struct rxhostdescriptor *) (((UINT8 *) pDc->pRxHostDescQPool)
					 + align_offs);
	edx = pDc->RxHostDescQPoolPhyAddr + align_offs;

	hw->RxHostDescPoolStart = edx;

	bufphy = pDc->RxBufferPoolPhyAddr;
	esi = (struct rxbuffer *) pDc->pRxBufferPool;
	var_4 = 0;

	if (hw->RxHostDescNo != 1) {
		/* var_C is set to 0 and thus irrelevant here */
		for (; var_4 < hw->RxHostDescNo - 1; var_4++) {
			pDesc->pNextDescPhyAddr =
			    edx +
			    sizeof(struct rxhostdescriptor) /* 0x2c */ ;
			pDesc->Ctl &= 0xff7f;
			pDesc->pThisDescPhyAddr = edx;
			pDesc->pBufPhyAddr = bufphy;
			pDesc->buffersize = 2390;	/* 0x956 */
			pDesc->pThisBuf = esi;
			*(UINT32 *) &pDesc->val0x28 = 0x2;
			pDesc += 1;	/* actually 0x2c */
			edx += sizeof(struct rxhostdescriptor);
			bufphy += 2390;	/* 0x956 */
			esi = (struct rxbuffer *) ((UINT8 *) esi + 2390);	/* 0x956 */
		}
	}
	pDesc->pNextDescPhyAddr = pDc->RxHostDescQPoolPhyAddr + align_offs;
	pDesc->Ctl &= 0xff7f;
	pDesc->pThisDescPhyAddr = edx;
	pDesc->pBufPhyAddr = bufphy;
	pDesc->buffersize = 2390;
	pDesc->pThisBuf = esi;
	*(UINT32 *) &pDesc->val0x28 = 0x2;
	result = 0;
      fail:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* dmaCreateRxDescQ
* FIXME: rename to acx100_create_rx_desc_queue
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

/*
 * STATUS: UNVERIFIED
 */
int dmaCreateRxDescQ(TIWLAN_DC * pDc)
{
	wlandevice_t *hw;
	UINT var_4, var_8;
	struct rxdescriptor *edx;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;
	hw = pDc->wldev;
	pDc->rxdescq1 =
	    (char *) ((hw->TxQueueNo * sizeof(struct txdescriptor))
		      + (UINT) pDc->pTxDescQPool);
	pDc->rxdescq2 = hw->RxQueueNo;
	var_8 = pDc->ui32ACXRxQueueStart;
	edx = (struct rxdescriptor *) pDc->rxdescq1;

	for (var_4 = 0; var_4 < pDc->rxdescq2; var_4++) {
		memset(edx, 0, sizeof(struct rxdescriptor) /* 0x34 */ );
		edx->val0x28 = 0xc;
		edx->pPhyAddr = var_8 + sizeof(struct rxdescriptor);

		edx++;

		var_8 += sizeof(struct rxdescriptor);
	}

	edx--;
	edx->pPhyAddr = pDc->ui32ACXRxQueueStart;

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* acxInitMemoryPools
* FIXME: rename to acx100_init_memory_pools
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

/* acxInitMemoryPools()
 * STATUS: UNVERIFIED
 */
int acxInitMemoryPools(wlandevice_t * hw, memmap_t * mmt)
{
	UINT TotalMemoryBlocks;  // var_40
	/* FIXME: is a memmap_t with ConfigWrite3 union type in it.
	 * This ConfigWrite3 is specially made for this function
	 * because vali and valj need to be UINT16's. Or am I
	 * completely wrong
	 * Actually, I think there needs to be 2 memmaps in this function- one
	 * for the sizewrite and one for optionswrite
	 */
	struct {
		UINT16 val0x0;
		UINT16 val0x2;
		UINT16 size;  // val0x4
/*		UINT16 val0x6;
		UINT32 val0x8; It's size only 4 + ACX100RID_BLOCK_SIZE_LEN = 6
		UINT32 val0xc; */
	} MemoryBlockSize;   // var_3c

	struct {
		UINT16 val0x0;
		UINT16 val0x2;
		UINT DMA_config; // val0x4
		UINT val0x8;
		UINT val0xc;
		UINT val0x10;
		UINT16 TxBlockNum; // val0x14
		UINT16 RxBlockNum; // val0x16;
	} MemoryConfigOption;    // var_2c

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;

	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = hw->memblocksize;

	/* Then we alert the card to our decision of block size */
	if (!ctlConfigure(hw, &MemoryBlockSize, ACX100_RID_BLOCK_SIZE)) {
		acxlog(L_BINSTD, "Ctl: MemoryBlockSizeWrite failed\n");
		return 0;
	}

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers. IE. end-start/size */
	TotalMemoryBlocks = (mmt->m.cw2.val0x28 - mmt->m.cw2.vali) / hw->memblocksize;

	/* This one I have no idea on */
	MemoryConfigOption.DMA_config = 0x30000; //dma config
	/* Declare start of the Rx host pool */
	MemoryConfigOption.val0x8 = hw->RxHostDescPoolStart;

	/* 50% of the allotment of memory blocks go to tx descriptors */
	MemoryConfigOption.TxBlockNum = TotalMemoryBlocks / 2;
	/* and 50% go to the rx descriptors */
	MemoryConfigOption.RxBlockNum = TotalMemoryBlocks - MemoryConfigOption.TxBlockNum;

	/* in this block, we save the information we gleaned from the
	   card into our wlandevice structure; # of tx desc blocks */
	hw->val0x24fc = MemoryConfigOption.TxBlockNum;
	/* # of rx desc blocks */
	hw->val0x24e4 = MemoryConfigOption.RxBlockNum;
	/* size of the tx and rx descriptor queues */
	hw->val0x2500 = MemoryConfigOption.TxBlockNum * hw->memblocksize;
	hw->val0x24e8 = MemoryConfigOption.RxBlockNum * hw->memblocksize;

	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.val0xc = (mmt->m.cw2.vali + 0x1f) & 0xffffffe0;
	/* align the rx descriptor queue to units of 0x20 and offset it
	   by the tx descriptor queue */
	MemoryConfigOption.val0x10 =
	    (0x1f + mmt->m.cw2.vali + hw->val0x2500) & 0xffffffe0;

	/* alert the device to our decision */
	if (!ctlConfigure(hw, &MemoryConfigOption, ACX100_RID_MEMORY_CONFIG_OPTIONS)) {
		return 0;
	}
	/* and tell the device to kick it into gear */
	if (!ctlIssueCommand(hw, ACX100_CMD_INIT_MEMORY, 0, 0, 5000)) {
		return 0;
	}

	FN_EXIT(0, 0);

	return 1;
}

/*----------------------------------------------------------------
* get_tx_desc
* FIXME: rename to acx100_get_tx_desc
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

struct txdescriptor *get_tx_desc(wlandevice_t * wlandev)
{
	struct TIWLAN_DC * pDc = &wlandev->dc;
	struct txdescriptor *tx_desc;
	int flags;

	FN_ENTER;

	spin_lock_irqsave(&tx_lock, flags);
	tx_desc = &pDc->pTxDescQPool[pDc->pool_idx];
	
	if ((tx_desc->Ctl & 0x80) == 0) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
		tx_desc = NULL;
		goto error;
	}
	wlandev->TxQueueFree--;
	acxlog(L_BUF, "got Tx desc %d, %d remain.\n", pDc->pool_idx, wlandev->TxQueueFree);
/* This doesn't do anything other than limit our maximum number of 
 * buffers used at a single time (we might as well just declare 
 * MINFREE_TX less descriptors when we open up.) We should just let it 
 * slide here, and back off MINFREE_TX in dmaTxDataISR, when given the 
 * opportunity to let the queue start back up.
 */
/*
	if (wlandev->TxQueueFree < MINFREE_TX)
	{
		acxlog(L_XFER, "stop queue (avail. Tx desc %d).\n", wlandev->TxQueueFree);
		netif_stop_queue(wlandev->netdev);
	}
*/
	/* returning current descriptor, so advance to next free one */
	pDc->pool_idx = (pDc->pool_idx + 1) % pDc->pool_count;
error:
	spin_unlock_irqrestore(&tx_lock, flags);

	FN_EXIT(0, (int) tx_desc);
	return tx_desc;
}

/*----------------------------------------------------------------
* DumpTxDesc
* FIXME: get rid of this
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

/* DumpTxDesc()
 * STATUS: ok.
 *
 */
void DumpTxDesc(TIWLAN_DC * pDc, UINT32 no)
{
	struct txdescriptor *pTxDesc = &pDc->pTxDescQPool[pDc->pool_idx];

	acxlog(L_DEBUG, "<DumpTxDesc> No = %d, pTxDesc = 0x%x\n", (int) no,
	       (unsigned int) pTxDesc);

	acxlog(L_BINDEBUG, "pTxDesc->HostMemPtr = 0x%x\n",
	       pTxDesc->HostMemPtr);
	acxlog(L_BINDEBUG, "pTxDesc->AcxMemPtr = 0x%x\n",
	       pTxDesc->AcxMemPtr);
	acxlog(L_BINDEBUG, "pTxDesc->Ctl = 0x%x\n", pTxDesc->Ctl);
	acxlog(L_BINDEBUG, "pTxDesc->AckFailures = 0x%x\n",
	       pTxDesc->AckFailures);
	acxlog(L_BINDEBUG, "==============================\n");
}

static char type_string[32];	/* I *really* don't care that this is static,
				   so don't complain, else... ;-) */

/*----------------------------------------------------------------
* GetPacketTypeString
* FIXME: rename to acx100_get_packet_type_string
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

char *GetPacketTypeString(UINT16 fc)
{
	char *ftype = "UNKNOWN", *fstype = "UNKNOWN";

	switch (WLAN_GET_FC_FTYPE(fc)) {
	case WLAN_FTYPE_MGMT:
		ftype = "MGMT";
		switch (WLAN_GET_FC_FSTYPE(fc)) {
		case WLAN_FSTYPE_ASSOCREQ:
			fstype = "AssocReq";
			break;
		case WLAN_FSTYPE_ASSOCRESP:
			fstype = "AssocResp";
			break;
		case WLAN_FSTYPE_REASSOCREQ:
			fstype = "ReassocReq";
			break;
		case WLAN_FSTYPE_REASSOCRESP:
			fstype = "ReassocResp";
			break;
		case WLAN_FSTYPE_PROBEREQ:
			fstype = "ProbeReq";
			break;
		case WLAN_FSTYPE_PROBERESP:
			fstype = "ProbeResp";
			break;
		case WLAN_FSTYPE_BEACON:
			fstype = "Beacon";
			break;
		case WLAN_FSTYPE_ATIM:
			fstype = "ATIM";
			break;
		case WLAN_FSTYPE_DISASSOC:
			fstype = "Disassoc";
			break;
		case WLAN_FSTYPE_AUTHEN:
			fstype = "Authen";
			break;
		case WLAN_FSTYPE_DEAUTHEN:
			fstype = "Deauthen";
			break;
		}
		break;
	case WLAN_FTYPE_CTL:
		ftype = "CTL";
		switch (WLAN_GET_FC_FSTYPE(fc)) {
		case WLAN_FSTYPE_PSPOLL:
			fstype = "PSPoll";
			break;
		case WLAN_FSTYPE_RTS:
			fstype = "RTS";
			break;
		case WLAN_FSTYPE_CTS:
			fstype = "CTS";
			break;
		case WLAN_FSTYPE_ACK:
			fstype = "Ack";
			break;
		case WLAN_FSTYPE_CFEND:
			fstype = "CFEnd";
			break;
		case WLAN_FSTYPE_CFENDCFACK:
			fstype = "CFEndCFAck";
			break;
		}
		break;
	case WLAN_FTYPE_DATA:
		ftype = "DATA";
		switch (WLAN_GET_FC_FSTYPE(fc)) {
		case WLAN_FSTYPE_DATAONLY:
			fstype = "DataOnly";
			break;
		case WLAN_FSTYPE_DATA_CFACK:
			fstype = "Data CFAck";
			break;
		case WLAN_FSTYPE_DATA_CFPOLL:
			fstype = "Data CFPoll";
			break;
		case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
			fstype = "Data CFAck/CFPoll";
			break;
		case WLAN_FSTYPE_NULL:
			fstype = "Null";
			break;
		case WLAN_FSTYPE_CFACK:
			fstype = "CFAck";
			break;
		case WLAN_FSTYPE_CFPOLL:
			fstype = "CFPoll";
			break;
		case WLAN_FSTYPE_CFACK_CFPOLL:
			fstype = "CFAck/CFPoll";
			break;
		}
		break;
	}
	sprintf(type_string, "%s/%s", ftype, fstype);
	return type_string;
}
