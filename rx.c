#include "acx_debug.h"

#include "acx.h"
#include "pci.h"
#include "mem.h"
#include "merge.h"
#include "usb.h"
#include "utils.h"
#include "rx.h"

/*
 * Calculate level like the feb 2003 windows driver seems to do
 *
 * Note: the FreeBSD and DragonFlyBSD drivers seems to use different
 * so-called correction constants depending on the chip. They will be
 * defined for now, but as it is still unknown whether they are correct
 * or not, only the original value will be used. Something else to take
 * into account is that the OpenBSD driver uses another approach and
 * defines the maximum RSSI value depending on the chip, rather than
 * using a value of 100 for all of them, as it is currently done here.
 */
#define ACX100_RSSI_CORR 8
#define ACX111_RSSI_CORR 5
static u8 acx_signal_to_winlevel(u8 rawlevel)
{
	/* u8 winlevel = (u8) (0.5 + 0.625 * rawlevel); */
	u8 winlevel = (((ACX100_RSSI_CORR / 2) + (rawlevel * 5)) /
			ACX100_RSSI_CORR);

	if (winlevel > 100)
		winlevel = 100;
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

const char* acx_get_packet_type_string(u16 fc)
{
	static const char * const mgmt_arr[] = {
		"MGMT/AssocReq", "MGMT/AssocResp", "MGMT/ReassocReq",
		"MGMT/ReassocResp", "MGMT/ProbeReq", "MGMT/ProbeResp",
		"MGMT/UNKNOWN", "MGMT/UNKNOWN", "MGMT/Beacon", "MGMT/ATIM",
		"MGMT/Disassoc", "MGMT/Authen", "MGMT/Deauthen",
		"MGMT/UNKNOWN"
	};
	static const char * const ctl_arr[] = {
		"CTL/PSPoll", "CTL/RTS", "CTL/CTS", "CTL/Ack", "CTL/CFEnd",
		"CTL/CFEndCFAck", "CTL/UNKNOWN"
	};
	static const char * const data_arr[] = {
		"DATA/DataOnly", "DATA/Data CFAck", "DATA/Data CFPoll",
		"DATA/Data CFAck/CFPoll", "DATA/Null", "DATA/CFAck",
		"DATA/CFPoll", "DATA/CFAck/CFPoll", "DATA/UNKNOWN"
	};
	const char *str;
	u8 fstype = (IEEE80211_FCTL_STYPE & fc) >> 4;
	u8 ctl;

	switch (IEEE80211_FCTL_FTYPE & fc) {
	case IEEE80211_FTYPE_MGMT:
		str = mgmt_arr[min((size_t)fstype, ARRAY_SIZE(mgmt_arr) - 1)];
		break;
	case IEEE80211_FTYPE_CTL:
		ctl = fstype - 0x0a;
		str = ctl_arr[min((size_t)ctl, ARRAY_SIZE(ctl_arr) - 1)];
		break;
	case IEEE80211_FTYPE_DATA:
		str = data_arr[min((size_t)fstype, ARRAY_SIZE(data_arr) - 1)];
		break;
	default:
		str = "UNKNOWN";
		break;
	}
	return str;
}


/*
 * acx_l_rx
 *
 * The end of the Rx path. Pulls data from a rxhostdesc into a socket
 * buffer and feeds it to the network stack via netif_rx().
 */
static void acx_rx(acx_device_t *adev, rxbuffer_t *rxbuf)
{
	struct ieee80211_rx_status *status;

	struct ieee80211_hdr *w_hdr;
	struct sk_buff *skb;
	int buflen;
	int level;

	FN_ENTER;

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP))) {
		pr_info("asked to receive a packet but the interface is down??\n");
		goto out;
	}

	w_hdr = acx_get_wlan_hdr(adev, rxbuf);
	buflen = RXBUF_BYTES_RCVD(adev, rxbuf);

	/* Allocate our skb */
	skb = dev_alloc_skb(buflen);
	if (!skb) {
		pr_info("skb allocation FAILED\n");
		goto out;
	}

	skb_put(skb, buflen);
	memcpy(skb->data, w_hdr, buflen);

	status = IEEE80211_SKB_RXCB(skb);
	memset(status, 0, sizeof(*status));

	status->mactime = rxbuf->time;

	level = acx_signal_to_winlevel(rxbuf->phy_level);
	/* FIXME cleanup ?: noise = acx_signal_to_winlevel(rxbuf->phy_snr); */

	/* status->signal = acx_signal_determine_quality(level, noise);
	 * TODO OW 20100619 On ACX100 seem to be always zero (seen during hx4700 tests ?!)
	 */
	status->signal = level;

	if(adev->hw_encrypt_enabled)
		status->flag = RX_FLAG_DECRYPTED | RX_FLAG_IV_STRIPPED;

	status->freq = adev->rx_status.freq;
	status->band = adev->rx_status.band;

	status->antenna = 1;

	/* TODO I'm not sure whether this is (currently) really required. In tests
	 * this didn't made a difference. Maybe compare what other drivers do.
	 * TODO Verify translation to rate_idx.
	 */
#if 0
	if (rxbuf->phy_stat_baseband & (1 << 3)) /* Uses OFDM */
		status->rate_idx =
			acx_plcp_get_bitrate_ofdm(rxbuf->phy_plcp_signal);
	else
		status->rate_idx =
			acx_plcp_get_bitrate_cck(rxbuf->phy_plcp_signal);
#endif

	if (IS_PCI(adev)) {
#if CONFIG_ACX_MAC80211_VERSION <= KERNEL_VERSION(2, 6, 32)
		local_bh_disable();
		ieee80211_rx(adev->ieee, skb);
		local_bh_enable();
#else
		ieee80211_rx_ni(adev->ieee, skb);
#endif
	}
	/* Usb Rx is happening in_interupt() */
	else if (IS_USB(adev) || IS_MEM(adev))
		ieee80211_rx_irqsafe(adev->ieee, skb);
	else
		logf0(L_ANY, "ERROR: Undefined device type !?\n");

	adev->stats.rx_packets++;
	adev->stats.rx_bytes += skb->len;
out:
	FN_EXIT0;
}

/*
 * acx_l_process_rxbuf
 *
 * NB: used by USB code also
 */
void acx_process_rxbuf(acx_device_t *adev, rxbuffer_t *rxbuf)
{
	struct ieee80211_hdr *hdr;
	u16 fc, buf_len;

	FN_ENTER;

	hdr = acx_get_wlan_hdr(adev, rxbuf);
	fc = le16_to_cpu(hdr->frame_control);
	/* length of frame from control field to first byte of FCS */
	buf_len = RXBUF_BYTES_RCVD(adev, rxbuf);

	/* For debugging */
	if (((IEEE80211_FCTL_STYPE & fc) != IEEE80211_STYPE_BEACON)
		&& (acx_debug & (L_XFER|L_DATA))) {

		printk_ratelimited(
			"acx: rx: %s time:%u len:%u signal:%u,raw=%u"
			"SNR:%u,raw=%u macstat:%02X "
			"phystat:%02X phyrate:%u status:%u\n",
			acx_get_packet_type_string(fc),
			le32_to_cpu(rxbuf->time), buf_len,
			acx_signal_to_winlevel(rxbuf->phy_level),
			rxbuf->phy_level,
			acx_signal_to_winlevel(rxbuf->phy_snr),
			rxbuf->phy_snr, rxbuf->mac_status,
			rxbuf->phy_stat_baseband,
			rxbuf->phy_plcp_signal,
			adev->status);
	}

	if (unlikely(acx_debug & L_DATA)) {
		pr_info("rx: 802.11 buf[%u]: \n", buf_len);
		acx_dump_bytes(hdr, buf_len);
	}

	acx_rx(adev, rxbuf);

	/* Now check Rx quality level, AFTER processing packet.  I
	 * tried to figure out how to map these levels to dBm values,
	 * but for the life of me I really didn't manage to get
	 * it. Either these values are not meant to be expressed in
	 * dBm, or it's some pretty complicated calculation. */

	/* FIXME OW 20100619 Is this still required. Only for adev local use.
	 * Mac80211 signal level is reported in acx_l_rx for each skb.
	 */
	/* TODO: only the RSSI seems to be reported */
	adev->rx_status.signal = acx_signal_to_winlevel(rxbuf->phy_level);

	FN_EXIT0;
}
