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
#include <asm/io.h>

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
#include <idma.h>

/* About the locking:
 *  I only locked the device whenever calls to the hardware are made or
 *  parts of the wlandevice struct are modified, otherwise no locking is
 *  performed. I don't now if this is safe, we'll see.
 */

extern UINT8 acx_signal_determine_quality(UINT8 signal, UINT8 noise);


/* if you plan to reorder something, make sure to reorder all other places
 * accordingly! */
#define ACX100_IOCTL			SIOCIWFIRSTPRIV
#define ACX100_IOCTL_DEBUG		ACX100_IOCTL + 0x00
#define ACX100_IOCTL_LIST_DOM		ACX100_IOCTL + 0x01
#define ACX100_IOCTL_SET_DOM		ACX100_IOCTL + 0x02
#define ACX100_IOCTL_GET_DOM		ACX100_IOCTL + 0x03
#define ACX100_IOCTL_SET_SCAN_MODE	ACX100_IOCTL + 0x04
#define ACX100_IOCTL_SET_SCAN_CHAN_DELAY	ACX100_IOCTL + 0x05
#define ACX100_IOCTL_SET_PREAMB		ACX100_IOCTL + 0x06
#define ACX100_IOCTL_GET_PREAMB		ACX100_IOCTL + 0x07
#define ACX100_IOCTL_SET_ANT		ACX100_IOCTL + 0x08
#define ACX100_IOCTL_GET_ANT		ACX100_IOCTL + 0x09
#define ACX100_IOCTL_RX_ANT		ACX100_IOCTL + 0x0a
#define ACX100_IOCTL_TX_ANT		ACX100_IOCTL + 0x0b
#define ACX100_IOCTL_SET_PHY_AMP_BIAS	ACX100_IOCTL + 0x0c
#define ACX100_IOCTL_GET_PHY_MEDIUM_BUSY	ACX100_IOCTL + 0x0d
#define ACX100_IOCTL_SET_ED		ACX100_IOCTL + 0x0e
#define ACX100_IOCTL_SET_CCA		ACX100_IOCTL + 0x0f
#define ACX100_IOCTL_SET_PLED		ACX100_IOCTL + 0x10
#define ACX100_IOCTL_MONITOR		ACX100_IOCTL + 0x11
#define ACX100_IOCTL_TEST		ACX100_IOCTL + 0x12
#define ACX100_IOCTL_DBG_SET_MASKS	ACX100_IOCTL + 0x13
#define ACX100_IOCTL_DBG_GET_IO		ACX100_IOCTL + 0x14
#define ACX100_IOCTL_DBG_SET_IO		ACX100_IOCTL + 0x15
#define ACX100_IOCTL_ACX111_INFO	ACX100_IOCTL + 0x16
#define ACX100_IOCTL_SET_RATES		ACX100_IOCTL + 0x17


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
	name : "SetDebug" },
#endif
{ cmd : ACX100_IOCTL_LIST_DOM,
	set_args : 0,
	get_args : 0,
	name : "ListRegDomain" },
{ cmd : ACX100_IOCTL_SET_DOM,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetRegDomain" },
{ cmd : ACX100_IOCTL_GET_DOM,
	set_args : 0,
	get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	name : "GetRegDomain" },
{ cmd : ACX100_IOCTL_SET_SCAN_MODE,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetScanMode" },
{ cmd : ACX100_IOCTL_SET_SCAN_CHAN_DELAY,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetScanDelay" },
{ cmd : ACX100_IOCTL_SET_PREAMB,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetSPreamble" },
{ cmd : ACX100_IOCTL_GET_PREAMB,
	set_args : 0,
	get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	name : "GetSPreamble" },
{ cmd : ACX100_IOCTL_SET_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetAntenna" },
{ cmd : ACX100_IOCTL_GET_ANT,
	set_args : 0,
	get_args : 0,
	name : "GetAntenna" },
{ cmd : ACX100_IOCTL_RX_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetRxAnt" },
{ cmd : ACX100_IOCTL_TX_ANT,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetTxAnt" },
{ cmd : ACX100_IOCTL_SET_PHY_AMP_BIAS,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetPhyAmpBias"},
{ cmd : ACX100_IOCTL_GET_PHY_MEDIUM_BUSY,
	set_args : 0,
	get_args : 0,
	name : "GetPhyChanBusy" },
{ cmd : ACX100_IOCTL_SET_ED,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetED" },
{ cmd : ACX100_IOCTL_SET_CCA,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetCCA" },
{ cmd : ACX100_IOCTL_SET_PLED,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetLEDPower" },
{ cmd : ACX100_IOCTL_MONITOR,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	get_args : 0,
	name : "monitor" },
{ cmd : ACX100_IOCTL_TEST,
	set_args : 0,
	get_args : 0,
	name : "Test" },
{ cmd : ACX100_IOCTL_DBG_SET_MASKS,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	get_args : 0,
	name : "DbgSetMasks" },
{ cmd : ACX100_IOCTL_DBG_GET_IO,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
	get_args : 0,
	name : "DbgGetIO" },
{ cmd : ACX100_IOCTL_DBG_SET_IO,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4,
	get_args : 0,
	name : "DbgSetIO" },
{ cmd : ACX100_IOCTL_ACX111_INFO,
	set_args : 0,
	get_args : 0,
	name : "GetAcx111Info" },
{ cmd : ACX100_IOCTL_SET_RATES,
	set_args : IW_PRIV_TYPE_CHAR | 256,
	get_args : 0,
	name : "SetRates" },
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
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	const char * const protocol_name =
		(CHIPTYPE_ACX100 == priv->chip_type) ? "IEEE 802.11b+" : "IEEE 802.11g+";
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

		for (i = 1; i <= 14; i++)
			if (fwrq->m == acx100_channel_freq[i - 1] * mult)
				channel = i;
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
	if (ACX_MODE_3_MANAGED_AP != priv->macmode_wanted) {
		/* trigger scanning if we're a client... */
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

	if ((RADIO_RFMD_11 == priv->radio_type)
	|| (RADIO_MAXIM_0D == priv->radio_type)
	|| (RADIO_RALINK_15 == priv->radio_type)) {
		priv->sensitivity = (1 == vwrq->disabled) ? 0 : vwrq->value;
		priv->set_mask |= GETSET_SENSITIVITY;
		return -EINPROGRESS;
	} else {
		printk("Don't know how to modify sensitivity for this radio type, please try to add that!\n");
		return -EINVAL;
	}
}

static inline int acx100_ioctl_get_sens(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	if ((RADIO_RFMD_11 == priv->radio_type)
	|| (RADIO_MAXIM_0D == priv->radio_type)
	|| (RADIO_RALINK_15 == priv->radio_type)) {
		acxlog(L_IOCTL, "Get Sensitivity ==> %d\n", priv->sensitivity);

		vwrq->value = priv->sensitivity;
		vwrq->disabled = (vwrq->value == 0);
		vwrq->fixed = 1;
		return 0;
	} else {
		printk("Don't know how to get sensitivity for this radio type, please try to add that!\n");
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

	/* We want to restrict to a specific AP when in Managed or Auto mode
	 * only, right? */
	if ((ACX_MODE_2_MANAGED_STA != priv->macmode_wanted)
	&& (ACX_MODE_FF_AUTO != priv->macmode_wanted)) {
		result = -EINVAL;
		goto end;
	}

	if (0 != acx100_is_mac_address_broadcast(ap)) {
		/* "any" == "auto" == FF:FF:FF:FF:FF:FF */
		MAC_BCAST(priv->ap);
		acxlog(L_IOCTL, "Forcing reassociation\n");
		if(priv->chip_type == CHIPTYPE_ACX100) {
			acx100_scan_chan(priv);
		} else if(priv->chip_type == CHIPTYPE_ACX111) {
			acx111_scan_chan(priv);
		}
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
					acxlog(L_STD | L_IOCTL, "The WEP setting of the matching AP (%d) differs from our WEP setting --> will NOT restrict association to its BSSID!\n", i);
					result = -EINVAL;
					goto end;
                        	} else {
					MAC_COPY(priv->ap, ap);
					acxlog(L_IOCTL, "Forcing reassociation\n");
					if(priv->chip_type == CHIPTYPE_ACX100) {
						acx100_scan_chan(priv);
					} else if(priv->chip_type == CHIPTYPE_ACX111) {
						acx111_scan_chan(priv);
					}
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

	if(priv->chip_type == CHIPTYPE_ACX100) {
		acx100_scan_chan(priv);
	} else if(priv->chip_type == CHIPTYPE_ACX111) {
		acx111_scan_chan(priv);
	}


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
	iwe.u.freq.m = acx100_channel_freq[bss->channel - 1] * 100000;
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
		acxlog(L_IOCTL, "iface not up yet\n");
		result = -EAGAIN;
		goto end;
	}

	if (priv->scan_start && time_before(jiffies, priv->scan_start + 3*HZ))
	{
		acxlog(L_IOCTL, "scan still in progress, so no results yet, sorry\n");
		result = -EAGAIN;
		goto end;
	}
	priv->scan_start = 0;

#if ENODATA_TO_BE_USED_AFTER_SCAN_ERROR_ONLY
	if (priv->bss_table_count == 0)
	{
		/* no stations found */
		result = -ENODATA;
		goto end;
	}
#endif

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
	acxlog(L_IOCTL, "Set ESSID <== %s, length %d, flags 0x%04x\n", extra, len, dwrq->flags);

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
* acx_ioctl_set_rate
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
static int acx111_rate_tbl[] = {
     1000000,
     2000000,
     5500000,
     6000000,
     9000000,
    11000000,
    12000000,
    18000000,
    22000000,
    24000000,
    36000000,
    48000000,
    54000000,
};
#define VEC_SIZE(a) (sizeof(a)/sizeof(a[0]))

static int
acx_ioctl_set_rate(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	UINT16 txrate_cfg = 1;
	unsigned long flags;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL,
	       "rate = %d, fixed = 0x%x, disabled = 0x%x, flags = 0x%x\n",
	       vwrq->value, vwrq->fixed, vwrq->disabled, vwrq->flags);

	if ((0 == vwrq->fixed) || (1 == vwrq->fixed)) {
		int i;
		if(vwrq->value == -1) vwrq->value = 54000000;
		i = VEC_SIZE(acx111_rate_tbl)-1;
		while(i >= 0) {
			if(vwrq->value == acx111_rate_tbl[i]) {
				while(i--)
					txrate_cfg <<= 1;
				if(vwrq->fixed==0)
					txrate_cfg = (txrate_cfg<<1)-1;
				i = 0;
				break;
			}
			i--;
		}
		if(i == -1) { /* no matching rate */
			result = -EINVAL;
			goto end;
		}
	} else {	/* rate N, N<1000 (driver specific): we don't use this */
		result = -EOPNOTSUPP;
		goto end;
	}
	
        if (CHIPTYPE_ACX100 == priv->chip_type)
		txrate_cfg &= RATE111_ACX100_COMPAT;

	result = acx100_lock(priv, &flags);
	if(result)
		goto end;

	priv->txrate_cfg = txrate_cfg;
	priv->txrate_curr = priv->txrate_cfg;
	priv->txrate_auto = (vwrq->fixed == 0);
	if (priv->txrate_auto)
	{
		if (RATE111_1 == txrate_cfg) { /* auto rate with 1Mbps max. useless */
			priv->txrate_auto = 0;
		} else {
			/* needed? curr = RATE111_1 + RATE111_2; */ /* 2Mbps, play it safe at the beginning */
			priv->txrate_fallback_count = 0;
			priv->txrate_stepup_count = 0;
		}
	}
	
	acx100_unlock(priv, &flags);

	acxlog(L_IOCTL, "txrate_cfg %04x txrate_curr %04x txrate_auto %d\n",
		priv->txrate_cfg, priv->txrate_curr, priv->txrate_auto);

	priv->set_mask |= SET_RATE_FALLBACK;
	result = -EINPROGRESS;
end:
	FN_EXIT(1, result);
	return result;
}
/*----------------------------------------------------------------
* acx_ioctl_get_rate
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
static int
acx_ioctl_get_rate(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int i = 0;
	UINT16 curr = priv->txrate_curr;

	acxlog(L_IOCTL, "txrate_cfg %04x txrate_curr %04x txrate_auto %d\n",
		priv->txrate_cfg, priv->txrate_curr, priv->txrate_auto);
		
	while(curr>1) {
		i++;
		curr>>=1;
	}
	vwrq->value = acx111_rate_tbl[i];
	vwrq->fixed = (__u8)(priv->txrate_auto == (UINT8)0);
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

	index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	if (dwrq->length > 0) {

		/* if index is 0 or invalid, use default key */
		if ((index < 0) || (index > 3))
			index = (int)priv->wep_current_index;

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
	int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_wanted) {
		/* it's most definitely supported now. */
		/* return -EOPNOTSUPP; */
	}

	if (priv->wep_enabled == (UINT8)0)
	{
		dwrq->flags = IW_ENCODE_DISABLED;
	}
	else
	{
		if ((index < 0) || (index > 3))
			index = (int)priv->wep_current_index;

		dwrq->flags =
			(priv->wep_restricted == (UINT8)1) ? IW_ENCODE_RESTRICTED : IW_ENCODE_OPEN;

		dwrq->length =
		    priv->wep_keys[index].size;

		memcpy(extra,
			     priv->wep_keys[index].key,
			     priv->wep_keys[index].size);
	}

	/* set the current index */
	dwrq->flags |= index + 1;

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
		for (i = 1; i <= 14; i++)
			if (0 != (priv->reg_dom_chanmask & (1 << (i - 1))))
			{
				range->freq[range->num_channels].i = i;
				range->freq[range->num_channels].m = acx100_channel_freq[i - 1] * 100000;
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
		(void)printk("current retry limits: short %d long %d\n", priv->short_retry, priv->long_retry);
                if (0 != (vwrq->flags & IW_RETRY_MAX)) {
                        priv->long_retry = vwrq->value;
                } else if (0 != (vwrq->flags & IW_RETRY_MIN)) {
                        priv->short_retry = vwrq->value;
                } else {
                        /* no modifier: set both */
                        priv->long_retry = vwrq->value;
                        priv->short_retry = vwrq->value;
                }
		(void)printk("new retry limits: short %d long %d\n", priv->short_retry, priv->long_retry);
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

	acxlog(0xffff, "%s: setting debug from 0x%04X to 0x%04X\n", __func__,
	       debug, debug_new);
	debug = debug_new;

	result = 0;
	return result;

}
#endif

extern const UINT8 reg_domain_ids[];
extern const UINT8 reg_domain_ids_len;

const char *reg_domain_strings[] =
{ "FCC (USA)        (1-11)",
  "DOC/IC (Canada)  (1-11)",
	/* BTW: WLAN use in ETSI is regulated by
	 * ETSI standard EN 300 328-2 V1.1.2 */
  "ETSI (Europe)    (1-13)",
  "Spain           (10-11)",
  "France          (10-13)",
  "MKK (Japan)        (14)",
  "MKK1             (1-14)",
  "Israel            (3-9) (not all firmware versions)",
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
	const char **entry;

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

	if ((*extra < 1) || ((size_t)*extra > reg_domain_ids_len)) {
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

	if (*extra > (char)2)
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

	(void)printk("current antenna value: 0x%02X (COMBINED bit mask)\n", priv->antenna);
	(void)printk("Rx antenna selection:\n");
	(void)printk("0x00 ant. 1\n");
	(void)printk("0x40 ant. 2\n");
	(void)printk("0x80 full diversity\n");
	(void)printk("0xc0 partial diversity\n");
	(void)printk("Tx antenna selection:\n");
	(void)printk("0x00 ant. 2\n"); /* yep, those ARE reversed! */
	(void)printk("0x20 ant. 1\n");
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

	(void)printk("current antenna value: 0x%02X (COMBINED bit mask)\n", priv->antenna);
	(void)printk("Rx antenna selection:\n");
	(void)printk("0x00 ant. 1\n");
	(void)printk("0x40 ant. 2\n");
	(void)printk("0x80 full diversity\n");
	(void)printk("0xc0 partial diversity\n");
	(void)printk("Tx antenna selection:\n");
	(void)printk("0x00 ant. 2\n"); /* yep, those ARE reversed! */
	(void)printk("0x20 ant. 1\n");

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_rx_antenna
*
*
* Arguments: 0 == antenna1; 1 == antenna2; 2 == full diversity; 3 == partial diversity
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

	if (*extra > 3)
		goto end;

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
* Arguments: 0 == antenna2; 1 == antenna1;
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

	if (*extra > 1)
		goto end;

	(void)printk("current antenna value: 0x%02X\n", priv->antenna);
	/* better keep the separate operations atomic */
	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}
	priv->antenna &= 0xdf;
	priv->antenna |= ((*extra & 0x01) << 5);
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

/* debug helper function to be able to debug various issues relatively easily */
static inline int acx100_ioctl_dbg_set_masks(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int *parms = (int*)extra;
	int result = -EINVAL;

	acxlog(L_IOCTL, "setting flags in settings mask: get_mask %08x set_mask %08x\n", (UINT32)parms[0], (UINT32)parms[1]);
	acxlog(L_IOCTL, "before: get_mask %08x set_mask %08x\n", priv->get_mask, priv->set_mask);
	priv->get_mask |= (UINT32)parms[0];
	priv->set_mask |= (UINT32)parms[1];
	acxlog(L_IOCTL, "after:  get_mask %08x set_mask %08x\n", priv->get_mask, priv->set_mask);
	result = -EINPROGRESS; /* immediately call commit handler */

	return result;
}

/* debug helper function to be able to debug I/O things relatively easily */
static inline int acx100_ioctl_dbg_get_io(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int *parms = (int*)extra;
	int result = -EINVAL;

	/* expected value order: DbgGetIO type address magic */

	if (parms[2] != 0x1234) {
		acxlog(L_IOCTL, "wrong magic: 0x%04x doesn't match 0x%04x! If you don't know what you're doing, then please stop NOW, this can be DANGEROUS!!\n", parms[2], 0x1234);
		goto end;
	}
	switch(parms[0]) {
		case 0x0: /* Internal RAM */
			acxlog(L_IOCTL, "sorry, access to internal RAM not implemented yet.\n");
			break;
		case 0xffff: /* MAC registers */
			acxlog(L_IOCTL, "value at register 0x%04x is 0x%08x\n", parms[1], acx100_read_reg32(priv, parms[1]));
			break;
		case 0x81: /* PHY RAM table */
			acxlog(L_IOCTL, "sorry, access to PHY RAM not implemented yet.\n");
			break;
		case 0x82: /* PHY registers */
			acxlog(L_IOCTL, "sorry, access to PHY registers not implemented yet.\n");
			break;
		default:
			acxlog(0xffff, "Invalid I/O type specified, aborting!\n");
			goto end;
	}
	result = 0;
end:
	return result;
}

/* debug helper function to be able to debug I/O things relatively easily */
static inline int acx100_ioctl_dbg_set_io(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int *parms = (int*)extra;
	int result = -EINVAL;

	/* expected value order: DbgSetIO type address value magic */

	if (parms[3] != 0x1234) {
		acxlog(0xffff, "wrong magic: 0x%04x doesn't match 0x%04x! If you don't know what you're doing, then please stop NOW, this can be DANGEROUS!!\n", parms[3], 0x1234);
		goto end;
	}
	switch(parms[0]) {
		case 0x0: /* Internal RAM */
			acxlog(L_IOCTL, "sorry, access to internal RAM not implemented yet.\n");
			break;
		case 0xffff: /* MAC registers */
			acxlog(L_IOCTL, "setting value at register 0x%04x to 0x%08x\n", parms[1], parms[2]);
			acx100_write_reg32(priv, parms[1], parms[2]);
			break;
		case 0x81: /* PHY RAM table */
			acxlog(L_IOCTL, "sorry, access to PHY RAM not implemented yet.\n");
			break;
		case 0x82: /* PHY registers */
			acxlog(L_IOCTL, "sorry, access to PHY registers not implemented yet.\n");
			break;
		default:
			acxlog(0xffff, "Invalid I/O type specified, aborting!\n");
			goto end;
	}
	result = 0;
end:
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_acx111_info
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
static inline int acx100_ioctl_acx111_info(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	TIWLAN_DC *pDc = &priv->dc;
	struct rxdescriptor *pRxDesc = (struct rxdescriptor *) pDc->pRxDescQPool;
	int i;
	int result = -EINVAL;
	struct ACX111MemoryConfig memconf;
	struct ACX111QueueConfig queueconf;
	char memmap[0x34];
	char rxconfig[0x8];
	char fcserror[0x8];
	char ratefallback[0x5];
	struct rxhostdescriptor *rx_host_desc;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *tx_host_desc;

/*
	if ((err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}*/

	if (CHIPTYPE_ACX111 != priv->chip_type) {
		acxlog(L_STD | L_IOCTL, "ACX111-specific function called with non-ACX111 chip, aborting!\n");
		return 0;
	}

	/* get Acx111 Memory Configuration */
	memset(&memconf, 0x00, sizeof(memconf));

	if (!acx100_interrogate(priv, &memconf, ACX1xx_IE_QUEUE_CONFIG)) {
		acxlog(L_BINSTD, "read memconf returns error\n");
	}

	/* get Acx111 Queue Configuration */
	memset(&queueconf, 0x00, sizeof(queueconf));

	if (!acx100_interrogate(priv, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS)) {
		acxlog(L_BINSTD, "read queuehead returns error\n");
	}

	/* get Acx111 Memory Map */
	memset(memmap, 0x00, sizeof(memmap));

	if (!acx100_interrogate(priv, &memmap, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_BINSTD, "read mem map returns error\n");
	}

	/* get Acx111 Rx Config */
	memset(rxconfig, 0x00, sizeof(rxconfig));

	if (!acx100_interrogate(priv, &rxconfig, ACX1xx_IE_RXCONFIG)) {
		acxlog(L_BINSTD, "read rxconfig returns error\n");
	}
	
	/* get Acx111 fcs error count */
	memset(fcserror, 0x00, sizeof(fcserror));

	if (!acx100_interrogate(priv, &fcserror, ACX1xx_IE_FCS_ERROR_COUNT)) {
		acxlog(L_BINSTD, "read fcserror returns error\n");
	}
	
	/* get Acx111 rate fallback */
	memset(ratefallback, 0x00, sizeof(ratefallback));

	if (!acx100_interrogate(priv, &ratefallback, ACX1xx_IE_RATE_FALLBACK)) {
		acxlog(L_BINSTD, "read ratefallback returns error\n");
	}

#if (WLAN_HOSTIF!=WLAN_USB)
	/* force occurrence of a beacon interrupt */
	acx100_write_reg16(priv, priv->io[IO_ACX_HINT_TRIG], 0x20);
#endif

	/* dump Acx111 Mem Configuration */
	acxlog(L_STD, "dump mem config:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", memconf.len, sizeof(memconf));
	acxlog(L_STD, "Number of stations: %1X\n", memconf.no_of_stations);
	acxlog(L_STD, "Memory block size: %1X\n", memconf.memory_block_size);
	acxlog(L_STD, "tx/rx memory block allocation: %1X\n", memconf.tx_rx_memory_block_allocation);
	acxlog(L_STD, "count rx: %X / tx: %X queues\n", memconf.count_rx_queues, memconf.count_tx_queues);
	acxlog(L_STD, "options %1X\n", memconf.options);
	acxlog(L_STD, "fragmentation %1X\n", memconf.fragmentation);
	
	acxlog(L_STD, "Rx Queue 1 Count Descriptors: %X\n", memconf.rx_queue1_count_descs);
	acxlog(L_STD, "Rx Queue 1 Host Memory Start: %X\n", memconf.rx_queue1_host_rx_start);

	acxlog(L_STD, "Tx Queue 1 Count Descriptors: %X\n", memconf.tx_queue1_count_descs);
	acxlog(L_STD, "Tx Queue 1 Attributes: %X\n", memconf.tx_queue1_attributes);


	/* dump Acx111 Queue Configuration */
	acxlog(L_STD, "dump queue head:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", queueconf.len, sizeof(queueconf));
	acxlog(L_STD, "tx_memory_block_address (from card): %X\n", queueconf.tx_memory_block_address);
	acxlog(L_STD, "rx_memory_block_address (from card): %X\n", queueconf.rx_memory_block_address);

	acxlog(L_STD, "rx1_queue address (from card): %X\n", queueconf.rx1_queue_address);
	acxlog(L_STD, "tx1_queue address (from card): %X\n", queueconf.tx1_queue_address);
	acxlog(L_STD, "tx1_queue attributes (from card): %X\n", queueconf.tx1_attributes);

	/* dump Acx111 Mem Map */
	acxlog(L_STD, "dump mem map:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", *((UINT16 *)&memmap[0x02]), sizeof(memmap));
	acxlog(L_STD, "Code start: %X\n", *((UINT32 *)&memmap[0x04]));
	acxlog(L_STD, "Code end: %X\n", *((UINT32 *)&memmap[0x08]));
	acxlog(L_STD, "WEP default key start: %X\n", *((UINT32 *)&memmap[0x0C]));
	acxlog(L_STD, "WEP default key end: %X\n", *((UINT32 *)&memmap[0x10]));
	acxlog(L_STD, "STA table start: %X\n", *((UINT32 *)&memmap[0x14]));
	acxlog(L_STD, "STA table end: %X\n", *((UINT32 *)&memmap[0x18]));
	acxlog(L_STD, "Packet template start: %X\n", *((UINT32 *)&memmap[0x1C]));
	acxlog(L_STD, "Packet template end: %X\n", *((UINT32 *)&memmap[0x20]));
	acxlog(L_STD, "Queue memory start: %X\n", *((UINT32 *)&memmap[0x24]));
	acxlog(L_STD, "Queue memory end: %X\n", *((UINT32 *)&memmap[0x28]));
	acxlog(L_STD, "Packet memory pool start: %X\n", *((UINT32 *)&memmap[0x2C]));
	acxlog(L_STD, "Packet memory pool end: %X\n", *((UINT32 *)&memmap[0x30]));

	acxlog(L_STD, "iobase: %p\n", priv->iobase);
	acxlog(L_STD, "iobase2: %p\n", priv->iobase2);

	/* dump Acx111 Rx Config */
	acxlog(L_STD, "dump rx config:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", *((UINT16 *)&rxconfig[0x02]), sizeof(rxconfig));
	acxlog(L_STD, "rx config: %X\n", *((UINT16 *)&rxconfig[0x04]));
	acxlog(L_STD, "rx filter config: %X\n", *((UINT16 *)&rxconfig[0x06]));

	/* dump Acx111 fcs error */
	acxlog(L_STD, "dump fcserror:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", *((UINT16 *)&fcserror[0x02]), sizeof(fcserror));
	acxlog(L_STD, "fcserrors: %X\n", *((UINT32 *)&fcserror[0x04]));

	/* dump Acx111 rate fallback */
	acxlog(L_STD, "dump rate fallback:\n");
	acxlog(L_STD, "data read: %d, struct size: %d\n", *((UINT16 *)&ratefallback[0x02]), sizeof(ratefallback));
	acxlog(L_STD, "ratefallback: %X\n", *((UINT8 *)&ratefallback[0x04]));

	/* dump acx111 internal rx descriptor ring buffer */

	/* loop over complete receive pool */
	for (i = 0; i < pDc->rx_pool_count; i++) {

		acxlog(L_STD, "\ndump internal rxdescriptor %d:\n", i);
		acxlog(L_STD, "mem pos %p\n", pRxDesc);
		acxlog(L_STD, "next 0x%X\n", pRxDesc->pNextDesc);
		acxlog(L_STD, "acx mem pointer (dynamic) 0x%X\n", pRxDesc->ACXMemPtr);
		acxlog(L_STD, "CTL (dynamic) 0x%X\n", pRxDesc->Ctl_8);
		acxlog(L_STD, "Rate (dynamic) 0x%X\n", pRxDesc->rate);
		acxlog(L_STD, "RxStatus (dynamic) 0x%X\n", pRxDesc->error);
		acxlog(L_STD, "Mod/Pre (dynamic) 0x%X\n", pRxDesc->SNR);

		pRxDesc++;
	}

	/* dump host rx descriptor ring buffer */

	rx_host_desc = (struct rxhostdescriptor *) pDc->pRxHostDescQPool;

	/* loop over complete receive pool */
	for (i = 0; i < pDc->rx_pool_count; i++) {

		acxlog(L_STD, "\ndump host rxdescriptor %d:\n", i);
		acxlog(L_STD, "mem pos 0x%X\n", (UINT32)rx_host_desc);
		acxlog(L_STD, "buffer mem pos 0x%X\n", (UINT32)rx_host_desc->data_phy);
		acxlog(L_STD, "buffer mem offset 0x%X\n", rx_host_desc->data_offset);
		acxlog(L_STD, "CTL 0x%X\n", le16_to_cpu(rx_host_desc->Ctl_16));
		acxlog(L_STD, "Length 0x%X\n", rx_host_desc->length);
		acxlog(L_STD, "next 0x%X\n", (UINT32)rx_host_desc->desc_phy_next);
		acxlog(L_STD, "Status 0x%X\n", rx_host_desc->Status);

		rx_host_desc++;

	}

	/* dump acx111 internal tx descriptor ring buffer */
	tx_desc = (struct txdescriptor *) pDc->pTxDescQPool;

	/* loop over complete transmit pool */
	for (i = 0; i < pDc->tx_pool_count; i++) {

		acxlog(L_STD, "\ndump internal txdescriptor %d:\n", i);
		acxlog(L_STD, "size 0x%X\n", sizeof(struct txdescriptor));
		acxlog(L_STD, "mem pos 0x%X\n", (UINT32)tx_desc);
		acxlog(L_STD, "next 0x%X\n", tx_desc->pNextDesc);
		acxlog(L_STD, "acx mem pointer (dynamic) 0x%X\n", tx_desc->AcxMemPtr);
		acxlog(L_STD, "host mem pointer (dynamic) 0x%X\n", tx_desc->HostMemPtr);
		acxlog(L_STD, "length (dynamic) 0x%X\n", tx_desc->total_length);
		acxlog(L_STD, "CTL (dynamic) 0x%X\n", tx_desc->Ctl_8);
		acxlog(L_STD, "CTL2 (dynamic) 0x%X\n", tx_desc->Ctl2_8);
		acxlog(L_STD, "Status (dynamic) 0x%X\n", tx_desc->error);
		acxlog(L_STD, "Rate (dynamic) 0x%X\n", tx_desc->u.r1.rate);

		tx_desc = GET_NEXT_TX_DESC_PTR(pDc, tx_desc);
	}


	/* dump host tx descriptor ring buffer */

	tx_host_desc = (struct txhostdescriptor *) pDc->pTxHostDescQPool;

	/* loop over complete host send pool */
	for (i = 0; i < pDc->tx_pool_count * 2; i++) {

		acxlog(L_STD, "\ndump host txdescriptor %d:\n", i);
		acxlog(L_STD, "mem pos 0x%X\n", (UINT32)tx_host_desc);
		acxlog(L_STD, "buffer mem pos 0x%X\n", (UINT32)tx_host_desc->data_phy);
		acxlog(L_STD, "buffer mem offset 0x%X\n", tx_host_desc->data_offset);
		acxlog(L_STD, "CTL 0x%X\n", le16_to_cpu(tx_host_desc->Ctl_16));
		acxlog(L_STD, "Length 0x%X\n", tx_host_desc->length);
		acxlog(L_STD, "next 0x%X\n", (UINT32)tx_host_desc->desc_phy_next);
		acxlog(L_STD, "Status 0x%X\n", tx_host_desc->Status);

		tx_host_desc++;

	}

	/* acx100_write_reg16(priv, 0xb4, 0x4); */

	/* acx100_unlock(priv, &flags); */
	result = 0;

	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_set_rates
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
#include "setrate.c"

static int
acx111_supported(int mbit, int modulation, void *opaque)
{
	if(mbit==33) return -ENOTSUPP;
	return 0;
}

static u16
acx111mask[] = {
	[DOT11_RATE_1 ] = RATE111_1 ,
	[DOT11_RATE_2 ] = RATE111_2 ,
	[DOT11_RATE_5 ] = RATE111_5 ,
	[DOT11_RATE_11] = RATE111_11,
	[DOT11_RATE_22] = RATE111_22,
	/* [DOT11_RATE_33] = */
	[DOT11_RATE_6 ] = RATE111_6 ,
	[DOT11_RATE_9 ] = RATE111_9 ,
	[DOT11_RATE_12] = RATE111_12,
	[DOT11_RATE_18] = RATE111_18,
	[DOT11_RATE_24] = RATE111_24,
	[DOT11_RATE_36] = RATE111_36,
	[DOT11_RATE_48] = RATE111_48,
	[DOT11_RATE_54] = RATE111_54,
};

static u32
acx111_gen_mask(int mbit, int modulation, void *opaque)
{
	/* lower 16 bits show selected 1, 2, CCK and CCKOFDM rates */
	/* upper 16 bits show selected PBCC and OFDM rates */
	u32 m = acx111mask[rate_mbit2enum(mbit)];
	if(modulation==DOT11_MOD_PBCC || modulation==DOT11_MOD_OFDM)
		return m<<16;
	return m;
}


static int
acx_ioctl_set_rates(struct net_device *dev, struct iw_request_info *info,
		 struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int result;
	u32 brate = 0, erate = 0;

	result = fill_ratemasks(extra, &brate, &erate, acx111_supported, acx111_gen_mask, 0);
	if(result)
		goto end;
	erate |= brate;

	/* simplistic for now: disallow PBCC and OFDM */
	/* can be improved, but needs complicated handling in tx code */
	if(erate & 0xffff0000) return -EINVAL;
	if(!erate) return -EINVAL;
        if (CHIPTYPE_ACX100 == priv->chip_type) {
		if(erate & ~RATE111_ACX100_COMPAT) return -EINVAL;
	}

	result = acx100_lock(priv, &flags);
	if(result)
		goto end;

	priv->txrate_cfg = erate; /* TODO: we need separate txrate_cfg for mgmt packets */
	priv->txrate_curr = priv->txrate_cfg;
	while( !(erate & 0x8000) ) erate<<=1;
	priv->txrate_auto = (erate!=0x8000); /* more than one bit is set? */
	if (priv->txrate_auto)
	{
		/* TODO: priv->txrate_curr = RATE111_1 + RATE111_2; */ /* 2Mbps, play it safe at the beginning */
		priv->txrate_fallback_count = 0;
		priv->txrate_stepup_count = 0;
	}

	acx100_unlock(priv, &flags);

	acxlog(L_IOCTL, "txrate_cfg %04x txrate_curr %04x txrate_auto %d\n",
		priv->txrate_cfg, priv->txrate_curr, priv->txrate_auto);

	priv->set_mask |= SET_RATE_FALLBACK;
	result = -EINPROGRESS;
end:
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_set_phy_amp_bias
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
static inline int acx100_ioctl_set_phy_amp_bias(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	UINT16 gpio_old;

	if (priv->chip_type != CHIPTYPE_ACX100) {
		/* WARNING!!! removing this check *might* damage
		 * hardware, since we're tweaking GPIOs here after all!!!
		 * You've been warned... */
		acxlog(L_IOCTL, "sorry, setting bias level for non-ACX100 not supported yet.\n");
		return 0;
	}
	
	if (*extra > 7)
	{
		acxlog(0xffff, "invalid bias parameter, range is 0 - 7\n");
		return -EINVAL;
	}

	gpio_old = acx100_read_reg16(priv, priv->io[IO_ACX_GPIO_OUT]);
	acxlog(L_DEBUG, "gpio_old: 0x%04x\n", gpio_old);
	(void)printk("current PHY power amplifier bias: %d\n", (gpio_old & 0x0700) >> 8);
	acx100_write_reg16(priv, priv->io[IO_ACX_GPIO_OUT], (gpio_old & 0xf8ff) | ((UINT16)*extra << 8));
	(void)printk("new PHY power amplifier bias: %d\n", (unsigned char)*extra);
	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_phy_chan_busy_percentage
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
static inline int acx100_ioctl_get_phy_chan_busy_percentage(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	struct {
		UINT16 rid;
		UINT16 len;
		UINT32 busytime;
		UINT32 totaltime;
	} usage;

	if (!acx100_interrogate(priv, &usage, ACX1xx_IE_MEDIUM_USAGE))
		return 0;
	(void)printk("Medium busy percentage since last invocation: %d%% (microseconds: %u of %u)\n", 100 * (usage.busytime / 100) / (usage.totaltime / 100), usage.busytime, usage.totaltime); /* prevent calculation overflow */
	return 0;
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

static inline int acx100_ioctl_set_scan_mode(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;
	char *scan_modes[] = { "active", "passive", "background" };

	if (*extra > 2)
		goto end;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	(void)printk("current scan mode: %d (%s)\n", priv->scan_mode, scan_modes[priv->scan_mode]);
	priv->scan_mode = (UINT16)*extra;
	(void)printk("new scan mode: %d (%s)\n", priv->scan_mode, scan_modes[priv->scan_mode]);
	acx100_unlock(priv, &flags);
	result = 0;

end:
	return result;
}

static inline int acx100_ioctl_set_scan_chan_delay(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if (0 != (err = acx100_lock(priv, &flags))) {
		result = err;
		goto end;
	}

	(void)printk("current scan channel delay: %dms\n", priv->scan_probe_delay);
	priv->scan_probe_delay = (UINT16)*extra;
	(void)printk("new scan channel delay: %dms\n", (UINT16)*extra);
	acx100_unlock(priv, &flags);
	result = 0;

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
	(iw_handler) acx_ioctl_set_rate,	/* SIOCSIWRATE */
	(iw_handler) acx_ioctl_get_rate,	/* SIOCGIWRATE */
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
	(iw_handler) acx100_ioctl_set_scan_mode,
	(iw_handler) acx100_ioctl_set_scan_chan_delay,
	(iw_handler) acx100_ioctl_set_short_preamble,
	(iw_handler) acx100_ioctl_get_short_preamble,
	(iw_handler) acx100_ioctl_set_antenna,
	(iw_handler) acx100_ioctl_get_antenna,
	(iw_handler) acx100_ioctl_set_rx_antenna,
	(iw_handler) acx100_ioctl_set_tx_antenna,
	(iw_handler) acx100_ioctl_set_phy_amp_bias,
	(iw_handler) acx100_ioctl_get_phy_chan_busy_percentage,
	(iw_handler) acx100_ioctl_set_ed_threshold,
	(iw_handler) acx100_ioctl_set_cca,
	(iw_handler) acx100_ioctl_set_led_power,
	(iw_handler) acx100_ioctl_wlansniff,
	(iw_handler) acx100_ioctl_unknown11,
	(iw_handler) acx100_ioctl_dbg_set_masks,
	(iw_handler) acx100_ioctl_dbg_get_io,
	(iw_handler) acx100_ioctl_dbg_set_io,
	(iw_handler) acx100_ioctl_acx111_info,
	(iw_handler) acx_ioctl_set_rates,
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
	.spy_offset = ((void *) (&((struct wlandevice *) NULL)->spy_data) - (void *) NULL),
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
	/* FIXME: do proper interfacing to activate that! */
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
		result = acx_ioctl_set_rate(dev, NULL, &(iwr->u.bitrate),
					       NULL);
		break;

	case SIOCGIWRATE:
		/* get default bit rate (bps) */
		result = acx_ioctl_get_rate(dev, NULL, &(iwr->u.bitrate),
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
		
	case ACX100_IOCTL_SET_SCAN_CHAN_DELAY:
		acx100_ioctl_set_scan_chan_delay(dev, NULL, NULL, iwr->u.name);
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

	case ACX100_IOCTL_ACX111_INFO:
		acx100_ioctl_acx111_info(dev, NULL, NULL, NULL);
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

