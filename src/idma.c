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

#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif /* WE >= 13 */

#include <wlan_compat.h>

#include <linux/pci.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
#include <linux/dma-mapping.h>
#endif



/*================================================================*/
/* Project Includes */

#include <acx.h>

/* Now that the pci_alloc_consistent() problem has been resolved,
 * feel free to modify buffer count for ACX100 to 32, too.
 * But it's not required since the card isn't too fast anyway */
#define RXBUFFERCOUNT_ACX100 16
#define TXBUFFERCOUNT_ACX100 16
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
/* dma_alloc_coherent() uses GFP_KERNEL, much less problematic than
 * the pci_alloc_consistent() used below using GFP_ATOMIC (quite often causes
 * a larger alloc to fail), so use less buffers there to be more successful */
#define RXBUFFERCOUNT_ACX111 32
#define TXBUFFERCOUNT_ACX111 32
#else
#define RXBUFFERCOUNT_ACX111 16
#define TXBUFFERCOUNT_ACX111 16
#endif
#define TXBUFFERCOUNT_USB 10
#define RXBUFFERCOUNT_USB 10

#define MINFREE_TX 3

static const char *acx_get_packet_type_string(u16 fc);
void acx_dump_bytes(const void *, int);
#if (WLAN_HOSTIF==WLAN_USB)
extern void acx100usb_tx_data(wlandevice_t *,void *);
#endif

static inline void *acx_alloc_coherent(struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle, int flag)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
	return dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, dma_handle, flag);
#else
#warning Using old PCI-specific DMA allocation, may fail with out-of-mem! Upgrade kernel if it does...
	return pci_alloc_consistent(hwdev, size, dma_handle);
#endif
}

static inline void acx_free_coherent(struct pci_dev *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
	dma_free_coherent(hwdev == NULL ? NULL : &hwdev->dev, size, vaddr, dma_handle);
#else
	pci_free_consistent(hwdev, size, vaddr, dma_handle);
#endif
}

/*----------------------------------------------------------------
* acx_free_desc_queues
*
*	Releases the queues that have been allocated, the
*	others have been initialised to NULL in acx100.c so this
*	function can be used if only part of the queues were
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
static void acx_free_desc_queues(TIWLAN_DC *pDc)
{
#if (WLAN_HOSTIF!=WLAN_USB)
#define ACX_FREE_QUEUE(size, ptr, phyaddr) \
	if (NULL != ptr) { \
		acx_free_coherent(0, size, ptr, phyaddr); \
		ptr = NULL; \
		size = 0; \
	}
#else
#define ACX_FREE_QUEUE(size, ptr, phyaddr) \
	if (NULL != ptr) { \
		kfree(ptr); \
		ptr = NULL; \
		size = 0; \
	}
#endif

	FN_ENTER;

	ACX_FREE_QUEUE(pDc->TxHostDescQPoolSize, pDc->pTxHostDescQPool, pDc->TxHostDescQPoolPhyAddr);
	ACX_FREE_QUEUE(pDc->FrameHdrQPoolSize, pDc->pFrameHdrQPool, pDc->FrameHdrQPoolPhyAddr);
	ACX_FREE_QUEUE(pDc->TxBufferPoolSize, pDc->pTxBufferPool, pDc->TxBufferPoolPhyAddr);
	
	pDc->pTxDescQPool = NULL;
	pDc->tx_pool_count = 0;

	ACX_FREE_QUEUE(pDc->RxHostDescQPoolSize, pDc->pRxHostDescQPool, pDc->RxHostDescQPoolPhyAddr);
	ACX_FREE_QUEUE(pDc->RxBufferPoolSize, pDc->pRxBufferPool, pDc->RxBufferPoolPhyAddr);

	pDc->pRxDescQPool = NULL;
	pDc->rx_pool_count = 0;

	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_dma_tx_data
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

/* NB: this table maps RATE111 bits to ACX100 tx descr constants */
static const u8 bitpos2rate100[] = {
	RATE100_1	,
	RATE100_2	,
	RATE100_5	,
	0		,
	0		,
	RATE100_11	,
	0		,
	0		,
	RATE100_22	,
};

void acx_dma_tx_data(wlandevice_t *priv, struct txdescriptor *tx_desc)
{
	struct peer *peer;
	struct txrate_ctrl *txrate;
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;
	int header_len, payload_len;
	u16 fc;
	u8 Ctl_8, Ctl2_8;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned long flags;
#endif

	FN_ENTER;
	/* header and payload are located in adjacent descriptors */
	header = tx_desc->fixed_size.s.host_desc;
	payload = header + 1;

	header_len = le16_to_cpu(header->length);
	payload_len = le16_to_cpu(payload->length);

#if (WLAN_HOSTIF!=WLAN_USB)
	/* need to protect access to Ctl field */
	spin_lock_irqsave(&priv->dc.tx_lock, flags);
#endif
	/* modify flag status in separate variable to be able to write it back
	 * in one big swoop later (also in order to have less device memory
	 * accesses) */
	Ctl_8 = tx_desc->Ctl_8;
	Ctl2_8 = tx_desc->Ctl2_8;
	
	/* DON'T simply set Ctl field to 0 here globally,
	 * it needs to maintain a consistent flag status (those are state flags!!),
	 * otherwise it may lead to severe disruption. Only set or reset particular
	 * flags at the exact moment this is needed...
	 * FIXME: what about Ctl2? Equally problematic? */


	/* let chip do RTS/CTS handshaking before sending
	 * in case packet size exceeds threshold */
	if (le16_to_cpu(tx_desc->total_length) > priv->rts_threshold)
		SET_BIT(Ctl2_8, DESC_CTL2_RTS);
	else
		CLEAR_BIT(Ctl2_8, DESC_CTL2_RTS);

#if DEBUG_WEP
	if (priv->wep_enabled)
		SET_BIT(Ctl2_8, DESC_CTL2_WEP);
	else
		CLEAR_BIT(Ctl2_8, DESC_CTL2_WEP);
#endif

	/* TODO: we really need ASSOCIATED_TO_AP/_TO_IBSS */
	if(priv->status == ACX_STATUS_4_ASSOCIATED
	&& priv->station_assoc.bssid[0] == 0
	)
		peer = &priv->ap_peer;
	else
		peer = &priv->defpeer;

	fc = le16_to_cpu(((p80211_hdr_t*)header->data)->a3.fc);
	if ((fc & WF_FC_FTYPE) == WF_FTYPE_MGMT)
		txrate = &peer->txbase;
	else
		txrate = &peer->txrate;
 
	tx_desc->fixed_size.s.txc = txrate; /* used in tx cleanup routine for auto rate and accounting */
 
	if (CHIPTYPE_ACX111 == priv->chip_type) {
		/* note that if !tx_desc->do_auto, txrate->cur
		** has only one nonzero bit */
		tx_desc->u.r2.rate111 = cpu_to_le16(
			txrate->cur | (txrate->pbcc511 ? RATE111_PBCC511 : 0)
			/* WARNING: I was never able to make it work with prism54 AP.
			** It was falling down to 1Mbit where shortpre is not applicable,
			** and not working at all at "5,11 basic rates only" setting.
			** I even didn't see tx packets in radio packet capture.
			** Disabled for now --vda */
			/*| ((peer->shortpre && txrate->cur!=RATE111_1) ? RATE111_SHORTPRE : 0) */
			);

		if(header_len != sizeof(struct framehdr)) {
			acxlog(L_DATA, "UHOH, packet has a different length than struct framehdr (0x%X vs. 0x%X)\n", header_len, sizeof(struct framehdr));

			/* copy the data at the right place */
			memcpy(header->data + header_len, payload->data, payload_len);
		}

		header_len += payload_len;
		header->length = cpu_to_le16(header_len);
	} else { /* ACX100 */
		/* FIXME: this expensive calculation part should be stored
		 * in advance whenever rate config changes! */

		/* set rate */
		/* find pos of highest nonzero bit */
		int n = 0;
		u16 t = txrate->cur;
		while(t>0x7) { t>>=3; n+=3; }
		while(t>1) { t>>=1; n++; }

		if (n >= sizeof(bitpos2rate100) || bitpos2rate100[n] == 0) {
			printk(KERN_ERR "%s: driver BUG! n=%d. please report\n", __func__, n);
			n = 0;
		}
		n = bitpos2rate100[n];

		if (txrate->pbcc511) {
			if (n == RATE100_5 || n == RATE100_11)
				n |= RATE100_PBCC511;
		}
		tx_desc->u.r1.rate = n;
 
		if (peer->shortpre && (txrate->cur != RATE111_1))
			SET_BIT(Ctl_8, DESC_CTL_SHORT_PREAMBLE); /* set Short Preamble */

		/* set autodma and reclaim and 1st mpdu */
		SET_BIT(Ctl_8, DESC_CTL_AUTODMA | DESC_CTL_RECLAIM | DESC_CTL_FIRSTFRAG);
	}
	/* don't need to clean ack/rts statistics here, already
	 * done on descr cleanup */

#if (WLAN_HOSTIF!=WLAN_USB)
	/* clears Ctl DESC_CTL_HOSTOWN bit, thus telling that the descriptors are now owned by the acx100; do this as LAST operation */
	CLEAR_BIT(Ctl_8, DESC_CTL_HOSTOWN);
	wmb(); /* make sure everything else is written */
	CLEAR_BIT(header->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
	CLEAR_BIT(payload->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));

	/* write back modified flags */
	tx_desc->Ctl2_8 = Ctl2_8;
	tx_desc->Ctl_8 = Ctl_8;

	spin_unlock_irqrestore(&priv->dc.tx_lock, flags);

	tx_desc->tx_time = cpu_to_le32(jiffies);
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_TXPRC);
#else
	/* write back modified flags */
	tx_desc->Ctl2_8 = Ctl2_8;
	tx_desc->Ctl_8 = Ctl_8;

	tx_desc->tx_time = cpu_to_le32(jiffies);
	acx100usb_tx_data(priv, tx_desc);
#endif
	/* log the packet content AFTER sending it,
	 * in order to not delay sending any further than absolutely needed
	 * Do separate logs for acx100/111 to have human-readable rates */
	if (unlikely(debug & (L_XFER | L_DATA))) {
		if (CHIPTYPE_ACX111 == priv->chip_type)
			printk("tx: pkt (%s): len %i (%i/%i) rate %04x%s status %d\n",
				acx_get_packet_type_string(le16_to_cpu(((p80211_hdr_t*)header->data)->a3.fc)),
			        le16_to_cpu(tx_desc->total_length),
				header_len,
				payload_len,
				le16_to_cpu(tx_desc->u.r2.rate111),
				(le16_to_cpu(tx_desc->u.r2.rate111) & RATE111_SHORTPRE) ? "(SPr)" : "",
				priv->status);
		else
			printk("tx: pkt (%s): len %i (%i/%i) rate %03d%s status %d\n",
				acx_get_packet_type_string(((p80211_hdr_t*)header->data)->a3.fc),
				tx_desc->total_length, /* endianness? */
				header_len,
				payload_len,
				tx_desc->u.r1.rate,
				(Ctl_8 & DESC_CTL_SHORT_PREAMBLE) ? "(SPr)" : "",
				priv->status);

		if (debug & L_DATA) {
			printk("tx: 802.11 header[%d]: ", header_len);
			acx_dump_bytes(header->data, header_len);
			printk("tx: 802.11 payload[%d]: ", payload_len);
			acx_dump_bytes(payload->data, payload_len);
		}
	}
	FN_EXIT0();
}

static void acx_handle_tx_error(wlandevice_t *priv, u8 error, unsigned int finger)
{
	const char *err = "unknown error";
	static unsigned int retry_errors = 0;

	/* hmm, should we handle this as a mask
	 * of *several* bits?
	 * For now I think only caring about
	 * individual bits is ok... */
	switch (error) {
		case 0x01:
			err = "no Tx due to error in other fragment";
			priv->wstats.discard.fragment++;
			break;
		case 0x02:
			err = "Tx aborted";
			priv->stats.tx_aborted_errors++;
			break;
		case 0x04:
			err = "Tx desc wrong parameters";
			priv->wstats.discard.misc++;
			break;
		case 0x08:
			err = "WEP key not found";
			priv->wstats.discard.misc++;
			break;
		case 0x10:
			err = "MSDU lifetime timeout? - try changing 'iwconfig retry lifetime XXX'";
			priv->wstats.discard.misc++;
			break;
		case 0x20:
			err = "excessive Tx retries due to either distance too high "
			"or unable to Tx or Tx frame error - try changing "
			"'iwconfig txpower XXX' or 'sens'itivity or 'retry'";
			priv->wstats.discard.retries++;
			/* FIXME: set (GETSET_TX|GETSET_RX) here
			 * (this seems to recalib radio on ACX100)
			 * after some more jiffies passed??
			 * But OTOH Tx error 0x20 also seems to occur on
			 * overheating, so I'm not sure whether we
			 * actually want that, since people maybe won't notice
			 * then that their hardware is slowly getting
			 * cooked...
			 * Or is it still a safe long distance from utter
			 * radio non-functionality despite many radio
			 * recalibs
			 * to final destructive overheating of the hardware?
			 * In this case we really should do recalib here...
			 * I guess the only way to find out is to do a
			 * potentially fatal self-experiment :-\
			 * Or maybe only recalib in case we're using Tx
			 * rate auto (on errors switching to lower speed
			 * --> less heat?) or 802.11 power save mode? */

			/* ok, just do it.
			 * ENABLE_TX|ENABLE_RX helps, so even do
			 * DISABLE_TX and DISABLE_RX in order to perhaps
			 * have more impact. */
			if (++retry_errors % 4 == 0) {
				if (retry_errors < 20)
					printk(KERN_WARNING "several excessive Tx retry errors occurred, attempting to recalibrate the radio!! This radio drift *might* be due to increasing card temperature, so you may want to verify proper card temperature, since recalibration might delay card over-temperature failure until it's too late (final fatal card damage). Just a (over?)cautious warning...\n");
				else
				if (retry_errors == 20)
					printk(KERN_WARNING "several radio recalibrations occurred, DISABLING notification message.\n");

				acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
			}
			break;
		case 0x40:
			err = "Tx buffer overflow";
			priv->stats.tx_fifo_errors++;
			break;
		case 0x80:
			err = "DMA error";
			priv->wstats.discard.misc++;
			break;
	}
	priv->stats.tx_errors++;
	if (priv->stats.tx_errors <= 20)
		acxlog(L_STD, "tx: error 0x%02X, buf %02d! (%s)\n", error, finger, err);
	else
		acxlog(L_STD, "tx: err 0x%02X, buf %02d!\n", error, finger);
}

/* Theory of operation:
priv->txrate.cfg is a bitmask of allowed (configured) rates.
It is set as a result of iwconfig rate N [auto]
or iwpriv set_rates "N,N,N N,N,N" commands.
It can be fixed (e.g. 0x0080 == 18Mbit only),
auto (0x00ff == 18Mbit or any lower value),
and code handles any bitmask (0x1081 == try 54Mbit,18Mbit,1Mbit _only_).

priv->txrate.cur is a value for rate111 field in tx descriptor.
It is always set to txrate_cfg sans zero or more most significant
bits. This routine handles selection of txrate_curr depending on
outcome of last tx event.

You cannot configure mixed usage of 5.5 and/or 11Mbit rate
with PBCC and CCK modulation. Either both at CCK or both at PBCC.
In theory you can implement it, but so far it is considered not worth doing.

22Mbit, of course, is PBCC always.
*/

/* maps acx100 tx descr rate field to acx111 one */
static u16
rate100to111(u8 r)
{
	switch(r) {
	case RATE100_1:	return RATE111_1;
	case RATE100_2:	return RATE111_2;
	case RATE100_5:
	case (RATE100_5 | RATE100_PBCC511):	return RATE111_5;
	case RATE100_11:
	case (RATE100_11 | RATE100_PBCC511):	return RATE111_11;
	case RATE100_22:	return RATE111_22;
	default:
		printk(KERN_DEBUG "Unexpected acx100 txrate of %d! Please report\n",r);
		return RATE111_2;
	}
}

static void
acx_handle_txrate_auto(wlandevice_t *priv, struct txrate_ctrl *txc, unsigned int idx, u8 rate100, u16 rate111, u8 error)
{
	u16 sent_rate;
	u16 cur = txc->cur;
	int slower_rate_was_used;

	/* FIXME: need to implement some kind of rate success memory
	 * which stores the success percentage per rate, to be taken
	 * into account when considering allowing a new rate, since it
	 * doesn't really help to stupidly count fallback/stepup,
	 * since one invalid rate will spoil the party anyway
	 * (such as 22M in case of 11M-only peers) */
 
	/* do some preparations, i.e. calculate the one rate that was
	 * used to send this packet */
	if (CHIPTYPE_ACX111 == priv->chip_type) {
		int n = 0;
		sent_rate = rate111 & RATE111_ALL;
		while (sent_rate>7) { sent_rate>>=3; n+=3; }
		while (sent_rate>1) { sent_rate>>=1; n++; }
		sent_rate = 1<<n;
	} else {
		sent_rate = rate100to111(rate100);
	}
	/* sent_rate has only one bit set now, corresponding to tx rate 
	 * which was used by hardware to tx this particular packet */
 
	/* now do the actual auto rate management */
	acxlog(L_DEBUG, "tx: sent_rate %s mask %04x/%04x/%04x, __=%d/%d, ^^=%d/%d\n",
		(txc->ignore_count > 0) ? "IGN" : "OK", sent_rate, cur, txc->cfg,
		txc->fallback_count, txc->fallback_threshold,
		txc->stepup_count, txc->stepup_threshold
	);

	/* we need to ignore old packets already in the tx queue since
	 * they use older rate bytes configured before our last rate change,
	 * otherwise our mechanism will get confused by interpreting old data.
	 * Do it here only, in order to have the logging above */
	if (txc->ignore_count) {
		txc->ignore_count--;
		return;
	}

	/* true only if the only nonzero bit in sent_rate is
	** less significant than highest nonzero bit in cur */
	slower_rate_was_used = ( cur > ((sent_rate<<1)-1) );
	
	if (slower_rate_was_used || (error & 0x30)) {
		txc->stepup_count = 0;
		if (++txc->fallback_count <= txc->fallback_threshold) 
			return;
		txc->fallback_count = 0;

		/* clear highest 1 bit in cur */
		sent_rate = RATE111_54;
		while (!(cur & sent_rate)) sent_rate >>= 1;
		CLEAR_BIT(cur, sent_rate);

		if (cur) { /* we can't disable all rates! */
			acxlog(L_XFER, "tx: falling back to sent_rate mask %04x\n", cur);
			txc->cur = cur;
			txc->ignore_count = priv->TxQueueCnt - priv->TxQueueFree;
		}
	} else if (!slower_rate_was_used) {
		txc->fallback_count = 0;
		if (++txc->stepup_count <= txc->stepup_threshold)
			return;
		txc->stepup_count = 0;

		/* sanitize. Sort of not needed, but I dont trust hw that much...
		** what if it can report bogus tx rates sometimes? */
		while (!(cur & sent_rate)) sent_rate >>= 1;
		/* try to find a higher sent_rate that isn't yet in our
		 * current set, but is an allowed cfg */
		while (1) {
			sent_rate <<= 1;
			if (sent_rate > txc->cfg)
				/* no higher rates allowed by config */
				return;
			if (!(cur & sent_rate) && (txc->cfg & sent_rate))
				/* found */
				break;
			/* not found, try higher one */
		}
		SET_BIT(cur, sent_rate);
		acxlog(L_XFER, "tx: stepping up to sent_rate mask %04x\n", cur);
		txc->cur = cur;
		txc->ignore_count = priv->TxQueueCnt - priv->TxQueueFree;
	}
}
 
/*----------------------------------------------------------------
* acx_log_txbuffer
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

static inline void acx_log_txbuffer(const TIWLAN_DC *pDc)
{
	unsigned int u;
	const txdesc_t *pTxDesc;

	/* no FN_ENTER here, we don't want that */
	/* no locks here, since it's entirely non-critical code */
	pTxDesc = pDc->pTxDescQPool;
	for (u = 0; u < pDc->tx_pool_count; u++) {
		if ((pTxDesc->Ctl_8 & DESC_CTL_DONE) == DESC_CTL_DONE)
			acxlog(L_DEBUG, "tx: buf %u done\n", u);
		pTxDesc = GET_NEXT_TX_DESC_PTR(pDc, pTxDesc);
	}
}

/*----------------------------------------------------------------
* acx_clean_tx_desc
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
*	(somehow we don't get the IRQ for acx_clean_tx_desc any more when
*	too many packets are being sent!)
*	FIXME: currently we only process one packet, but this gets out of
*	sync for some reason when ping flooding, so we need to loop,
*	but the previous smart loop implementation causes the ping latency
*	to rise dramatically (~3000 ms), at least on CardBus PheeNet WL-0022.
*	Dunno what to do :-\
*
*----------------------------------------------------------------*/

/* old locking will lock the whole processing loop,
 * whereas new locking will only lock inner parts, thus it's faster
 * (and hopefully still robust enough). */
#define OLD_LOCKING 0
#if OLD_LOCKING
#define OLD_SPIN_LOCK(x) spin_lock(x)
#define NEW_SPIN_LOCK(x)
#define OLD_SPIN_UNLOCK(x) spin_unlock(x)
#define NEW_SPIN_UNLOCK(x)
#else
#define OLD_SPIN_LOCK(x)
#define NEW_SPIN_LOCK(x) spin_lock(x)
#define OLD_SPIN_UNLOCK(x)
#define NEW_SPIN_UNLOCK(x) spin_unlock(x)
#endif

inline void acx_clean_tx_desc(wlandevice_t *priv)
{
	TIWLAN_DC *pDc = &priv->dc;
	txdesc_t *pTxDesc;
	unsigned int finger, watch;
	u8 error, ack_failures, rts_failures, rts_ok, r100; /* keep old desc status */
	u16 r111;
	unsigned int num_cleaned = 0, num_processed = 0;
	struct txrate_ctrl *txc;

	FN_ENTER;

	if (unlikely(debug & L_DEBUG))
		acx_log_txbuffer(pDc);
	acxlog(L_BUFT, "tx: cleaning up bufs from %d\n", pDc->tx_tail);

	OLD_SPIN_LOCK(&pDc->tx_lock);

	NEW_SPIN_LOCK(&pDc->tx_lock);
	finger = pDc->tx_tail;
	NEW_SPIN_UNLOCK(&pDc->tx_lock);
	watch = finger;

	do {
		pTxDesc = GET_TX_DESC_PTR(pDc, finger);

		NEW_SPIN_LOCK(&pDc->tx_lock);
		/* abort if txdesc is not marked as "Tx finished" and "owned" */
		if ((pTxDesc->Ctl_8 & DESC_CTL_DONE) != DESC_CTL_DONE) {
			NEW_SPIN_UNLOCK(&pDc->tx_lock);
			/* we do need to have at least one cleaned,
			 * otherwise we wouldn't get called in the first place.
			 * So better stay around some more, unless
			 * we already processed more descs than the ring
			 * size. */
			if ((num_cleaned == 0) && (num_processed < pDc->tx_pool_count))
				goto next;
			else
				break;
		}

		/* remember descr values... */
		error = pTxDesc->error;
		ack_failures = pTxDesc->ack_failures;
		rts_failures = pTxDesc->rts_failures;
		rts_ok = pTxDesc->rts_ok;
		r100 = pTxDesc->u.r1.rate;
		r111 = pTxDesc->u.r2.rate111;

#if WIRELESS_EXT > 13 /* wireless_send_event() and IWEVTXDROP are WE13 */
		/* need to check for certain error conditions before we
		 * clean the descriptor: we still need valid descr data here */
		if (unlikely(0x30 & error)) {
			/* only send IWEVTXDROP in case of retry or lifetime exceeded;
			 * all other errors mean we screwed up locally */
			union iwreq_data wrqu;
			p80211_hdr_t *hdr = (p80211_hdr_t *)pTxDesc->fixed_size.s.host_desc->data;

			MAC_COPY(wrqu.addr.sa_data, hdr->a3.a1);
			wireless_send_event(priv->netdev, IWEVTXDROP, &wrqu, NULL);
		}
#endif
		/* ...and free the descr */
		pTxDesc->error = 0;
		pTxDesc->ack_failures = 0;
		pTxDesc->rts_failures = 0;
		pTxDesc->rts_ok = 0;
		/* signal host owning it LAST, since ACX already knows that this
		 * descriptor is finished since it set Ctl_8 accordingly:
		 * if _OWN is set at the beginning instead, our own get_tx
		 * might choose a Tx desc that isn't fully cleared
		 * (in case of bad locking) */
		pTxDesc->Ctl_8 = DESC_CTL_HOSTOWN;
		priv->TxQueueFree++;
		num_cleaned++;

		if ((priv->TxQueueFree >= MINFREE_TX + 3)
		&& (priv->status == ACX_STATUS_4_ASSOCIATED)
		&& (acx_queue_stopped(priv->netdev))
		) {
			acxlog(L_BUF, "tx: wake queue (avail. Tx desc %d)\n", priv->TxQueueFree);
			acx_wake_queue(priv->netdev, NULL);
		}
		NEW_SPIN_UNLOCK(&pDc->tx_lock);

		/* do error checking, rate handling and logging
		 * AFTER having done the work, it's faster */

		/* do rate handling */
		txc = pTxDesc->fixed_size.s.txc;
		if (txc != &priv->defpeer.txbase
		&&  txc != &priv->defpeer.txrate
		&&  txc != &priv->ap_peer.txbase
		&&  txc != &priv->ap_peer.txrate
		) {
			printk(KERN_WARNING "Probable BUG in acx100 driver: txdescr->txc %08x is bad!\n", (u32)txc);
		} else if (txc->do_auto) {
			acx_handle_txrate_auto(priv, txc, finger, r100, r111, error);
		}

		if (unlikely(error))
			acx_handle_tx_error(priv, error, finger);

		if (CHIPTYPE_ACX111 == priv->chip_type)
			acxlog(L_BUFT, "tx: cleaned %d: !ACK=%d !RTS=%d RTS=%d r111=%04x\n",
				finger, ack_failures, rts_failures, rts_ok, r111);
		else
			acxlog(L_BUFT, "tx: cleaned %d: !ACK=%d !RTS=%d RTS=%d rate=%d\n",
				finger, ack_failures, rts_failures, rts_ok, le16_to_cpu(r111) & 0xff);

next:
		/* update pointer for descr to be cleaned next */
		finger = (finger + 1) % pDc->tx_pool_count;
		num_processed++;
	} while (watch != finger);

	/* remember last position */
	NEW_SPIN_LOCK(&pDc->tx_lock);
	pDc->tx_tail = finger;
	NEW_SPIN_UNLOCK(&pDc->tx_lock);

	OLD_SPIN_UNLOCK(&pDc->tx_lock);


	FN_EXIT0();
}

/* clean *all* Tx descriptors, and regardless of their previous state.
 * Used for brute-force reset handling. */
void acx_clean_tx_desc_emergency(wlandevice_t *priv)
{
	TIWLAN_DC *pDc = &priv->dc;
	txdesc_t *pTxDesc;
	unsigned int i;

	FN_ENTER;

	/* spin_lock(&pDc->tx_lock); don't care about tx_lock */

	for (i = 0; i < pDc->tx_pool_count; i++) {
		pTxDesc = GET_TX_DESC_PTR(pDc, i);

		/* free it */
		pTxDesc->ack_failures = 0;
		pTxDesc->rts_failures = 0;
		pTxDesc->rts_ok = 0;
		pTxDesc->error = 0;
		pTxDesc->Ctl_8 = DESC_CTL_HOSTOWN;
	}

	priv->TxQueueFree = pDc->tx_pool_count;

	/* spin_unlock(&pDc->tx_lock); */

	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_rxmonitor
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

static void acx_rxmonitor(wlandevice_t *priv, const struct rxbuffer *buf)
{
	unsigned int packet_len = MAC_CNT_RCVD(buf);
	p80211msg_lnxind_wlansniffrm_t *msg;

	int payload_offset = 0;
	unsigned int skb_len;
	struct sk_buff *skb;
	void *datap;

	FN_ENTER;

	if (!(priv->rx_config_1 & RX_CFG1_PLUS_ADDIT_HDR)) {
		printk("rx_config_1 is missing RX_CFG1_PLUS_ADDIT_HDR\n");
		FN_EXIT0();
		return;
	}

	if (priv->rx_config_1 & RX_CFG1_PLUS_ADDIT_HDR)	{
		payload_offset += 3*4; /* status words         */
		packet_len     += 3*4; /* length is w/o status */
	}
		
	if (priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR)
		payload_offset += 4;   /* phy header   */

	/* we are in big luck: the acx100 doesn't modify any of the fields */
	/* in the 802.11 frame. just pass this packet into the PF_PACKET */
	/* subsystem. yeah. */

	skb_len = packet_len - payload_offset;

	if (priv->netdev->type == ARPHRD_IEEE80211_PRISM)
		skb_len += sizeof(p80211msg_lnxind_wlansniffrm_t);

		/* sanity check */
	if (skb_len > (WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN + WLAN_CRC_LEN)) {
		printk("monitor mode panic: oversized frame!\n");
		FN_EXIT0();
		return;
	}

		/* allocate skb */
	skb = dev_alloc_skb(skb_len);
	if (!skb) {
		printk("alloc_skb FAILED trying to allocate %d bytes\n", skb_len);
		FN_EXIT0();
		return;
	}

	skb_put(skb, skb_len);

		/* when in raw 802.11 mode, just copy frame as-is */
	if (priv->netdev->type == ARPHRD_IEEE80211)
		datap = skb->data;
	else { /* otherwise, emulate prism header */
		msg = (p80211msg_lnxind_wlansniffrm_t*)skb->data;
		datap = msg + 1;
		
		msg->msgcode = DIDmsg_lnxind_wlansniffrm;
		msg->msglen = sizeof(p80211msg_lnxind_wlansniffrm_t);
		strcpy(msg->devname, priv->netdev->name);
		
		msg->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
		msg->hosttime.status = 0;
		msg->hosttime.len = 4;
		msg->hosttime.data = jiffies;
		
		msg->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
		msg->mactime.status = 0;
		msg->mactime.len = 4;
		msg->mactime.data = buf->time;
		
		msg->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
		msg->channel.status = 0;
		msg->channel.len = 4;
		msg->channel.data = priv->channel;

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
		msg->signal.data = buf->phy_snr;

		msg->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
		msg->noise.status = 0;
		msg->noise.len = 4;
		msg->noise.data = buf->phy_level;

		msg->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
		msg->rate.status = P80211ENUM_msgitem_status_no_value; /* FIXME */
		msg->rate.len = 4;
		msg->rate.data = 0; /* FIXME */

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

	skb->dev = priv->netdev;
	skb->dev->last_rx = jiffies;

	skb->mac.raw = skb->data;
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_80211_RAW);
	netif_rx(skb);

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += skb->len;

	FN_EXIT0();
}

/*
 * Calculate level like the feb 2003 windows driver seems to do
 */
u8 acx_signal_to_winlevel(u8 rawlevel)
{
	/* u8 winlevel = (u8) (0.5 + 0.625 * rawlevel); */
	u8 winlevel = ((4 + (rawlevel * 5)) / 8);

	if(winlevel>100)
		winlevel=100;

	return winlevel;
}

u8 acx_signal_determine_quality(u8 signal, u8 noise)
{
	int qual;

	qual = (((signal - 30) * 100 / 70) + (100 - noise * 4)) / 2;

	if (qual > 100)
		return 100;
	if (qual < 0)
		return 0;
	return qual;
}

/*----------------------------------------------------------------
* acx_log_rxbuffer
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

static inline void acx_log_rxbuffer(const TIWLAN_DC *pDc)
{
	unsigned int i;
	const struct rxhostdescriptor *pRxDesc;

	/* no FN_ENTER here, we don't want that */
	if (unlikely(debug & L_BUFR)) {
		/* no locks here, since it's entirely non-critical code */
		pRxDesc = pDc->pRxHostDescQPool;
		for (i = 0; i < pDc->rx_pool_count; i++) {
#if (WLAN_HOSTIF==WLAN_USB)
			acxlog(L_DEBUG,"rx: buf %d Ctl=%X val0x14=%X\n",i,le16_to_cpu(pRxDesc->Ctl_16),pRxDesc->Status);
#endif
			if ((le16_to_cpu(pRxDesc->Ctl_16) & DESC_CTL_HOSTOWN) && (le32_to_cpu(pRxDesc->Status) & BIT31))
				acxlog(L_BUFR, "rx: buf %d full\n", i);
			pRxDesc++;
		}
	}
}

/* currently we don't need to lock anything, since we access Rx
 * descriptors from one IRQ handler only (which is locked), here.
 * Will need to use spin_lock() when adding user context Rx descriptor
 * access. */
#define RX_SPIN_LOCK(lock)
#define RX_SPIN_UNLOCK(lock)

/*------------------------------------------------------------------------------
 * acx_process_rx_desc
 *
 * RX path top level entry point called directly and only from the IRQ handler
 *----------------------------------------------------------------------------*/
void acx_process_rx_desc(wlandevice_t *priv)
{
	struct rxhostdescriptor *RxHostPool;
	TIWLAN_DC *pDc;
	struct rxhostdescriptor *pRxHostDesc;
	u16 buf_len;
	unsigned int curr_idx;
	unsigned int count = 0;
	p80211_hdr_t *buf;
	unsigned int qual;

	FN_ENTER;

	pDc = &priv->dc;
	acx_log_rxbuffer(pDc);

	/* there used to be entry count code here, but because this function is called
	 * by the interrupt handler, we are sure that this will only be entered once
	 * because the kernel locks the interrupt handler */

	RxHostPool = pDc->pRxHostDescQPool;

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * rx_tail and the full descriptor we're supposed to handle. */
	while (1) {
		/* we're not in user context but in IRQ context here,
		 * so we don't need to use _irqsave() since an IRQ can
		 * only happen once anyway. user context access DOES
		 * need to prevent US from having an IRQ, however
		 * (_irqsave) */
		RX_SPIN_LOCK(&pDc->rx_lock);
		curr_idx = pDc->rx_tail;
		pRxHostDesc = &RxHostPool[pDc->rx_tail];
		pDc->rx_tail = (pDc->rx_tail + 1) % pDc->rx_pool_count;
		rmb();
		if ((le16_to_cpu(pRxHostDesc->Ctl_16) & DESC_CTL_HOSTOWN) && (le32_to_cpu(pRxHostDesc->Status) & BIT31)) {
			/* found it! */
			RX_SPIN_UNLOCK(&pDc->rx_lock);
			break;
		}
		RX_SPIN_UNLOCK(&pDc->rx_lock);
		count++;
		if (unlikely(count > pDc->rx_pool_count)) {
			/* hmm, no luck: all descriptors empty, bail out */
			FN_EXIT0();
			return;
		}
	}

	/* now process descriptors, starting with the first we figured out */
	while (1) {
		acxlog(L_BUFR, "%s: using curr_idx %d, rx_tail is now %d\n", __func__, curr_idx, pDc->rx_tail);

		if (priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
			/* take into account additional header in front of packet */
			if(CHIPTYPE_ACX111 == priv->chip_type) {
				buf = (p80211_hdr_t*)((u8*)&pRxHostDesc->data->hdr_a3 + 8);
			} else {
				buf = (p80211_hdr_t*)((u8*)&pRxHostDesc->data->hdr_a3 + 4);
			}
		} else {
			buf = (p80211_hdr_t *)&pRxHostDesc->data->hdr_a3;
		}

		rmb();
		buf_len = MAC_CNT_RCVD(pRxHostDesc->data);

		/* FIXME: maybe it is ieee2host16? Use XXi consts then */
		if ( ((WF_FC_FSTYPE & le16_to_cpu(buf->a3.fc)) != WF_FSTYPE_BEACON)
		  || (debug & L_XFER_BEACON)
		) {
			acxlog(L_XFER|L_DATA, "rx: buf %02d (%s): "
				"time %u len %i signal %d SNR %d macstat %02x phystat %02x phyrate %u status %d\n",
				curr_idx,
				acx_get_packet_type_string(le16_to_cpu(buf->a3.fc)),
				le32_to_cpu(pRxHostDesc->data->time),
				buf_len,
				acx_signal_to_winlevel(pRxHostDesc->data->phy_level),
				acx_signal_to_winlevel(pRxHostDesc->data->phy_snr),
				pRxHostDesc->data->mac_status,
				pRxHostDesc->data->phy_stat_baseband,
				pRxHostDesc->data->phy_plcp_signal,
				priv->status);
		}

		if (unlikely(debug & L_DATA)) {
			acxlog(L_DATA, "rx: 802.11 buf[%d]: ", buf_len);
			acx_dump_bytes(buf, buf_len);
		}

		/* FIXME: should check for Rx errors (pRxHostDesc->data->mac_status?
		 * discard broken packets - but NOT for monitor!)
		 * and update Rx packet statistics here */

		if (unlikely(priv->mode == ACX_MODE_MONITOR)) {
			acx_rxmonitor(priv, pRxHostDesc->data);
		} else if (likely(buf_len >= 14)) {
			acx_rx_ieee802_11_frame(priv, pRxHostDesc);
		} else {
			acxlog(L_DEBUG | L_XFER | L_DATA,
			       "rx: NOT receiving packet (%s): size too small (%d)\n",
			       acx_get_packet_type_string(le16_to_cpu(buf->a3.fc)), buf_len);
		}

		/* Now check Rx quality level, AFTER processing packet.
		 * I tried to figure out how to map these levels to dBm
		 * values, but for the life of me I really didn't
		 * manage to get it. Either these values are not meant to
		 * be expressed in dBm, or it's some pretty complicated
		 * calculation. */

#if FROM_SCAN_SOURCE_ONLY
		/* only consider packets originating from the MAC
		 * address of the device that's managing our BSSID.
		 * Disable it for now, since it removes information (levels
		 * from different peers) and slows the Rx path. */
		if (mac_is_equal(buf->a3.a2, priv->station_assoc.mac_addr)) {
#endif
			priv->wstats.qual.level = acx_signal_to_winlevel(pRxHostDesc->data->phy_level);
			priv->wstats.qual.noise = acx_signal_to_winlevel(pRxHostDesc->data->phy_snr);
#ifndef OLD_QUALITY
			qual = acx_signal_determine_quality(priv->wstats.qual.level, priv->wstats.qual.noise);
#else
			qual = (priv->wstats.qual.noise <= 100) ?
					100 - priv->wstats.qual.noise : 0;
#endif
			priv->wstats.qual.qual = qual;
			priv->wstats.qual.updated = 7; /* all 3 indicators updated */
#if FROM_SCAN_SOURCE_ONLY
		}
#endif

		RX_SPIN_LOCK(&pDc->rx_lock);
		pRxHostDesc->Status = 0;
		wmb(); /* DON'T reorder mem access here, CTL_OWN LAST! */
		CLEAR_BIT(pRxHostDesc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN)); /* Host no longer owns this */

		/* ok, descriptor is handled, now check the next descriptor */
		curr_idx = pDc->rx_tail;
		pRxHostDesc = &RxHostPool[pDc->rx_tail];

		/* if next descriptor is empty, then bail out */
		/* FIXME: is this check really entirely correct?? */
		rmb();
		/* if (!((le16_to_cpu(pRxHostDesc->Ctl) & DESC_CTL_HOSTOWN) && (!(le32_to_cpu(pRxHostDesc->Status) & BIT31)))) */
		if (!(le32_to_cpu(pRxHostDesc->Status) & BIT31)) {
			RX_SPIN_UNLOCK(&pDc->rx_lock);
			break;
		}
		else
			pDc->rx_tail = (pDc->rx_tail + 1) % pDc->rx_pool_count;
		RX_SPIN_UNLOCK(&pDc->rx_lock);
	}
	FN_EXIT0();
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

#if (WLAN_HOSTIF!=WLAN_USB)
static inline void* allocate(wlandevice_t *priv, size_t size, dma_addr_t *phy, const char *msg)
{
	void *ptr = acx_alloc_coherent(priv->pdev, size, phy, GFP_KERNEL);
	if (ptr) {
		acxlog(L_DEBUG, "%s sz=%d adr=0x%p phy=0x%08llx\n", msg, size, ptr, (unsigned long long)*phy);
		return ptr;
	}
	acxlog(L_STD, "%s memory allocation FAILED (%d bytes)\n", msg, size);
	return NULL;
}
#define ALLOCATE(p,sz,phy,msg) allocate(p,sz,phy,msg)
#else
static inline void* allocate(size_t size, const char *msg)
{
	void *ptr = kmalloc(size, GFP_KERNEL);
	if (ptr) {
		memset(ptr,0,size);
		return ptr;
	}
	acxlog(L_STD, "%s memory allocation FAILED (%d bytes)\n", msg, size);
	return NULL;
}
#define ALLOCATE(p,sz,phy,msg) allocate(sz,msg)
#endif

static int acx100_create_tx_host_desc_queue(TIWLAN_DC *pDc)
{
	wlandevice_t *priv = pDc->priv;
	int res = 2;

	unsigned int i;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned int align_offs, alignment;

	struct framehdr *frame_hdr_phy;
	u8 *frame_payload_phy;
	struct txhostdescriptor *host_desc_phy;
#endif

	struct framehdr *frame_hdr;
	u8 *frame_payload;

	struct txhostdescriptor *host_desc;

	FN_ENTER;

	/* allocate TX header pool */
	pDc->FrameHdrQPoolSize = (priv->TxQueueCnt * sizeof(struct framehdr));

	pDc->pFrameHdrQPool = ALLOCATE(priv, pDc->FrameHdrQPoolSize,
			&pDc->FrameHdrQPoolPhyAddr,"pFrameHdrQPool");
	if (!pDc->pFrameHdrQPool)
		goto fail;

	/* allocate TX payload pool */
	pDc->TxBufferPoolSize = priv->TxQueueCnt * (WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN);
	pDc->pTxBufferPool = ALLOCATE(priv, pDc->TxBufferPoolSize,
			&pDc->TxBufferPoolPhyAddr,"pTxBufferPool");
	if (!pDc->pTxBufferPool)
		goto fail;

	/* allocate the TX host descriptor queue pool */
	pDc->TxHostDescQPoolSize = priv->TxQueueCnt * sizeof(struct txhostdescriptor) + 3;
	pDc->pTxHostDescQPool = ALLOCATE(priv, pDc->TxHostDescQPoolSize,
			&pDc->TxHostDescQPoolPhyAddr, "pTxHostDescQPool");
	if (!pDc->pTxHostDescQPool) 
		goto fail;

#if (WLAN_HOSTIF!=WLAN_USB)
	/* check for proper alignment of TX host descriptor pool */
	alignment = (u32) pDc->pTxHostDescQPool & 3;
	if (alignment) {
		acxlog(L_STD, "%s: TxHostDescQPool not aligned properly\n", __func__);
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	host_desc = (struct txhostdescriptor *) ((u8 *) pDc->pTxHostDescQPool + align_offs);
	host_desc_phy = (struct txhostdescriptor *) ((u8 *) pDc->TxHostDescQPoolPhyAddr + align_offs);
	frame_hdr_phy = (struct framehdr *) pDc->FrameHdrQPoolPhyAddr;
	frame_payload_phy = (u8 *) pDc->TxBufferPoolPhyAddr;
#else
	host_desc = pDc->pTxHostDescQPool;
#endif
	frame_hdr = pDc->pFrameHdrQPool;
	frame_payload = (u8 *) pDc->pTxBufferPool;

	for (i = 0; i < priv->TxQueueCnt*2 - 1; i++) {
		if (!(i & 1)) {
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->data_phy = (ACX_PTR)cpu_to_le32(frame_hdr_phy);
			frame_hdr_phy++;
			host_desc->pNext = (ACX_PTR)cpu_to_le32(((u8 *) host_desc_phy + sizeof(struct txhostdescriptor)));
#endif
			host_desc->data = (u8 *)frame_hdr;
			frame_hdr++;
		} else {
#if (WLAN_HOSTIF!=WLAN_USB)
			host_desc->data_phy = (ACX_PTR)cpu_to_le32(frame_payload_phy);
			frame_payload_phy += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;
#endif
			host_desc->data = frame_payload;
			frame_payload += WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;
			host_desc->pNext = (ACX_PTR)NULL;
		}

		host_desc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->desc_phy = host_desc_phy;
		host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) host_desc_phy + sizeof(struct txhostdescriptor)));
		host_desc_phy++;
#endif
		host_desc++;
	}
	host_desc->data = frame_payload;
	host_desc->pNext = (ACX_PTR)NULL;
	host_desc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);

#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->data_phy = (ACX_PTR)cpu_to_le32(frame_payload_phy);
	host_desc->desc_phy = host_desc_phy;
	host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) pDc->TxHostDescQPoolPhyAddr + align_offs));
	/* host_desc->desc_phy_next = (struct txhostdescriptor *) pDc->TxHostDescQPoolPhyAddr; */
#endif
	res = 0;

fail:
	/* dealloc will be done by free function on error case */
	FN_EXIT1(res);
	return res;
}

/* FIXME: shouldn't free memory on failure, do it just like
 * acx100_create_tx_host_desc_queue instead... */
static int acx111_create_tx_host_desc_queue(TIWLAN_DC *pDc)
{
	wlandevice_t *priv = pDc->priv;

	unsigned int i;
	u16 eth_body_len = WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned int align_offs, alignment;

	u8 *frame_buffer_phy;
	struct txhostdescriptor *host_desc_phy;
#endif

	u8 *frame_buffer;
	struct txhostdescriptor *host_desc;

	FN_ENTER;

	/* allocate TX buffer */
	/* FIXME: this should probably be separated into header and body
	 * pool, just like in acx100 code... */
	pDc->TxBufferPoolSize = (priv->TxQueueCnt * sizeof(struct framehdr))
				+ priv->TxQueueCnt * eth_body_len;
	pDc->pTxBufferPool = ALLOCATE(priv, pDc->TxBufferPoolSize,
			&pDc->TxBufferPoolPhyAddr, "pTxBufferPool");
	if (!pDc->pTxBufferPool) {
		FN_EXIT1(2);
		return 2;
	}

	/* allocate the TX host descriptor queue pool */
	pDc->TxHostDescQPoolSize = priv->TxQueueCnt * sizeof(struct txhostdescriptor) + 3;
	pDc->pTxHostDescQPool = ALLOCATE(priv, pDc->TxHostDescQPoolSize,
				&pDc->TxHostDescQPoolPhyAddr, "pTxHostDescQPool");
	if (!pDc->pTxHostDescQPool) {
		FN_EXIT1(2);
		return 2;
	}

#if (WLAN_HOSTIF!=WLAN_USB)
	/* check for proper alignment of TX host descriptor pool */
	alignment = (u32) pDc->pTxHostDescQPool & 3;
	if (alignment) {
		acxlog(L_STD, "%s: TxHostDescQPool not aligned properly\n", __func__);
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	host_desc = (struct txhostdescriptor *) ((u8 *) pDc->pTxHostDescQPool + align_offs);
	host_desc_phy = (struct txhostdescriptor *) ((u8 *) pDc->TxHostDescQPoolPhyAddr + align_offs);
	frame_buffer_phy = (u8 *) pDc->TxBufferPoolPhyAddr;
#else
	host_desc = pDc->pTxHostDescQPool;
#endif
	frame_buffer = (u8 *) pDc->pTxBufferPool;

	for (i = 0; i < priv->TxQueueCnt*2 - 1; i++) {
		host_desc->data = frame_buffer;
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->data_phy = (ACX_PTR)cpu_to_le32(frame_buffer_phy);
#endif
		if (!(i & 1)) {
			frame_buffer += sizeof(struct framehdr);
#if (WLAN_HOSTIF!=WLAN_USB)
			frame_buffer_phy += sizeof(struct framehdr);
			host_desc->pNext = (ACX_PTR)cpu_to_le32(((u8 *) host_desc_phy + sizeof(struct txhostdescriptor)));
#endif
		} else {
			frame_buffer += eth_body_len;
#if (WLAN_HOSTIF!=WLAN_USB)
			frame_buffer_phy += eth_body_len;
#endif
			host_desc->pNext = (ACX_PTR)NULL;
		}

		host_desc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);

#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->desc_phy = host_desc_phy;
		host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) host_desc_phy + sizeof(struct txhostdescriptor)));
		host_desc_phy++;
#endif
		host_desc++;
	}
	/* now fill in the last 2nd buffer type (frame body) pointing back to first ring entry */
	host_desc->data = frame_buffer;
	host_desc->pNext = (ACX_PTR)NULL;
	host_desc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);

#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->data_phy = (ACX_PTR)cpu_to_le32(frame_buffer_phy);
	host_desc->desc_phy = host_desc_phy;
	host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) pDc->TxHostDescQPoolPhyAddr + align_offs));
#endif

	FN_EXIT0();
	return 0;
}

/*----------------------------------------------------------------
* acx_create_rx_host_desc_queue
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

/* the whole size of a data buffer (header plus data body)
 * plus 32 bytes safety offset at the end */
#define RX_BUFFER_SIZE (sizeof(struct rxbuffer) + 32)
static int acx_create_rx_host_desc_queue(TIWLAN_DC *pDc)
{
	wlandevice_t *priv = pDc->priv;

	unsigned int i;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned int align_offs, alignment;

	struct rxhostdescriptor *host_desc_phy;
	struct rxbuffer *data_phy;
#endif

	struct rxhostdescriptor *host_desc;
	struct rxbuffer *data;

	int result = 2;

	FN_ENTER;

	/* allocate the RX host descriptor queue pool */
	pDc->RxHostDescQPoolSize = (priv->RxQueueCnt * sizeof(struct rxhostdescriptor)) + 0x3;

	pDc->pRxHostDescQPool = ALLOCATE(priv, pDc->RxHostDescQPoolSize,
			&pDc->RxHostDescQPoolPhyAddr, "pRxHostDescQPool");
	if (!pDc->pRxHostDescQPool)
		goto fail;

	/* allocate Rx buffer pool which will be used by the acx
	 * to store the whole content of the received frames in it */
	pDc->RxBufferPoolSize = ( priv->RxQueueCnt * RX_BUFFER_SIZE );
	pDc->pRxBufferPool = ALLOCATE(priv, pDc->RxBufferPoolSize,
			&pDc->RxBufferPoolPhyAddr, "pRxBufferPool");
	if (!pDc->pRxBufferPool)
		goto fail;
	
	data = (struct rxbuffer *)pDc->pRxBufferPool;
#if (WLAN_HOSTIF!=WLAN_USB)
	data_phy = (struct rxbuffer *) pDc->RxBufferPoolPhyAddr;

	/* check for proper alignment of RX host descriptor pool */
	alignment = ((u32)pDc->pRxHostDescQPool) & 3;
	if (alignment) {
		acxlog(L_STD, "acx_create_rx_host_desc_queue: RxHostDescQPool not aligned properly\n");
		align_offs = 4 - alignment;
	} else {
		align_offs = 0;
	}

	host_desc = (struct rxhostdescriptor *) ((u8 *) pDc->pRxHostDescQPool + align_offs);
	host_desc_phy = (struct rxhostdescriptor *) ((u8 *) pDc->RxHostDescQPoolPhyAddr + align_offs);

	priv->RxHostDescPoolStart = host_desc_phy;
#else
	host_desc = pDc->pRxHostDescQPool;
#endif

	/* don't make any popular C programming pointer arithmetic mistakes
	 * here, otherwise I'll kill you...
	 * (and don't dare asking me why I'm warning you about that...) */
	for (i = 0; i < priv->RxQueueCnt - 1; i++) {
		host_desc->data = data;
		data++; /* proceed to content of next buffer */
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->data_phy = (ACX_PTR)cpu_to_le32(data_phy);
		data_phy++; /* proceed to content of next buffer */
#endif
		host_desc->length = cpu_to_le16(RX_BUFFER_SIZE);

		CLEAR_BIT(host_desc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
#if (WLAN_HOSTIF!=WLAN_USB)
		host_desc->desc_phy = host_desc_phy;
		host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) host_desc_phy + sizeof(struct rxhostdescriptor)));
		host_desc_phy++;
#endif
		host_desc++;
	}
	host_desc->data = data;
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->data_phy = (ACX_PTR)cpu_to_le32(data_phy);
#endif
	host_desc->length = cpu_to_le16(RX_BUFFER_SIZE);

	CLEAR_BIT(host_desc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
#if (WLAN_HOSTIF!=WLAN_USB)
	host_desc->desc_phy = host_desc_phy;
	host_desc->desc_phy_next = (ACX_PTR)cpu_to_le32(((u8 *) pDc->RxHostDescQPoolPhyAddr + align_offs));
#endif
	result = 0;

fail:
	/* dealloc will be done by free function on error case */
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_create_tx_desc_queue
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

/* optimized to nothing - it's a compile-time check, really */
extern void error_txdescriptor_must_be_0x30_bytes_in_length(void);
static void acx_create_tx_desc_queue(TIWLAN_DC *pDc)
{
	wlandevice_t *priv = pDc->priv;

	u32 mem_offs;
	u32 i;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *tx_hostdesc;
	dma_addr_t hostmemptr; /* remains 0 in USB case */

	FN_ENTER;

	pDc->tx_pool_count = priv->TxQueueCnt;

	pDc->TxDescrSize = sizeof(struct txdescriptor);

	if(sizeof(struct txdescriptor) != 0x30)
		error_txdescriptor_must_be_0x30_bytes_in_length();

	if(priv->chip_type == CHIPTYPE_ACX111) {
		/* the acx111 txdescriptor is 4 bytes larger */
		pDc->TxDescrSize = sizeof(struct txdescriptor)+4;
	}

#if (WLAN_HOSTIF!=WLAN_USB)
	if(pDc->pTxDescQPool == NULL) { /* calculate it */
		pDc->pTxDescQPool = (struct txdescriptor *) (priv->iobase2 +
					     pDc->ui32ACXTxQueueStart);
	}

	acxlog(L_DEBUG, "priv->iobase2 = 0x%p\n"
	                "pDc->ui32ACXTxQueueStart = 0x%08x\n"
	                "pDc->pTxDescQPool = 0x%p\n",
			priv->iobase2,
			pDc->ui32ACXTxQueueStart,
			pDc->pTxDescQPool);
#else
  /* allocate memory for TxDescriptors */
	pDc->pTxDescQPool = kmalloc(pDc->tx_pool_count*pDc->TxDescrSize, GFP_KERNEL);
	if (!pDc->pTxDescQPool) {
		acxlog(L_STD, "Not enough memory to allocate txdescriptor queue\n");
		return;
	}
#endif

	priv->TxQueueFree = priv->TxQueueCnt;
	pDc->tx_head = 0;
	pDc->tx_tail = 0;
	tx_desc = pDc->pTxDescQPool;
#if (WLAN_HOSTIF!=WLAN_USB)
	mem_offs = pDc->ui32ACXTxQueueStart;
	hostmemptr = pDc->TxHostDescQPoolPhyAddr;
#else
	mem_offs = (u32)pDc->pTxDescQPool;
	hostmemptr = 0; /* will remain 0 for USB */
#endif
	tx_hostdesc = pDc->pTxHostDescQPool;

	if (CHIPTYPE_ACX111 == priv->chip_type) {
		/* ACX111 has a preinitialized Tx buffer! */
		/* loop over whole send pool */
		/* FIXME: do we have to do the hostmemptr stuff here?? */
		for (i = 0; i < pDc->tx_pool_count; i++) {
			tx_desc->HostMemPtr = cpu_to_le32(hostmemptr);
			tx_desc->Ctl_8 = DESC_CTL_HOSTOWN;
			tx_desc->fixed_size.s.host_desc = tx_hostdesc;

			/* reserve two (hdr desc and payload desc) */
			tx_hostdesc += 2;
#if (WLAN_HOSTIF!=WLAN_USB)
			hostmemptr += 2 * sizeof(struct txhostdescriptor);
#endif
			tx_desc = (struct txdescriptor *)(((u8 *)tx_desc) + pDc->TxDescrSize);
		}

	} else {
		/* ACX100 Tx buffer needs to be initialized by us */
		/* clear whole send pool */
		memset(pDc->pTxDescQPool, 0, pDc->tx_pool_count * pDc->TxDescrSize);

		/* loop over whole send pool */
		for (i = 0; i < pDc->tx_pool_count; i++) {
	
			acxlog(L_DEBUG, "configure card tx descriptor = 0x%p, size: 0x%X\n", tx_desc, pDc->TxDescrSize);
	
			/* pointer to hostdesc memory */
			/* FIXME: type-incorrect assignment, might cause trouble
			 * in some cases */
			tx_desc->HostMemPtr = cpu_to_le32(hostmemptr);
			/* initialise ctl */
			tx_desc->Ctl_8 = DESC_CTL_INIT;
			tx_desc->Ctl2_8 = 0;
			/* point to next txdesc */
			tx_desc->pNextDesc = cpu_to_le32(mem_offs + pDc->TxDescrSize);
			/* pointer to first txhostdesc */
			tx_desc->fixed_size.s.host_desc = tx_hostdesc;
	
			/* reserve two (hdr desc and payload desc) */
			tx_hostdesc += 2;
#if (WLAN_HOSTIF!=WLAN_USB)
			hostmemptr += 2 * sizeof(struct txhostdescriptor);
#endif
			/* go to the next one */
			mem_offs += pDc->TxDescrSize;
			tx_desc = (struct txdescriptor *)(((u8 *)tx_desc) + pDc->TxDescrSize);
		}
		/* go to the last one */
		tx_desc = (struct txdescriptor *)(((u8 *)tx_desc) - pDc->TxDescrSize);
		/* tx_desc--; */
		/* and point to the first making it a ring buffer */
#if (WLAN_HOSTIF!=WLAN_USB)
		tx_desc->pNextDesc = cpu_to_le32(pDc->ui32ACXTxQueueStart);
#else
		tx_desc->pNextDesc = cpu_to_le32((u32)pDc->pTxDescQPool);
#endif
	}
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_create_rx_desc_queue
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

static void acx_create_rx_desc_queue(TIWLAN_DC *pDc)
{
	wlandevice_t *priv = pDc->priv;
#if (WLAN_HOSTIF!=WLAN_USB)
	u32 mem_offs;
	u32 i;
	struct rxdescriptor *rx_desc;
#endif

	FN_ENTER;

	pDc->rx_pool_count = priv->RxQueueCnt;
	pDc->rx_tail = 0;

#if (WLAN_HOSTIF!=WLAN_USB)
	/* ACX111 doesn't need any further config: preconfigures itself.
	 * Simply print ring buffer for debugging */
	if (CHIPTYPE_ACX111 == priv->chip_type) {
		/* pRxDescQPool already set here */
		rx_desc = pDc->pRxDescQPool;
		for (i = 0; i < pDc->rx_pool_count; i++) {
			acxlog(L_DEBUG, "rx descriptor %u @ 0x%p\n", i, rx_desc);
			rx_desc = pDc->pRxDescQPool =
				(struct rxdescriptor *) (priv->iobase2 +
				     rx_desc->pNextDesc);
		}
	} else {
		/* we didn't pre-calculate pRxDescQPool in case of ACX100 */
		/* pRxDescQPool should be right AFTER Tx pool */
		pDc->pRxDescQPool = (struct rxdescriptor *) ((u8 *) pDc->pTxDescQPool + (priv->TxQueueCnt * sizeof(struct txdescriptor)));
		
		memset(pDc->pRxDescQPool, 0, pDc->rx_pool_count * sizeof(struct rxdescriptor));

		/* loop over whole receive pool */
		rx_desc = pDc->pRxDescQPool;
		mem_offs = pDc->ui32ACXRxQueueStart;
		for (i = 0; i < pDc->rx_pool_count; i++) {
	
			acxlog(L_DEBUG, "configure card rx descriptor = 0x%p\n", rx_desc);
	
			rx_desc->Ctl_8 = DESC_CTL_RECLAIM | DESC_CTL_AUTODMA;
	
			/* point to next rxdesc */
			rx_desc->pNextDesc = cpu_to_le32(mem_offs + sizeof(struct rxdescriptor)); /* next rxdesc pNextDesc */
	
			/* go to the next one */
			mem_offs += sizeof(struct rxdescriptor);
			rx_desc++;
		}
		/* go to the last one */
		rx_desc--;
	
		/* and point to the first making it a ring buffer */
		rx_desc->pNextDesc = cpu_to_le32(pDc->ui32ACXRxQueueStart);
	}
#endif
	FN_EXIT0();
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

static int acx100_init_memory_pools(wlandevice_t *priv, const acx_ie_memmap_t *mmt)
{
	u32 TotalMemoryBlocks;
        acx100_ie_memblocksize_t MemoryBlockSize;

        acx100_ie_memconfigoption_t MemoryConfigOption;

	FN_ENTER;

	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = cpu_to_le16(priv->memblocksize);

	/* Then we alert the card to our decision of block size */
	if (OK != acx_configure(priv, &MemoryBlockSize, ACX100_IE_BLOCK_SIZE)) {
		acxlog(L_STD, "Ctl: MemoryBlockSizeWrite FAILED\n");
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers, i.e.: end-start/size */
	TotalMemoryBlocks = (le32_to_cpu(mmt->PoolEnd) - le32_to_cpu(mmt->PoolStart)) / priv->memblocksize;

	acxlog(L_DEBUG,"TotalMemoryBlocks=%d (%d bytes)\n",TotalMemoryBlocks,TotalMemoryBlocks*priv->memblocksize);

	/* This one I have no idea on */
	/* block-transfer=0x20000
	 * indirect descriptors=0x10000
	 */
#if (WLAN_HOSTIF==WLAN_USB)
	MemoryConfigOption.DMA_config = cpu_to_le32(0x20000);
#else
	MemoryConfigOption.DMA_config = cpu_to_le32(0x30000);
#endif


	/* Declare start of the Rx host pool */
	MemoryConfigOption.pRxHostDesc = cpu_to_le32((u32)priv->RxHostDescPoolStart);
	acxlog(L_DEBUG, "pRxHostDesc 0x%08x, RxHostDescPoolStart 0x%p\n", MemoryConfigOption.pRxHostDesc, priv->RxHostDescPoolStart);

	/* 50% of the allotment of memory blocks go to tx descriptors */
	priv->TxBlockNum = TotalMemoryBlocks / 2;
	MemoryConfigOption.TxBlockNum = cpu_to_le16(priv->TxBlockNum);

	/* and 50% go to the rx descriptors */
	priv->RxBlockNum = TotalMemoryBlocks - priv->TxBlockNum;
	MemoryConfigOption.RxBlockNum = cpu_to_le16(priv->RxBlockNum);

	/* size of the tx and rx descriptor queues */
	priv->TotalTxBlockSize = priv->TxBlockNum * priv->memblocksize;
	priv->TotalRxBlockSize = priv->RxBlockNum * priv->memblocksize;
	acxlog(L_DEBUG, "TxBlockNum %d RxBlockNum %d TotalTxBlockSize %d TotalTxBlockSize %d\n", priv->TxBlockNum, priv->RxBlockNum, priv->TotalTxBlockSize, priv->TotalRxBlockSize);


	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.rx_mem =
		cpu_to_le32((le32_to_cpu(mmt->PoolStart) + 0x1f) & ~0x1f);

	/* align the rx descriptor queue to units of 0x20
	 * and offset it by the tx descriptor queue */
	MemoryConfigOption.tx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + priv->TotalRxBlockSize + 0x1f) & ~0x1f);
	acxlog(L_DEBUG, "rx_mem %08x rx_mem %08x\n", MemoryConfigOption.tx_mem, MemoryConfigOption.rx_mem);

	/* alert the device to our decision */
	if (OK != acx_configure(priv, &MemoryConfigOption, ACX1xx_IE_MEMORY_CONFIG_OPTIONS)) {
		acxlog(L_STD,"%s: configure memory config options FAILED\n", __func__);
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	/* and tell the device to kick it into gear */
	if (OK != acx_issue_cmd(priv, ACX100_CMD_INIT_MEMORY, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT)) {
		acxlog(L_STD,"%s: init memory FAILED\n", __func__);
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_delete_dma_regions
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

int acx_delete_dma_regions(wlandevice_t *priv)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	/* disable radio Tx/Rx. Shouldn't we use the firmware commands
	 * here instead? Or are we that much down the road that it's no
	 * longer possible here? */
	acx_write_reg16(priv, IO_ACX_ENABLE, 0);

	/* used to be a for loop 1000, do scheduled delay instead */
	acx_schedule(HZ / 10);
#endif
	acx_free_desc_queues(&priv->dc);

	FN_EXIT0();
	return OK;
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

int acx100_create_dma_regions(wlandevice_t *priv)
{
	acx100_ie_queueconfig_t qcfg;
	
	acx_ie_memmap_t MemMap;
	struct TIWLAN_DC *pDc;
	int res = NOT_OK;

	FN_ENTER;

	pDc = &priv->dc;
	pDc->priv = priv;
	spin_lock_init(&pDc->rx_lock);
	spin_lock_init(&pDc->tx_lock);


	/* read out the acx100 physical start address for the queues */
	if (OK != acx_interrogate(priv, &MemMap, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "ctlMemoryMapRead returns error\n");
		goto fail;
	}

	/* # of items in Rx and Tx queues */
	priv->TxQueueCnt = TXBUFFERCOUNT_ACX100;
	priv->RxQueueCnt = RXBUFFERCOUNT_ACX100;

#if (WLAN_HOSTIF==WLAN_USB)
	qcfg.NumTxDesc = TXBUFFERCOUNT_USB;
	qcfg.NumRxDesc = RXBUFFERCOUNT_USB;
#endif

	/* calculate size of queues */
	qcfg.AreaSize = cpu_to_le32(
			(sizeof(struct txdescriptor) * TXBUFFERCOUNT_ACX100 +
	                 sizeof(struct rxdescriptor) * RXBUFFERCOUNT_ACX100 + 8)
			);
	qcfg.NumTxQueues = 1;  /* number of tx queues */

	/* sets the beginning of the tx descriptor queue */
	pDc->ui32ACXTxQueueStart = le32_to_cpu(MemMap.QueueStart);
	qcfg.TxQueueStart = MemMap.QueueStart;
	qcfg.TxQueuePri = 0;

	/* sets the beginning of the rx descriptor queue, after the tx descrs */
	pDc->ui32ACXRxQueueStart = pDc->ui32ACXTxQueueStart + priv->TxQueueCnt * sizeof(struct txdescriptor);
	qcfg.RxQueueStart = cpu_to_le32(pDc->ui32ACXRxQueueStart);
	qcfg.QueueOptions = 1;		/* auto reset descriptor */

	/* sets the end of the rx descriptor queue */
	qcfg.QueueEnd = cpu_to_le32(pDc->ui32ACXRxQueueStart + priv->RxQueueCnt * sizeof(struct rxdescriptor));

	/* sets the beginning of the next queue */
	qcfg.HostQueueEnd = cpu_to_le32(le32_to_cpu(qcfg.QueueEnd) + 8);

	acxlog(L_DEBUG, "<== Initialize the Queue Indicator\n");

	if (OK != acx_configure_length(priv, &qcfg, ACX1xx_IE_QUEUE_CONFIG, sizeof(acx100_ie_queueconfig_t)-4)){ /* 0x14 + (qcfg.vale * 8))) { */
		acxlog(L_STD, "ctlQueueConfigurationWrite FAILED\n");
		goto fail;
	}

	if (OK != acx100_create_tx_host_desc_queue(pDc)) {
		acxlog(L_STD, "acx100_create_tx_host_desc_queue FAILED\n");
		goto fail;
	}
	if (OK != acx_create_rx_host_desc_queue(pDc)) {
		acxlog(L_STD, "acx_create_rx_host_desc_queue FAILED\n");
		goto fail;
	}

	pDc->pTxDescQPool = NULL;
	pDc->pRxDescQPool = NULL;

	acx_create_tx_desc_queue(pDc);
	acx_create_rx_desc_queue(pDc);
	if (OK != acx_interrogate(priv, &MemMap, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "FAILED to read memory map\n");
		goto fail;
	}

	/* FIXME: huh, why call ACX1xx_IE_MEMORY_MAP twice in the USB case?
	   Is this needed?? */
#if (WLAN_HOSTIF==WLAN_USB)
	if (OK != acx_configure(priv, &MemMap, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "FAILED to write memory map\n");
		goto fail;
	}
#endif

	MemMap.PoolStart = cpu_to_le32((le32_to_cpu(MemMap.QueueEnd) + 0x1f + 4) & ~0x1f);

	if (OK != acx_configure(priv, &MemMap, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "ctlMemoryMapWrite FAILED\n");
		goto fail;
	}

	if (OK != acx100_init_memory_pools(priv, &MemMap)) {
		acxlog(L_STD, "acx100_init_memory_pools FAILED\n");
		goto fail;
	}

	res = OK;
	goto ok;

fail:
	acxlog(0xffff, "dma error!!\n");
	acx_schedule(10 * HZ);
	acx_free_desc_queues(pDc);

ok:
	FN_EXIT1(res);
	return res;
}


/*----------------------------------------------------------------
* acx111_create_dma_regions
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
#define ACX111_PERCENT(percent) (percent / 5)
int acx111_create_dma_regions(wlandevice_t *priv)
{
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	struct TIWLAN_DC *pDc;

	FN_ENTER;

	pDc = &priv->dc;
	pDc->priv = priv;
	spin_lock_init(&pDc->rx_lock);
	spin_lock_init(&pDc->tx_lock);

	/* ### Calculate memory positions and queue sizes #### */

	priv->TxQueueCnt = TXBUFFERCOUNT_ACX111;
	priv->RxQueueCnt = RXBUFFERCOUNT_ACX111;

	acxlog(L_DEBUG, "<== Initialize the Queue Indicator\n");


	/* ### Set up the card #### */

	memset(&memconf, 0, sizeof(memconf));

	/* the number of STAs (STA contexts) to support.
	 * FIXME: needs to be set differently for Master mode. */
	memconf.no_of_stations = cpu_to_le16(1);

	/* specify the memory block size. Default is 256 */
	memconf.memory_block_size = cpu_to_le16(priv->memblocksize); 	

	/* let's use 50%/50% for tx/rx (specify percentage, units of 5%) */
	memconf.tx_rx_memory_block_allocation = ACX111_PERCENT(50);

	/* set the count of our queues */
	memconf.count_rx_queues = 1; /* TODO place this in constants */
	memconf.count_tx_queues = 1;

	if ((memconf.count_rx_queues != 1) || (memconf.count_tx_queues != 1)) {
		acxlog(L_STD, 
			"%s: Requested more queues than supported. Please adapt the structure! rxbuffers:%d txbuffers:%d\n", 
			__func__, memconf.count_rx_queues, memconf.count_tx_queues);
		goto fail;
	}

	memconf.options = 0; /* 0 == Busmaster Indirect Memory Organization, which is what we want (using linked host descs with their allocated mem). 2 == Generic Bus Slave */

	/* let's use 25% for fragmentations and 75% for frame transfers
	 * (specified in units of 5%) */
	memconf.fragmentation = ACX111_PERCENT(75); 

	/* Set up our host descriptor pool + data pool */
	if (OK != acx111_create_tx_host_desc_queue(pDc)) {
		acxlog(L_STD, "acx111_create_tx_host_desc_queue FAILED\n");
		goto fail;
	}
	if (OK != acx_create_rx_host_desc_queue(pDc)) {
		acxlog(L_STD, "acx_create_rx_host_desc_queue FAILED\n");
		goto fail;
	}

	/* Rx descriptor queue config */
	memconf.rx_queue1_count_descs = RXBUFFERCOUNT_ACX111;
	memconf.rx_queue1_type = 7; /* must be set to 7 */
	memconf.rx_queue1_prio = 0; /* low prio */
	memconf.rx_queue1_host_rx_start = cpu_to_le32(pDc->RxHostDescQPoolPhyAddr);

	/* Tx descriptor queue config */
	memconf.tx_queue1_count_descs = TXBUFFERCOUNT_ACX111;
	memconf.tx_queue1_attributes = 0; /* lowest priority */

	acxlog(L_INIT, "%s: set up acx111 queue memory configuration (queue configs + descriptors)\n", __func__);
	if (OK != acx_configure(priv, &memconf, ACX1xx_IE_QUEUE_CONFIG /*0x03*/)) {
		acxlog(L_STD, "setting up memory configuration FAILED\n");
		goto fail;
	}

	/* read out queueconfig */
	/* memset(&queueconf, 0xff, sizeof(queueconf)); */

	if (OK != acx_interrogate(priv, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS /*0x05*/)) {
		acxlog(L_STD, "read queuehead FAILED\n");
	}

	pDc->ui32ACXRxQueueStart = le32_to_cpu(queueconf.rx1_queue_address);
	pDc->ui32ACXTxQueueStart = le32_to_cpu(queueconf.tx1_queue_address);

	acxlog(L_INIT, "dump queue head (from card):\n"
	               "len: %d\n"
	               "tx_memory_block_address: %X\n"
	               "rx_memory_block_address: %X\n"
	               "rx1_queue address: %X\n"
	               "tx1_queue address: %X\n",
		       le16_to_cpu(queueconf.len),
		       le32_to_cpu(queueconf.tx_memory_block_address),
		       le32_to_cpu(queueconf.rx_memory_block_address),
		       pDc->ui32ACXRxQueueStart,
		       pDc->ui32ACXTxQueueStart);

	pDc->pRxDescQPool = (struct rxdescriptor *) (priv->iobase2 +
				     pDc->ui32ACXRxQueueStart);

	pDc->pTxDescQPool = (struct txdescriptor *) (priv->iobase2 +
				     pDc->ui32ACXTxQueueStart);

	acx_create_tx_desc_queue(pDc);
	acx_create_rx_desc_queue(pDc);

	FN_EXIT1(OK);
	return OK;

fail:
	acx_free_desc_queues(pDc);

	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/*----------------------------------------------------------------
* acx_get_tx_desc
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

struct txdescriptor *acx_get_tx_desc(wlandevice_t *priv)
{
	struct TIWLAN_DC *pDc = &priv->dc;
	struct txdescriptor *tx_desc;
	unsigned long flags;

	FN_ENTER;

	spin_lock_irqsave(&pDc->tx_lock, flags);

	tx_desc = GET_TX_DESC_PTR(pDc, pDc->tx_head);

	rmb();
	if (unlikely(DESC_CTL_HOSTOWN != (tx_desc->Ctl_8 & DESC_CTL_DONE))) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
		/* FIXME: this causes a deadlock situation (endless
		 * loop) in case the current descriptor remains busy,
		 * so handle it a bit better in the future!! */
		tx_desc = NULL;
		goto fail;
	}

	priv->TxQueueFree--;
	acxlog(L_BUFT, "tx: got desc %d, %d remain\n", pDc->tx_head, priv->TxQueueFree);

/*
 * This comment is probably not entirely correct, needs further discussion
 * (restored commented-out code below to fix Tx ring buffer overflow,
 * since it's much better to have a slightly less efficiently used ring
 * buffer rather than one which easily overflows):
 *
 * This doesn't do anything other than limit our maximum number of
 * buffers used at a single time (we might as well just declare
 * MINFREE_TX less descriptors when we open up.) We should just let it
 * slide here, and back off MINFREE_TX in acx_clean_tx_desc, when given the
 * opportunity to let the queue start back up.
 */
	if (priv->TxQueueFree < MINFREE_TX) {
		acxlog(L_BUF, "stop queue (avail. Tx desc %d).\n", priv->TxQueueFree);
		acx_stop_queue(priv->netdev, NULL);
	}

	/* returning current descriptor, so advance to next free one */
	pDc->tx_head = (pDc->tx_head + 1) % pDc->tx_pool_count;
fail:
	spin_unlock_irqrestore(&pDc->tx_lock, flags);

	FN_EXIT0();
	return tx_desc;
}

static char type_string[32];	/* I *really* don't care that this is static,
				   so don't complain, else... ;-) */

/*----------------------------------------------------------------
* acx_get_packet_type_string
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

static const char *acx_get_packet_type_string(u16 fc)
{
	const char *str_ftype = "UNKNOWN", *str_fstype = "UNKNOWN";
	static const char * const mgmt_arr[] = {
		"AssocReq", "AssocResp", "ReassocReq", "ReassocResp",
		"ProbeReq", "ProbeResp", "UNKNOWN", "UNKNOWN",
		"Beacon", "ATIM", "Disassoc", "Authen", "Deauthen"
	};
	static const char * const ctl_arr[] = {
		"PSPoll", "RTS", "CTS", "Ack", "CFEnd", "CFEndCFAck"
	};
	static const char * const data_arr[] = {
		"DataOnly", "Data CFAck", "Data CFPoll", "Data CFAck/CFPoll",
		"Null", "CFAck", "CFPoll", "CFAck/CFPoll"
	};

	FN_ENTER;
	switch (WF_FC_FTYPE & fc) {
	case WF_FTYPE_MGMT:
		str_ftype = "MGMT";
		if (WLAN_GET_FC_FSTYPE(fc) < VEC_SIZE(mgmt_arr))
			str_fstype = mgmt_arr[WLAN_GET_FC_FSTYPE(fc)];
		break;
	case WF_FTYPE_CTL: {
			unsigned ctl = WLAN_GET_FC_FSTYPE(fc)-0x0a;
			str_ftype = "CTL";
			if (ctl < VEC_SIZE(ctl_arr))
				str_fstype = ctl_arr[ctl];
			break;
		}
	case WF_FTYPE_DATA:
		str_ftype = "DATA";
		if (WLAN_GET_FC_FSTYPE(fc) < VEC_SIZE(data_arr))
			str_fstype = data_arr[WLAN_GET_FC_FSTYPE(fc)];
		break;
	}
	sprintf(type_string, "%s/%s", str_ftype, str_fstype);
	FN_EXIT1((int)type_string);
	return type_string;
}

void acx_dump_bytes(const void *data, int num)
{
	int i,remain=num;
	const unsigned char *ptr=(const unsigned char *)data;

	while (remain>0) {
		if (remain<16) {
    			for (i=0;i<remain;i++) printk("%02X ",*ptr++);
			remain=0;
		} else {
			for (i=0;i<16;i++) printk("%02X ",*ptr++);
			remain-=16;
		}
		printk("\n");
	}
}
