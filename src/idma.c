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

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/pm.h>

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
#include <acx100.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <monitor.h>

/* these used to be increased to 32 each,
 * but several people had memory allocation issues,
 * so back to 16 again... */
#if (WLAN_HOSTIF==WLAN_USB)
#define TXBUFFERNO 10
#define RXBUFFERNO 10
#else
#define RXBUFFERNO 16
#define TXBUFFERNO 16
#endif

#define MINFREE_TX 3
spinlock_t tx_lock;
spinlock_t rx_lock;

#ifdef ACX_DEBUG
#if (WLAN_HOSTIF==WLAN_USB)
extern void acx100usb_dump_bytes(void *,int);
#endif
#endif


/*----------------------------------------------------------------
* acx100_enable_irq
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

void acx100_enable_irq(wlandevice_t * wlandev)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_IRQ_MASK], wlandev->irq_mask);
	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_FEMR], 0x8000);
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_disable_irq
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

void acx100_disable_irq(wlandevice_t * wlandev)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_IRQ_MASK], 0x7fff);
	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_FEMR], 0x0);
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_create_dma_regions
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

int acx100_create_dma_regions(wlandevice_t * wlandev)
{
	QueueConfig_t qcfg;
	/* FIXME: way too big to reside on the stack */
	struct {
		UINT16 val0x0[14];
		UINT queue_start;
		UINT val0x20;
		UINT val0x24;
		UINT val0x28;
		UINT val0x2c;
	} MemMap;
	struct TIWLAN_DC *pDc;

	FN_ENTER;

	spin_lock_init(&rx_lock);
	spin_lock_init(&tx_lock);

	pDc = &wlandev->dc;
	pDc->wlandev = wlandev;

	/* FIXME: which memmap is read here, acx100_init_memory_pools did not get called yet ? */
	/* read out the acx100 physical start address for the queues */
	if (!acx100_interrogate(wlandev, &MemMap, ACX100_RID_MEMORY_MAP)) {
		acxlog(L_BINSTD, "ctlMemoryMapRead returns error\n");
		FN_EXIT(1, 2);
		return 2;
	}

	/* # of items in Rx and Tx queues */
	wlandev->TxQueueNo = TXBUFFERNO;
	wlandev->RxQueueNo = RXBUFFERNO;

	/* calculate size of queues */
	qcfg.AreaSize = (sizeof(struct txdescriptor) * TXBUFFERNO +
	                 sizeof(struct rxdescriptor) * RXBUFFERNO + 8);
	qcfg.vale = 1;  /* number of tx queues */
	qcfg.valf1=RXBUFFERNO;

	/* sets the beginning of the tx descriptor queue */
	pDc->ui32ACXTxQueueStart = MemMap.queue_start;
	qcfg.TxQueueStart = pDc->ui32ACXTxQueueStart;
	qcfg.valj = 0;
	qcfg.valk = TXBUFFERNO;

	/* sets the beginning of the rx descriptor queue */
	pDc->ui32ACXRxQueueStart = wlandev->TxQueueNo * sizeof(struct txdescriptor) + MemMap.queue_start;
	qcfg.RxQueueStart = pDc->ui32ACXRxQueueStart;
	qcfg.vald = 1;

	/* sets the end of the rx descriptor queue */
	qcfg.QueueEnd = wlandev->RxQueueNo * sizeof(struct rxdescriptor) + pDc->ui32ACXRxQueueStart;

	/* sets the beginning of the next queue */
	qcfg.QueueEnd2 = qcfg.QueueEnd + 8;

	acxlog(L_BINDEBUG, "<== Initialize the Queue Indicator\n");

	if (!acx100_configure_length(wlandev, &qcfg, ACX100_RID_QUEUE_CONFIG, sizeof(QueueConfig_t)-4)){ //0x14 + (qcfg.vale * 8))) {
		acxlog(L_BINSTD, "ctlQueueConfigurationWrite returns error\n");
		goto error;
	}

	if (acx100_create_tx_host_desc_queue(pDc)) {
		acxlog(L_BINSTD, "acx100_create_tx_host_desc_queue returns error\n");
		goto error;
	}
	if (acx100_create_rx_host_desc_queue(pDc)) {
		acxlog(L_BINSTD, "acx100_create_rx_host_desc_queue returns error\n");
		goto error;
	}
	acx100_create_tx_desc_queue(pDc);
	acx100_create_rx_desc_queue(pDc);
	if (!acx100_interrogate(wlandev, &MemMap, ACX100_RID_MEMORY_MAP)) {
		acxlog(L_BINSTD, "Failed to read memory map\n");
		goto error;
	}

#if (WLAN_HOSTIF==WLAN_USB)
	acxlog(L_DEBUG,"Memory Map before configure 1\n");
	acx100usb_dump_bytes(&MemMap,44);
#endif
	if (!acx100_configure(wlandev,&MemMap,ACX100_RID_MEMORY_MAP)) {
		acxlog(L_BINSTD,"Failed to write memory map\n");
		goto error;
	}


	/* FIXME: what does this do? name the fields */
	/* start at least 4 bytes away from the end of the last pool */
	MemMap.val0x24 = (MemMap.val0x20 + 0x1F + 4) & 0xffffffe0;
#if (WLAN_HOSTIF==WLAN_USB)
#ifdef ACX_DEBUG
	acxlog(L_DEBUG,"Memory map before configure:\n");
	acx100usb_dump_bytes(&MemMap,44);
#endif
#endif

	if (!acx100_configure(wlandev, &MemMap, ACX100_RID_MEMORY_MAP)) {
		acxlog(L_BINSTD, "ctlMemoryMapWrite returns error\n");
		goto error;
	}

	if (!acx100_init_memory_pools(wlandev, (memmap_t *) &MemMap)) {
		acxlog(L_BINSTD, "acx100_init_memory_pools returns error\n");
		goto error;
	}

#if THIS_IS_BOGUS_ISNT_IT
	acx100_set_defaults(wlandev);
#endif

	FN_EXIT(1, 0);
	return 0;

error:
	acx100_free_desc_queues(pDc);

	FN_EXIT(1, 1);
	return 1;
}

/*----------------------------------------------------------------
* acx100_delete_dma_region
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

int acx100_delete_dma_region(wlandevice_t * wlandev)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_ENABLE], 0);

	/* used to be a for loop 1000, do scheduled delay instead */
	acx100_schedule(HZ / 10);
#endif
	acx100_free_desc_queues(&wlandev->dc);

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_dma_tx_data
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

void acx100_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *tx_desc)
{
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;
	unsigned long flags;
	int i;

	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	/* header and payload are located in adjacent descriptors */
	header = tx_desc->host_desc;
	payload = tx_desc->host_desc + 1;

	tx_desc->tx_time = jiffies;

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
	tx_desc->Ctl |= DESC_CTL_AUTODMA | DESC_CTL_RECLAIM | DESC_CTL_FIRST_MPDU;

	/* set rate */
	if (WLAN_GET_FC_FTYPE(((p80211_hdr_t*)header->data)->a3.fc) == WLAN_FTYPE_MGMT) {
		tx_desc->rate = 20;	/* 2Mbps for MGMT pkt compatibility */
	} else {
		tx_desc->rate = wlandev->bitrateval;
	}

	acxlog(L_XFER | L_DATA,
		"Tx pkt (%s): len %i, hdr_len %i, pyld_len %i, mode %d, status %d\n",
		acx100_get_packet_type_string(((p80211_hdr_t*)header->data)->a3.fc),
		tx_desc->total_length,
		header->length,
		payload->length,
		wlandev->mode,
		wlandev->status);

	acxlog(L_DATA, "802.11 header[%d]: ", header->length);
	for (i = 0; i < header->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) header->data)[i]);
	acxlog(L_DATA, "\n");
	acxlog(L_DATA, "802.11 payload[%d]: ", payload->length);
	for (i = 0; i < payload->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) payload->data)[i]);
	acxlog(L_DATA, "\n");

	spin_lock_irqsave(&tx_lock, flags);

	/* sets Ctl DESC_CTL_FREE to zero telling that the descriptors are now owned by the acx100 */
	header->Ctl &= (UINT16) ~DESC_CTL_FREE;
	payload->Ctl &= (UINT16) ~DESC_CTL_FREE;
	tx_desc->Ctl &= (UINT16) ~DESC_CTL_FREE;

	acx100_write_reg16(wlandev, wlandev->io[IO_ACX_INT_TRIG], 0x4);

	spin_unlock_irqrestore(&tx_lock, flags);
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_log_txbuffer
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

void acx100_log_txbuffer(TIWLAN_DC *pDc)
{
	unsigned int i;
	txdesc_t *pTxDesc;

	FN_ENTER;
	for (i = 0; i < pDc->tx_pool_count; i++)
	{
		pTxDesc = &pDc->pTxDescQPool[i];

		if ((pTxDesc->Ctl & DESC_CTL_DONE) == DESC_CTL_DONE)
			acxlog(L_BUF, "txbuf %d done\n", i);
	}
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_clean_tx_desc
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
*	This function probably resets the txdescs' status when the ACX100
*	signals the TX done IRQ (txdescs have been processed), starting with
*	the pool index of the descriptor which we would use next,
*	in order to make sure that we can be as fast as possible
*	in filling new txdescs.
*	Oops, now we have our own index, so everytime we get called we know
*	where the next packet to be cleaned is.
*	Hmm, still need to loop through the whole ring buffer now,
*	since we lost sync for some reason when ping flooding or so...
*	(somehow we don't get the IRQ for acx100_clean_tx_desc any more when
*	too many packets are being sent!)
*	FIXME: currently we only process one packet, but this gets out of
*	sync for some reason when ping flooding, so we need to loop,
*	but the previous smart loop implementation causes the ping latency
*	to rise dramatically (~3000 ms), at least on CardBus PheeNet WL-0022.
*	Dunno what to do :-\
*
*----------------------------------------------------------------*/

void acx100_clean_tx_desc(wlandevice_t *wlandev)
{
	TIWLAN_DC *pDc = &wlandev->dc;
	txdesc_t *pTxDesc;
	UINT finger, watch;
	unsigned long flags;

	FN_ENTER;

	acx100_log_txbuffer(pDc);
	acxlog(L_BUF, "cleaning up Tx bufs from %d\n", pDc->tx_tail);

	spin_lock_irqsave(&tx_lock, flags);

	finger = pDc->tx_tail;
	watch = finger;

	do {
		pTxDesc = &pDc->pTxDescQPool[finger];

		/* check if txdesc is marked as "Tx finished" and "owned" */
		if ((pTxDesc->Ctl & DESC_CTL_DONE) == DESC_CTL_DONE) {

			acxlog(L_BUF, "cleaning %d\n", finger);

			if (pTxDesc->error != 0) {
				char *err;

				switch(pTxDesc->error) {
					case 0x10:
						err = "MSDU lifetime timeout? - change 'iwconfig retry lifetime XXX'";
						break;
					case 0x20:
						err = "maybe distance too high? - change 'iwconfig txpower XXX'";
						break;
					default:
						err = "unknown error";
						break;
				}
				acxlog(L_STD, "Tx error occurred (error 0x%02X)!! (%s)\n", pTxDesc->error, err);
				wlandev->stats.tx_carrier_errors++;
				wlandev->stats.tx_errors++;
			}

			/* free it */
			pTxDesc->Ctl = DESC_CTL_FREE;
			
			wlandev->TxQueueFree++;

			if ((wlandev->TxQueueFree >= MINFREE_TX + 3)
			&& (wlandev->status == ISTATUS_4_ASSOCIATED)
			&& (netif_queue_stopped(wlandev->netdev)))
			{
				/* FIXME: if construct is ugly:
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
		finger = (finger + 1) % pDc->tx_pool_count;
	} while (watch != finger);

	/* remember last position */
	pDc->tx_tail = finger;

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

void acx100_rxmonitor(wlandevice_t * wlandev, struct rxbuffer *buf)
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

	FN_ENTER;

	if (!(wlandev->rx_config_1 & RX_CFG1_PLUS_ADDIT_HDR))
	{
		printk("rx_config_1 misses RX_CFG1_PLUS_ADDIT_HDR\n");
		FN_EXIT(0, 0);
		return;
	}

	if (wlandev->rx_config_1 & RX_CFG1_PLUS_ADDIT_HDR)
	{
		payload_offset += 3*4; /* status words         */
		packet_len     += 3*4; /* length is w/o status */
	}
		
	if (wlandev->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR)
		payload_offset += 4;   /* phy header   */

	/* we are in big luck: the acx100 doesn't modify any of the fields */
	/* in the 802.11-frame. just pass this packet into the PF_PACKET-  */
	/* subsystem. yeah. */

	skb_len = packet_len - payload_offset;

	if (wlandev->netdev->type == ARPHRD_IEEE80211_PRISM)
		skb_len += sizeof(p80211msg_lnxind_wlansniffrm_t);

		/* sanity check */
	if (skb_len > (WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN + WLAN_CRC_LEN))
	{
		printk("monitor mode panic: oversized frame!\n");
		FN_EXIT(0, 0);
		return;
	}

		/* allocate skb */
	if ( (skb = dev_alloc_skb(skb_len)) == NULL)
	{
		printk("alloc_skb failed trying to allocate %d bytes\n", skb_len);
		FN_EXIT(0, 0);
		return;
	}

	skb_put(skb, skb_len);

		/* when in raw 802.11 mode, just copy frame as-is */
	if (wlandev->netdev->type == ARPHRD_IEEE80211)
		datap = skb->data;
	else  /* otherwise, emulate prism header */
	{
		msg = (p80211msg_lnxind_wlansniffrm_t*)skb->data;
		datap = msg + 1;
		
		msg->msgcode = DIDmsg_lnxind_wlansniffrm;
		msg->msglen = sizeof(p80211msg_lnxind_wlansniffrm_t);
		strcpy(msg->devname, wlandev->netdev->name);
		
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

	skb->dev = wlandev->netdev;
	skb->dev->last_rx = jiffies;

	skb->mac.raw = skb->data;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_80211_RAW);

	wlandev->stats.rx_packets++;
	wlandev->stats.rx_bytes += skb->len;

	netif_rx(skb);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_log_rxbuffer
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

void acx100_log_rxbuffer(TIWLAN_DC *pDc)
{
	unsigned int i;
	struct rxhostdescriptor *pDesc;

	FN_ENTER;
	for (i = 0; i < pDc->rx_pool_count; i++)
	{
		pDesc = &pDc->pRxHostDescQPool[i];
#if (WLAN_HOSTIF==WLAN_USB)
		acxlog(L_DEBUG,"rxbuf %d Ctl=%X  val0x14=%X\n",i,pDesc->Ctl,pDesc->val0x14);
#endif
		if ((pDesc->Ctl & DESC_CTL_FREE) && (pDesc->val0x14 < 0))
			acxlog(L_BUF, "rxbuf %d full\n", i);
	}
	FN_EXIT(0, 0);
}

/*------------------------------------------------------------------------------
 * acx100_process_rx_desc
 *
 * RX path top level entry point called directly and only from the IRQ handler
 *
 * Arguments:
 *
 * Returns:
 *
 * Side effects:
 *
 * Call context: Hard IRQ
 *
 * STATUS:
 *
 * Comment:
 *
 *----------------------------------------------------------------------------*/
void acx100_process_rx_desc(wlandevice_t *wlandev)
{
	struct rxhostdescriptor *RxPool;
	TIWLAN_DC *pDc;
	struct rxhostdescriptor *pDesc;
	UINT16 buf_len;
	unsigned long flags;
	int curr_idx;
	unsigned int count = 0;
	p80211_hdr_t *buf;

	FN_ENTER;

	pDc = &wlandev->dc;
	acx100_log_rxbuffer(pDc);

	/* there used to be entry count code here, but because this function is called
	 * by the interrupt handler, we are sure that this will only be entered once
	 * because the kernel locks the interrupt handler */

	RxPool = pDc->pRxHostDescQPool;

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * rx_tail and the full descriptor we're supposed to handle. */
	spin_lock_irqsave(&rx_lock, flags);
	do {
		count++;
		if (count > pDc->rx_pool_count)
		{ /* hmm, no luck: all descriptors empty, bail out */
			spin_unlock_irqrestore(&rx_lock, flags);
			FN_EXIT(0, 0);
			return;
		}
		curr_idx = pDc->rx_tail;
		pDesc = &RxPool[pDc->rx_tail];
		pDc->rx_tail = (pDc->rx_tail + 1) % pDc->rx_pool_count;
	}
	/* "pDesc->val0x14 < 0" is there to check whether MSB
	 * is set or not */
	while (!((pDesc->Ctl & DESC_CTL_FREE) && (pDesc->val0x14 < 0))); /* check whether descriptor full, advance to next one if not */
	spin_unlock_irqrestore(&rx_lock, flags);

	while (1)
	{
		acxlog(L_BUF, "%s: using curr_idx %d, rx_tail is now %d\n", __func__, curr_idx, pDc->rx_tail);

		buf = (p80211_hdr_t *)&pDesc->data->buf;
		if (wlandev->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
			/* take into account additional header in front of packet */
			buf = (p80211_hdr_t*)((UINT8*)buf + 4);
		}

		buf_len = pDesc->data->status & 0xfff;      /* somelength */
		if ((WLAN_GET_FC_FSTYPE(buf->a3.fc) != WLAN_FSTYPE_BEACON)
		||  (debug & L_XFER_BEACON))
			acxlog(L_XFER|L_DATA, "Rx pkt %02d (%s): time %lu, len %i, signal %d, SNR %d, mode %d, status %d\n",
				curr_idx,
				acx100_get_packet_type_string(buf->a3.fc),
				pDesc->data->time,
				buf_len,
				pDesc->data->level,
				pDesc->data->snr,
				wlandev->mode,
				wlandev->status);

		/* I tried to figure out how to map these levels to dBm
		 * values, but for the life of me I really didn't
		 * manage to get it. Either these values are not meant to
		 * be expressed in dBm, or it's some pretty complicated
		 * calculation. */
		wlandev->wstats.qual.level = pDesc->data->level * 100 / 255;
		wlandev->wstats.qual.noise = pDesc->data->snr * 100 / 255;
		wlandev->wstats.qual.qual =
			(wlandev->wstats.qual.noise <= 100) ?
			      100 - wlandev->wstats.qual.noise : 0;
		wlandev->wstats.qual.updated = 7;

		if (wlandev->monitor) {
			acx100_rxmonitor(wlandev, pDesc->data);
		} else if (buf_len >= 14) {
			acx100_rx_ieee802_11_frame(wlandev, pDesc);
		} else {
			acxlog(L_DEBUG | L_XFER | L_DATA,
			       "NOT receiving packet (%s): size too small (%d)\n",
			       acx100_get_packet_type_string(buf->a3.fc), buf_len);
		}

		pDesc->Ctl &= ~DESC_CTL_FREE; /* Host no longer owns this */
		pDesc->val0x14 = 0;

		/* ok, descriptor is handled, now check the next descriptor */
		spin_lock_irqsave(&rx_lock, flags);
		curr_idx = pDc->rx_tail;
		pDesc = &RxPool[pDc->rx_tail];

		/* if next descriptor is empty, then bail out */
		if (!((pDesc->Ctl & DESC_CTL_FREE) && (pDesc->val0x14 < 0)))
		{
			spin_unlock_irqrestore(&rx_lock, flags);
			break;
		}
		else
		{
			pDc->rx_tail = (pDc->rx_tail + 1) % pDc->rx_pool_count;
			spin_unlock_irqrestore(&rx_lock, flags);
		}
	}
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_create_tx_host_desc_queue
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

int acx100_create_tx_host_desc_queue(TIWLAN_DC * pDc)
{
	wlandevice_t *wlandev;

	UINT i;
	UINT align_offs;
	UINT alignment;

	struct framehdr *frame_hdr;
	struct framehdr *frame_hdr_phy;

	UINT8 *frame_payload;
	UINT8 *frame_payload_phy;

	struct txhostdescriptor *host_desc;
	struct txhostdescriptor *host_desc_phy;

	FN_ENTER;

	wlandev = pDc->wlandev;

	/* allocate TX header pool */
	pDc->FrameHdrQPoolSize = (wlandev->TxQueueNo * sizeof(struct framehdr));
#if (WLAN_HOSTIF!=WLAN_USB)
	if (!(pDc->pFrameHdrQPool =
	      pci_alloc_consistent(0, pDc->FrameHdrQPoolSize, &pDc->FrameHdrQPoolPhyAddr))) {
		acxlog(L_BINSTD, "pDc->pFrameHdrQPool memory allocation error\n");
		FN_EXIT(1, 2);
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->pFrameHdrQPool = 0x%8x\n", (UINT) pDc->pFrameHdrQPool);
	acxlog(L_BINDEBUG, "pDc->pFrameHdrQPoolPhyAddr = 0x%8x\n", (UINT) pDc->FrameHdrQPoolPhyAddr);
#else
	if ((pDc->pFrameHdrQPool=kmalloc(pDc->FrameHdrQPoolSize,GFP_KERNEL))==NULL) {
		acxlog(L_STD,"pDc->pFrameHdrQPool memory allocation error\n");
		FN_EXIT(1,2);
		return(2);
	}
	memset(pDc->pFrameHdrQPool,0,pDc->FrameHdrQPoolSize);
#endif

	/* allocate TX payload pool */
	pDc->TxBufferPoolSize = wlandev->TxQueueNo*2 * (WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN);
#if (WLAN_HOSTIF!=WLAN_USB)
	if (!(pDc->pTxBufferPool =
	      pci_alloc_consistent(0, pDc->TxBufferPoolSize, &pDc->TxBufferPoolPhyAddr))) {
		acxlog(L_BINSTD, "pDc->pTxBufferPool memory allocation error\n");
		pci_free_consistent(0, pDc->FrameHdrQPoolSize,
				    pDc->pFrameHdrQPool,
				    pDc->FrameHdrQPoolPhyAddr);
		FN_EXIT(1, 2);
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->TxBufferPool = 0x%8x\n", (UINT) pDc->pTxBufferPool);
	acxlog(L_BINDEBUG, "pDc->TxBufferPoolPhyAddr = 0x%8x\n", (UINT) pDc->TxBufferPoolPhyAddr);
#else
	if ((pDc->pTxBufferPool=kmalloc(pDc->TxBufferPoolSize,GFP_KERNEL))==NULL) {
		acxlog(L_STD,"pDc->pTxBufferPool memory allocation error\n");
		kfree(pDc->pFrameHdrQPool);
		FN_EXIT(1,2);
		return(2);
	}
	memset(pDc->pTxBufferPool,0,pDc->TxBufferPoolSize);
#endif

	/* allocate the TX host descriptor queue pool */
	pDc->TxHostDescQPoolSize =  wlandev->TxQueueNo*2 * sizeof(struct txhostdescriptor) + 3;
#if (WLAN_HOSTIF!=WLAN_USB)
	if (!(pDc->pTxHostDescQPool =
	      pci_alloc_consistent(0, pDc->TxHostDescQPoolSize,
				   &pDc->TxHostDescQPoolPhyAddr))) {
		acxlog(L_BINSTD, "Failed to allocate shared memory for TxHostDesc queue\n");
		pci_free_consistent(0, pDc->FrameHdrQPoolSize,
				    pDc->pFrameHdrQPool,
				    pDc->FrameHdrQPoolPhyAddr);
		pci_free_consistent(0, pDc->TxBufferPoolSize,
				    pDc->pTxBufferPool,
				    pDc->TxBufferPoolPhyAddr);
		FN_EXIT(1, 2);
		return 2;
	}
	acxlog(L_BINDEBUG, "pDc->pTxHostDescQPool = 0x%8x\n", (UINT) pDc->pTxHostDescQPool);
	acxlog(L_BINDEBUG, "pDc->TxHostDescQPoolPhyAddr = 0x%8x\n", pDc->TxHostDescQPoolPhyAddr);
#else
	if ((pDc->pTxHostDescQPool=kmalloc(pDc->TxHostDescQPoolSize,GFP_KERNEL))==NULL) {
		acxlog(L_STD,"Failed to allocate memory for TxHostDesc queue\n");
		kfree(pDc->pFrameHdrQPool);
		kfree(pDc->pTxBufferPool);
		FN_EXIT(1,2);
		return(2);
	}
	memset(pDc->pTxHostDescQPool,0,pDc->TxHostDescQPoolSize);
#endif

#if (WLAN_HOSTIF!=WLAN_USB)
	/* check for proper alignment of TX host descriptor pool */
	alignment = (UINT) pDc->pTxHostDescQPool & 3;
	if (alignment) {
		acxlog(L_BINSTD, "%s: TxHostDescQPool not aligned properly\n", __func__);
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	host_desc = (struct txhostdescriptor *) ((UINT8 *) pDc->pTxHostDescQPool + align_offs);
	host_desc_phy = (struct txhostdescriptor *) ((UINT8 *) pDc->TxHostDescQPoolPhyAddr + align_offs);
#else
	host_desc=(struct txhostdescriptor *)pDc->pTxHostDescQPool;
#endif
	frame_hdr = (struct framehdr *) pDc->pFrameHdrQPool;
#if (WLAN_HOSTIF!=WLAN_USB)
	frame_hdr_phy = (struct framehdr *) pDc->FrameHdrQPoolPhyAddr;
#endif

	frame_payload = (UINT8 *) pDc->pTxBufferPool;
#if (WLAN_HOSTIF!=WLAN_USB)
	frame_payload_phy = (UINT8 *) pDc->TxBufferPoolPhyAddr;
#endif

	for (i = 0; i < wlandev->TxQueueNo*2 - 1; i++)
	{
		if (!(i & 1)) {
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->data_phy = (UINT8 *) frame_hdr_phy;
#endif
			host_desc->data = (UINT8 *) frame_hdr;
#if (WLAN_HOSTIF!=WLAN_USB)			
			frame_hdr_phy++;
#endif
			frame_hdr++;
#if (WLAN_HOSTIF!=WLAN_USB)			
			host_desc->val0x10 = (struct txhostdescriptor *)((UINT8 *) host_desc_phy + sizeof(struct txhostdescriptor));
#endif
		} else {
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->data_phy = (UINT8 *) frame_payload_phy;
#endif
			host_desc->data = (UINT8 *) frame_payload;
#if (WLAN_HOSTIF!=WLAN_USB)			
			frame_payload_phy += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;
#endif
			frame_payload += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;
			
			host_desc->val0x10 = NULL;
		}

		host_desc->Ctl |= DESC_CTL_FREE;
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->desc_phy = host_desc_phy;
		host_desc->desc_phy_next = (struct txhostdescriptor *)((UINT8 *) host_desc_phy + sizeof(struct txhostdescriptor));
#endif

		host_desc++;
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc_phy++;
#endif
	}
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->data_phy = (UINT8 *) frame_payload_phy;
#endif
	host_desc->data = (UINT8 *) frame_payload;
	host_desc->val0x10 = 0;

	host_desc->Ctl |= DESC_CTL_FREE;
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->desc_phy = host_desc_phy;
	host_desc->desc_phy_next = (struct txhostdescriptor *)((UINT8 *) pDc->TxHostDescQPoolPhyAddr + align_offs);
#endif

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_create_rx_host_desc_queue
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

int acx100_create_rx_host_desc_queue(TIWLAN_DC * pDc)
{
	wlandevice_t *wlandev;

	UINT i;
	UINT align_offs;
	UINT alignment;

	struct rxbuffer *data;
	struct rxbuffer *data_phy;

	struct rxhostdescriptor *host_desc;
	struct rxhostdescriptor *host_desc_phy;

	int result = 0;

	FN_ENTER;

	wlandev = pDc->wlandev;

	/* allocate the RX host descriptor queue pool */
	pDc->RxHostDescQPoolSize = (wlandev->RxQueueNo * sizeof(struct rxhostdescriptor)) + 0x3;
#if (WLAN_HOSTIF!=WLAN_USB)
	if ((pDc->pRxHostDescQPool =
	     pci_alloc_consistent(0, pDc->RxHostDescQPoolSize,
				  &pDc->RxHostDescQPoolPhyAddr)) == NULL) {
		acxlog(L_BINSTD,
		       "Failed to allocate shared memory for RxHostDesc queue\n");
		result = 2;
		goto fail;
	}
#else
	if ((pDc->pRxHostDescQPool = kmalloc(pDc->RxHostDescQPoolSize,GFP_KERNEL))==NULL) {
		acxlog(L_STD,"Failed to allocate memory for RxHostDesc queue\n");
		result=2;
		goto fail;
	}
	memset(pDc->pRxHostDescQPool,0,pDc->RxHostDescQPoolSize);
#endif

	/* allocate RX buffer pool */
	pDc->RxBufferPoolSize = (wlandev->RxQueueNo * sizeof(struct rxbuffer));
#if (WLAN_HOSTIF!=WLAN_USB)
	if ((pDc->pRxBufferPool =
	     pci_alloc_consistent(0, pDc->RxBufferPoolSize,
				  &pDc->RxBufferPoolPhyAddr)) == NULL) {
		acxlog(L_BINSTD, "Failed to allocate shared memory for Rx buffer\n");
		pci_free_consistent(0, pDc->RxHostDescQPoolSize,
				    pDc->pRxHostDescQPool,
				    pDc->RxHostDescQPoolPhyAddr);
		result = 2;
		goto fail;
	}
#else
	if ((pDc->pRxBufferPool = kmalloc(pDc->RxBufferPoolSize,GFP_KERNEL))==NULL) {
		acxlog(L_STD,"Failed to allocate memory for Rx buffer\n");
		result=2;
		goto fail;
	}
	memset(pDc->pRxBufferPool,0,pDc->RxBufferPoolSize);
#endif

	acxlog(L_BINDEBUG, "pDc->pRxHostDescQPool = 0x%8x\n", (UINT) pDc->pRxHostDescQPool);
#if (WLAN_HOSTIF!=WLAN_USB)
	acxlog(L_BINDEBUG, "pDc->RxHostDescQPoolPhyAddr = 0x%8x\n", (UINT) pDc->RxHostDescQPoolPhyAddr);
#endif
	acxlog(L_BINDEBUG, "pDc->pRxBufferPool = 0x%8x\n", (UINT) pDc->pRxBufferPool);
#if (WLAN_HOSTIF!=WLAN_USB)
	acxlog(L_BINDEBUG, "pDc->RxBufferPoolPhyAddr = 0x%8x\n", (UINT) pDc->RxBufferPoolPhyAddr);
#endif
#if (WLAN_HOSTIF!=WLAN_USB)
	/* check for proper alignment of RX host descriptor pool */
	if ((alignment = ((UINT) pDc->pRxHostDescQPool) & 3)) {
		acxlog(L_BINSTD, "acx100_create_rx_host_desc_queue: RxHostDescQPool not aligned properly\n");
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	host_desc = (struct rxhostdescriptor *) ((UINT8 *) pDc->pRxHostDescQPool + align_offs);
	host_desc_phy = (struct rxhostdescriptor *) ((UINT8 *) pDc->RxHostDescQPoolPhyAddr + align_offs);

	wlandev->RxHostDescPoolStart = host_desc_phy;
#else
	host_desc = (struct rxhostdescriptor *)pDc->pRxHostDescQPool;
#endif
	data = (struct rxbuffer *) pDc->pRxBufferPool;
#if (WLAN_HOSTIF!=WLAN_USB)
	data_phy = (struct rxbuffer *) pDc->RxBufferPoolPhyAddr;
#endif

	if (wlandev->RxQueueNo != 1) {
		for (i = 0; i < wlandev->RxQueueNo - 1; i++) {
			host_desc->data = data;
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->data_phy = data_phy;
#endif
			host_desc->length = sizeof(struct rxbuffer);

			data++;
#if (WLAN_HOSTIF!=WLAN_USB)
			data_phy++;
#endif

			/* FIXME: what do these mean ? */
			host_desc->val0x28 = 2;
			host_desc->Ctl &= ~0x80;
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->desc_phy = host_desc_phy;
			host_desc->desc_phy_next = (struct rxhostdescriptor *)((UINT8 *) host_desc_phy + sizeof(struct rxhostdescriptor));
#endif
			host_desc++;
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc_phy++;
#endif
		}
	}
	host_desc->data = data;
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->data_phy = data_phy;
#endif
	host_desc->length = sizeof(struct rxbuffer);

	host_desc->val0x28 = 2;
	host_desc->Ctl &= 0xff7f;
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->desc_phy = host_desc_phy;
	host_desc->desc_phy_next = (struct rxhostdescriptor *)((UINT8 *) pDc->RxHostDescQPoolPhyAddr + align_offs);
#endif
	result = 0;
fail:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_create_tx_desc_queue
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

void acx100_create_tx_desc_queue(TIWLAN_DC * pDc)
{
	wlandevice_t *wlandev;

	UINT32 mem_offs;
	UINT32 i;

	struct txdescriptor *tx_desc;
	struct txhostdescriptor *tx_hostdesc;

	UINT hostmemptr;

	FN_ENTER;

	wlandev = pDc->wlandev;
	pDc->tx_pool_count = wlandev->TxQueueNo;
#if (WLAN_HOSTIF!=WLAN_USB)

	pDc->pTxDescQPool = (struct txdescriptor *) (wlandev->iobase2 +
				     pDc->ui32ACXTxQueueStart);

	acxlog(L_BINDEBUG, "wlandev->iobase2 = 0x%08x\n", wlandev->iobase2);
	acxlog(L_BINDEBUG, "pDc->ui32ACXTxQueueStart = 0x%08x\n",
	       pDc->ui32ACXTxQueueStart);
	acxlog(L_BINDEBUG, "pDc->pTxDescQPool = 0x%08x\n",
	       (UINT) pDc->pTxDescQPool);
#endif
	wlandev->TxQueueFree = wlandev->TxQueueNo;
	pDc->tx_head = 0;
	pDc->tx_tail = 0;
#if (WLAN_HOSTIF!=WLAN_USB)
	mem_offs = pDc->ui32ACXTxQueueStart;
	tx_desc = pDc->pTxDescQPool;

	hostmemptr = pDc->TxHostDescQPoolPhyAddr;
	tx_hostdesc = (struct txhostdescriptor *) pDc->pTxHostDescQPool;

	/* loop over complete send pool */
	for (i = 0; i < pDc->tx_pool_count; i++) {
		memset(tx_desc, 0, sizeof(struct txdescriptor));
		/* pointer to hostdesc memory */
		tx_desc->HostMemPtr = hostmemptr;
		/* initialise ctl */
		tx_desc->Ctl = DESC_CTL_INIT;
		tx_desc->something2 = 0;
		/* point to next txdesc */
		tx_desc->pNextDesc = mem_offs + sizeof(struct txdescriptor);
		/* pointer to first txhostdesc */
		tx_desc->host_desc = tx_hostdesc;

		/* reserve two (hdr desc and payload desc) */
		tx_hostdesc += 2;
		hostmemptr += 2 * sizeof(struct txhostdescriptor);
		/* go to the next */
		mem_offs += sizeof(struct txdescriptor);
		tx_desc++;
	}
	/* go to the last one */
	tx_desc--;
	/* and point to the first making it a ring buffer */
	tx_desc->pNextDesc = pDc->ui32ACXTxQueueStart;
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_create_rx_desc_queue
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

void acx100_create_rx_desc_queue(TIWLAN_DC * pDc)
{
	wlandevice_t *wlandev;

	UINT32 mem_offs;
	UINT32 i;

	struct rxdescriptor *rx_desc;

	FN_ENTER;
	wlandev = pDc->wlandev;
#if (WLAN_HOSTIF!=WLAN_USB)
	/* WHY IS IT "TxQueueNo" ? */
	pDc->pRxDescQPool = (struct rxdescriptor *) ((wlandev->TxQueueNo * sizeof(struct txdescriptor)) + (UINT8 *) pDc->pTxDescQPool);
#endif
	pDc->rx_pool_count = wlandev->RxQueueNo;
	pDc->rx_tail = 0;
#if (WLAN_HOSTIF!=WLAN_USB)
	mem_offs = pDc->ui32ACXRxQueueStart;
	rx_desc = (struct rxdescriptor *) pDc->pRxDescQPool;

	/* loop over complete receive pool */
	for (i = 0; i < pDc->rx_pool_count; i++) {
		memset(rx_desc, 0, sizeof(struct rxdescriptor));

		/* FIXME: what is this? is it a ctl field ? */
		rx_desc->val0x28 = 0xc;

		/* point to next rxdesc */
		rx_desc->pNextDesc = mem_offs + sizeof(struct rxdescriptor); // next rxdesc pNextDesc

		/* go to the next */
		mem_offs += sizeof(struct rxdescriptor);
		rx_desc++;
	}
	/* go to the last one */
	rx_desc--;
	/* and point to the first making it a ring buffer */
	rx_desc->pNextDesc = pDc->ui32ACXRxQueueStart;
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_free_desc_queues
*
*	Releases the queues that have been allocated, the
*	others have been initialised to NULL in acx100.c so this
*	function can be used if only part of the queues where
*	allocated.
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

void acx100_free_desc_queues(TIWLAN_DC * pDc)
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

	pDc->pTxDescQPool = NULL;
	pDc->tx_pool_count = 0;

	if (pDc->pRxHostDescQPool) {
		pci_free_consistent(0, pDc->RxHostDescQPoolSize,
				    pDc->pRxHostDescQPool,
				    pDc->RxHostDescQPoolPhyAddr);
		pDc->pRxHostDescQPool = NULL;
		pDc->RxHostDescQPoolSize = 0;
	}
	if (pDc->pRxBufferPool) {
		pci_free_consistent(0, pDc->RxBufferPoolSize,
				    pDc->pRxBufferPool,
				    pDc->RxBufferPoolPhyAddr);
		pDc->pRxBufferPool = NULL;
		pDc->RxBufferPoolSize = 0;
	}

	pDc->pRxDescQPool = NULL;
	pDc->rx_pool_count = 0;
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_init_memory_pools
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
*	FIXME: This function still needs a cleanup
*----------------------------------------------------------------*/

int acx100_init_memory_pools(wlandevice_t * wlandev, memmap_t * mmt)
{
#if (WLAN_HOSTIF==WLAN_USB)
	UINT TotalMemoryBlocks;  // var_40
	acx100usb_memmap_t *map;
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
		UINT16 rid;
		UINT16 length;
		UINT DMA_config; // val0x4
		struct rxhostdescriptor *val0x8;
		UINT val0xc;  /* rx memory */
		UINT val0x10;   /* tx memory */
		UINT16 TxBlockNum; // val0x14
		UINT16 RxBlockNum; // val0x16;
	} MemoryConfigOption;    // var_2c

	FN_ENTER;
	map = (acx100usb_memmap_t *)mmt;
#ifdef ACX_DEBUG
	acxlog(L_DEBUG,"Dump of Memory Map:\n");
	acx100usb_dump_bytes(mmt,44);
#endif
	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = wlandev->memblocksize;

	/* Then we alert the card to our decision of block size */
	if (!acx100_configure(wlandev, &MemoryBlockSize, ACX100_RID_BLOCK_SIZE)) {
		acxlog(L_BINSTD, "Ctl: MemoryBlockSizeWrite failed\n");
		return 0;
	}

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers. IE. end-start/size */
	TotalMemoryBlocks = (map->PoolEnd-map->PoolStart) / wlandev->memblocksize;
	acxlog(L_DEBUG,"TotalMemoryBlocks=%d (%d bytes)\n",TotalMemoryBlocks,TotalMemoryBlocks*wlandev->memblocksize);

	/* This one I have no idea on */
	/* block-transfer=0x20000
	 * indirect descriptors=0x10000
	 */
	MemoryConfigOption.DMA_config = 0x20000; //dma config
	/* Declare start of the Rx host pool */
	MemoryConfigOption.val0x8 = wlandev->RxHostDescPoolStart;

	/* 50% of the allotment of memory blocks go to tx descriptors */
	MemoryConfigOption.TxBlockNum = TotalMemoryBlocks / 2;
	/* and 50% go to the rx descriptors */
	MemoryConfigOption.RxBlockNum = TotalMemoryBlocks - MemoryConfigOption.TxBlockNum;

	/* in this block, we save the information we gleaned from the
	   card into our wlandevice structure; # of tx desc blocks */
	wlandev->val0x24fc = MemoryConfigOption.TxBlockNum;
	/* # of rx desc blocks */
	wlandev->val0x24e4 = MemoryConfigOption.RxBlockNum;
	/* size of the tx and rx descriptor queues */
	wlandev->val0x2500 = MemoryConfigOption.TxBlockNum * wlandev->memblocksize;
	wlandev->val0x24e8 = MemoryConfigOption.RxBlockNum * wlandev->memblocksize;

	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.val0xc = (map->PoolStart + 0x1f) & 0xffffffe0;
	/* align the rx descriptor queue to units of 0x20 and offset it
	   by the tx descriptor queue */
	MemoryConfigOption.val0x10 =
	    (0x1f + map->PoolStart + wlandev->val0x2500) & 0xffffffe0;

	/* alert the device to our decision */
	if (!acx100_configure(wlandev, &MemoryConfigOption, ACX100_RID_MEMORY_CONFIG_OPTIONS)) {
		return 0;
	}
	/* and tell the device to kick it into gear */
	if (!acx100_issue_cmd(wlandev, ACX100_CMD_INIT_MEMORY, 0, 0, 5000)) {
		return 0;
	}
	FN_EXIT(0, 0);
	return(1);
#else
	/* the default PCI code */
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
		struct rxhostdescriptor *val0x8;
		UINT val0xc;
		UINT val0x10;
		UINT16 TxBlockNum; // val0x14
		UINT16 RxBlockNum; // val0x16;
	} MemoryConfigOption;    // var_2c

	FN_ENTER;

	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = wlandev->memblocksize;

	/* Then we alert the card to our decision of block size */
	if (!acx100_configure(wlandev, &MemoryBlockSize, ACX100_RID_BLOCK_SIZE)) {
		acxlog(L_BINSTD, "Ctl: MemoryBlockSizeWrite failed\n");
		FN_EXIT(1, 0);
		return 0;
	}

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers. IE. end-start/size */
	TotalMemoryBlocks = (mmt->m.cw2.val0x28 - mmt->m.cw2.vali) / wlandev->memblocksize;

	/* This one I have no idea on */
	MemoryConfigOption.DMA_config = 0x30000; //dma config
	/* Declare start of the Rx host pool */
	MemoryConfigOption.val0x8 = wlandev->RxHostDescPoolStart;

	/* 50% of the allotment of memory blocks go to tx descriptors */
	MemoryConfigOption.TxBlockNum = TotalMemoryBlocks / 2;
	/* and 50% go to the rx descriptors */
	MemoryConfigOption.RxBlockNum = TotalMemoryBlocks - MemoryConfigOption.TxBlockNum;

	/* in this block, we save the information we gleaned from the
	   card into our wlandevice structure; # of tx desc blocks */
	wlandev->val0x24fc = MemoryConfigOption.TxBlockNum;
	/* # of rx desc blocks */
	wlandev->val0x24e4 = MemoryConfigOption.RxBlockNum;
	/* size of the tx and rx descriptor queues */
	wlandev->val0x2500 = MemoryConfigOption.TxBlockNum * wlandev->memblocksize;
	wlandev->val0x24e8 = MemoryConfigOption.RxBlockNum * wlandev->memblocksize;

	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.val0xc = (mmt->m.cw2.vali + 0x1f) & 0xffffffe0;
	/* align the rx descriptor queue to units of 0x20 and offset it
	   by the tx descriptor queue */
	MemoryConfigOption.val0x10 =
	    (0x1f + mmt->m.cw2.vali + wlandev->val0x2500) & 0xffffffe0;

	/* alert the device to our decision */
	if (!acx100_configure(wlandev, &MemoryConfigOption, ACX100_RID_MEMORY_CONFIG_OPTIONS)) {
		FN_EXIT(1, 0);
		return 0;
	}
	/* and tell the device to kick it into gear */
	if (!acx100_issue_cmd(wlandev, ACX100_CMD_INIT_MEMORY, 0, 0, 5000)) {
		FN_EXIT(1, 0);
		return 0;
	}

	FN_EXIT(1, 1);
	return 1;
#endif
}

/*----------------------------------------------------------------
* acx100_get_tx_desc
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

struct txdescriptor *acx100_get_tx_desc(wlandevice_t * wlandev)
{
	struct TIWLAN_DC * pDc = &wlandev->dc;
	struct txdescriptor *tx_desc;
	unsigned long flags;

	FN_ENTER;

	spin_lock_irqsave(&tx_lock, flags);

	tx_desc = &pDc->pTxDescQPool[pDc->tx_head];

	if ((tx_desc->Ctl & DESC_CTL_FREE) == 0) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
		tx_desc = NULL;
		goto error;
	}

	wlandev->TxQueueFree--;
	acxlog(L_BUF, "got Tx desc %d, %d remain.\n", pDc->tx_head, wlandev->TxQueueFree);

/*
 * This comment is probably not entirely correct, needs further discussion
 * (restored commented-out code below to fix Tx ring buffer overflow,
 * since it's much better to have a slightly less efficiently used ring
 * buffer rather than one which easily overflows):
 *
 * This doesn't do anything other than limit our maximum number of
 * buffers used at a single time (we might as well just declare
 * MINFREE_TX less descriptors when we open up.) We should just let it
 * slide here, and back off MINFREE_TX in acx100_clean_tx_desc, when given the
 * opportunity to let the queue start back up.
 */
	if (wlandev->TxQueueFree < MINFREE_TX)
	{
		acxlog(L_XFER, "stop queue (avail. Tx desc %d).\n", wlandev->TxQueueFree);
		netif_stop_queue(wlandev->netdev);
	}

	/* returning current descriptor, so advance to next free one */
	pDc->tx_head = (pDc->tx_head + 1) % pDc->tx_pool_count;
error:
	spin_unlock_irqrestore(&tx_lock, flags);

	FN_EXIT(0, (int)tx_desc);
	return tx_desc;
}

static char type_string[32];	/* I *really* don't care that this is static,
				   so don't complain, else... ;-) */

/*----------------------------------------------------------------
* acx100_get_packet_type_string
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

char *acx100_get_packet_type_string(UINT16 fc)
{
	char *ftype = "UNKNOWN", *fstype = "UNKNOWN";

	FN_ENTER;
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
	FN_EXIT(1, (int)type_string);
	return type_string;
}
