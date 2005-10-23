/***********************************************************************
** Copyright (C) 2003  ACX100 Open Source Project
**
** The contents of this file are subject to the Mozilla Public
** License Version 1.1 (the "License"); you may not use this file
** except in compliance with the License. You may obtain a copy of
** the License at http://www.mozilla.org/MPL/
**
** Software distributed under the License is distributed on an "AS
** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
** implied. See the License for the specific language governing
** rights and limitations under the License.
**
** Alternatively, the contents of this file may be used under the
** terms of the GNU Public License version 2 (the "GPL"), in which
** case the provisions of the GPL are applicable instead of the
** above.  If you wish to allow the use of your version of this file
** only under the terms of the GPL and not to allow others to use
** your version of this file under the MPL, indicate your decision
** by deleting the provisions above and replace them with the notice
** and other provisions required by the GPL.  If you do not delete
** the provisions above, a recipient may use your version of this
** file under either the MPL or the GPL.
** ---------------------------------------------------------------------
** Inquiries regarding the ACX100 Open Source Project can be
** made directly to:
**
** acx100-users@lists.sf.net
** http://acx100.sf.net
** ---------------------------------------------------------------------
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/uaccess.h> /* required for 2.4.x kernels; verify_write() */

#include <linux/if_arp.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif /* WE >= 13 */

#include "acx.h"


/*================================================================*/

/* if you plan to reorder something, make sure to reorder all other places
 * accordingly! */
/* SET/GET convention: SETs must have even position, GETs odd */
#define ACX100_IOCTL SIOCIWFIRSTPRIV
enum {
	ACX100_IOCTL_DEBUG = ACX100_IOCTL,
	ACX100_IOCTL_GET__________UNUSED1,
	ACX100_IOCTL_SET_PLED,
	ACX100_IOCTL_GET_PLED,
	ACX100_IOCTL_SET_RATES,
	ACX100_IOCTL_LIST_DOM,
	ACX100_IOCTL_SET_DOM,
	ACX100_IOCTL_GET_DOM,
	ACX100_IOCTL_SET_SCAN_PARAMS,
	ACX100_IOCTL_GET_SCAN_PARAMS,
	ACX100_IOCTL_SET_PREAMB,
	ACX100_IOCTL_GET_PREAMB,
	ACX100_IOCTL_SET_ANT,
	ACX100_IOCTL_GET_ANT,
	ACX100_IOCTL_RX_ANT,
	ACX100_IOCTL_TX_ANT,
	ACX100_IOCTL_SET_PHY_AMP_BIAS,
	ACX100_IOCTL_GET_PHY_CHAN_BUSY,
	ACX100_IOCTL_SET_ED,
	ACX100_IOCTL_GET__________UNUSED3,
	ACX100_IOCTL_SET_CCA,
	ACX100_IOCTL_GET__________UNUSED4,
	ACX100_IOCTL_MONITOR,
	ACX100_IOCTL_TEST,
	ACX100_IOCTL_DBG_SET_MASKS,
	ACX111_IOCTL_INFO,
	ACX100_IOCTL_DBG_SET_IO,
	ACX100_IOCTL_DBG_GET_IO
};

/* channel frequencies
 * TODO: Currently, every other 802.11 driver keeps its own copy of this. In
 * the long run this should be integrated into ieee802_11.h or wireless.h or
 * whatever IEEE802.11x framework evolves */
static const u16 acx_channel_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484,
};

static const struct iw_priv_args acx_ioctl_private_args[] = {
#if ACX_DEBUG
{ cmd : ACX100_IOCTL_DEBUG,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	get_args : 0,
	name : "SetDebug" },
#endif
{ cmd : ACX100_IOCTL_SET_PLED,
	set_args : IW_PRIV_TYPE_BYTE | 2,
	get_args : 0,
	name : "SetLEDPower" },
{ cmd : ACX100_IOCTL_GET_PLED,
	set_args : 0,
	get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 2,
	name : "GetLEDPower" },
{ cmd : ACX100_IOCTL_SET_RATES,
	set_args : IW_PRIV_TYPE_CHAR | 256,
	get_args : 0,
	name : "SetRates" },
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
{ cmd : ACX100_IOCTL_SET_SCAN_PARAMS,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4,
	get_args : 0,
	name : "SetScanParams" },
{ cmd : ACX100_IOCTL_GET_SCAN_PARAMS,
	set_args : 0,
	get_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4,
	name : "GetScanParams" },
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
{ cmd : ACX100_IOCTL_GET_PHY_CHAN_BUSY,
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
{ cmd : ACX111_IOCTL_INFO,
	set_args : 0,
	get_args : 0,
	name : "GetAcx111Info" },
{ cmd : ACX100_IOCTL_DBG_SET_IO,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4,
	get_args : 0,
	name : "DbgSetIO" },
{ cmd : ACX100_IOCTL_DBG_GET_IO,
	set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
	get_args : 0,
	name : "DbgGetIO" },
};


/*------------------------------------------------------------------------------
 * acx_ioctl_commit
 *----------------------------------------------------------------------------*/
static int
acx_ioctl_commit(struct net_device *dev,
				      struct iw_request_info *info,
				      void *zwrq, char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	FN_ENTER;

	acx_sem_lock(priv);
	if (ACX_STATE_IFACE_UP & priv->dev_state_mask)
		acx_s_update_card_settings(priv, 0, 0);
	acx_sem_unlock(priv);

	FN_EXIT0;
	return OK;
}


/***********************************************************************
*/
static int
acx_ioctl_get_name(
	struct net_device *dev,
	struct iw_request_info *info,
	char *cwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	static const char * const names[] = { "IEEE 802.11b+/g+", "IEEE 802.11b+" };

	strcpy(cwrq, names[IS_ACX111(priv) ? 0 : 1]);

	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_freq
*----------------------------------------------------------------*/
static int
acx_ioctl_set_freq(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_freq *fwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int channel = -1;
	unsigned int mult = 1;
	int result;

	FN_ENTER;

	if (fwrq->e == 0 && fwrq->m <= 1000) {
		/* Setting by channel number */
		channel = fwrq->m;
	} else {
		/* If setting by frequency, convert to a channel */
		int i;

		for (i = 0; i < (6 - fwrq->e); i++)
			mult *= 10;

		for (i = 1; i <= 14; i++)
			if (fwrq->m == acx_channel_freq[i - 1] * mult)
				channel = i;
	}

	if (channel > 14) {
		result = -EINVAL;
		goto end;
	}

	acx_sem_lock(priv);

	priv->channel = channel;
	/* hmm, the following code part is strange, but this is how
	 * it was being done before... */
	acxlog(L_IOCTL, "Changing to channel %d\n", channel);
	SET_BIT(priv->set_mask, GETSET_CHANNEL);

	result = -EINPROGRESS; /* need to call commit handler */

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static inline int
acx_ioctl_get_freq(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_freq *fwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	fwrq->e = 0;
	fwrq->m = priv->channel;
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_mode
*----------------------------------------------------------------*/
static int
acx_ioctl_set_mode(
	struct net_device *dev,
	struct iw_request_info *info,
	u32 *uwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	acx_sem_lock(priv);

	switch (*uwrq) {
	case IW_MODE_AUTO:
		priv->mode = ACX_MODE_OFF;
		break;
#if WIRELESS_EXT > 14
	case IW_MODE_MONITOR:
		priv->mode = ACX_MODE_MONITOR;
		break;
#endif /* WIRELESS_EXT > 14 */
	case IW_MODE_ADHOC:
		priv->mode = ACX_MODE_0_ADHOC;
		break;
	case IW_MODE_INFRA:
		priv->mode = ACX_MODE_2_STA;
		break;
	case IW_MODE_MASTER:
		printk("acx: master mode (HostAP) is very, very "
			"experimental! It might work partially, but "
			"better get prepared for nasty surprises "
			"at any time\n");
		priv->mode = ACX_MODE_3_AP;
		break;
	case IW_MODE_REPEAT:
	case IW_MODE_SECOND:
	default:
		result = -EOPNOTSUPP;
		goto end_unlock;
	}

	acxlog(L_ASSOC, "new priv->mode=%d\n", priv->mode);
	SET_BIT(priv->set_mask, GETSET_MODE);
	result = -EINPROGRESS;

end_unlock:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_get_mode(
	struct net_device *dev,
	struct iw_request_info *info,
	u32 *uwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result = 0;

	switch (priv->mode) {
	case ACX_MODE_OFF:
		*uwrq = IW_MODE_AUTO; break;
#if WIRELESS_EXT > 14
	case ACX_MODE_MONITOR:
		*uwrq = IW_MODE_MONITOR; break;
#endif /* WIRELESS_EXT > 14 */
	case ACX_MODE_0_ADHOC:
		*uwrq = IW_MODE_ADHOC; break;
	case ACX_MODE_2_STA:
		*uwrq = IW_MODE_INFRA; break;
	case ACX_MODE_3_AP:
		*uwrq = IW_MODE_MASTER; break;
	default:
		result = -EOPNOTSUPP;
	}
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_set_sens(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	acx_sem_lock(priv);

	priv->sensitivity = (1 == vwrq->disabled) ? 0 : vwrq->value;
	SET_BIT(priv->set_mask, GETSET_SENSITIVITY);

	acx_sem_unlock(priv);

	return -EINPROGRESS;
}


/***********************************************************************
*/
static int
acx_ioctl_get_sens(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	/* acx_sem_lock(priv); */

	vwrq->value = priv->sensitivity;
	vwrq->disabled = (vwrq->value == 0);
	vwrq->fixed = 1;

	/* acx_sem_unlock(priv); */

	return OK;
}


/*------------------------------------------------------------------------------
 * acx_ioctl_set_ap
 *
 * Sets the MAC address of the AP to associate with
 *----------------------------------------------------------------------------*/
static int
acx_ioctl_set_ap(
	struct net_device *dev,
	struct iw_request_info *info,
	struct sockaddr *awrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result = 0;
	const u8 *ap;

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
	acxlog_mac(L_IOCTL, "Set AP=", ap, "\n");

	MAC_COPY(priv->ap, ap);

	/* We want to start rescan in managed or ad-hoc mode,
	** otherwise just set priv->ap.
	** "iwconfig <if> ap <mac> mode managed": we must be able
	** to set ap _first_ and _then_ set mode */
	switch (priv->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_2_STA:
		/* FIXME: if there is a convention on what zero AP means,
		** please add a comment about that. I don't know of any --vda */
		if (mac_is_zero(ap)) {
			/* "off" == 00:00:00:00:00:00 */
			MAC_BCAST(priv->ap);
			acxlog(L_IOCTL, "Not reassociating\n");
		} else {
			acxlog(L_IOCTL, "Forcing reassociation\n");
			SET_BIT(priv->set_mask, GETSET_RESCAN);
		}
		break;
	}
	result = -EINPROGRESS;
end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_get_ap(
	struct net_device *dev,
	struct iw_request_info *info,
	struct sockaddr *awrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	if (ACX_STATUS_4_ASSOCIATED == priv->status) {
		/* as seen in Aironet driver, airo.c */
		MAC_COPY(awrq->sa_data, priv->bssid);
	} else {
		MAC_ZERO(awrq->sa_data);
	}
	awrq->sa_family = ARPHRD_ETHER;
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_get_aplist
*
* Comment: deprecated in favour of iwscan.
* We simply return the list of currently available stations in range,
* don't do a new scan.
*----------------------------------------------------------------*/
static int
acx_ioctl_get_aplist(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	struct sockaddr *address = (struct sockaddr *) extra;
	struct iw_quality qual[IW_MAX_AP];
	int i, cur;
	int result = OK;

	FN_ENTER;

	/* we have AP list only in STA mode */
	if (ACX_MODE_2_STA != priv->mode) {
		result = -EOPNOTSUPP;
		goto end;
	}

	cur = 0;
	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		struct client *bss = &priv->sta_list[i];
		if (!bss->used) continue;
		MAC_COPY(address[cur].sa_data, bss->bssid);
		address[cur].sa_family = ARPHRD_ETHER;
		qual[cur].level = bss->sir;
		qual[cur].noise = bss->snr;
#ifndef OLD_QUALITY
		qual[cur].qual = acx_signal_determine_quality(qual[cur].level,
						    qual[cur].noise);
#else
		qual[cur].qual = (qual[cur].noise <= 100) ?
			       100 - qual[cur].noise : 0;
#endif
		/* no scan: level/noise/qual not updated: */
		qual[cur].updated = 0;
		cur++;
	}
	if (cur) {
		dwrq->flags = 1;
		memcpy(extra + sizeof(struct sockaddr)*cur, &qual,
				sizeof(struct iw_quality)*cur);
	}
	dwrq->length = cur;
end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_set_scan(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	acx_sem_lock(priv);

	/* don't start scan if device is not up yet */
	if (!(priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		result = -EAGAIN;
		goto end_unlock;
	}

	/* This is NOT a rescan for new AP!
	** Do not use SET_BIT(GETSET_RESCAN); */
	acx_s_cmd_start_scan(priv);
	result = OK;

end_unlock:
	acx_sem_unlock(priv);
/* end: */
	FN_EXIT1(result);
	return result;
}


#if WIRELESS_EXT > 13
/***********************************************************************
** acx_s_scan_add_station
*/
/* helper. not sure wheter it's really a _s_leeping fn */
static char*
acx_s_scan_add_station(
	wlandevice_t *priv,
	char *ptr,
	char *end_buf,
	struct client *bss)
{
	struct iw_event iwe;
	char *ptr_rate;

	FN_ENTER;

	/* MAC address has to be added first */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	MAC_COPY(iwe.u.ap_addr.sa_data, bss->bssid);
	acxlog_mac(L_IOCTL, "scan, station address: ", bss->bssid, "\n");
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_ADDR_LEN);

	/* Add ESSID */
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.length = bss->essid_len;
	iwe.u.data.flags = 1;
	acxlog(L_IOCTL, "scan, essid: %s\n", bss->essid);
	ptr = iwe_stream_add_point(ptr, end_buf, &iwe, bss->essid);

	/* Add mode */
	iwe.cmd = SIOCGIWMODE;
	if (bss->cap_info & (WF_MGMT_CAP_ESS | WF_MGMT_CAP_IBSS)) {
		if (bss->cap_info & WF_MGMT_CAP_ESS)
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_ADHOC;
		acxlog(L_IOCTL, "scan, mode: %d\n", iwe.u.mode);
		ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_UINT_LEN);
	}

	/* Add frequency */
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = acx_channel_freq[bss->channel - 1] * 100000;
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
	iwe.u.qual.qual = acx_signal_determine_quality(iwe.u.qual.level,
							iwe.u.qual.noise);
#else
	iwe.u.qual.qual = (iwe.u.qual.noise <= 100) ?
				100 - iwe.u.qual.noise : 0;
#endif
	iwe.u.qual.updated = 7;
	acxlog(L_IOCTL, "scan, link quality: %d/%d/%d\n",
			iwe.u.qual.level, iwe.u.qual.noise, iwe.u.qual.qual);
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_QUAL_LEN);

	/* Add encryption */
	iwe.cmd = SIOCGIWENCODE;
	if (bss->cap_info & WF_MGMT_CAP_PRIVACY)
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	acxlog(L_IOCTL, "scan, encryption flags: %X\n", iwe.u.data.flags);
	ptr = iwe_stream_add_point(ptr, end_buf, &iwe, bss->essid);

	/* add rates */
	iwe.cmd = SIOCGIWRATE;
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
	ptr_rate = ptr + IW_EV_LCP_LEN;

	{
	u16 rate = bss->rate_cap;
	const u8* p = acx_bitpos2ratebyte;
	while (rate) {
		if (rate & 1) {
			iwe.u.bitrate.value = *p * 500000; /* units of 500kb/s */
			acxlog(L_IOCTL, "scan, rate: %d\n", iwe.u.bitrate.value);
			ptr = iwe_stream_add_value(ptr, ptr_rate, end_buf,
						&iwe, IW_EV_PARAM_LEN);
		}
		rate >>= 1;
		p++;
	}}

	if ((ptr_rate - ptr) > (ptrdiff_t)IW_EV_LCP_LEN)
		ptr = ptr_rate;

	/* drop remaining station data items for now */

	FN_EXIT0;
	return ptr;
}


/***********************************************************************
 * acx_ioctl_get_scan
 */
static int
acx_ioctl_get_scan(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	char *ptr = extra;
	int i;
	int result = OK;

	FN_ENTER;

	acx_sem_lock(priv);

	/* no scan available if device is not up yet */
	if (!(priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		acxlog(L_IOCTL, "iface not up yet\n");
		result = -EAGAIN;
		goto end_unlock;
	}

#ifdef ENODATA_TO_BE_USED_AFTER_SCAN_ERROR_ONLY
	if (priv->bss_table_count == 0)	{
		/* no stations found */
		result = -ENODATA;
		goto end_unlock;
	}
#endif

	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		struct client *bss = &priv->sta_list[i];
		if (!bss->used) continue;
		ptr = acx_s_scan_add_station(priv, ptr,
			extra + IW_SCAN_MAX_DATA, bss);
	}
	dwrq->length = ptr - extra;
	dwrq->flags = 0;

end_unlock:
	acx_sem_unlock(priv);
/* end: */
	FN_EXIT1(result);
	return result;
}
#endif /* WIRELESS_EXT > 13 */


/*----------------------------------------------------------------
* acx_ioctl_set_essid
*----------------------------------------------------------------*/
static int
acx_ioctl_set_essid(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int len = dwrq->length;
	int result;

	FN_ENTER;

	acxlog(L_IOCTL, "Set ESSID '%*s', length %d, flags 0x%04X\n",
					len, extra, len, dwrq->flags);

	if (len < 0) {
		result = -EINVAL;
		goto end;
	}

	acx_sem_lock(priv);

	/* ESSID disabled? */
	if (0 == dwrq->flags) {
		priv->essid_active = 0;

	} else {
		if (dwrq->length > IW_ESSID_MAX_SIZE+1)	{
			result = -E2BIG;
			goto end_unlock;
		}

		if (len > sizeof(priv->essid))
			len = sizeof(priv->essid);
		memcpy(priv->essid, extra, len-1);
		priv->essid[len-1] = '\0';
		/* Paranoia: just in case there is a '\0'... */
		priv->essid_len = strlen(priv->essid);
		priv->essid_active = 1;
	}

	SET_BIT(priv->set_mask, GETSET_RESCAN);

	result = -EINPROGRESS;

end_unlock:
	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_get_essid(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	dwrq->flags = priv->essid_active;
	if (priv->essid_active)	{
		memcpy(extra, priv->essid, priv->essid_len);
		extra[priv->essid_len] = '\0';
		dwrq->length = priv->essid_len + 1;
		dwrq->flags = 1;
	}
	return OK;
}


/*----------------------------------------------------------------
* acx_l_update_client_rates
*----------------------------------------------------------------*/
static void
acx_l_update_client_rates(wlandevice_t *priv, u16 rate)
{
	int i;
	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		client_t *clt = &priv->sta_list[i];
		if (!clt->used)	continue;
		clt->rate_cfg = (clt->rate_cap & rate);
		if (!clt->rate_cfg) {
			/* no compatible rates left: kick client */
			acxlog_mac(L_ASSOC, "client ",clt->address," kicked: "
				"rates are not compatible anymore\n");
			acx_l_sta_list_del(priv, clt);
			continue;
		}
		clt->rate_cur &= clt->rate_cfg;
		if (!clt->rate_cur) {
			/* current rate become invalid, choose a valid one */
			clt->rate_cur = 1 << lowest_bit(clt->rate_cfg);
		}
		if (IS_ACX100(priv))
			clt->rate_100 = acx_bitpos2rate100[highest_bit(clt->rate_cur)];
		clt->fallback_count = clt->stepup_count = 0;
		clt->ignore_count = 16;
	}
	switch (priv->mode) {
	case ACX_MODE_2_STA:
		if (priv->ap_client && !priv->ap_client->used) {
			/* Owwww... we kicked our AP!! :) */
			SET_BIT(priv->set_mask, GETSET_RESCAN);
		}
	}
}


/***********************************************************************
*/
/* maps bits from acx111 rate to rate in Mbits */
static const unsigned int
acx111_rate_tbl[] = {
     1000000, /* 0 */
     2000000, /* 1 */
     5500000, /* 2 */
     6000000, /* 3 */
     9000000, /* 4 */
    11000000, /* 5 */
    12000000, /* 6 */
    18000000, /* 7 */
    22000000, /* 8 */
    24000000, /* 9 */
    36000000, /* 10 */
    48000000, /* 11 */
    54000000, /* 12 */
      500000, /* 13, should not happen */
      500000, /* 14, should not happen */
      500000, /* 15, should not happen */
};

/***********************************************************************
 * acx_ioctl_set_rate
 */
static int
acx_ioctl_set_rate(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	u16 txrate_cfg = 1;
	unsigned long flags;
	int autorate;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "rate %d fixed 0x%X disabled 0x%X flags 0x%X\n",
		vwrq->value, vwrq->fixed, vwrq->disabled, vwrq->flags);

	if ((0 == vwrq->fixed) || (1 == vwrq->fixed)) {
		int i = VEC_SIZE(acx111_rate_tbl)-1;
		if (vwrq->value == -1)
			/* "iwconfig rate auto" --> choose highest */
			vwrq->value = IS_ACX100(priv) ?	22000000 : 54000000;
		while (i >= 0) {
			if (vwrq->value == acx111_rate_tbl[i]) {
				txrate_cfg <<= i;
				i = 0;
				break;
			}
			i--;
		}
		if (i == -1) { /* no matching rate */
			result = -EINVAL;
			goto end;
		}
	} else {	/* rate N, N<1000 (driver specific): we don't use this */
		result = -EOPNOTSUPP;
		goto end;
	}
	/* now: only one bit is set in txrate_cfg, corresponding to
	** indicated rate */

	autorate = (vwrq->fixed == 0) && (RATE111_1 != txrate_cfg);
	if (autorate) {
		/* convert 00100000 -> 00111111 */
		txrate_cfg = (txrate_cfg<<1)-1;
	}

	if (IS_ACX100(priv)) {
		txrate_cfg &= RATE111_ACX100_COMPAT;
		if (!txrate_cfg) {
			result = -ENOTSUPP; /* rate is not supported by acx100 */
			goto end;
		}
	}

	acx_sem_lock(priv);
	acx_lock(priv, flags);

	priv->rate_auto = autorate;
	priv->rate_oper = txrate_cfg;
	priv->rate_basic = txrate_cfg;
	/* only do that in auto mode, non-auto will be able to use
	 * one specific Tx rate only anyway */
	if (autorate) {
		/* only use 802.11b base rates, for standard 802.11b H/W
		 * compatibility */
		priv->rate_basic &= RATE111_80211B_COMPAT;
	}
	priv->rate_bcast = 1 << lowest_bit(txrate_cfg);
	if (IS_ACX100(priv))
		priv->rate_bcast100 = acx_rate111to100(priv->rate_bcast);
	acx_l_update_ratevector(priv);
	acx_l_update_client_rates(priv, txrate_cfg);

	/* Do/don't do tx rate fallback; beacon contents and rate */
	SET_BIT(priv->set_mask, SET_RATE_FALLBACK|SET_TEMPLATES);
	result = -EINPROGRESS;

	acx_unlock(priv, flags);
	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_rate
*----------------------------------------------------------------*/
static int
acx_ioctl_get_rate(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	u16 rate;

	acx_lock(priv, flags);
	rate = priv->rate_oper;
	if (priv->ap_client)
		rate = priv->ap_client->rate_cur;
	vwrq->value = acx111_rate_tbl[highest_bit(rate)];
	vwrq->fixed = !priv->rate_auto;
	vwrq->disabled = 0;
	acx_unlock(priv, flags);

	return OK;
}

static int
acx_ioctl_set_rts(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int val = vwrq->value;

	if (vwrq->disabled)
		val = 2312;
	if ((val < 0) || (val > 2312))
		return -EINVAL;

	priv->rts_threshold = val;
	return OK;
}

static inline int
acx_ioctl_get_rts(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	vwrq->value = priv->rts_threshold;
	vwrq->disabled = (vwrq->value >= 2312);
	vwrq->fixed = 1;
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_encode
*----------------------------------------------------------------*/
static int
acx_ioctl_set_encode(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int index;
	int result;

	FN_ENTER;

	acxlog(L_IOCTL, "Set Encoding flags=0x%04X, size=%d, key: %s\n",
			dwrq->flags, dwrq->length, extra ? "set" : "No key");

	acx_sem_lock(priv);

	index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	if (dwrq->length > 0) {
		/* if index is 0 or invalid, use default key */
		if ((index < 0) || (index > 3))
			index = (int)priv->wep_current_index;

		if (0 == (dwrq->flags & IW_ENCODE_NOKEY)) {
			if (dwrq->length > 29)
				dwrq->length = 29; /* restrict it */

			if (dwrq->length > 13) {
				/* 29*8 == 232, WEP256 */
				priv->wep_keys[index].size = 29;
			} else if (dwrq->length > 5) {
				/* 13*8 == 104bit, WEP128 */
				priv->wep_keys[index].size = 13;
			} else if (dwrq->length > 0) {
				/* 5*8 == 40bit, WEP64 */
				priv->wep_keys[index].size = 5;
			} else {
				/* disable key */
				priv->wep_keys[index].size = 0;
			}

			memset(priv->wep_keys[index].key, 0,
				sizeof(priv->wep_keys[index].key));
			memcpy(priv->wep_keys[index].key, extra, dwrq->length);
		}
	} else {
		/* set transmit key */
		if ((index >= 0) && (index <= 3))
			priv->wep_current_index = index;
		else if (0 == (dwrq->flags & IW_ENCODE_MODE)) {
			/* complain if we were not just setting
			 * the key mode */
			result = -EINVAL;
			goto end_unlock;
		}
	}

	priv->wep_enabled = !(dwrq->flags & IW_ENCODE_DISABLED);

	if (dwrq->flags & IW_ENCODE_OPEN) {
		priv->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
		priv->wep_restricted = 0;

	} else if (dwrq->flags & IW_ENCODE_RESTRICTED) {
		priv->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
		priv->wep_restricted = 1;
	}

	/* set flag to make sure the card WEP settings get updated */
	SET_BIT(priv->set_mask, GETSET_WEP);

	acxlog(L_IOCTL, "len=%d, key at 0x%p, flags=0x%X\n",
		dwrq->length, extra, dwrq->flags);

	for (index = 0; index <= 3; index++) {
		if (priv->wep_keys[index].size) {
			acxlog(L_IOCTL,	"index=%d, size=%d, key at 0x%p\n",
				priv->wep_keys[index].index,
				(int) priv->wep_keys[index].size,
				priv->wep_keys[index].key);
		}
	}
	result = -EINPROGRESS;

end_unlock:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_encode
*----------------------------------------------------------------*/
static int
acx_ioctl_get_encode(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	FN_ENTER;

	if (priv->wep_enabled == 0) {
		dwrq->flags = IW_ENCODE_DISABLED;
	} else {
		if ((index < 0) || (index > 3))
			index = (int)priv->wep_current_index;

		dwrq->flags = (priv->wep_restricted == 1) ?
				IW_ENCODE_RESTRICTED : IW_ENCODE_OPEN;
		dwrq->length = priv->wep_keys[index].size;

		memcpy(extra, priv->wep_keys[index].key,
			      priv->wep_keys[index].size);
	}

	/* set the current index */
	SET_BIT(dwrq->flags, index + 1);

	acxlog(L_IOCTL, "len=%d, key=%p, flags=0x%X\n",
	       dwrq->length, dwrq->pointer,
	       dwrq->flags);

	FN_EXIT1(OK);
	return OK;
}


/***********************************************************************
*/
static int
acx_ioctl_set_power(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result = -EINPROGRESS;

	FN_ENTER;

	acxlog(L_IOCTL, "Set 802.11 Power Save flags=0x%04X\n", vwrq->flags);

	acx_sem_lock(priv);

	if (vwrq->disabled) {
		CLEAR_BIT(priv->ps_wakeup_cfg, PS_CFG_ENABLE);
		SET_BIT(priv->set_mask, GETSET_POWER_80211);
		goto end;
	}
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		u16 ps_timeout = (vwrq->value * 1024) / 1000;

		if (ps_timeout > 255)
			ps_timeout = 255;
		acxlog(L_IOCTL, "setting PS timeout value to %d time units "
				"due to %dus\n", ps_timeout, vwrq->value);
		priv->ps_hangover_period = ps_timeout;
	} else if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_PERIOD) {
		u16 ps_periods = vwrq->value / 1000000;

		if (ps_periods > 255)
			ps_periods = 255;
		acxlog(L_IOCTL, "setting PS period value to %d periods "
				"due to %dus\n", ps_periods, vwrq->value);
		priv->ps_listen_interval = ps_periods;
		CLEAR_BIT(priv->ps_wakeup_cfg, PS_CFG_WAKEUP_MODE_MASK);
		SET_BIT(priv->ps_wakeup_cfg, PS_CFG_WAKEUP_EACH_ITVL);
	}

	switch (vwrq->flags & IW_POWER_MODE) {
		/* FIXME: are we doing the right thing here? */
		case IW_POWER_UNICAST_R:
			CLEAR_BIT(priv->ps_options, PS_OPT_STILL_RCV_BCASTS);
			break;
		case IW_POWER_MULTICAST_R:
			SET_BIT(priv->ps_options, PS_OPT_STILL_RCV_BCASTS);
			break;
		case IW_POWER_ALL_R:
			SET_BIT(priv->ps_options, PS_OPT_STILL_RCV_BCASTS);
			break;
		case IW_POWER_ON:
			break;
		default:
			acxlog(L_IOCTL, "unknown PS mode\n");
			result = -EINVAL;
			goto end;
	}

	SET_BIT(priv->ps_wakeup_cfg, PS_CFG_ENABLE);
	SET_BIT(priv->set_mask, GETSET_POWER_80211);
end:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
static int
acx_ioctl_get_power(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	FN_ENTER;

	acxlog(L_IOCTL, "Get 802.11 Power Save flags = 0x%04X\n", vwrq->flags);
	vwrq->disabled = ((priv->ps_wakeup_cfg & PS_CFG_ENABLE) == 0);
	if (vwrq->disabled)
		return OK;
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		vwrq->value = priv->ps_hangover_period * 1000 / 1024;
		vwrq->flags = IW_POWER_TIMEOUT;
	} else {
		vwrq->value = priv->ps_listen_interval * 1000000;
		vwrq->flags = IW_POWER_PERIOD|IW_POWER_RELATIVE;
	}
	if (priv->ps_options & PS_OPT_STILL_RCV_BCASTS)
		SET_BIT(vwrq->flags, IW_POWER_ALL_R);
	else
		SET_BIT(vwrq->flags, IW_POWER_UNICAST_R);

	FN_EXIT1(OK);
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_get_txpow
*----------------------------------------------------------------*/
static inline int
acx_ioctl_get_txpow(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	FN_ENTER;

	vwrq->flags = IW_TXPOW_DBM;
	vwrq->disabled = 0;
	vwrq->fixed = 1;
	vwrq->value = priv->tx_level_dbm;

	acxlog(L_IOCTL, "get txpower:%d dBm\n", priv->tx_level_dbm);

	FN_EXIT1(OK);
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_txpow
*----------------------------------------------------------------*/
static int
acx_ioctl_set_txpow(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	acxlog(L_IOCTL, "set txpower:%d, disabled:%d, flags:0x%04X\n",
			vwrq->value, vwrq->disabled, vwrq->flags);

	acx_sem_lock(priv);

	if (vwrq->disabled != priv->tx_disabled) {
		SET_BIT(priv->set_mask, GETSET_TX);
	}

	priv->tx_disabled = vwrq->disabled;
	if (vwrq->value == -1) {
		if (vwrq->disabled) {
			priv->tx_level_dbm = 0;
			acxlog(L_IOCTL, "disable radio tx\n");
		} else {
			/* priv->tx_level_auto = 1; */
			acxlog(L_IOCTL, "set tx power auto (NIY)\n");
		}
	} else {
		priv->tx_level_dbm = vwrq->value <= 20 ? vwrq->value : 20;
		/* priv->tx_level_auto = 0; */
		acxlog(L_IOCTL, "set txpower=%d dBm\n", priv->tx_level_dbm);
	}
	SET_BIT(priv->set_mask, GETSET_TXPOWER);

	result = -EINPROGRESS;

	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_range
*----------------------------------------------------------------*/
static int
acx_ioctl_get_range(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	wlandevice_t *priv = netdev_priv(dev);
	int i,n;

	FN_ENTER;

	if (!dwrq->pointer)
		goto end;

	dwrq->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));
	n = 0;
	for (i = 1; i <= 14; i++) {
		if (priv->reg_dom_chanmask & (1 << (i - 1))) {
			range->freq[n].i = i;
			range->freq[n].m = acx_channel_freq[i - 1] * 100000;
			range->freq[n].e = 1; /* units are MHz */
			n++;
		}
	}
	range->num_channels = n;
	range->num_frequency = n;

	range->min_rts = 0;
	range->max_rts = 2312;
	/* range->min_frag = 256;
	 * range->max_frag = 2312;
	 */

	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->encoding_size[2] = 29;
	range->num_encoding_sizes = 3;
	range->max_encoding_tokens = 4;

	range->min_pmp = 0;
	range->max_pmp = 5000000;
	range->min_pmt = 0;
	range->max_pmt = 65535 * 1000;
	range->pmp_flags = IW_POWER_PERIOD;
	range->pmt_flags = IW_POWER_TIMEOUT;
	range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_ALL_R;

	for (i = 0; i <= IW_MAX_TXPOWER - 1; i++)
		range->txpower[i] = 20 * i / (IW_MAX_TXPOWER - 1);
	range->num_txpower = IW_MAX_TXPOWER;
	range->txpower_capa = IW_TXPOW_DBM;

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 0x9;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 1;
	range->max_retry = 255;

	range->r_time_flags = IW_RETRY_LIFETIME;
	range->min_r_time = 0;
	/* FIXME: lifetime ranges and orders of magnitude are strange?? */
	range->max_r_time = 65535;

	if (IS_USB(priv))
		range->sensitivity = 0;
	else if (IS_ACX111(priv))
		range->sensitivity = 3;
	else
		range->sensitivity = 255;

	for (i=0; i < priv->rate_supported_len; i++) {
		range->bitrate[i] = (priv->rate_supported[i] & ~0x80) * 500000;
		/* never happens, but keep it, to be safe: */
		if (range->bitrate[i] == 0)
			break;
	}
	range->num_bitrates = i;

	range->max_qual.qual = 100;
	range->max_qual.level = 100;
	range->max_qual.noise = 100;
	/* TODO: better values */
	range->avg_qual.qual = 90;
	range->avg_qual.level = 80;
	range->avg_qual.noise = 2;

end:
	FN_EXIT1(OK);
	return OK;
}


/*================================================================*/
/* Private functions						  */
/*================================================================*/

#if WIRELESS_EXT < 13
/*----------------------------------------------------------------
* acx_ioctl_get_iw_priv
*
* Comment: I added the monitor mode and changed the stuff below
* to look more like the orinoco driver
*----------------------------------------------------------------*/
static int
acx_ioctl_get_iw_priv(struct iwreq *iwr)
{
	int result = -EINVAL;

	if (!iwr->u.data.pointer)
		return -EINVAL;
	result = verify_area(VERIFY_WRITE, iwr->u.data.pointer,
			sizeof(acx_ioctl_private_args));
	if (result)
		return result;

	iwr->u.data.length = VEC_SIZE(acx_ioctl_private_args);
	if (copy_to_user(iwr->u.data.pointer,
	    acx_ioctl_private_args, sizeof(acx_ioctl_private_args)) != 0)
		result = -EFAULT;

	return result;
}
#endif


/*----------------------------------------------------------------
* acx_ioctl_get_nick
*----------------------------------------------------------------*/
static inline int
acx_ioctl_get_nick(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	strcpy(extra, priv->nick);
	dwrq->length = strlen(extra) + 1;

	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_nick
*----------------------------------------------------------------*/
static int
acx_ioctl_set_nick(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_point *dwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	acx_sem_lock(priv);

	if (dwrq->length > IW_ESSID_MAX_SIZE + 1) {
		result = -E2BIG;
		goto end_unlock;
	}

	/* extra includes trailing \0, so it's ok */
	strcpy(priv->nick, extra);
	result = OK;

end_unlock:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*------------------------------------------------------------------------------
 * acx_ioctl_get_retry
 *----------------------------------------------------------------------------*/
static int
acx_ioctl_get_retry(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned int type = vwrq->flags & IW_RETRY_TYPE;
	unsigned int modifier = vwrq->flags & IW_RETRY_MODIFIER;
	int result;

	FN_ENTER;

	acx_sem_lock(priv);

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
			SET_BIT(vwrq->flags, IW_RETRY_MIN);
		vwrq->value = priv->short_retry;
	}

	/* can't be disabled */
	vwrq->disabled = (u8)0;
	result = OK;

	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_set_retry
*----------------------------------------------------------------*/
static int
acx_ioctl_set_retry(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	if (!vwrq) {
		result = -EFAULT;
		goto end;
	}
	if (vwrq->disabled) {
		result = -EINVAL;
		goto end;
	}

	acx_sem_lock(priv);

	result = -EINVAL;
	if (IW_RETRY_LIMIT == (vwrq->flags & IW_RETRY_TYPE)) {
		printk("old retry limits: short %d long %d\n",
				priv->short_retry, priv->long_retry);
		if (vwrq->flags & IW_RETRY_MAX) {
			priv->long_retry = vwrq->value;
		} else if (vwrq->flags & IW_RETRY_MIN) {
			priv->short_retry = vwrq->value;
		} else {
			/* no modifier: set both */
			priv->long_retry = vwrq->value;
			priv->short_retry = vwrq->value;
		}
		printk("new retry limits: short %d long %d\n",
				priv->short_retry, priv->long_retry);
		SET_BIT(priv->set_mask, GETSET_RETRY);
		result = -EINPROGRESS;
	}
	else if (vwrq->flags & IW_RETRY_LIFETIME) {
		priv->msdu_lifetime = vwrq->value;
		printk("new MSDU lifetime: %d\n", priv->msdu_lifetime);
		SET_BIT(priv->set_mask, SET_MSDU_LIFETIME);
		result = -EINPROGRESS;
	}

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/******************************* private ioctls ******************************/


/*----------------------------------------------------------------
* acx_ioctl_set_debug
*----------------------------------------------------------------*/
#if ACX_DEBUG
static int
acx_ioctl_set_debug(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	unsigned int debug_new = *((unsigned int *)extra);
	int result = -EINVAL;

	acxlog(L_ANY, "setting debug from %04X to %04X\n", acx_debug, debug_new);
	acx_debug = debug_new;

	result = OK;
	return result;

}
#endif

/*----------------------------------------------------------------
* acx_ioctl_list_reg_domain
*----------------------------------------------------------------*/
static const char * const
reg_domain_strings[] = {
	" 1-11 FCC (USA)",
	" 1-11 DOC/IC (Canada)",
	/* BTW: WLAN use in ETSI is regulated by
	 * ETSI standard EN 300 328-2 V1.1.2 */
	" 1-13 ETSI (Europe)",
	"10-11 Spain",
	"10-13 France",
	"   14 MKK (Japan)",
	" 1-14 MKK1",
	"  3-9 Israel (not all firmware versions)",
	NULL /* needs to remain as last entry */
};

static int
acx_ioctl_list_reg_domain(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{

	int i = 1;
	const char * const *entry = reg_domain_strings;

	printk("dom# chan# domain/country\n");
	while (*entry)
		printk("%4d %s\n", i++, *entry++);
	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_reg_domain
*----------------------------------------------------------------*/
static int
acx_ioctl_set_reg_domain(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	if ((*extra < 1) || ((size_t)*extra > acx_reg_domain_ids_len)) {
		result = -EINVAL;
		goto end;
	}

	acx_sem_lock(priv);

	priv->reg_dom_id = acx_reg_domain_ids[*extra - 1];
	SET_BIT(priv->set_mask, GETSET_REG_DOMAIN);

	result = -EINPROGRESS;

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_reg_domain
*----------------------------------------------------------------*/
static int
acx_ioctl_get_reg_domain(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int dom,i;

	/* no locking */
	dom = priv->reg_dom_id;

	for (i=1; i <= 7; i++) {
		if (acx_reg_domain_ids[i-1] == dom) {
			acxlog(L_IOCTL, "regulatory domain is currently set "
				"to %d (0x%X): %s\n", i, dom,
				reg_domain_strings[i-1]);
			*extra = i;
			break;
		}
	}

	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_short_preamble
*----------------------------------------------------------------*/
static const char * const
preamble_modes[] = {
	"off",
	"on",
	"auto (peer capability dependent)",
	"unknown mode, error"
};

static int
acx_ioctl_set_short_preamble(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int i;
	int result;

	FN_ENTER;

	if ((unsigned char)*extra > 2) {
		result = -EINVAL;
		goto end;
	}

	acx_sem_lock(priv);

	priv->preamble_mode = (u8)*extra;
	switch (priv->preamble_mode) {
	case 0: /* long */
		priv->preamble_cur = 0;
		break;
	case 1:
		/* short, kick incapable peers */
		priv->preamble_cur = 1;
		for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
			client_t *clt = &priv->sta_list[i];
			if (!clt->used) continue;
			if (!(clt->cap_info & WF_MGMT_CAP_SHORT)) {
				clt->used = CLIENT_EMPTY_SLOT_0;
			}
		}
		switch (priv->mode) {
		case ACX_MODE_2_STA:
			if (priv->ap_client && !priv->ap_client->used) {
				/* We kicked our AP :) */
				SET_BIT(priv->set_mask, GETSET_RESCAN);
			}
		}
		break;
	case 2: /* auto. short only if all peers are short-capable */
		priv->preamble_cur = 1;
		for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
			client_t *clt = &priv->sta_list[i];
			if (!clt->used) continue;
			if (!(clt->cap_info & WF_MGMT_CAP_SHORT)) {
				priv->preamble_cur = 0;
				break;
			}
		}
		break;
	}
	printk("new short preamble setting: configured %s, active %s\n",
			preamble_modes[priv->preamble_mode],
			preamble_modes[priv->preamble_cur]);
	result = OK;

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_short_preamble
*----------------------------------------------------------------*/
static int
acx_ioctl_get_short_preamble(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	acx_sem_lock(priv);

	printk("current short preamble setting: configured %s, active %s\n",
			preamble_modes[priv->preamble_mode],
			preamble_modes[priv->preamble_cur]);

	*extra = (char)priv->preamble_mode;

	acx_sem_unlock(priv);

	return OK;
}


/*----------------------------------------------------------------
* acx_ioctl_set_antenna
*
* Comment: TX and RX antenna can be set separately but this function good
*          for testing 0-4 bits
*----------------------------------------------------------------*/
static int
acx_ioctl_set_antenna(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	acx_sem_lock(priv);

	printk("old antenna value: 0x%02X (COMBINED bit mask)\n"
		     "Rx antenna selection:\n"
		     "0x00 ant. 1\n"
		     "0x40 ant. 2\n"
		     "0x80 full diversity\n"
		     "0xc0 partial diversity\n"
		     "0x0f dwell time mask (in units of us)\n"
		     "Tx antenna selection:\n"
		     "0x00 ant. 2\n" /* yep, those ARE reversed! */
		     "0x20 ant. 1\n"
		     "new antenna value: 0x%02X\n",
		     priv->antenna, (u8)*extra);

	priv->antenna = (u8)*extra;
	SET_BIT(priv->set_mask, GETSET_ANTENNA);

	acx_sem_unlock(priv);

	return -EINPROGRESS;
}


/*----------------------------------------------------------------
* acx_ioctl_get_antenna
*----------------------------------------------------------------*/
static int
acx_ioctl_get_antenna(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	/* no locking. it's pointless to lock a single load */
	printk("current antenna value: 0x%02X (COMBINED bit mask)\n"
		     "Rx antenna selection:\n"
		     "0x00 ant. 1\n"
		     "0x40 ant. 2\n"
		     "0x80 full diversity\n"
		     "0xc0 partial diversity\n"
		     "Tx antenna selection:\n"
		     "0x00 ant. 2\n" /* yep, those ARE reversed! */
		     "0x20 ant. 1\n", priv->antenna);

	return 0;
}


/*----------------------------------------------------------------
* acx_ioctl_set_rx_antenna
*
*
* Arguments:
*	0 = antenna1; 1 = antenna2; 2 = full diversity; 3 = partial diversity
* Comment: Could anybody test which antenna is the external one
*----------------------------------------------------------------*/
static int
acx_ioctl_set_rx_antenna(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	if (*extra > 3) {
		result = -EINVAL;
		goto end;
	}

	printk("old antenna value: 0x%02X\n", priv->antenna);

	acx_sem_lock(priv);

	priv->antenna &= 0x3f;
	SET_BIT(priv->antenna, (*extra << 6));
	SET_BIT(priv->set_mask, GETSET_ANTENNA);
	printk("new antenna value: 0x%02X\n", priv->antenna);
	result = -EINPROGRESS;

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_set_tx_antenna
*
* Arguments: 0 == antenna2; 1 == antenna1;
* Comment: Could anybody test which antenna is the external one
*----------------------------------------------------------------*/
static int
acx_ioctl_set_tx_antenna(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	FN_ENTER;

	if (*extra > 1) {
		result = -EINVAL;
		goto end;
	}

	printk("old antenna value: 0x%02X\n", priv->antenna);

	acx_sem_lock(priv);

	priv->antenna &= ~0x30;
	SET_BIT(priv->antenna, ((*extra & 0x01) << 5));
	SET_BIT(priv->set_mask, GETSET_ANTENNA);
	printk("new antenna value: 0x%02X\n", priv->antenna);
	result = -EINPROGRESS;

	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_wlansniff
*
* can we just remove this in favor of monitor mode? --vda
*----------------------------------------------------------------*/
static int
acx_ioctl_wlansniff(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned int *params = (unsigned int*)extra;
	unsigned int enable = (unsigned int)(params[0] > 0);
	int result;

	FN_ENTER;

	acx_sem_lock(priv);

	/* not using printk() here, since it distorts kismet display
	 * when printk messages activated */
	acxlog(L_IOCTL, "setting monitor to: 0x%02X\n", params[0]);

	switch (params[0]) {
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

	if (params[0]) {
		priv->mode = ACX_MODE_MONITOR;
		SET_BIT(priv->set_mask, GETSET_MODE);
	}

	if (enable) {
		priv->channel = params[1];
		SET_BIT(priv->set_mask, GETSET_RX);
	}
	result = -EINPROGRESS;

	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_unknown11
* FIXME: looks like some sort of "iwpriv kick_sta MAC" but it's broken
*----------------------------------------------------------------*/
static int
acx_ioctl_unknown11(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
#ifdef BROKEN
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	client_t client;
	int result;

	acx_sem_lock(priv);
	acx_lock(priv, flags);

	acx_l_transmit_disassoc(priv, &client);
	result = OK;

	acx_unlock(priv, flags);
	acx_sem_unlock(priv);

	return result;
#endif
	return -EINVAL;
}


/***********************************************************************
** debug helper function to be able to debug various issues relatively easily
*/
static int
acx_ioctl_dbg_set_masks(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	const unsigned int *params = (unsigned int*)extra;
	int result;

	acx_sem_lock(priv);

	acxlog(L_IOCTL, "setting flags in settings mask: "
			"get_mask %08X set_mask %08X\n"
			"before: get_mask %08X set_mask %08X\n",
			params[0], params[1],
			priv->get_mask, priv->set_mask);
	SET_BIT(priv->get_mask, params[0]);
	SET_BIT(priv->set_mask, params[1]);
	acxlog(L_IOCTL, "after: get_mask %08X set_mask %08X\n",
			priv->get_mask, priv->set_mask);
	result = -EINPROGRESS; /* immediately call commit handler */

	acx_sem_unlock(priv);

	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_set_rates
*
* This ioctl takes string parameter. Examples:
* iwpriv wlan0 SetRates "1,2"
*	use 1 and 2 Mbit rates, both are in basic rate set
* iwpriv wlan0 SetRates "1,2 5,11"
*	use 1,2,5.5,11 Mbit rates. 1 and 2 are basic
* iwpriv wlan0 SetRates "1,2 5c,11c"
*	same ('c' means 'CCK modulation' and it is a default for 5 and 11)
* iwpriv wlan0 SetRates "1,2 5p,11p"
*	use 1,2,5.5,11 Mbit, 1,2 are basic. 5 and 11 are using PBCC
* iwpriv wlan0 SetRates "1,2,5,11 22p"
*	use 1,2,5.5,11,22 Mbit. 1,2,5.5 and 11 are basic. 22 is using PBCC
*	(this is the maximum acx100 can do (modulo x4 mode))
* iwpriv wlan0 SetRates "1,2,5,11 22"
*	same. 802.11 defines only PBCC modulation
*	for 22 and 33 Mbit rates, so there is no ambiguity
* iwpriv wlan0 SetRates "1,2,5,11 6o,9o,12o,18o,24o,36o,48o,54o"
*	1,2,5.5 and 11 are basic. 11g OFDM rates are enabled but
*	they are not in basic rate set.	22 Mbit is disabled.
* iwpriv wlan0 SetRates "1,2,5,11 6,9,12,18,24,36,48,54"
*	same. OFDM is default for 11g rates except 22 and 33 Mbit,
*	thus 'o' is optional
* iwpriv wlan0 SetRates "1,2,5,11 6d,9d,12d,18d,24d,36d,48d,54d"
*	1,2,5.5 and 11 are basic. 11g CCK-OFDM rates are enabled
*	(acx111 does not support CCK-OFDM, driver will reject this cmd)
* iwpriv wlan0 SetRates "6,9,12 18,24,36,48,54"
*	6,9,12 are basic, rest of 11g rates is enabled. Using OFDM
*----------------------------------------------------------------*/
#include "setrate.c"

/* disallow: 33Mbit (unsupported by hw) */
/* disallow: CCKOFDM (unsupported by hw) */
static int
acx111_supported(int mbit, int modulation, void *opaque)
{
	if (mbit==33) return -ENOTSUPP;
	if (modulation==DOT11_MOD_CCKOFDM) return -ENOTSUPP;
	return OK;
}

static const u16
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
	/* lower 16 bits show selected 1, 2, CCK and OFDM rates */
	/* upper 16 bits show selected PBCC rates */
	u32 m = acx111mask[rate_mbit2enum(mbit)];
	if (modulation==DOT11_MOD_PBCC)
		return m<<16;
	return m;
}

static int
verify_rate(u32 rate, int chip_type)
{
	/* never happens. be paranoid */
	if (!rate) return -EINVAL;

	/* disallow: mixing PBCC and CCK at 5 and 11Mbit
	** (can be supported, but needs complicated handling in tx code) */
	if (( rate & ((RATE111_11+RATE111_5)<<16) )
	&&  ( rate & (RATE111_11+RATE111_5) )
	) {
		return -ENOTSUPP;
	}
	if (CHIPTYPE_ACX100 == chip_type) {
		if ( rate & ~(RATE111_ACX100_COMPAT+(RATE111_ACX100_COMPAT<<16)) )
			return -ENOTSUPP;
	}
	return 0;
}

static int
acx_ioctl_set_rates(struct net_device *dev, struct iw_request_info *info,
		 struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	int result;
	u32 brate = 0, orate = 0; /* basic, operational rate set */

	FN_ENTER;

	acxlog(L_IOCTL, "set_rates %s\n", extra);
	result = fill_ratemasks(extra, &brate, &orate,
				acx111_supported, acx111_gen_mask, 0);
	if (result) goto end;
	SET_BIT(orate, brate);
	acxlog(L_IOCTL, "brate %08X orate %08X\n", brate, orate);

	result = verify_rate(brate, priv->chip_type);
	if (result) goto end;
	result = verify_rate(orate, priv->chip_type);
	if (result) goto end;

	acx_sem_lock(priv);
	acx_lock(priv, flags);

	priv->rate_basic = brate;
	priv->rate_oper = orate;
	/* TODO: ideally, we shall monitor highest basic rate
	** which was successfully sent to every peer
	** (say, last we checked, everybody could hear 5.5 Mbits)
	** and use that for bcasts when we want to reach all peers.
	** For beacons, we probably shall use lowest basic rate
	** because we want to reach all *potential* new peers too */
	priv->rate_bcast = 1 << lowest_bit(brate);
	if (IS_ACX100(priv))
		priv->rate_bcast100 = acx_rate111to100(priv->rate_bcast);
	priv->rate_auto = !has_only_one_bit(orate);
	acx_l_update_client_rates(priv, orate);
	/* TODO: get rid of ratevector, build it only when needed */
	acx_l_update_ratevector(priv);

	/* Do/don't do tx rate fallback; beacon contents and rate */
	SET_BIT(priv->set_mask, SET_RATE_FALLBACK|SET_TEMPLATES);
	result = -EINPROGRESS;

	acx_unlock(priv, flags);
	acx_sem_unlock(priv);
end:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_get_phy_chan_busy_percentage
*----------------------------------------------------------------*/
static int
acx_ioctl_get_phy_chan_busy_percentage(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	struct { /* added ACX_PACKED, not tested --vda */
		u16 type ACX_PACKED;
		u16 len ACX_PACKED;
		u32 busytime ACX_PACKED;
		u32 totaltime ACX_PACKED;
	} usage;
	int result;

	acx_sem_lock(priv);

	if (OK != acx_s_interrogate(priv, &usage, ACX1xx_IE_MEDIUM_USAGE)) {
		result = NOT_OK;
		goto end_unlock;
	}

	usage.busytime = le32_to_cpu(usage.busytime);
	usage.totaltime = le32_to_cpu(usage.totaltime);
	printk("%s: average busy percentage since last invocation: %d%% "
		"(%u of %u microseconds)\n",
		dev->name,
		usage.busytime / ((usage.totaltime / 100) + 1),
		usage.busytime, usage.totaltime);

	result = OK;

end_unlock:
	acx_sem_unlock(priv);

	return result;
}


/*----------------------------------------------------------------
* acx_ioctl_set_ed_threshold
*----------------------------------------------------------------*/
static inline int
acx_ioctl_set_ed_threshold(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	acx_sem_lock(priv);

	printk("old ED threshold value: %d\n", priv->ed_threshold);
	priv->ed_threshold = (unsigned char)*extra;
	printk("new ED threshold value: %d\n", (unsigned char)*extra);
	SET_BIT(priv->set_mask, GETSET_ED_THRESH);

	acx_sem_unlock(priv);

	return -EINPROGRESS;
}


/*----------------------------------------------------------------
* acx_ioctl_set_cca
*----------------------------------------------------------------*/
static inline int
acx_ioctl_set_cca(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;

	acx_sem_lock(priv);

	printk("old CCA value: 0x%02X\n", priv->cca);
	priv->cca = (unsigned char)*extra;
	printk("new CCA value: 0x%02X\n", (unsigned char)*extra);
	SET_BIT(priv->set_mask, GETSET_CCA);
	result = -EINPROGRESS;

	acx_sem_unlock(priv);

	return result;
}


/***********************************************************************
*/
static const char * const
scan_modes[] = { "active", "passive", "background" };

static void
acx_print_scan_params(wlandevice_t *priv, const char* head)
{
	printk("%s: %smode %d (%s), min chan time %dTU, "
		"max chan time %dTU, max scan rate byte: %d\n",
		priv->netdev->name, head,
		priv->scan_mode, scan_modes[priv->scan_mode],
		priv->scan_probe_delay, priv->scan_duration, priv->scan_rate);
}

static int
acx_ioctl_set_scan_params(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;
	const int *params = (int *)extra;

	acx_sem_lock(priv);

	acx_print_scan_params(priv, "old scan parameters: ");
	if ((params[0] != -1) && (params[0] >= 0) && (params[0] <= 2))
		priv->scan_mode = params[0];
	if (params[1] != -1)
		priv->scan_probe_delay = params[1];
	if (params[2] != -1)
		priv->scan_duration = params[2];
	if ((params[3] != -1) && (params[3] <= 255))
		priv->scan_rate = params[3];
	acx_print_scan_params(priv, "new scan parameters: ");
	SET_BIT(priv->set_mask, GETSET_RESCAN);
	result = -EINPROGRESS;

	acx_sem_unlock(priv);

	return result;
}

static int
acx_ioctl_get_scan_params(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result;
	int *params = (int *)extra;

	acx_sem_lock(priv);

	acx_print_scan_params(priv, "current scan parameters: ");
	params[0] = priv->scan_mode;
	params[1] = priv->scan_probe_delay;
	params[2] = priv->scan_duration;
	params[3] = priv->scan_rate;
	result = OK;

	acx_sem_unlock(priv);

	return result;
}


/***********************************************************************
*/
static int
acx100_ioctl_set_led_power(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	static const char * const led_modes[] = { "off", "on", "LinkQuality" };

	wlandevice_t *priv = netdev_priv(dev);
	int result;

	acx_sem_lock(priv);

	printk("%s: power LED status: old %d (%s), ",
			dev->name,
			priv->led_power,
			led_modes[priv->led_power]);
	priv->led_power = extra[0];
	if (priv->led_power > 2) priv->led_power = 2;
	printk("new %d (%s)\n",
			priv->led_power,
			led_modes[priv->led_power]);

	if (priv->led_power == 2) {
		printk("%s: max link quality setting: old %d, ",
			dev->name, priv->brange_max_quality);
		if (extra[1])
			priv->brange_max_quality = extra[1];
		printk("new %d\n", priv->brange_max_quality);
	}

	SET_BIT(priv->set_mask, GETSET_LED_POWER);

	result = -EINPROGRESS;

	acx_sem_unlock(priv);

	return result;
}


/***********************************************************************
*/
static inline int
acx100_ioctl_get_led_power(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);

	acx_sem_lock(priv);

	extra[0] = priv->led_power;
	if (priv->led_power == 2)
		extra[1] = priv->brange_max_quality;
	else
		extra[1] = -1;

	acx_sem_unlock(priv);

	return OK;
}


/***********************************************************************
*/
static int
acx111_ioctl_info(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	if (!IS_PCI((wlandevice_t*)netdev_priv(dev)))
		return OK;
	return acx111pci_ioctl_info(dev, info, vwrq, extra);
}


/***********************************************************************
*/
static int
acx100_ioctl_set_phy_amp_bias(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	if (!IS_PCI((wlandevice_t*)netdev_priv(dev))) {
		printk("acx: set_phy_amp_bias() is not supported on USB\n");
		return OK;
	}
	return acx100pci_ioctl_set_phy_amp_bias(dev, info, vwrq, extra);
}


/***********************************************************************
*/
#if WIRELESS_EXT >= 13
static const iw_handler acx_ioctl_handler[] =
{
	(iw_handler) acx_ioctl_commit,		/* SIOCSIWCOMMIT */
	(iw_handler) acx_ioctl_get_name,	/* SIOCGIWNAME */
	(iw_handler) NULL,			/* SIOCSIWNWID */
	(iw_handler) NULL,			/* SIOCGIWNWID */
	(iw_handler) acx_ioctl_set_freq,	/* SIOCSIWFREQ */
	(iw_handler) acx_ioctl_get_freq,	/* SIOCGIWFREQ */
	(iw_handler) acx_ioctl_set_mode,	/* SIOCSIWMODE */
	(iw_handler) acx_ioctl_get_mode,	/* SIOCGIWMODE */
	(iw_handler) acx_ioctl_set_sens,	/* SIOCSIWSENS */
	(iw_handler) acx_ioctl_get_sens,	/* SIOCGIWSENS */
	(iw_handler) NULL,			/* SIOCSIWRANGE */
	(iw_handler) acx_ioctl_get_range,	/* SIOCGIWRANGE */
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
	(iw_handler) NULL /* acx_ioctl_set_spy FIXME */,	/* SIOCSIWSPY */
	(iw_handler) NULL /* acx_ioctl_get_spy */,		/* SIOCGIWSPY */
#else /* WSPY */
	(iw_handler) NULL,			/* SIOCSIWSPY */
	(iw_handler) NULL,			/* SIOCGIWSPY */
#endif /* WSPY */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) NULL,			/* [nothing] */
#endif /* IW_HANDLER_VERSION > 4 */
	(iw_handler) acx_ioctl_set_ap,		/* SIOCSIWAP */
	(iw_handler) acx_ioctl_get_ap,		/* SIOCGIWAP */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) acx_ioctl_get_aplist,	/* SIOCGIWAPLIST */
#if WIRELESS_EXT > 13
	(iw_handler) acx_ioctl_set_scan,	/* SIOCSIWSCAN */
	(iw_handler) acx_ioctl_get_scan,	/* SIOCGIWSCAN */
#else /* WE > 13 */
	(iw_handler) NULL,			/* SIOCSIWSCAN */
	(iw_handler) NULL,			/* SIOCGIWSCAN */
#endif /* WE > 13 */
	(iw_handler) acx_ioctl_set_essid,	/* SIOCSIWESSID */
	(iw_handler) acx_ioctl_get_essid,	/* SIOCGIWESSID */
	(iw_handler) acx_ioctl_set_nick,	/* SIOCSIWNICKN */
	(iw_handler) acx_ioctl_get_nick,	/* SIOCGIWNICKN */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) NULL,			/* [nothing] */
	(iw_handler) acx_ioctl_set_rate,	/* SIOCSIWRATE */
	(iw_handler) acx_ioctl_get_rate,	/* SIOCGIWRATE */
	(iw_handler) acx_ioctl_set_rts,		/* SIOCSIWRTS */
	(iw_handler) acx_ioctl_get_rts,		/* SIOCGIWRTS */
	(iw_handler) NULL /* acx_ioctl_set_frag FIXME*/,	/* SIOCSIWFRAG */
	(iw_handler) NULL /* acx_ioctl_get_frag */,		/* SIOCGIWFRAG */
	(iw_handler) acx_ioctl_set_txpow,	/* SIOCSIWTXPOW */
	(iw_handler) acx_ioctl_get_txpow,	/* SIOCGIWTXPOW */
	(iw_handler) acx_ioctl_set_retry,	/* SIOCSIWRETRY */
	(iw_handler) acx_ioctl_get_retry,	/* SIOCGIWRETRY */
	(iw_handler) acx_ioctl_set_encode,	/* SIOCSIWENCODE */
	(iw_handler) acx_ioctl_get_encode,	/* SIOCGIWENCODE */
	(iw_handler) acx_ioctl_set_power,	/* SIOCSIWPOWER */
	(iw_handler) acx_ioctl_get_power,	/* SIOCGIWPOWER */
};

static const iw_handler acx_ioctl_private_handler[] =
{
#if ACX_DEBUG
[ACX100_IOCTL_DEBUG		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_debug,
#else
[ACX100_IOCTL_DEBUG		- ACX100_IOCTL] = (iw_handler) NULL,
#endif
[ACX100_IOCTL_SET_PLED		- ACX100_IOCTL] = (iw_handler) acx100_ioctl_set_led_power,
[ACX100_IOCTL_GET_PLED		- ACX100_IOCTL] = (iw_handler) acx100_ioctl_get_led_power,
[ACX100_IOCTL_SET_RATES		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_rates,
[ACX100_IOCTL_LIST_DOM		- ACX100_IOCTL] = (iw_handler) acx_ioctl_list_reg_domain,
[ACX100_IOCTL_SET_DOM		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_reg_domain,
[ACX100_IOCTL_GET_DOM		- ACX100_IOCTL] = (iw_handler) acx_ioctl_get_reg_domain,
[ACX100_IOCTL_SET_SCAN_PARAMS	- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_scan_params,
[ACX100_IOCTL_GET_SCAN_PARAMS	- ACX100_IOCTL] = (iw_handler) acx_ioctl_get_scan_params,
[ACX100_IOCTL_SET_PREAMB	- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_short_preamble,
[ACX100_IOCTL_GET_PREAMB	- ACX100_IOCTL] = (iw_handler) acx_ioctl_get_short_preamble,
[ACX100_IOCTL_SET_ANT		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_antenna,
[ACX100_IOCTL_GET_ANT		- ACX100_IOCTL] = (iw_handler) acx_ioctl_get_antenna,
[ACX100_IOCTL_RX_ANT		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_rx_antenna,
[ACX100_IOCTL_TX_ANT		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_tx_antenna,
[ACX100_IOCTL_SET_PHY_AMP_BIAS	- ACX100_IOCTL] = (iw_handler) acx100_ioctl_set_phy_amp_bias,
[ACX100_IOCTL_GET_PHY_CHAN_BUSY	- ACX100_IOCTL] = (iw_handler) acx_ioctl_get_phy_chan_busy_percentage,
[ACX100_IOCTL_SET_ED		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_ed_threshold,
[ACX100_IOCTL_SET_CCA		- ACX100_IOCTL] = (iw_handler) acx_ioctl_set_cca,
[ACX100_IOCTL_MONITOR		- ACX100_IOCTL] = (iw_handler) acx_ioctl_wlansniff,
[ACX100_IOCTL_TEST		- ACX100_IOCTL] = (iw_handler) acx_ioctl_unknown11,
[ACX100_IOCTL_DBG_SET_MASKS	- ACX100_IOCTL] = (iw_handler) acx_ioctl_dbg_set_masks,
[ACX111_IOCTL_INFO		- ACX100_IOCTL] = (iw_handler) acx111_ioctl_info,
};

const struct iw_handler_def acx_ioctl_handler_def =
{
	.num_standard = VEC_SIZE(acx_ioctl_handler),
	.num_private = VEC_SIZE(acx_ioctl_private_handler),
	.num_private_args = VEC_SIZE(acx_ioctl_private_args),
	.standard = (iw_handler *) acx_ioctl_handler,
	.private = (iw_handler *) acx_ioctl_private_handler,
	.private_args = (struct iw_priv_args *) acx_ioctl_private_args,
};

#endif /* WE >= 13 */


#if WIRELESS_EXT < 13
/*================================================================*/
/* Main function						  */
/*================================================================*/
/*----------------------------------------------------------------
* acx_e_ioctl_old
*
* Comment:
* This is the *OLD* ioctl handler.
* Make sure to not only place your additions here, but instead mainly
* in the new one (acx_ioctl_handler[])!
*----------------------------------------------------------------*/
int
acx_e_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result = 0;
	struct iwreq *iwr = (struct iwreq *)ifr;

	acxlog(L_IOCTL, "%s cmd = 0x%04X\n", __func__, cmd);

	/* This is the way it is done in the orinoco driver.
	 * Check to see if device is present.
	 */
	if (0 == netif_device_present(dev)) {
		return -ENODEV;
	}

	switch (cmd) {
/* WE 13 and higher will use acx_ioctl_handler_def */
	case SIOCGIWNAME:
		/* get name == wireless protocol */
		result = acx_ioctl_get_name(dev, NULL,
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
		result = acx_ioctl_set_freq(dev, NULL, &(iwr->u.freq), NULL);
		break;

	case SIOCGIWFREQ:
		/* get channel/frequency (Hz) */
		result = acx_ioctl_get_freq(dev, NULL, &(iwr->u.freq), NULL);
		break;

	case SIOCSIWMODE:
		/* set operation mode */
		result = acx_ioctl_set_mode(dev, NULL, &(iwr->u.mode), NULL);
		break;

	case SIOCGIWMODE:
		/* get operation mode */
		result = acx_ioctl_get_mode(dev, NULL, &(iwr->u.mode), NULL);
		break;

	case SIOCSIWSENS:
		/* Set sensitivity */
		result = acx_ioctl_set_sens(dev, NULL, &(iwr->u.sens), NULL);
		break;

	case SIOCGIWSENS:
		/* Get sensitivity */
		result = acx_ioctl_get_sens(dev, NULL, &(iwr->u.sens), NULL);
		break;

#if WIRELESS_EXT > 10
	case SIOCGIWRANGE:
		/* Get range of parameters */
		{
			struct iw_range range;
			result = acx_ioctl_get_range(dev, NULL,
					&(iwr->u.data), (char *)&range);
			if (copy_to_user(iwr->u.data.pointer, &range,
					 sizeof(struct iw_range)))
				result = -EFAULT;
		}
		break;
#endif

	case SIOCGIWPRIV:
		result = acx_ioctl_get_iw_priv(iwr);
		break;

	/* case SIOCSIWSPY: */
	/* case SIOCGIWSPY: */
	/* case SIOCSIWTHRSPY: */
	/* case SIOCGIWTHRSPY: */

	case SIOCSIWAP:
		/* set access point by MAC address */
		result = acx_ioctl_set_ap(dev, NULL, &(iwr->u.ap_addr),
					     NULL);
		break;

	case SIOCGIWAP:
		/* get access point MAC address */
		result = acx_ioctl_get_ap(dev, NULL, &(iwr->u.ap_addr),
					     NULL);
		break;

	case SIOCGIWAPLIST:
		/* get list of access points in range */
		result = acx_ioctl_get_aplist(dev, NULL, &(iwr->u.data),
						 NULL);
		break;

#if NOT_FINISHED_YET
	case SIOCSIWSCAN:
		/* start a station scan */
		result = acx_ioctl_set_scan(iwr, priv);
		break;

	case SIOCGIWSCAN:
		/* get list of stations found during scan */
		result = acx_ioctl_get_scan(iwr, priv);
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
			result = acx_ioctl_set_essid(dev, NULL,
					&(iwr->u.essid), essid);
		}
		break;

	case SIOCGIWESSID:
		/* get ESSID */
		{
			char essid[IW_ESSID_MAX_SIZE+1];
			if (iwr->u.essid.pointer)
				result = acx_ioctl_get_essid(dev, NULL,
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
			result = acx_ioctl_set_nick(dev, NULL,
					&(iwr->u.data), nick);
		}
		break;

	case SIOCGIWNICKN:
		/* get nick */
		{
			char nick[IW_ESSID_MAX_SIZE+1];
			if (iwr->u.data.pointer)
				result = acx_ioctl_get_nick(dev, NULL,
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
		result = acx_ioctl_set_rts(dev, NULL, &(iwr->u.rts), NULL);
		break;
	case  SIOCGIWRTS:
		/* get RTS threshold value */
		result = acx_ioctl_get_rts(dev, NULL,  &(iwr->u.rts), NULL);
		break;

	/* case  SIOCSIWFRAG: */
	/* case  SIOCGIWFRAG: */

#if WIRELESS_EXT > 9
	case SIOCGIWTXPOW:
		/* get tx power */
		result = acx_ioctl_get_txpow(dev, NULL, &(iwr->u.txpower),
						NULL);
		break;

	case SIOCSIWTXPOW:
		/* set tx power */
		result = acx_ioctl_set_txpow(dev, NULL, &(iwr->u.txpower),
						NULL);
		break;
#endif

	case SIOCSIWRETRY:
		result = acx_ioctl_set_retry(dev, NULL, &(iwr->u.retry), NULL);
		break;

	case SIOCGIWRETRY:
		result = acx_ioctl_get_retry(dev, NULL, &(iwr->u.retry), NULL);
		break;

	case SIOCSIWENCODE:
		{
			/* set encoding token & mode */
			u8 key[29];
			if (iwr->u.encoding.pointer) {
				if (iwr->u.encoding.length > 29) {
					result = -E2BIG;
					break;
				}
				if (copy_from_user(key, iwr->u.encoding.pointer,
						iwr->u.encoding.length)) {
					result = -EFAULT;
					break;
				}
			}
			else
			if (iwr->u.encoding.length) {
				result = -EINVAL;
				break;
			}
			result = acx_ioctl_set_encode(dev, NULL,
					&(iwr->u.encoding), key);
		}
		break;

	case SIOCGIWENCODE:
		{
			/* get encoding token & mode */
			u8 key[29];

			result = acx_ioctl_get_encode(dev, NULL,
					&(iwr->u.encoding), key);
			if (iwr->u.encoding.pointer) {
				if (copy_to_user(iwr->u.encoding.pointer,
						key, iwr->u.encoding.length))
					result = -EFAULT;
			}
		}
		break;

	/******************** iwpriv ioctls below ********************/
#if ACX_DEBUG
	case ACX100_IOCTL_DEBUG:
		acx_ioctl_set_debug(dev, NULL, NULL, iwr->u.name);
		break;
#endif

	case ACX100_IOCTL_SET_PLED:
		acx100_ioctl_set_led_power(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_GET_PLED:
		acx100_ioctl_get_led_power(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_LIST_DOM:
		acx_ioctl_list_reg_domain(dev, NULL, NULL, NULL);
		break;

	case ACX100_IOCTL_SET_DOM:
		acx_ioctl_set_reg_domain(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_GET_DOM:
		acx_ioctl_get_reg_domain(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_SET_SCAN_PARAMS:
		acx_ioctl_set_scan_params(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_GET_SCAN_PARAMS:
		acx_ioctl_get_scan_params(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_SET_PREAMB:
		acx_ioctl_set_short_preamble(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_GET_PREAMB:
		acx_ioctl_get_short_preamble(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_SET_ANT:
		acx_ioctl_set_antenna(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_GET_ANT:
		acx_ioctl_get_antenna(dev, NULL, NULL, NULL);
		break;

	case ACX100_IOCTL_RX_ANT:
		acx_ioctl_set_rx_antenna(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_TX_ANT:
		acx_ioctl_set_tx_antenna(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_SET_ED:
		acx_ioctl_set_ed_threshold(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_SET_CCA:
		acx_ioctl_set_cca(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_MONITOR:	/* set sniff (monitor) mode */
		acxlog(L_IOCTL, "%s: IWPRIV monitor\n", dev->name);

		/* can only be done by admin */
		if (!capable(CAP_NET_ADMIN)) {
			result = -EPERM;
			break;
		}
		result = acx_ioctl_wlansniff(dev, NULL, NULL, iwr->u.name);
		break;

	case ACX100_IOCTL_TEST:
		acx_ioctl_unknown11(dev, NULL, NULL, NULL);
		break;

	case ACX111_IOCTL_INFO:
		acx111_ioctl_info(dev, NULL, NULL, NULL);
		break;

	default:
		acxlog(L_IOCTL, "wireless ioctl 0x%04X queried "
				"but not implemented yet\n", cmd);
		result = -EOPNOTSUPP;
		break;
	}

	if ((priv->dev_state_mask & ACX_STATE_IFACE_UP) && priv->set_mask) {
		acx_sem_lock(priv);
		acx_s_update_card_settings(priv, 0, 0);
		acx_sem_unlock(priv);
	}

	/* older WEs don't have a commit handler,
	 * so we need to fix return code in this case */
	if (-EINPROGRESS == result)
		result = 0;

	return result;
}
#endif /* WE < 13 */
