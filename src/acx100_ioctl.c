/* src/acx100_ioctl.c - all the ioctl calls
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

#ifdef S_SPLINT_S /* some crap that splint needs to not crap out */
#define __signed__ signed
#define __u64 unsigned long long
#define u64 unsigned long long
#define loff_t unsigned long
#define sigval_t unsigned long
#define siginfo_t unsigned long
#define stack_t unsigned long
#define __s64 signed long long
#endif
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/if_arp.h>
#include <linux/wireless.h>
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif /* WE > 12 */
#include <asm/uaccess.h>

/*================================================================*/
/* Project Includes */

#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <ihw.h>

/* About the locking:
 *  I only locked the device whenever calls to the hardware are made or
 *  parts of the wlandevice struct are modified, otherwise no locking is
 *  performed. I don't now if this is safe, we'll see.
 */

extern inline UINT8 acx_signal_determine_quality(UINT8 signal, UINT8 noise);


/* if you plan to reorder something, make sure to reorder all other places
 * accordingly! */
#define ACX100_IOCTL			SIOCIWFIRSTPRIV
#define ACX100_IOCTL_DEBUG		ACX100_IOCTL + 0x00
#define ACX100_IOCTL_LIST_DOM		ACX100_IOCTL + 0x01
#define ACX100_IOCTL_SET_DOM		ACX100_IOCTL + 0x02
#define ACX100_IOCTL_GET_DOM		ACX100_IOCTL + 0x03
#define ACX100_IOCTL_SET_PREAMB		ACX100_IOCTL + 0x04
#define ACX100_IOCTL_GET_PREAMB		ACX100_IOCTL + 0x05
#define ACX100_IOCTL_SET_ANT		ACX100_IOCTL + 0x06
#define ACX100_IOCTL_GET_ANT		ACX100_IOCTL + 0x07
#define ACX100_IOCTL_RX_ANT		ACX100_IOCTL + 0x08
#define ACX100_IOCTL_TX_ANT		ACX100_IOCTL + 0x09
#define ACX100_IOCTL_SET_ED		ACX100_IOCTL + 0x0a
#define ACX100_IOCTL_SET_CCA		ACX100_IOCTL + 0x0b
#define ACX100_IOCTL_SET_PLED		ACX100_IOCTL + 0x0c
#define ACX100_IOCTL_MONITOR		ACX100_IOCTL + 0x0d
#define ACX100_IOCTL_TEST		ACX100_IOCTL + 0x0e

/* channel frequencies
 * TODO: Currently, every other 802.11 driver keeps its own copy of this. In
 * the long run this should be integrated into ieee802_11.h or wireless.h or
 * whatever IEEE802.11x framework evolves */
static const long acx100_channel_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484,
};

static const struct iw_priv_args acx100_ioctl_private_args[] = {
#ifdef ACX_DEBUG
{ cmd : ACX100_IOCTL_DEBUG,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_debug" },
#endif
{ cmd : ACX100_IOCTL_LIST_DOM,
	set_args : 0,
	get_args : 0,
	name : "list_reg_domain" },
{ cmd : ACX100_IOCTL_SET_DOM,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_reg_domain" },
{ cmd : ACX100_IOCTL_GET_DOM,
	set_args : 0,
	get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	name : "get_reg_domain" },
{ cmd : ACX100_IOCTL_SET_PREAMB,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_s_preamble" },
{ cmd : ACX100_IOCTL_GET_PREAMB,
	set_args : 0,
	get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	name : "get_s_preamble" },
{ cmd : ACX100_IOCTL_SET_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_antenna" },
{ cmd : ACX100_IOCTL_GET_ANT,
	set_args : 0,
	get_args : 0,
	name : "get_antenna" },
{ cmd : ACX100_IOCTL_RX_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_rx_ant" },
{ cmd : ACX100_IOCTL_TX_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_tx_ant" },
{ cmd : ACX100_IOCTL_SET_ED,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_ed" },
{ cmd : ACX100_IOCTL_SET_CCA,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_cca" },
{ cmd : ACX100_IOCTL_SET_PLED,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "set_led_power" },
{ cmd : ACX100_IOCTL_MONITOR,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	get_args : 0,
	name : "monitor" },
{ cmd : ACX100_IOCTL_TEST,
	set_args : 0,
	get_args : 0,
	name : "test" }
};

/*------------------------------------------------------------------------------
 * acx100_ioctl_commit
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
 *----------------------------------------------------------------------------*/
static inline int acx100_ioctl_commit(struct net_device *dev,
				      struct iw_request_info *info,
				      void *zwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	
	FN_ENTER;
	acx100_update_card_settings(priv, 0, 0, 0);
	FN_EXIT(0, 0);
	return 0;
}

static inline int acx100_ioctl_get_name(struct net_device *dev, struct iw_request_info *info, char *cwrq, char *extra)
{
	const char * const protocol_name = "IEEE 802.11b+";
	acxlog(L_IOCTL, "Get Name ==> %s\n", protocol_name);
	strcpy(cwrq, protocol_name);
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_freq
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
static inline int acx100_ioctl_set_freq(struct net_device *dev, struct iw_request_info *info, struct iw_freq *fwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int channel = -1;
	int mult = 1;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set Frequency <== %i (%i)\n", fwrq->m, fwrq->e);

	if (fwrq->e == 0 && fwrq->m <= 1000) {
		/* Setting by channel number */
		channel = fwrq->m;
	} else {
		/* If setting by frequency, convert to a channel */
		int i;

		for (i = 0; i < (6 - fwrq->e); i++)
			mult *= 10;

		for (i = 0; i <= 13; i++)
			if (fwrq->m ==
			    acx100_channel_freq[i] * mult)
				channel = i + 1;
	}

	if (channel > 14) {
		result = -EINVAL;
		goto end;
	}

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	priv->channel = (UINT16)channel;
	/* hmm, the following code part is strange, but this is how
	 * it was being done before... */
	if (ACX_MODE_3_MANAGED_AP == priv->macmode_wanted) {
		/* hmm, AP mode? So simply set channel... */
		acxlog(L_IOCTL, "Changing to channel %d\n", priv->channel);
		priv->set_mask |= GETSET_TX|GETSET_RX;
	}
	else
	if ((ACX_MODE_2_MANAGED_STA == priv->macmode_wanted)
	 || (ACX_MODE_0_IBSS_ADHOC == priv->macmode_wanted)) {
		/* trigger scanning... */
		priv->set_mask |= GETSET_CHANNEL;
	}
	acx100_unlock(priv, &flags);
	result = -EINPROGRESS; /* need to call commit handler */
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_freq(struct net_device *dev, struct iw_request_info *info, struct iw_freq *fwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	fwrq->e = 0;
	fwrq->m = priv->channel;
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_mode
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
static inline int acx100_ioctl_set_mode(struct net_device *dev, struct iw_request_info *info, __u32 *uwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set Mode <== %i\n", *uwrq);

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	switch(*uwrq) {
		case IW_MODE_AUTO:
			priv->macmode_wanted = ACX_MODE_FF_AUTO;
			break;
		case IW_MODE_ADHOC:
			priv->macmode_wanted = ACX_MODE_0_IBSS_ADHOC;
			break;
		case IW_MODE_INFRA:
			priv->macmode_wanted = ACX_MODE_2_MANAGED_STA;
			break;
		case IW_MODE_MASTER:
			priv->macmode_wanted = ACX_MODE_3_MANAGED_AP;
			break;
		default:
			result = -EOPNOTSUPP;
			goto end_unlock;
	}

	priv->set_mask |= GETSET_MODE;
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(priv, &flags);
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_mode(struct net_device *dev, struct iw_request_info *info, __u32 *uwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int result;

#if SHOW_SPECIFIC_MACMODE_JOINED
	if (priv->status != ISTATUS_4_ASSOCIATED)
#endif
	{ /* connection not up yet, so for now indicate the mode we want,
	     not the one we are in */
		switch (priv->macmode_wanted) {
			case ACX_MODE_FF_AUTO:
				*uwrq = IW_MODE_AUTO;
				break;
			case ACX_MODE_0_IBSS_ADHOC:
				*uwrq = IW_MODE_ADHOC;
				break;
			case ACX_MODE_2_MANAGED_STA:
				*uwrq = IW_MODE_INFRA;
				break;
			case ACX_MODE_3_MANAGED_AP:
				*uwrq = IW_MODE_MASTER;
				break;
			default:
				result = -EOPNOTSUPP;
				goto end;
		}
	}
#if SHOW_SPECIFIC_MACMODE_JOINED
	else
	{
		switch (priv->macmode_joined) {
			case ACX_MODE_0_IBSS_ADHOC:
				*uwrq = IW_MODE_ADHOC;
				break;
			case ACX_MODE_2_MANAGED_STA:
				*uwrq = IW_MODE_INFRA;
				break;
			case ACX_MODE_3_MANAGED_AP:
				*uwrq = IW_MODE_MASTER;
				break;
			default:
				result = -EOPNOTSUPP;
				goto end;
		}
	}
#endif
	result = 0;
end:
	acxlog(L_IOCTL, "Get Mode ==> %d\n", *uwrq);

	return result;
}

static inline int acx100_ioctl_set_sens(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Set Sensitivity <== %d\n", vwrq->value);

	if ((RADIO_RFMD_11 == priv->radio_type) || (RADIO_MAXIM_0D == priv->radio_type)) {
		priv->sensitivity = (0 == vwrq->disabled) ? 0 : vwrq->value;
		priv->set_mask |= GETSET_SENSITIVITY;
		return -EINPROGRESS;
	} else {
		printk("ERROR: don't know how to set sensitivity for this radio type, please try to add that!\n");
		return -EINVAL;
	}
}

static inline int acx100_ioctl_get_sens(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	if ((RADIO_RFMD_11 == priv->radio_type) || (RADIO_MAXIM_0D == priv->radio_type)) {
		acxlog(L_IOCTL, "Get Sensitivity ==> %d\n", priv->sensitivity);

		vwrq->value = priv->sensitivity;
		vwrq->disabled = (vwrq->value == 0);
		vwrq->fixed = 1;
		return 0;
	} else {
		printk("ERROR: don't know how to get sensitivity for this radio type, please try to add that!\n");
		return -EINVAL;
	}
}

/*------------------------------------------------------------------------------
 * acx100_set_ap
 * 
 * Sets the MAC address of the AP to associate with 
 *
 * Arguments:
 *
 * Returns:
 *
 * Side effects:
 *
 * Call context: Process
 *
 * STATUS: NEW
 *
 *----------------------------------------------------------------------------*/
static inline int acx100_ioctl_set_ap(struct net_device *dev,
				      struct iw_request_info *info,
				      struct sockaddr *awrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	static const unsigned char off[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
	int result = 0;
	UINT16 i;
	unsigned char *ap;

	FN_ENTER;
	if (NULL == awrq) {
		result = -EFAULT;
		goto end;
	}
	if (ARPHRD_ETHER != awrq->sa_family) {
                result = -EINVAL;
		goto end;
	}
	
	ap = awrq->sa_data;
	acxlog(L_IOCTL, "Set AP <== %02x:%02x:%02x:%02x:%02x:%02x\n",
               ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);

	if (ACX_MODE_2_MANAGED_STA != priv->macmode_wanted) {
		result = -EINVAL;
		goto end;
	}

	if (0 != acx100_is_mac_address_broadcast(ap)) {
		/* "any" == "auto" == FF:FF:FF:FF:FF:FF */
		MAC_BCAST(priv->ap);
		acxlog(L_IOCTL, "Forcing reassociation\n");
		acx100_scan_chan(priv);
		result = -EINPROGRESS;
	} else if (!memcmp(off, ap, ETH_ALEN)) {
		/* "off" == 00:00:00:00:00:00 */
		MAC_BCAST(priv->ap);
		acxlog(L_IOCTL, "Not reassociating\n");
	} else {
		/* AB:CD:EF:01:23:45 */
		for (i = 0; i < priv->bss_table_count; i++) {
			struct bss_info *bss = &priv->bss_table[i];
			if (!memcmp(bss->bssid, ap, ETH_ALEN)) {
				if ((!!priv->wep_enabled) != !!(bss->caps & IEEE802_11_MGMT_CAP_WEP)) {
					result = -EINVAL;
					goto end;
                        	} else {
					MAC_COPY(priv->ap, ap);
					acxlog(L_IOCTL, "Forcing reassociation\n");
					acx100_scan_chan(priv);
					result = -EINPROGRESS;
					goto end;
				}
			}
                }
	}

end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_ap(struct net_device *dev, struct iw_request_info *info, struct sockaddr *awrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get BSSID\n");
	if (ISTATUS_4_ASSOCIATED == priv->status) {
		/* as seen in Aironet driver, airo.c */
		MAC_COPY(awrq->sa_data, priv->bssid);
	} else {
		MAC_FILL(awrq->sa_data, 0x0);
	}
	awrq->sa_family = ARPHRD_ETHER;
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_aplist
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
* Comment: deprecated in favour of iwscan.
* We simply return the list of currently available stations in range,
* don't do a new scan.
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_get_aplist(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	struct sockaddr *address = (struct sockaddr *) extra;
	struct iw_quality qual[IW_MAX_AP];
	UINT16 i;
	int result = 0;

	FN_ENTER;

	/* in Master mode of course we don't have an AP list... */
	if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
	{
		result = -EOPNOTSUPP;
		goto end;
	}

	for (i = 0; i < priv->bss_table_count; i++) {
		MAC_COPY(address[i].sa_data, priv->bss_table[i].bssid);
		address[i].sa_family = ARPHRD_ETHER;
		qual[i].level = priv->bss_table[i].sir;
		qual[i].noise = priv->bss_table[i].snr;
#ifndef OLD_QUALITY
		qual[i].qual = acx_signal_determine_quality(qual[i].level, qual[i].noise);
#else
		qual[i].qual = (qual[i].noise <= 100) ?
			       100 - qual[i].noise : 0;;
#endif
		qual[i].updated = 0; /* no scan: level/noise/qual not updated */
	}
	if (0 != i)
	{
		dwrq->flags = 1;
		memcpy(extra + sizeof(struct sockaddr)*i, &qual,
				sizeof(struct iw_quality)*i);
	}

	dwrq->length = priv->bss_table_count;

end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_set_scan(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int result = -EINVAL;

	FN_ENTER;

	/* don't start scan if device is not up yet */
	if (0 == (priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		result = -EAGAIN;
		goto end;
	}

	acx100_scan_chan(priv);
	priv->scan_start = jiffies;
	priv->scan_running = 1;
	result = 0;

end:
	FN_EXIT(1, result);
	return result;
}

#if WIRELESS_EXT > 13
static char *acx100_ioctl_scan_add_station(wlandevice_t *priv, char *ptr, char *end_buf, struct bss_info *bss)
{
	struct iw_event iwe;
	int i;
	char *ptr_rate;

	FN_ENTER;

	/* MAC address has to be added first */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	MAC_COPY(iwe.u.ap_addr.sa_data, bss->bssid);
	acxlog(L_IOCTL, "scan, station address:\n");
	acx100_log_mac_address(L_IOCTL, bss->bssid);
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_ADDR_LEN);

	/* Add ESSID */
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.length = bss->essid_len;
	iwe.u.data.flags = 1;
	acxlog(L_IOCTL, "scan, essid: %s\n", bss->essid);
	ptr = iwe_stream_add_point(ptr, end_buf, &iwe, bss->essid);
	
	/* Add mode */
	iwe.cmd = SIOCGIWMODE;
	if (0 != (bss->caps & (IEEE802_11_MGMT_CAP_ESS | IEEE802_11_MGMT_CAP_IBSS))) {
		if (0 != (bss->caps & IEEE802_11_MGMT_CAP_ESS))
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_ADHOC;
		acxlog(L_IOCTL, "scan, mode: %d\n", iwe.u.mode);
		ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_UINT_LEN);
	}

	/* Add frequency */
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = acx100_channel_freq[bss->channel] * 100000;
	iwe.u.freq.e = 1;
	acxlog(L_IOCTL, "scan, frequency: %d\n", iwe.u.freq.m);
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_FREQ_LEN);

	/* Add link quality */
	iwe.cmd = IWEVQUAL;
	/* FIXME: these values should be expressed in dBm, but we don't know
	 * how to calibrate it yet */
	iwe.u.qual.level = bss->sir;
	iwe.u.qual.noise = bss->snr;
#ifndef OLD_QUALITY
	iwe.u.qual.qual = acx_signal_determine_quality(iwe.u.qual.level, iwe.u.qual.noise);
#else
	iwe.u.qual.qual = (iwe.u.qual.noise <= 100) ?
				100 - iwe.u.qual.noise : 0;
#endif
	iwe.u.qual.updated = 7;
	acxlog(L_IOCTL, "scan, link quality: %d/%d/%d\n", iwe.u.qual.level, iwe.u.qual.noise, iwe.u.qual.qual);
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_QUAL_LEN);

	/* Add encryption */
	iwe.cmd = SIOCGIWENCODE;
	if (0 != (bss->caps & IEEE802_11_MGMT_CAP_WEP))
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	acxlog(L_IOCTL, "scan, encryption flags: %x\n", iwe.u.data.flags);
	ptr = iwe_stream_add_point(ptr, end_buf, &iwe, bss->essid);

	/* add rates */
	iwe.cmd = SIOCGIWRATE;
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
	ptr_rate = ptr + IW_EV_LCP_LEN;
	for (i = 0; 0 != bss->supp_rates[i]; i++)
	{
		iwe.u.bitrate.value = (bss->supp_rates[i] & ~0x80) * 500000; /* units of 500kb/s */
		acxlog(L_IOCTL, "scan, rate: %d [%02x]\n", iwe.u.bitrate.value, bss->supp_rates[i]);
		ptr = iwe_stream_add_value(ptr, ptr_rate, end_buf, &iwe, IW_EV_PARAM_LEN);
	}
	if ((ptr_rate - ptr) > (ptrdiff_t)IW_EV_LCP_LEN)
		ptr = ptr_rate;

	/* drop remaining station data items for now */

	FN_EXIT(1, (int)ptr);
	return ptr;
}

static inline int acx100_ioctl_get_scan(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	char *ptr = extra;
	int i;
	int result = 0;

	FN_ENTER;

	/* no scan available if device is not up yet */
	if (0 == (priv->dev_state_mask & ACX_STATE_IFACE_UP))
	{
		result = -EAGAIN;
		goto end;
	}

	if (priv->scan_start && time_before(jiffies, priv->scan_start + 3*HZ))
	{
		/* scan still in progress, so no results yet, sorry */
		result = -EAGAIN;
		goto end;
	}
	priv->scan_start = 0;

	if (priv->bss_table_count == 0)
	{
		/* no stations found */
		result = -ENODATA;
		goto end;
	}

	for (i = 0; i < priv->bss_table_count; i++)
	{
		struct bss_info *bss = &priv->bss_table[i];

		ptr = acx100_ioctl_scan_add_station(priv, ptr, extra + IW_SCAN_MAX_DATA, bss);
	}
	dwrq->length = ptr - extra;
	dwrq->flags = 0;

end:
	FN_EXIT(1, result);
	return result;
}
#endif /* WIRELESS_EXT > 13 */

/*----------------------------------------------------------------
* acx100_ioctl_set_essid
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
static inline int acx100_ioctl_set_essid(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int len = dwrq->length;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set ESSID <== %s, length %d, flags 0x%04x\n", dwrq->pointer, len, dwrq->flags);

	if (len < 0) {
		result = -EINVAL;
		goto end;
	}

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	/* ESSID disabled? */
	if (0 == dwrq->flags)
	{
		priv->essid_active = (UINT8)0;
	}
	else
	{
		if (dwrq->length > IW_ESSID_MAX_SIZE+1)
		{
			result = -E2BIG;
			goto end_unlock;
		}

		priv->essid_len = (UINT8)(len - 1);
		memcpy(priv->essid, extra, priv->essid_len);
		priv->essid[priv->essid_len] = '\0';
		priv->essid_active = (UINT8)1;
	}

	priv->set_mask |= GETSET_ESSID;

end_unlock:
	acx100_unlock(priv, &flags);
	result = -EINPROGRESS;
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_essid(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get ESSID ==> %s\n", priv->essid);

	dwrq->flags = priv->essid_active;
	if ((UINT8)0 != priv->essid_active)
	{
		memcpy(extra, priv->essid, priv->essid_len);
		extra[priv->essid_len] = '\0';
		dwrq->length = priv->essid_len + 1;
		dwrq->flags = 1;
	}
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_rate
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
static inline int acx100_ioctl_set_rate(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	UINT8 txrate_cfg, txrate_auto_idx_max;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL,
	       "rate = %d, fixed = 0x%x, disabled = 0x%x, flags = 0x%x\n",
	       vwrq->value, vwrq->fixed, vwrq->disabled, vwrq->flags);

#define BITRATE_AUTO 1
#if BITRATE_AUTO
	if ((0 == vwrq->fixed) || (1 == vwrq->fixed)) {
#else
	if (1 == vwrq->fixed) {
#endif

		switch (vwrq->value) {

		case 1000000:	/* 1Mbps */
			txrate_cfg = ACX_TXRATE_1;
			txrate_auto_idx_max = 0;
			break;

		case 2000000:	/* 2Mbps */
			txrate_cfg = ACX_TXRATE_2;
			txrate_auto_idx_max = 1;
			break;

		case 5500000:	/* 5.5Mbps */
			txrate_cfg = ACX_TXRATE_5_5;
			txrate_auto_idx_max = 2;
			break;

		case 11000000:	/* 11Mbps */
			txrate_cfg = ACX_TXRATE_11;
			txrate_auto_idx_max = 3;
			break;

		case 22000000:	/* 22Mbps */
			txrate_cfg = ACX_TXRATE_22PBCC;
			txrate_auto_idx_max = 4;
			break;

#if BITRATE_AUTO
		case -1: /* highest available */
			/* -1 is used to set highest available rate in
			 * case of iwconfig calls without rate given
			 * (iwconfig wlan0 rate auto etc.) */
			txrate_cfg = ACX_TXRATE_22PBCC; /* 22Mbps is not supported by many APs, thus fallback needs to work properly to be able to safely go back to 11! */
			txrate_auto_idx_max = 4;
			break;
#endif
		default:
			result = -EINVAL;
			goto end;
		}

	} else if (2 == vwrq->fixed) {

		switch (vwrq->value) {

		case 0:
			txrate_cfg = ACX_TXRATE_1;
			txrate_auto_idx_max = 0;
			break;

		case 1:
			txrate_cfg = ACX_TXRATE_2;
			txrate_auto_idx_max = 1;
			break;

		case 2:
			txrate_cfg = ACX_TXRATE_5_5;
			txrate_auto_idx_max = 2;
			break;

		case 3:
			txrate_cfg = ACX_TXRATE_5_5PBCC;
			txrate_auto_idx_max = 2;
			break;

		case 4:
			txrate_cfg = ACX_TXRATE_11;
			txrate_auto_idx_max = 3;
			break;

		case 5:
			txrate_cfg = ACX_TXRATE_11PBCC;
			txrate_auto_idx_max = 3;
			break;

		case 6:
			txrate_cfg = ACX_TXRATE_22PBCC;
			txrate_auto_idx_max = 4;
			break;

		default:
			result = -EINVAL;
			goto end;
		}

	} else {
		result = -EOPNOTSUPP;
		goto end;
	}

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	priv->txrate_cfg = txrate_cfg;
#if BITRATE_AUTO
	priv->txrate_auto = (UINT8)(vwrq->fixed == (__u8)0);
	if (1 == priv->txrate_auto)
	{
		if (ACX_TXRATE_1 == txrate_cfg) { /* auto rate with 1Mbps max. useless */
			priv->txrate_auto = (UINT8)0;
			priv->txrate_curr = priv->txrate_cfg;
		} else {
			priv->txrate_auto_idx_max = txrate_auto_idx_max;
			priv->txrate_auto_idx = 1; /* 2Mbps */
			priv->txrate_curr = ACX_TXRATE_2; /* 2Mbps, play it safe at the beginning */
			priv->txrate_fallback_count = 0;
			priv->txrate_stepup_count = 0;
		}
	}
	else
#endif
		priv->txrate_curr = priv->txrate_cfg;
	
	acx100_unlock(priv, &flags);

#if BITRATE_AUTO
	acxlog(L_IOCTL, "Tx rate = %d, auto rate %d, current rate %d\n", priv->txrate_cfg, priv->txrate_auto, priv->txrate_curr);
	priv->set_mask |= SET_RATE_FALLBACK;
	result = -EINPROGRESS;
#else
	acxlog(L_IOCTL, "Tx rate = %d\n", priv->txrate_cfg);
	result = 0;
#endif

end:
	FN_EXIT(1, result);
	return result;
}
/*----------------------------------------------------------------
* acx100_ioctl_get_rate
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
static inline int acx100_ioctl_get_rate(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	vwrq->value = priv->txrate_curr * 100000;
#if BITRATE_AUTO
	vwrq->fixed = (__u8)(priv->txrate_auto == (UINT8)0);
#else
	vwrq->fixed = 1;
#endif
	vwrq->disabled = (__u8)0;
	return 0;
}

static inline int acx100_ioctl_set_rts(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int val = vwrq->value;

	if ((__u8)0 != vwrq->disabled)
		val = 2312;
	if ((val < 0) || (val > 2312))
		return -EINVAL;

	priv->rts_threshold = val;
	return 0;
		
}

static inline int acx100_ioctl_get_rts(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	vwrq->value = priv->rts_threshold;
	vwrq->disabled = (__u8)(vwrq->value >= 2312);
	vwrq->fixed = (__u8)1;
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_encode
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
static inline int acx100_ioctl_set_encode(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int index;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL,
	       "Set Encoding flags = 0x%04x, size = %i, key: %s\n",
	       dwrq->flags, dwrq->length, extra ? "set" : "No key");

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	index = dwrq->flags & IW_ENCODE_INDEX;

	if (dwrq->length > 0) {

		/* if index is 0 or invalid, use default key */
		if ((index == 0) || (index > 4))
			index = (int)priv->wep_current_index;
		else
			index--;

		if (0 == (dwrq->flags & IW_ENCODE_NOKEY)) {
			if (dwrq->length > 29)
				dwrq->length = 29; /* restrict it */

			if (dwrq->length > 13)
				priv->wep_keys[index].size = 29; /* 29*8 == 232, WEP256 */
			else
			if (dwrq->length > 5)
				priv->wep_keys[index].size = 13; /* 13*8 == 104bit, WEP128 */
			else
			if (dwrq->length > 0)
				priv->wep_keys[index].size = 5; /* 5*8 == 40bit, WEP64 */
			else
				/* disable key */
				priv->wep_keys[index].size = 0;

			memset(priv->wep_keys[index].key, 0, sizeof(priv->wep_keys[index].key));
			memcpy(priv->wep_keys[index].key, extra, dwrq->length);
		}

	} else {
		/* set transmit key */
		if ((index >= 1) && (index <= 4))
			priv->wep_current_index = (UINT8)(index - 1);
		else
			if (0 == (dwrq->flags & IW_ENCODE_MODE))
			{
				/* complain if we were not just setting
				 * the key mode */
				result =  -EINVAL;
				goto end_unlock;
			}
	}

	priv->wep_enabled = (UINT8)(0 == (dwrq->flags & IW_ENCODE_DISABLED));

	if (0 != (dwrq->flags & IW_ENCODE_OPEN)) {
		priv->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
		priv->wep_restricted = (UINT8)0;
	} else if (0 != (dwrq->flags & IW_ENCODE_RESTRICTED)) {
		priv->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
		priv->wep_restricted = (UINT8)1;
	}

	/* set flag to make sure the card WEP settings get updated */
	priv->set_mask |= GETSET_WEP;

	acxlog(L_IOCTL, "len = %d, key at 0x%p, flags = 0x%x\n",
	       dwrq->length, extra,
	       dwrq->flags);

	for (index = 0; index <= 3; index++) {
		if (0 != priv->wep_keys[index].size) {
			acxlog(L_IOCTL,
			       "index = %d, size = %d, key at 0x%p\n",
			       priv->wep_keys[index].index,
			       priv->wep_keys[index].size,
			       priv->wep_keys[index].key);
		}
	}
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(priv, &flags);
end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_encode
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
static inline int acx100_ioctl_get_encode(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_wanted) {
		/* ok, let's assume it's supported, but print a
		 * warning message
		 * FIXME: should be removed once it's definitely working. */
		acxlog(L_STD, "Warning: WEP support might not be supported in Ad-Hoc mode yet!\n");
		/* return -EOPNOTSUPP; */
	}

	if (priv->wep_enabled == (UINT8)0)
	{
		dwrq->flags = IW_ENCODE_DISABLED;
	}
	else
	{
		dwrq->flags =
			(priv->wep_restricted == (UINT8)1) ? IW_ENCODE_RESTRICTED : IW_ENCODE_OPEN;

		dwrq->length =
		    priv->wep_keys[priv->wep_current_index].size;

		memcpy(extra,
			     priv->wep_keys[priv->wep_current_index].key,
			     priv->wep_keys[priv->wep_current_index].size);
	}

	/* set the current index */
	dwrq->flags |=
	    priv->wep_keys[priv->wep_current_index].index + 1;

	acxlog(L_IOCTL, "len = %d, key = %p, flags = 0x%x\n",
	       dwrq->length, dwrq->pointer,
	       dwrq->flags);

	return 0;
}

static inline int acx100_ioctl_set_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Set 802.11 Power Save flags = 0x%04x\n", vwrq->flags);
	if ((__u8)0 != vwrq->disabled) {
		priv->ps_wakeup_cfg &= ~PS_CFG_ENABLE;
		priv->set_mask |= GETSET_POWER_80211;
		return -EINPROGRESS;
	}
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		UINT16 ps_timeout = (vwrq->value * 1024) / 1000;

		if (ps_timeout > 255)
			ps_timeout = 255;
		acxlog(L_IOCTL, "setting PS timeout value to %d time units due to %dus\n", ps_timeout, vwrq->value);
		priv->ps_hangover_period = ps_timeout;
	} else if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_PERIOD) {
		UINT16 ps_periods = vwrq->value / 1000000;

		if (ps_periods > 255)
			ps_periods = 255;
		acxlog(L_IOCTL, "setting PS period value to %d periods due to %dus\n", ps_periods, vwrq->value);
		priv->ps_listen_interval = ps_periods;
		priv->ps_wakeup_cfg &= ~PS_CFG_WAKEUP_MODE_MASK;
		priv->ps_wakeup_cfg |= PS_CFG_WAKEUP_EACH_ITVL;
	}
	switch (vwrq->flags & IW_POWER_MODE) {
		/* FIXME: are we doing the right thing here? */
		case IW_POWER_UNICAST_R:
			priv->ps_options &= ~PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_MULTICAST_R:
			priv->ps_options |= PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_ALL_R:
			priv->ps_options |= PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_ON:
			break;
		default:
			acxlog(L_IOCTL, "unknown PS mode\n");
			return -EINVAL;
	}
	priv->ps_wakeup_cfg |= PS_CFG_ENABLE;
	priv->set_mask |= GETSET_POWER_80211;
	return -EINPROGRESS;
			
}

static inline int acx100_ioctl_get_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get 802.11 Power Save flags = 0x%04x\n", vwrq->flags);
	vwrq->disabled = ((priv->ps_wakeup_cfg & PS_CFG_ENABLE) == (UINT8)0);
	if (0 != vwrq->disabled)
		return 0;
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		vwrq->value = priv->ps_hangover_period * 1000 / 1024;
		vwrq->flags = IW_POWER_TIMEOUT;
	} else {
		vwrq->value = priv->ps_listen_interval * 1000000;
		vwrq->flags = IW_POWER_PERIOD|IW_POWER_RELATIVE;
	}
	if (0 != (priv->ps_options & PS_OPT_STILL_RCV_BCASTS))
		vwrq->flags |= IW_POWER_ALL_R;
	else
		vwrq->flags |= IW_POWER_UNICAST_R;

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_txpow
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
static inline int acx100_ioctl_get_txpow(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	vwrq->flags = IW_TXPOW_DBM;
	vwrq->disabled = (__u8)0;
	vwrq->fixed = (__u8)0;
	vwrq->value = priv->tx_level_dbm;

	acxlog(L_IOCTL, "Get Tx power ==> %d dBm\n", priv->tx_level_dbm);

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_txpow
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
static inline int acx100_ioctl_set_txpow(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set Tx power <== %d, disabled %d, flags 0x%04x\n", vwrq->value, vwrq->disabled, vwrq->flags);
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	if (vwrq->disabled != priv->tx_disabled) {
		priv->set_mask |= GETSET_TX; /* Tx status needs update later */
	}

	priv->tx_disabled = vwrq->disabled;
	if (vwrq->value == -1) {
		if (0 != vwrq->disabled) {
			priv->tx_level_dbm = (UINT8)0;
			acxlog(L_IOCTL, "Disable radio Tx\n");
		} else {
			priv->tx_level_auto = (UINT8)1;
			acxlog(L_IOCTL, "Set Tx power auto (NIY)\n");
		}
	} else {
		priv->tx_level_dbm = vwrq->value <= 20 ? vwrq->value : 20;
		priv->tx_level_auto = (UINT8)0;
		acxlog(L_IOCTL, "Set Tx power = %d dBm\n", priv->tx_level_dbm);
	}
	priv->set_mask |= GETSET_TXPOWER;
	acx100_unlock(priv, &flags);
	result = -EINPROGRESS;
end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_range
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
static inline int acx100_ioctl_get_range(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	if (dwrq->pointer != NULL) {
		struct iw_range *range = (struct iw_range *)extra;
		wlandevice_t *priv = (wlandevice_t *) dev->priv;
		UINT16 i;

		dwrq->length = sizeof(struct iw_range);
		memset(range, 0, sizeof(struct iw_range));
		range->num_channels = 0;
		for (i = 0; i < 14; i++)
			if (0 != (priv->reg_dom_chanmask & (1 << i)))
			{
				range->freq[range->num_channels].i = i + 1;
				range->freq[range->num_channels].m = acx100_channel_freq[i] * 100000;
				range->freq[range->num_channels++].e = 1; /* MHz values */
			}
		range->num_frequency = (__u8)range->num_channels;

		range->min_rts = 0;
		range->max_rts = 2312;
		/* range->min_frag = 256;
		 * range->max_frag = 2312;
		 */

		range->encoding_size[0] = 5;
		range->encoding_size[1] = 13;
		range->encoding_size[2] = 29;
		range->num_encoding_sizes = (__u8)3;
		range->max_encoding_tokens = (__u8)4;
		
		range->min_pmp = 0;
		range->max_pmp = 5000000;
		range->min_pmt = 0;
		range->max_pmt = 65535 * 1000;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_ALL_R;

		for (i = 0; i <= IW_MAX_TXPOWER - 1; i++)
			range->txpower[i] = 20 * i / (IW_MAX_TXPOWER - 1);
		range->num_txpower = (__u8)IW_MAX_TXPOWER;
		range->txpower_capa = IW_TXPOW_DBM;

		range->we_version_compiled = (__u8)WIRELESS_EXT;
		range->we_version_source = (__u8)0x9;

		range->retry_capa = IW_RETRY_LIMIT;
		range->retry_flags = IW_RETRY_LIMIT;
		range->min_retry = 1;
		range->max_retry = 255;

		range->r_time_flags = IW_RETRY_LIFETIME;
		range->min_r_time = 0;
		range->max_r_time = 65535; /* FIXME: lifetime ranges and orders of magnitude are strange?? */

		range->sensitivity = 0xff;

		for (i=0; i < (UINT16)priv->rate_spt_len; i++) {
			range->bitrate[i] = (priv->rate_support1[i] & ~0x80) * 500000;
			if (range->bitrate[i] == 0)
				break;
		}
		range->num_bitrates = (__u8)i;

		range->max_qual.qual = (__u8)100;
		range->max_qual.level = (__u8)100;
		range->max_qual.noise = (__u8)100;
		/* FIXME: better values */
		range->avg_qual.qual = (__u8)90;
		range->avg_qual.level = (__u8)80;
		range->avg_qual.noise = (__u8)2;
	}

	return 0;
}


/*================================================================*/
/* Private functions						  */
/*================================================================*/

/*----------------------------------------------------------------
* acx100_ioctl_get_iw_priv
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
* Comment: I added the monitor mode and changed the stuff below to look more like the orinoco driver
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_get_iw_priv(struct iwreq *iwr)
{
	int result = -EINVAL;

	if (iwr->u.data.pointer != 0) {
		result =
		    verify_area(VERIFY_WRITE, iwr->u.data.pointer,
				sizeof(acx100_ioctl_private_args));
		if (result != 0)
			return result;

		iwr->u.data.length = sizeof(acx100_ioctl_private_args) / sizeof(acx100_ioctl_private_args[0]);
		if (copy_to_user(iwr->u.data.pointer, acx100_ioctl_private_args, sizeof(acx100_ioctl_private_args)) !=
		    0)
			result = -EFAULT;
	}
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_nick
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
static inline int acx100_ioctl_get_nick(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	/* FIXME : consider spinlock here */
	strcpy(extra, priv->nick);
	/* FIXME : consider spinlock here */

	dwrq->length = strlen(extra)+1;

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_nick
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
* Comment: copied from orinoco.c
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_set_nick(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	if(dwrq->length > IW_ESSID_MAX_SIZE + 1) {
		result = -E2BIG;
		goto end_unlock;
	}

	/* extra includes trailing \0, so it's ok */
	strcpy(priv->nick, extra);
	result = 0;

end_unlock:
	acx100_unlock(priv, &flags);
end:
	FN_EXIT(1, result);
	return result;
}

/*------------------------------------------------------------------------------
 * acx100_ioctl_get_retry
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
 *----------------------------------------------------------------------------*/
static inline int acx100_ioctl_get_retry(struct net_device *dev,
					 struct iw_request_info *info,
					 struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	__u16 type = vwrq->flags & IW_RETRY_TYPE;
	__u16 modifier = vwrq->flags & IW_RETRY_MODIFIER;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	/* return the short retry number by default */
	if (type == IW_RETRY_LIFETIME) {
		vwrq->flags = IW_RETRY_LIFETIME;
		vwrq->value = priv->msdu_lifetime;
	} else if (modifier == IW_RETRY_MAX) {
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
		vwrq->value = priv->long_retry;
	} else {
		vwrq->flags = IW_RETRY_LIMIT;
		if (priv->long_retry != priv->short_retry)
			vwrq->flags |= IW_RETRY_MIN;
		vwrq->value = priv->short_retry;
	}
	acx100_unlock(priv, &flags);
	/* can't be disabled */
	vwrq->disabled = (__u8)0;
	result = 0;

end:
        return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_retry
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
static inline int acx100_ioctl_set_retry(struct net_device *dev,
					 struct iw_request_info *info,
					 struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	if (!vwrq) {
		result = -EFAULT;
		goto end;
	}
	if ((__u8)0 != vwrq->disabled) {
		result = -EINVAL;
		goto end;
	}
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	if (IW_RETRY_LIMIT == (vwrq->flags & IW_RETRY_TYPE)) {
		(void)printk("current retry limits: short %ld long %ld\n", priv->short_retry, priv->long_retry);
                if (0 != (vwrq->flags & IW_RETRY_MAX)) {
                        priv->long_retry = vwrq->value;
                } else if (0 != (vwrq->flags & IW_RETRY_MIN)) {
                        priv->short_retry = vwrq->value;
                } else {
                        /* no modifier: set both */
                        priv->long_retry = vwrq->value;
                        priv->short_retry = vwrq->value;
                }
		(void)printk("new retry limits: short %ld long %ld\n", priv->short_retry, priv->long_retry);
		priv->set_mask |= GETSET_RETRY;
		result = -EINPROGRESS;
	}
	else
	if (0 != (vwrq->flags & IW_RETRY_LIFETIME)) {
		priv->msdu_lifetime = vwrq->value;
		(void)printk("new MSDU lifetime: %d\n", priv->msdu_lifetime);
		priv->set_mask |= SET_MSDU_LIFETIME;
		result = -EINPROGRESS;
	}
	acx100_unlock(priv, &flags);
end:
	FN_EXIT(1, result);
	return result;
}


/******************************* private ioctls ******************************/


/*----------------------------------------------------------------
* acx100_ioctl_set_debug
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
#ifdef ACX_DEBUG
static inline int acx100_ioctl_set_debug(struct net_device *dev,
					 struct iw_request_info *info,
					 struct iw_param *vwrq, char *extra)
{
	int debug_new = *((int *)extra);
	int result = -EINVAL;

	acxlog(L_STD, "%s: setting debug from 0x%04X to 0x%04X\n", __func__,
	       debug, debug_new);
	debug = debug_new;

	result = 0;
	return result;

}
#endif

static unsigned char reg_domain_ids[] = {0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51};

static char *reg_domain_strings[] =
{ "FCC (USA)        (1-11)",
  "DOC/IC (Canada)  (1-11)",
	/* BTW: WLAN use in ETSI is regulated by
	 * ETSI standard EN 300 328-2 V1.1.2 */
  "ETSI (Europe)    (1-13)",
  "Spain           (10-11)",
  "France          (10-13)",
  "MKK (Japan)        (14)",
  "MKK1             (1-14)",
  "Israel? (new!)  (4?-8?) (not all firmware versions)",
  NULL /* needs to remain as last entry */
};

/*----------------------------------------------------------------
* acx100_ioctl_list_reg_domain
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
static inline int acx100_ioctl_list_reg_domain(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	int i;
	char **entry;

	(void)printk("Domain/Country  Channels  Setting\n");
	for (i = 0, entry = reg_domain_strings; *entry; i++, entry++)
		(void)printk("%s      %d\n", *entry, i+1);
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_reg_domain
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
static inline int acx100_ioctl_set_reg_domain(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	if ((*extra < 1) || ((size_t)*extra > sizeof(reg_domain_ids))) {
		result = -EINVAL;
		goto end_unlock;
	}
	priv->reg_dom_id = reg_domain_ids[*extra - 1];
	priv->set_mask |= GETSET_REG_DOMAIN;
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(priv, &flags);
end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_reg_domain
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
static inline int acx100_ioctl_get_reg_domain(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int i;

	for (i=1; i <= 7; i++)
		if (reg_domain_ids[i-1] == priv->reg_dom_id)
		{
			acxlog(L_STD, "regulatory domain is currently set to %d (0x%x): %s\n", i, priv->reg_dom_id, reg_domain_strings[i-1]);
			*extra = i;
			break;
		}

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_short_preamble
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
static inline int acx100_ioctl_set_short_preamble(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	char *descr = NULL;

	if ((*extra < (char)0) || (*extra > (char)2))
		return -EINVAL;

	priv->preamble_mode = (UINT8)*extra;
	switch (*extra) {
		case 0:
			descr = "off";
			priv->preamble_flag = (UINT8)0;
			break;
		case 1:
			descr = "on";
			priv->preamble_flag = (UINT8)1;
			break;
		case 2:
			descr = "auto (peer capability dependent)";

			/* associated to a station? */
			if ((UINT8)0x0 != priv->station_assoc.bssid[0])
				priv->preamble_flag = (UINT8)((priv->station_assoc.caps & IEEE802_11_MGMT_CAP_SHORT_PRE) == IEEE802_11_MGMT_CAP_SHORT_PRE);
			break;
		default:
			descr = "unknown mode, error";
			break;
	}
	(void)printk("new Short Preamble setting: %s\n", descr);

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_short_preamble
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
static inline int acx100_ioctl_get_short_preamble(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	char *descr = NULL;

	switch(priv->preamble_mode) {
		case 0:
			descr = "off";
			break;
		case 1:
			descr = "on";
			break;
		case 2:
			descr = "auto (peer capability dependent)";
			break;
		default:
			descr = "unknown mode, error";
			break;
	}
	(void)printk("current Short Preamble setting: %s\n", descr);

	*extra = (char)priv->preamble_mode;

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_antenna
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
* Comment: TX and RX antenna can be set separately but this function good
*          for testing 0-4 bits
*----------------------------------------------------------------*/
static inline int acx100_ioctl_set_antenna(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	(void)printk("current antenna value: 0x%02X\n", priv->antenna);
	(void)printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	(void)printk("Tx antenna selection seems to be bit 5 (0x20)\n");
	priv->antenna = (UINT8)*extra;
	(void)printk("new antenna value: 0x%02X\n", priv->antenna);
	priv->set_mask |= GETSET_ANTENNA;

	return -EINPROGRESS;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_antenna
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
static inline int acx100_ioctl_get_antenna(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	(void)printk("current antenna value: 0x%02X\n", priv->antenna);
	(void)printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	(void)printk("Tx antenna selection seems to be bit 5 (0x20)\n");

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_rx_antenna
*
*
* Arguments: 0 = antenna1; 1 = antenna2; 
* / 2 and 3 = diversity?  - need test / 
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS: NEW
*
* Comment: Could anybody test which antenna is the external one
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_set_rx_antenna(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	(void)printk("current antenna value: 0x%02X\n", priv->antenna);
	/* better keep the separate operations atomic */
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	priv->antenna &= 0x3f;
	priv->antenna |= (*extra << 6);
	priv->set_mask |= GETSET_ANTENNA;
	acx100_unlock(priv, &flags);
	(void)printk("new antenna value: 0x%02X\n", priv->antenna);
	result = -EINPROGRESS;

end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_tx_antenna
*
*
* Arguments: 0 = antenna1; 1 = antenna2;
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS: NEW
*
* Comment: Could anybody test which antenna is the external one
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_set_tx_antenna(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	(void)printk("current antenna value: 0x%02X\n", priv->antenna);
	/* better keep the separate operations atomic */
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	priv->antenna &= 0xdf;
	priv->antenna |= ((*extra &= 0x01) << 5);
	priv->set_mask |= GETSET_ANTENNA;
	acx100_unlock(priv, &flags);
	(void)printk("new antenna value: 0x%02X\n", priv->antenna);
	result = -EINPROGRESS;

end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_wlansniff
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
static inline int acx100_ioctl_wlansniff(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int *parms = (int*)extra;
	int enable = (int)(parms[0] > 0);
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	priv->monitor = parms[0];
	/* not using printk() here, since it distorts kismet display
	 * when printk messages activated */
	acxlog(L_IOCTL, "setting monitor to: 0x%02X\n", priv->monitor);

	switch (parms[0])
	{
	case 0:
		priv->netdev->type = ARPHRD_ETHER;
		break;
	case 1:
		priv->netdev->type = ARPHRD_IEEE80211_PRISM;
		break;
	case 2:
		priv->netdev->type = ARPHRD_IEEE80211;
		break;
	}

	if (0 != priv->monitor)
		priv->monitor_setting = 0x02; /* don't decrypt default key only, override decryption mechanism */
	else
		priv->monitor_setting = 0x00; /* don't decrypt default key only, don't override decryption */

	priv->set_mask |= SET_RXCONFIG | SET_WEP_OPTIONS;

	if (0 != enable)
	{
		priv->channel = parms[1];
		priv->set_mask |= GETSET_RX;
	}
	acx100_unlock(priv, &flags);
	result = -EINPROGRESS;

end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_unknown11
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
static inline int acx100_ioctl_unknown11(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	client_t client;
	int err;
	int result = -EINVAL;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	acx100_transmit_disassoc(&client, priv);
	acx100_unlock(priv, &flags);
	result = 0;

end:
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_ed_threshold
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
static inline int acx100_ioctl_set_ed_threshold(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;

	(void)printk("current ED threshold value: %d\n", priv->ed_threshold);
	priv->ed_threshold = (unsigned char)*extra;
	(void)printk("new ED threshold value: %d\n", (unsigned char)*extra);
	priv->set_mask |= GETSET_ED_THRESH;

	return -EINPROGRESS;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_cca
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
static inline int acx100_ioctl_set_cca(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	(void)printk("current CCA value: 0x%02X\n", priv->cca);
	priv->cca = (unsigned char)*extra;
	(void)printk("new CCA value: 0x%02X\n", (unsigned char)*extra);
	priv->set_mask |= GETSET_CCA;
	acx100_unlock(priv, &flags);
	result = -EINPROGRESS;

end:
	return result;
}

static inline int acx100_ioctl_set_led_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	(void)printk("current power LED status: %d\n", priv->led_power);
	priv->led_power = (unsigned char)*extra;
	(void)printk("new power LED status: %d\n", (unsigned char)*extra);
	priv->set_mask |= GETSET_LED_POWER;

	acx100_unlock(priv, &flags);
	result = -EINPROGRESS;

end:
	return result;
}

#if WIRELESS_EXT >= 13
#warning "(NOT a warning!) Compile info: choosing to use code infrastructure for NEWER wireless extension interface version (>= 13)"
static const iw_handler acx100_ioctl_handler[] =
{
	(iw_handler) acx100_ioctl_commit,	/* SIOCSIWCOMMIT */
	(iw_handler) acx100_ioctl_get_name,	/* SIOCGIWNAME */
	(iw_handler) NULL,			/* SIOCSIWNWID */
	(iw_handler) NULL,			/* SIOCGIWNWID */
	(iw_handler) acx100_ioctl_set_freq,	/* SIOCSIWFREQ */
	(iw_handler) acx100_ioctl_get_freq,	/* SIOCGIWFREQ */
	(iw_handler) acx100_ioctl_set_mode,	/* SIOCSIWMODE */
	(iw_handler) acx100_ioctl_get_mode,	/* SIOCGIWMODE */
	(iw_handler) acx100_ioctl_set_sens,	/* SIOCSIWSENS */
	(iw_handler) acx100_ioctl_get_sens,	/* SIOCGIWSENS */
	(iw_handler) NULL,			/* SIOCSIWRANGE */
	(iw_handler) acx100_ioctl_get_range,	/* SIOCGIWRANGE */
	(iw_handler) NULL,			/* SIOCSIWPRIV */
	(iw_handler) NULL,			/* SIOCGIWPRIV */
	(iw_handler) NULL,			/* SIOCSIWSTATS */
	(iw_handler) NULL,			/* SIOCGIWSTATS */
#if IW_HANDLER_VERSION > 4
	iw_handler_set_spy,			/* SIOCSIWSPY */
	iw_handler_get_spy,			/* SIOCGIWSPY */
	iw_handler_set_thrspy,			/* SIOCSIWTHRSPY */
	iw_handler_get_thrspy,			/* SIOCGIWTHRSPY */
#else /* IW_HANDLER_VERSION > 4 */
#ifdef WIRELESS_SPY
	(iw_handler) NULL /* acx100_ioctl_set_spy */,	/* SIOCSIWSPY */
	(iw_handler) NULL /* acx100_ioctl_get_spy */,	/* SIOCGIWSPY */
#else /* WSPY */
	(iw_handler) NULL,			/* SIOCSIWSPY */
	(iw_handler) NULL,			/* SIOCGIWSPY */
#endif /* WSPY */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) NULL,			/* [nothing] */
#endif /* IW_HANDLER_VERSION > 4 */
	(iw_handler) acx100_ioctl_set_ap,	/* SIOCSIWAP */
	(iw_handler) acx100_ioctl_get_ap,	/* SIOCGIWAP */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) acx100_ioctl_get_aplist,	/* SIOCGIWAPLIST */
#if WIRELESS_EXT > 13
	(iw_handler) acx100_ioctl_set_scan,	/* SIOCSIWSCAN */
	(iw_handler) acx100_ioctl_get_scan,	/* SIOCGIWSCAN */
#else /* WE > 13 */
	(iw_handler) NULL,			/* SIOCSIWSCAN */
	(iw_handler) NULL,			/* SIOCGIWSCAN */
#endif /* WE > 13 */
	(iw_handler) acx100_ioctl_set_essid,	/* SIOCSIWESSID */
	(iw_handler) acx100_ioctl_get_essid,	/* SIOCGIWESSID */
	(iw_handler) acx100_ioctl_set_nick,	/* SIOCSIWNICKN */
	(iw_handler) acx100_ioctl_get_nick,	/* SIOCGIWNICKN */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) acx100_ioctl_set_rate,	/* SIOCSIWRATE */
	(iw_handler) acx100_ioctl_get_rate,	/* SIOCGIWRATE */
	(iw_handler) acx100_ioctl_set_rts,	/* SIOCSIWRTS */
	(iw_handler) acx100_ioctl_get_rts,	/* SIOCGIWRTS */
	(iw_handler) NULL /* acx100_ioctl_set_frag FIXME */,	/* SIOCSIWFRAG */
	(iw_handler) NULL /* acx100_ioctl_get_frag FIXME */,	/* SIOCGIWFRAG */
	(iw_handler) acx100_ioctl_set_txpow,	/* SIOCSIWTXPOW */
	(iw_handler) acx100_ioctl_get_txpow,	/* SIOCGIWTXPOW */
	(iw_handler) acx100_ioctl_set_retry,	/* SIOCSIWRETRY */
	(iw_handler) acx100_ioctl_get_retry,	/* SIOCGIWRETRY */
	(iw_handler) acx100_ioctl_set_encode,	/* SIOCSIWENCODE */
	(iw_handler) acx100_ioctl_get_encode,	/* SIOCGIWENCODE */
	(iw_handler) acx100_ioctl_set_power,	/* SIOCSIWPOWER */
	(iw_handler) acx100_ioctl_get_power,	/* SIOCGIWPOWER */
};

static const iw_handler acx100_ioctl_private_handler[] =
{
#ifdef ACX_DEBUG
	(iw_handler) acx100_ioctl_set_debug,			/* SIOCIWFIRSTPRIV */
#else
	(iw_handler) NULL,
#endif
	(iw_handler) acx100_ioctl_list_reg_domain,
	(iw_handler) acx100_ioctl_set_reg_domain,
	(iw_handler) acx100_ioctl_get_reg_domain,
	(iw_handler) acx100_ioctl_set_short_preamble,
	(iw_handler) acx100_ioctl_get_short_preamble,
	(iw_handler) acx100_ioctl_set_antenna,
	(iw_handler) acx100_ioctl_get_antenna,
	(iw_handler) acx100_ioctl_set_rx_antenna,
	(iw_handler) acx100_ioctl_set_tx_antenna,
	(iw_handler) acx100_ioctl_set_ed_threshold,
	(iw_handler) acx100_ioctl_set_cca,
	(iw_handler) acx100_ioctl_set_led_power,
	(iw_handler) acx100_ioctl_wlansniff,
	(iw_handler) acx100_ioctl_unknown11
};

const struct iw_handler_def acx100_ioctl_handler_def =
{
	.num_standard = sizeof(acx100_ioctl_handler)/sizeof(iw_handler),
	.num_private = sizeof(acx100_ioctl_private_handler)/sizeof(iw_handler),
	.num_private_args = sizeof(acx100_ioctl_private_args)/sizeof(struct iw_priv_args),
	.standard = (iw_handler *) acx100_ioctl_handler,
	.private = (iw_handler *) acx100_ioctl_private_handler,
	.private_args = (struct iw_priv_args *) acx100_ioctl_private_args,
#if WIRELESS_EXT > 15
	.spy_offset = 0 /* FIXME */,
#endif /* WE > 15 */
};

#endif /* WE > 12 */



#if WIRELESS_EXT < 13
/*================================================================*/
/* Main function						  */
/*================================================================*/
/*----------------------------------------------------------------
* acx_ioctl_old
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
* This is the *OLD* ioctl handler.
* Make sure to not only place your additions here, but instead mainly
* in the new one (acx100_ioctl_handler[])!
*
*----------------------------------------------------------------*/
int acx_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	int result = 0;
#if WIRELESS_EXT < 13
	struct iwreq *iwr = (struct iwreq *)ifr;
#endif

	acxlog(L_IOCTL, "%s cmd = 0x%04X\n", __func__, cmd);

	/* This is the way it is done in the orinoco driver.
	 * Check to see if device is present.
	 */
	if (0 == netif_device_present(dev))
	{
		return -ENODEV;
	}

	switch (cmd) {
/* WE 13 and higher will use acx100_ioctl_handler_def */
#if WIRELESS_EXT < 13
#warning "(NOT a warning!) Compile info: choosing to use code infrastructure for OLDER wireless extension interface version (< 13)"
#warning "This support is not perfectly tested, please report any problems! Upgrading to relatively current Linux kernel package recommended (will also fix some security issues which older ones have)"
	case SIOCGIWNAME:
		/* get name == wireless protocol */
		result = acx100_ioctl_get_name(dev, NULL,
					       (char *)&(iwr->u.name), NULL);
		break;

	case SIOCSIWNWID: /* pre-802.11, */
	case SIOCGIWNWID: /* not supported. */
		result = -EOPNOTSUPP;
		break;

	case SIOCSIWFREQ:
		/* set channel/frequency (Hz)
		   data can be frequency or channel :
		   0-1000 = channel
		   > 1000 = frequency in Hz */
		result = acx100_ioctl_set_freq(dev, NULL, &(iwr->u.freq), NULL);
		break;

	case SIOCGIWFREQ:
		/* get channel/frequency (Hz) */
		result = acx100_ioctl_get_freq(dev, NULL, &(iwr->u.freq), NULL);
		break;

	case SIOCSIWMODE:
		/* set operation mode */
		result = acx100_ioctl_set_mode(dev, NULL, &(iwr->u.mode), NULL);
		break;

	case SIOCGIWMODE:
		/* get operation mode */
		result = acx100_ioctl_get_mode(dev, NULL, &(iwr->u.mode), NULL);
		break;

	case SIOCSIWSENS:
		/* Set sensitivity */
		result = acx100_ioctl_set_sens(dev, NULL, &(iwr->u.sens), NULL);
		break; 

	case SIOCGIWSENS:
		/* Get sensitivity */
		result = acx100_ioctl_get_sens(dev, NULL, &(iwr->u.sens), NULL);
		break;

#if WIRELESS_EXT > 10
	case SIOCGIWRANGE:
		/* Get range of parameters */
		{
			struct iw_range range;
			result = acx100_ioctl_get_range(dev, NULL,
					&(iwr->u.data), (char *)&range);
			if (copy_to_user(iwr->u.data.pointer, &range,
					 sizeof(struct iw_range)))
				result = -EFAULT;
		}
		break;
#endif

	case SIOCGIWPRIV:
		result = acx100_ioctl_get_iw_priv(iwr);
		break;

	/* case SIOCSIWSPY: FIXME */
	/* case SIOCGIWSPY: FIXME */
	/* case SIOCSIWTHRSPY: FIXME */
	/* case SIOCGIWTHRSPY: FIXME */

	case SIOCSIWAP:
		/* set access point by MAC address */
		result = acx100_ioctl_set_ap(dev, NULL, &(iwr->u.ap_addr),
					     NULL);
		break;

	case SIOCGIWAP:
		/* get access point MAC address */
		result = acx100_ioctl_get_ap(dev, NULL, &(iwr->u.ap_addr),
					     NULL);
		break;

	case SIOCGIWAPLIST:
		/* get list of access points in range */
		result = acx100_ioctl_get_aplist(dev, NULL, &(iwr->u.data),
						 NULL);
		break;

#if NOT_FINISHED_YET
	case SIOCSIWSCAN:
		/* start a station scan */
		result = acx100_ioctl_set_scan(iwr, priv);
		break;

	case SIOCGIWSCAN:
		/* get list of stations found during scan */
		result = acx100_ioctl_get_scan(iwr, priv);
		break;
#endif

	case SIOCSIWESSID:
		/* set ESSID (network name) */
		{
			char essid[IW_ESSID_MAX_SIZE+1];

			if (iwr->u.essid.length > IW_ESSID_MAX_SIZE)
			{
				result = -E2BIG;
				break;
			}
			if (copy_from_user(essid, iwr->u.essid.pointer,
						iwr->u.essid.length))
			{
				result = -EFAULT;
				break;
			}
			result = acx100_ioctl_set_essid(dev, NULL,
					&(iwr->u.essid), essid);
		}
		break;

	case SIOCGIWESSID:
		/* get ESSID */
		{
			char essid[IW_ESSID_MAX_SIZE+1];
			if (iwr->u.essid.pointer)
				result = acx100_ioctl_get_essid(dev, NULL,
					&(iwr->u.essid), essid);
			if (copy_to_user(iwr->u.essid.pointer, essid,
						iwr->u.essid.length))
				result = -EFAULT;
		}
		break;

	case SIOCSIWNICKN:
		/* set nick */
		{
			char nick[IW_ESSID_MAX_SIZE+1];

			if (iwr->u.data.length > IW_ESSID_MAX_SIZE)
			{
				result = -E2BIG;
				break;
			}
			if (copy_from_user(nick, iwr->u.data.pointer,
						iwr->u.data.length))
			{
				result = -EFAULT;
				break;
			}
			result = acx100_ioctl_set_nick(dev, NULL,
					&(iwr->u.data), nick);
		}
		break;

	case SIOCGIWNICKN:
		/* get nick */
		{
			char nick[IW_ESSID_MAX_SIZE+1];
			if (iwr->u.data.pointer)
				result = acx100_ioctl_get_nick(dev, NULL,
						&(iwr->u.data), nick);
			if (copy_to_user(iwr->u.data.pointer, nick,
						iwr->u.data.length))
				result = -EFAULT;
		}
		break;

	case SIOCSIWRATE:
		/* set default bit rate (bps) */
		result = acx100_ioctl_set_rate(dev, NULL, &(iwr->u.bitrate),
					       NULL);
		break;

	case SIOCGIWRATE:
		/* get default bit rate (bps) */
		result = acx100_ioctl_get_rate(dev, NULL, &(iwr->u.bitrate),
					       NULL);
		break;

	case  SIOCSIWRTS:
		/* set RTS threshold value */
		result = acx100_ioctl_set_rts(dev, NULL, &(iwr->u.rts), NULL);
		break;
	case  SIOCGIWRTS:
		/* get RTS threshold value */
		result = acx100_ioctl_get_rts(dev, NULL,  &(iwr->u.rts), NULL);
		break;

	/* case  SIOCSIWFRAG: FIXME */
	/* case  SIOCGIWFRAG: FIXME */

#if WIRELESS_EXT > 9
	case SIOCGIWTXPOW:
		/* get tx power */
		result = acx100_ioctl_get_txpow(dev, NULL, &(iwr->u.txpower),
						NULL);
		break;

	case SIOCSIWTXPOW:
		/* set tx power */
		result = acx100_ioctl_set_txpow(dev, NULL, &(iwr->u.txpower),
						NULL);
		break;
#endif

	case SIOCSIWRETRY:
		result = acx100_ioctl_set_retry(dev, NULL, &(iwr->u.retry), NULL);
		break;
		
	case SIOCGIWRETRY:
		result = acx100_ioctl_get_retry(dev, NULL, &(iwr->u.retry), NULL);
		break;

	case SIOCSIWENCODE:
		{
			/* set encoding token & mode */
			UINT8 key[29];
			if (iwr->u.encoding.pointer) {
				if (iwr->u.encoding.length > 29) {
					result = -E2BIG;
					break;
				}
				if (copy_from_user(key, iwr->u.encoding.pointer, iwr->u.encoding.length)) {
					result = -EFAULT;
					break;
				}
			}
			else
			if (0 != iwr->u.encoding.length) {
				result = -EINVAL;
				break;
			}
			result = acx100_ioctl_set_encode(dev, NULL,
					&(iwr->u.encoding), key);
		}
		break;

	case SIOCGIWENCODE:
		{
			/* get encoding token & mode */
			UINT8 key[29];

			result = acx100_ioctl_get_encode(dev, NULL,
					&(iwr->u.encoding), key);
			if (iwr->u.encoding.pointer) {
				if (copy_to_user(iwr->u.encoding.pointer,
						key, iwr->u.encoding.length))
					result = -EFAULT;
			}
		}
		break;

	/******************** iwpriv ioctls below ********************/
#ifdef ACX_DEBUG
	case ACX100_IOCTL_DEBUG:
		acx100_ioctl_set_debug(dev, NULL, NULL, iwr->u.name);
		break;
#endif
		
	case ACX100_IOCTL_LIST_DOM:
		acx100_ioctl_list_reg_domain(dev, NULL, NULL, NULL);
		break;
		
	case ACX100_IOCTL_SET_DOM:
		acx100_ioctl_set_reg_domain(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_GET_DOM:
		acx100_ioctl_get_reg_domain(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_SET_PREAMB:
		acx100_ioctl_set_short_preamble(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_GET_PREAMB:
		acx100_ioctl_get_short_preamble(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_SET_ANT:
		acx100_ioctl_set_antenna(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_GET_ANT:
		acx100_ioctl_get_antenna(dev, NULL, NULL, NULL);
		break;
		
	case ACX100_IOCTL_SET_ED:
		acx100_ioctl_set_ed_threshold(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_SET_CCA:
		acx100_ioctl_set_cca(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_SET_PLED:
		acx100_ioctl_set_led_power(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_MONITOR:	/* set sniff (monitor) mode */
		acxlog(L_IOCTL, "%s: IWPRIV monitor\n", dev->name);

		/* can only be done by admin */
		if (!capable(CAP_NET_ADMIN)) {
			result = -EPERM;
			break;
		}
		result = acx100_ioctl_wlansniff(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_RX_ANT:
		acx100_ioctl_set_rx_antenna(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_TX_ANT:
		acx100_ioctl_set_tx_antenna(dev, NULL, NULL, iwr->u.name);
		break;
		
	case ACX100_IOCTL_TEST:
		acx100_ioctl_unknown11(dev, NULL, NULL, NULL);
		break;
#endif

	default:
		acxlog(L_IOCTL, "wireless ioctl 0x%04X queried but not implemented yet!\n", cmd);
		result = -EOPNOTSUPP;
		break;
	}

	if ((0 != (priv->dev_state_mask & ACX_STATE_IFACE_UP))
	&& (0 != priv->set_mask))
		acx100_update_card_settings(priv, 0, 0, 0);

#if WIRELESS_EXT < 13
	/* older WEs don't have a commit handler,
	 * so we need to fix return code in this case */
	if (-EINPROGRESS == result)
		result = 0;
#endif

	return result;
}
#endif
