/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2012
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "acx_debug.h"

#include "acx.h"
#include "pci.h"
#include "mem.h"
#include "merge.h"
#include "usb.h"
#include "main.h"
#include "tx.h"

static int acx_is_hw_tx_queue_stop_limit(acx_device_t *adev)
{
	int i;
	for (i=0; i<adev->num_hw_tx_queues; i++)
	{
		if (adev->hw_tx_queue[i].free < TX_STOP_QUEUE)
		{
			logf1(L_BUF, "Tx_free < TX_STOP_QUEUE (queue_id=%d: %u tx desc left):"
				" Stop queue.\n", i, adev->hw_tx_queue[i].free);
			return 1;
		}
	}

	return 0;
}

static void acx_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_USB(adev))
		acxusb_dealloc_tx(tx_opaque);
	if (IS_MEM(adev))
		acxmem_dealloc_tx (adev, tx_opaque);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return;
}

static void* acx_get_txbuf(acx_device_t *adev, tx_t *tx_opaque, int q)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_get_txbuf(adev, tx_opaque, q);
	if (IS_USB(adev))
		return acxusb_get_txbuf(adev, tx_opaque);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return (NULL);
}

static void acx_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
	struct ieee80211_tx_info *ieeectl, struct sk_buff *skb, int q)
{
	if (IS_PCI(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb, q);
	if (IS_USB(adev))
		return acxusb_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_MEM(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb, q);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return;
}

/*
 * OW Included skb->len to check required blocks upfront in
 * acx_l_alloc_tx This should perhaps also go into pci and usb ?
 */
static tx_t* acx_alloc_tx(acx_device_t *adev, unsigned int len, int q)
{
	if (IS_PCI(adev))
		return acxpci_alloc_tx(adev, q);
	if (IS_USB(adev))
		return acxusb_alloc_tx(adev);
	if (IS_MEM(adev))
		return acxmem_alloc_tx(adev, len);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return (NULL);
}

static int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb)
{
	tx_t *tx;
	void *txbuf;
	struct ieee80211_tx_info *ctl;
	struct ieee80211_hdr *hdr;

	/* Default queue_id for data-frames */
	int queue_id=1;

	ctl = IEEE80211_SKB_CB(skb);
	hdr = (struct ieee80211_hdr*) skb->data;

	/* Sent unencrypted frames (e.g. mgmt- and eapol-frames) on NOENC_QUEUE_ID */
	if (!(hdr->frame_control & IEEE80211_FCTL_PROTECTED))
		queue_id=NOENC_QUEUE_ID;

	/* With hw-encyption disabled, sent all on the NOENC queue.
	 * This is required, if the was previously used using hw-encyption:
	 * once a queue was used, if will not stop encryption, and so the
	 * current solution is to avoid the encrypting queues entirely. */
	if (!adev->hw_encrypt_enabled)
		queue_id=NOENC_QUEUE_ID;

	tx = acx_alloc_tx(adev, skb->len, queue_id);

	if (unlikely(!tx)) {
		logf0(L_BUFT, "No tx available\n");
		return (-EBUSY);
	}

	txbuf = acx_get_txbuf(adev, tx, queue_id);

	if (unlikely(!txbuf)) {
		/* Card was removed */
		logf0(L_BUF, "Txbuf==NULL. (Card was removed ?!):"
			" Stop queue. Dealloc skb.\n");

		/* OW only USB implemented */
		acx_dealloc_tx(adev, tx);
		return (-ENXIO);
	}

	/* FIXME: Is this required for mem ? txbuf is actually not containing to the data
	 * for the device, but actually "addr = acxmem_allocate_acx_txbuf_space in acxmem_tx_data().
	 */
	memcpy(txbuf, skb->data, skb->len);

	acx_tx_data(adev, tx, skb->len, ctl, skb, queue_id);

	adev->stats.tx_packets++;
	adev->stats.tx_bytes += skb->len;

	return 0;
}

void acx_tx_queue_flush(acx_device_t *adev)
{
	struct sk_buff *skb;
	struct ieee80211_tx_info *info;

	while ((skb = skb_dequeue(&adev->tx_queue))) {
		info = IEEE80211_SKB_CB(skb);

		logf1(L_BUF, "Flushing skb 0x%p", skb);

		if (!(info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS))
			continue;

		ieee80211_tx_status(adev->hw, skb);
	}
}

void acx_stop_queue(struct ieee80211_hw *hw, const char *msg)
{

	ieee80211_stop_queues(hw);
	if (msg)
		log(L_BUFT, "tx: stop queue %s\n", msg);

}

int acx_queue_stopped(struct ieee80211_hw *ieee)
{
	return ieee80211_queue_stopped(ieee, 0);
}

void acx_wake_queue(struct ieee80211_hw *hw, const char *msg)
{

	ieee80211_wake_queues(hw);
	if (msg)
		log(L_BUFT, "tx: wake queue %s\n", msg);

}


/*
 * maps acx111 tx descr rate field to acx100 one
 */
/*
static u8 acx_rate111to100(u16 r)
{
	return acx_bitpos2rate100[highest_bit(r)];
}
*/


int acx_rate111_hwvalue_to_rateindex(u16 hw_value)
{
	int i, r=-1;

	for (i = 0; i < acx111_rates_sizeof; i++) {
		if (acx111_rates[i].hw_value == hw_value) {
			r = i;
			break;
		}
	}
	return (r);
}

u16 acx_rate111_hwvalue_to_bitrate(u16 hw_value)
{
	int i;
	u16 bitrate = -1;

	i = acx_rate111_hwvalue_to_rateindex(hw_value);
	if (i != -1)
		bitrate = acx111_rates[i].bitrate;

	return (bitrate);
}



u16 acx111_tx_build_rateset(acx_device_t *adev, txacxdesc_t *txdesc,
			struct ieee80211_tx_info *info)
{
	int i;

	char tmpstr[256];
	struct ieee80211_rate *tmpbitrate;
	int tmpcount;

	u16 rateset = 0;

	int debug = acx_debug & L_BUFT;

	if (debug)
		sprintf(tmpstr, "rates in info [bitrate,hw_value,count]: ");

	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		if (info->control.rates[i].idx < 0)
			break;

		tmpbitrate = &adev->hw->wiphy->bands[info->band]
			->bitrates[info->control.rates[i].idx];
		tmpcount = info->control.rates[i].count;

		rateset |= tmpbitrate->hw_value;

		if (debug)
			sprintf(tmpstr + strlen(tmpstr), "%i=[%i,0x%04X,%i]%s",
				i, tmpbitrate->bitrate, tmpbitrate->hw_value,
				tmpcount,
				(i < IEEE80211_TX_MAX_RATES - 1)
				? ", " : "");
	}
	if (debug)
		logf1(L_ANY, "%s: rateset=0x%04X\n", tmpstr, rateset);

	return (rateset);
}

void acx111_tx_build_txstatus(acx_device_t *adev,
			struct ieee80211_tx_info *txstatus, u16 r111,
			u8 ack_failures)
{
	u16 rate_hwvalue;
	u16 rate_bitrate;
	int rate_index;
	int j;

	rate_hwvalue = 1 << highest_bit(r111 & RATE111_ALL);
	rate_index = acx_rate111_hwvalue_to_rateindex(rate_hwvalue);

	for (j = 0; j < IEEE80211_TX_MAX_RATES; j++) {
		if (txstatus->status.rates[j].idx == rate_index) {
			txstatus->status.rates[j].count = ack_failures + 1;
			break;
		}
	}

	if ((acx_debug & L_BUFT) && (ack_failures > 0)) {

		rate_bitrate = acx_rate111_hwvalue_to_bitrate(rate_hwvalue);
		logf1(L_ANY,
			"sentrate(bitrate,hw_value)=(%d,0x%04X)"
			" status.rates[%d].count=%d\n",
			rate_bitrate, rate_hwvalue, j,
			(j < IEEE80211_TX_MAX_RATES)
			? txstatus->status.rates[j].count : -1);
	}
}

void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error,
			unsigned int finger,
			struct ieee80211_tx_info *info)
{
	int log_level = L_INIT;

	const char *err = "unknown error";

	/* hmm, should we handle this as a mask
	 * of *several* bits?
	 * For now I think only caring about
	 * individual bits is ok... */
	switch (error) {
	case 0x01:
		err = "no Tx due to error in other fragment";
		/* adev->wstats.discard.fragment++; */
		break;
	case 0x02:
		err = "Tx aborted";
		adev->stats.tx_aborted_errors++;
		break;
	case 0x04:
		err = "Tx desc wrong parameters";
		/* adev->wstats.discard.misc++; */
		break;
	case 0x08:
		err = "WEP key not found";
		/* adev->wstats.discard.misc++; */
		break;
	case 0x10:
		err = "MSDU lifetime timeout? - try changing "
		    "'iwconfig retry lifetime XXX'";
		/* adev->wstats.discard.misc++; */
		break;

	case 0x20:
		err = "excessive Tx retries due to either distance "
		    "too high or unable to Tx or Tx frame error - "
		    "try changing 'iwconfig txpower XXX' or "
		    "'sens'itivity or 'retry'";
		log_level = acx_debug & L_DEBUG;
		/* adev->wstats.discard.retries++; */
		/* Tx error 0x20 also seems to occur on
		 * overheating, so I'm not sure whether we
		 * actually want to do aggressive radio recalibration,
		 * since people maybe won't notice then that their hardware
		 * is slowly getting cooked...
		 * Or is it still a safe long distance from utter
		 * radio non-functionality despite many radio recalibs
		 * to final destructive overheating of the hardware?
		 * In this case we really should do recalib here...
		 * I guess the only way to find out is to do a
		 * potentially fatal self-experiment :-\
		 * Or maybe only recalib in case we're using Tx
		 * rate auto (on errors switching to lower speed
		 * --> less heat?) or 802.11 power save mode?
		 *
		 * ok, just do it. */
		if ((++adev->retry_errors_msg_ratelimit % 4 == 0)) {

			if (adev->retry_errors_msg_ratelimit <= 20) {

				logf1(L_DEBUG, "%s: several excessive Tx "
					"retry errors occurred, attempting "
					"to recalibrate radio. Radio "
					"drift might be caused by increasing "
					"card temperature, please check the "
					"card before it's too late!\n",
					wiphy_name(adev->hw->wiphy));

				if (adev->retry_errors_msg_ratelimit == 20)
					logf0(L_DEBUG,
						"Disabling above message\n");
			}

			/* On the acx111, we would normally have auto radio-recalibration enabled */
			if (!adev->recalib_auto){
				logf0(L_ANY, "Scheduling radio recalibration after high tx retries.\n");
				acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
			}
		}
		break;
	case 0x40:
		err = "Tx buffer overflow";
		adev->stats.tx_fifo_errors++;
		break;
	case 0x80:
		/* possibly ACPI C-state powersaving related!!!
		 * (DMA timeout due to excessively high wakeup
		 * latency after C-state activation!?)
		 * Disable C-State powersaving and try again,
		 * then PLEASE REPORT, I'm VERY interested in
		 * whether my theory is correct that this is
		 * actually the problem here.
		 * In that case, use new Linux idle wakeup latency
		 * requirements kernel API to prevent this issue. */
		err = "DMA error";
		/*adev->wstats.discard.misc++; */
		break;
	}

	adev->stats.tx_errors++;

	if (adev->stats.tx_errors <= 20)
		log(log_level, "%s: tx error 0x%02X, buf %02u! (%s)\n",
			wiphy_name(adev->hw->wiphy), error, finger, err);
	else
		log(log_level, "%s: tx error 0x%02X, buf %02u!\n",
			wiphy_name(adev->hw->wiphy), error, finger);
}

/*
 * OW 20100405 This comment somehow lost it's function (wasn't me though!)
 *
 * acx_l_handle_txrate_auto
 *
 * Theory of operation:
 * client->rate_cap is a bitmask of rates client is capable of.
 * client->rate_cfg is a bitmask of allowed (configured) rates.
 * It is set as a result of iwconfig rate N [auto]
 * or iwpriv set_rates "N,N,N N,N,N" commands.
 * It can be fixed (e.g. 0x0080 == 18Mbit only),
 * auto (0x00ff == 18Mbit or any lower value),
 * and code handles any bitmask (0x1081 == try 54Mbit,18Mbit,1Mbit _only_).
 *
 * client->rate_cur is a value for rate111 field in tx descriptor.  It
 * is always set to txrate_cfg sans zero or more most significant
 * bits. This routine handles selection of new rate_cur value
 * depending on outcome of last tx event.
 *
 * client->rate_100 is a precalculated rate value for acx100 (we can
 * do without it, but will need to calculate it on each tx).
 *
 * You cannot configure mixed usage of 5.5 and/or 11Mbit rate with
 * PBCC and CCK modulation. Either both at CCK or both at PBCC.  In
 * theory you can implement it, but so far it is considered not worth
 * doing.
 *
 * 22Mbit, of course, is PBCC always.
 */

/* maps acx100 tx descr rate field to acx111 one */
/*
static u16 rate100to111(u8 r)
{
	switch (r) {
	case RATE100_1:
		return RATE111_1;
	case RATE100_2:
		return RATE111_2;
	case RATE100_5:
	case (RATE100_5 | RATE100_PBCC511):
		return RATE111_5;
	case RATE100_11:
	case (RATE100_11 | RATE100_PBCC511):
		return RATE111_11;
	case RATE100_22:
		return RATE111_22;
	default:
		pr_info("unexpected acx100 txrate: %u! "
		       "Please report\n", r);
		return RATE111_1;
	}
}
*/

void acx_tx_work(struct work_struct *work)
{
	acx_device_t *adev = container_of(work, struct acx_device, tx_work);

	acx_sem_lock(adev);

	if (unlikely(!test_bit(ACX_FLAG_HW_UP, &adev->flags)))
		goto out;

	acx_tx_queue_go(adev);

	out:
	acx_sem_unlock(adev);

	return;
}


void acx_tx_queue_go(acx_device_t *adev)
{
	struct sk_buff *skb;
	int ret;

	while ((skb = skb_dequeue(&adev->tx_queue))) {

		ret = acx_tx_frame(adev, skb);

		if (ret == -EBUSY) {
			logf0(L_BUFT, "EBUSY: Stop queue. Requeuing skb.\n");
			acx_stop_queue(adev->hw, NULL);
			skb_queue_head(&adev->tx_queue, skb);
			goto out;
		} else if (ret < 0) {
			logf0(L_BUF, "Other ERR: (Card was removed ?!):"
				" Stop queue. Dealloc skb.\n");
			acx_stop_queue(adev->hw, NULL);
			dev_kfree_skb(skb);
			goto out;
		}

		/* Keep a few free descs between head and tail of tx
		 * ring. It is not absolutely needed, just feels
		 * safer */
		if (acx_is_hw_tx_queue_stop_limit(adev))
		{
			acx_stop_queue(adev->hw, NULL);
			goto out;
		}
	}
out:
	return;
}



