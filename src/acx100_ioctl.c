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
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif /* WE > 12 */
#include <linux/netdevice.h>
#include <asm/uaccess.h>

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
#define ACX100_IOCTL_SET_MAC		ACX100_IOCTL + 0x0d
#define ACX100_IOCTL_MONITOR		ACX100_IOCTL + 0x0e
#define ACX100_IOCTL_TEST		ACX100_IOCTL + 0x0f

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
{ cmd : ACX100_IOCTL_SET_MAC,
	set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6,
	get_args : 0,
	name : "set_mac_addr" },
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	
	FN_ENTER;
	acx100_update_card_settings(wlandev, 0, 0, 0);
	FN_EXIT(0, 0);
	return 0;
}

static inline int acx100_ioctl_get_name(struct net_device *dev, struct iw_request_info *info, char *cwrq, char *extra)
{
	const char * const protocol_name = "IEEE 802.11b+";
	acxlog(L_IOCTL, "Get Name => %s\n", protocol_name);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int channel = -1;
	int mult = 1;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set Frequency <= %i (%i)\n", fwrq->m, fwrq->e);

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

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	wlandev->channel = channel;
	/* hmm, the following code part is strange, but this is how
	 * it was being done before... */
	if (wlandev->macmode == WLAN_MACMODE_ESS_AP /* 3 */ ) {
		/* hmm, AP mode? So simply set channel... */
		acxlog(L_IOCTL, "Changing to channel %d\n", wlandev->channel);
		wlandev->set_mask |= GETSET_TX|GETSET_RX;
	} else if (wlandev->macmode == WLAN_MACMODE_ESS_STA	/* 2 */
		|| wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ ) {
		/* trigger scanning... */
		wlandev->set_mask |= GETSET_CHANNEL;
	}
	acx100_unlock(wlandev, &flags);
	result = -EINPROGRESS; /* need to call commit handler */
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_freq(struct net_device *dev, struct iw_request_info *info, struct iw_freq *fwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	fwrq->e = 0;
	fwrq->m = wlandev->channel;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set Mode <= %i\n", *uwrq);

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	if (*uwrq == IW_MODE_ADHOC)
		wlandev->mode = 0;
	else if (*uwrq == IW_MODE_INFRA)
		wlandev->mode = 2;
	else {
		result = -EOPNOTSUPP;
		goto end_unlock;
	}

	wlandev->set_mask |= GETSET_MODE;
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(wlandev, &flags);
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_mode(struct net_device *dev, struct iw_request_info *info, __u32 *uwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get Mode => %d\n", wlandev->macmode);

	if (wlandev->status != ISTATUS_4_ASSOCIATED)
	/* if (!wlandev->ifup) */
	{ /* connection not up yet, so for now indicate the mode we want,
	     not the one we are in */
		if (wlandev->mode == 0)
			*uwrq = IW_MODE_ADHOC;
		else if (wlandev->mode == 2)
			*uwrq = IW_MODE_INFRA;
	}
	else
	{
		if (wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ )
			*uwrq = IW_MODE_ADHOC;
		else if (wlandev->macmode == WLAN_MACMODE_ESS_STA /* 2 */ )
			*uwrq = IW_MODE_INFRA;
	}
	return 0;
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	static const unsigned char off[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
	int result = 0;
	int i;
	unsigned char *ap;

	FN_ENTER;
	if (!awrq) {
		result = -EFAULT;
		goto end;
	}
	if (awrq->sa_family != ARPHRD_ETHER) {
                result = -EINVAL;
		goto end;
	}
	
	ap = awrq->sa_data;
	acxlog(L_IOCTL, "Set AP <== %02x:%02x:%02x:%02x:%02x:%02x\n",
               ap[0], ap[1], ap[2], ap[3], ap[4], ap[5]);

	if (wlandev->macmode != WLAN_MACMODE_ESS_STA) {
		result = -EINVAL;
		goto end;
	}

	if (acx100_is_mac_address_broadcast(ap)) {
		/* "any" == "auto" == FF:FF:FF:FF:FF:FF */
		acx100_set_mac_address_broadcast(wlandev->ap);
		acxlog(L_IOCTL, "Forcing reassociation\n");
		acx100_scan_chan(wlandev);
		result = -EINPROGRESS;
	} else if (!memcmp(off, ap, ETH_ALEN)) {
		/* "off" == 00:00:00:00:00:00 */
		acx100_set_mac_address_broadcast(wlandev->ap);
		acxlog(L_IOCTL, "Not reassociating\n");
	} else {
		/* AB:CD:EF:01:23:45 */
		for (i = 0; i < wlandev->bss_table_count; i++) {
			struct bss_info *bss = &wlandev->bss_table[i];
			if (!memcmp(bss->bssid, ap, ETH_ALEN)) {
				if ((!!wlandev->wep_enabled) != !!(bss->caps & IEEE802_11_MGMT_CAP_WEP)) {
					result = -EINVAL;
					goto end;
                        	} else {
					memcpy(wlandev->ap, ap, ETH_ALEN);
					acxlog(L_IOCTL, "Forcing reassociation\n");
					acx100_scan_chan(wlandev);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get BSSID\n");
	/* as seen in Aironet driver, airo.c */
	memcpy(awrq->sa_data, wlandev->bssid, WLAN_BSSID_LEN);
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
* Comment:
*
*----------------------------------------------------------------*/
static inline int acx100_ioctl_get_aplist(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = 0;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	if (wlandev->macmode != WLAN_MACMODE_ESS_AP /* 3 */ )
	{
		result = -EOPNOTSUPP;
		goto end;
	}

	wlandev->unknown0x2350 = ISTATUS_5_UNKNOWN;
	acx100_scan_chan(wlandev);

#if (WLAN_HOSTIF!=WLAN_USB)
	while (!(acx100_read_reg16(wlandev, wlandev->io[IO_ACX_IRQ_STATUS_NON_DES]) & 0x2000)) {
		/* FIXME: urks, busy loop! */
	};
#endif

	acxlog(L_IOCTL, "after site survey status = %d\n",
	       wlandev->status);

	if (wlandev->status == ISTATUS_5_UNKNOWN) {
		acx100_set_status(wlandev, wlandev->unknown0x2350);
		wlandev->unknown0x2350 = 0;
	}

	if (dwrq->flags == SIOCGIWAPLIST) {
		struct ap {
			int essid_len;
			char essid[IW_ESSID_MAX_SIZE];
			int channel;
			char address[WLAN_BSSID_LEN];

			int cap;
			int wep;
		};
		struct ap *ap_table;
		int i = 0;

		ap_table = kmalloc(sizeof(struct ap) * IW_MAX_AP, GFP_USER);
		if (!ap_table) {
			result = -ENOMEM;
			goto end;
		}
		if (wlandev->bss_table_count != 0) {

			for (; i < wlandev->bss_table_count; i++) {

				ap_table[i].channel =
				    wlandev->bss_table[i].channel;
				ap_table[i].essid_len =
				    wlandev->bss_table[i].essid_len;
				memcpy(&(ap_table[i].essid),
				       wlandev->bss_table[i].essid,
				       ap_table[i].essid_len);
				memcpy(ap_table[i].address,
				       wlandev->bss_table[i].bssid,
				       WLAN_BSSID_LEN);

				ap_table[i].cap = ((wlandev->bss_table[i].caps >> 1) ^ 1) & 1;	/* IBSS capability flag */
				ap_table[i].wep = wlandev->bss_table[i].caps & 0x10;	/* Privacy/WEP capability flag */
			}
		}

		dwrq->length =
		    wlandev->bss_table_count * sizeof(struct ap);
		/* FIXME memcpy? */
		if (copy_to_user (dwrq->pointer, ap_table, dwrq->length) != 0)
			result = -EFAULT;
		kfree(ap_table);

	} else if (dwrq->pointer != 0) {

		struct ap_addr {
			sa_family_t sa_family;
			char sa_data[ETH_ALEN];
		};
		struct ap_addr *addresses;

		addresses = kmalloc(sizeof(struct ap_addr)*IW_MAX_AP, GFP_USER);
		if (!addresses) {
			result = -ENOMEM;
			goto end;
		}
		if (wlandev->bss_table_count != 0) {

			int i;

			for (i = 0; i < wlandev->bss_table_count; i++) {

				memcpy(addresses[i].sa_data,
				       wlandev->bss_table[i].bssid,
				       WLAN_BSSID_LEN);
				addresses[i].sa_family = ARPHRD_ETHER;
			}
		}


		dwrq->length = wlandev->bss_table_count;
		/* FIXME memcpy? */
		if (copy_to_user (dwrq->pointer, addresses,
		     wlandev->bss_table_count * sizeof(struct ap_addr)) != 0)
			result = -EFAULT;
		kfree(addresses);
	}

end:
	acx100_unlock(wlandev, &flags);

	return result;
}

static inline int acx100_ioctl_set_scan(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int result = -EINVAL;

	FN_ENTER;

	/* don't start scan if device is not up yet */
	if (wlandev->open == 0) {
		result = -EAGAIN;
		goto end;
	}

	acx100_scan_chan(wlandev);
	wlandev->scan_start = jiffies;
	wlandev->scan_running = 1;
	result = 0;

end:
	FN_EXIT(1, result);
	return result;
}

#if WIRELESS_EXT > 13
static char *acx100_ioctl_scan_add_station(wlandevice_t *wlandev, char *ptr, char *end_buf, struct bss_info *bss)
{
	struct iw_event iwe;
	int i;
	char *ptr_rate;

	FN_ENTER;

	/* MAC address has to be added first */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(iwe.u.ap_addr.sa_data, bss->bssid, ETH_ALEN);
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
	if (bss->caps & (IEEE802_11_MGMT_CAP_ESS | IEEE802_11_MGMT_CAP_IBSS)) {
		if (bss->caps & IEEE802_11_MGMT_CAP_ESS)
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
	/* FIXME: are these values being correctly calculated? */
	iwe.u.qual.level = bss->sir;
	iwe.u.qual.noise = bss->snr;
	iwe.u.qual.qual = 0;
	acxlog(L_IOCTL, "scan, link quality: %d/%d/%d\n", iwe.u.qual.level, iwe.u.qual.noise, iwe.u.qual.qual);
	ptr = iwe_stream_add_event(ptr, end_buf, &iwe, IW_EV_QUAL_LEN);

	/* Add encryption */
	iwe.cmd = SIOCGIWENCODE;
	if (bss->caps & IEEE802_11_MGMT_CAP_WEP)
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
	for (i = 0; bss->supp_rates[i]; i++)
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	char *ptr = extra;
	int i;

	FN_ENTER;

	/* no scan available if device is not up yet */
	if (wlandev->open == 0)
		return -EAGAIN;

	if (wlandev->scan_start && time_before(jiffies, wlandev->scan_start + 3*HZ))
	{
		/* scan still in progress, so no results yet, sorry */
		return -EAGAIN;
	}
	wlandev->scan_start = 0;

	if (wlandev->bss_table_count == 0)
		/* no stations found */
		return -ENODATA;

	for (i = 0; i < wlandev->bss_table_count; i++)
	{
		struct bss_info *bss = &wlandev->bss_table[i];

		ptr = acx100_ioctl_scan_add_station(wlandev, ptr, extra + IW_SCAN_MAX_DATA, bss);
	}
	dwrq->length = ptr - extra;
	dwrq->flags = 0;

	FN_EXIT(1, 0);
	return 0;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int len = dwrq->length;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL, "Set ESSID <= %s, length %d, flags 0x%04x\n", dwrq->pointer, len, dwrq->flags);

	if (len <= 0) {
		result = -EINVAL;
		goto end;
	}

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	/* ESSID disabled? */
	if (dwrq->flags == 0)
	{
		wlandev->essid_active = 0;
	}
	else
	{
		if (dwrq->length > IW_ESSID_MAX_SIZE+1)
		{
			result = -E2BIG;
			goto end_unlock;
		}

		wlandev->essid_len = len - 1;
		memcpy(wlandev->essid, extra, wlandev->essid_len);
		wlandev->essid[wlandev->essid_len] = '\0';
		wlandev->essid_active = 1;
	}

	wlandev->set_mask |= GETSET_ESSID;

end_unlock:
	acx100_unlock(wlandev, &flags);
	result = -EINPROGRESS;
end:
	FN_EXIT(1, result);
	return result;
}

static inline int acx100_ioctl_get_essid(struct net_device *dev, struct iw_request_info *info, struct iw_point *dwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get ESSID => %s\n", wlandev->essid);

	dwrq->flags = wlandev->essid_active;
	if (wlandev->essid_active)
	{
		memcpy(extra, wlandev->essid, wlandev->essid_len);
		extra[wlandev->essid_len] = '\0';
		dwrq->length = wlandev->essid_len + 1;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	unsigned char bitrateval;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL,
	       "rate = %d, fixed = 0x%x, disabled = 0x%x, flags = 0x%x\n",
	       vwrq->value, vwrq->fixed, vwrq->disabled, vwrq->flags);

#define BITRATE_AUTO 1
#if BITRATE_AUTO
	if ((vwrq->fixed == 0) || (vwrq->fixed == 1)) {
#else
	if (vwrq->fixed == 1) {
#endif

		switch (vwrq->value) {

		case 1000000:	/* 1Mbps */
			bitrateval = 10;
			break;

		case 2000000:	/* 2Mbps */
			bitrateval = 20;
			break;

		case 5500000:	/* 5.5Mbps */
			bitrateval = 55;
			break;

		case 11000000:	/* 11Mbps */
			bitrateval = 110;
			break;

		case 22000000:	/* 22Mbps */
			bitrateval = 220;
			break;

#if BITRATE_AUTO
		case -1: /* highest available */
			/* -1 is used to set highest available rate in
			 * case of iwconfig calls without rate given
			 * (iwconfig wlan0 rate auto etc.) */
			bitrateval = 110; /* FIXME: should be 220 instead, but since rate fallback doesn't actually work yet, we shouldn't use a rate not supported by many APs */
			break;
#endif
		default:
			result = -EINVAL;
			goto end;
		}

	} else if (vwrq->fixed == 2) {

		switch (vwrq->value) {

		case 0:
			bitrateval = 10;
			break;

		case 1:
			bitrateval = 20;
			break;

		case 2:
			bitrateval = 55;
			break;

		case 3:
			bitrateval = 183;
			break;

		case 4:
			bitrateval = 110;
			break;

		case 5:
			bitrateval = 238;
			break;

		case 6:
			bitrateval = 220;
			break;

		default:
			result = -EINVAL;
			goto end;
		}

	} else {
		result = -EOPNOTSUPP;
		goto end;
	}

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	wlandev->bitrateval = bitrateval;
#if BITRATE_AUTO
	wlandev->bitrate_auto = (vwrq->fixed == 0);
#endif
	
	acx100_unlock(wlandev, &flags);

#if BITRATE_AUTO
	acxlog(L_IOCTL, "Tx rate = %d, auto rate %d\n", wlandev->bitrateval, wlandev->bitrate_auto);
#else
	acxlog(L_IOCTL, "Tx rate = %d\n", wlandev->bitrateval);
#endif

	result = 0;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	/* FIXME: maybe bitrateval is the value we *wanted*, but not the
	 * value it actually chose automatically. Needs verification and
	 * perhaps fixing. */
	vwrq->value = wlandev->bitrateval * 100000;
#if BITRATE_AUTO
	vwrq->fixed = (wlandev->bitrate_auto == 0);
#else
	vwrq->fixed = 1;
#endif
	vwrq->disabled = 0;
	return 0;
}

static inline int acx100_ioctl_set_rts(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int val = vwrq->value;

	if (vwrq->disabled)
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
	vwrq->disabled = (vwrq->value >= 2312);
	vwrq->fixed = 1;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int index;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	acxlog(L_IOCTL,
	       "Set Encoding flags = 0x%04x, size = %i, key: %s\n",
	       dwrq->flags, dwrq->length, extra ? "set" : "No key");

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	index = dwrq->flags & IW_ENCODE_INDEX;

	if (dwrq->length > 0) {

		/* if index is 0 or invalid, use default key */
		if ((index == 0) || (index > 4))
			index = wlandev->wep_current_index;
		else
			index--;

		if (!(dwrq->flags & IW_ENCODE_NOKEY)) {
			if (dwrq->length > 29)
				dwrq->length = 29; /* restrict it */

			if (dwrq->length > 13)
				wlandev->wep_keys[index].size = 29; /* 29*8 == 232, WEP256 */
			else
			if (dwrq->length > 5)
				wlandev->wep_keys[index].size = 13; /* 13*8 == 104bit, WEP128 */
			else
			if (dwrq->length > 0)
				wlandev->wep_keys[index].size = 5; /* 5*8 == 40bit, WEP64 */
			else
				/* disable key */
				wlandev->wep_keys[index].size = 0;

			memset(wlandev->wep_keys[index].key, 0, sizeof(wlandev->wep_keys[index].key));
			memcpy(wlandev->wep_keys[index].key, extra, dwrq->length);
		}

	} else {
		/* set transmit key */
		if ((index >= 1) && (index <= 4))
			wlandev->wep_current_index = index - 1;
		else
			if (!(dwrq->flags & IW_ENCODE_MODE))
			{
				/* complain if we were not just setting
				 * the key mode */
				result =  -EINVAL;
				goto end_unlock;
			}
	}

	wlandev->wep_enabled = !(dwrq->flags & IW_ENCODE_DISABLED);

	if (dwrq->flags & IW_ENCODE_OPEN) {
		wlandev->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
		wlandev->wep_restricted = 0;
	} else if (dwrq->flags & IW_ENCODE_RESTRICTED) {
		wlandev->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
		wlandev->wep_restricted = 1;
	}

	/* set flag to make sure the card WEP settings get updated */
	wlandev->set_mask |= GETSET_WEP;

	acxlog(L_IOCTL, "len = %d, key at 0x%p, flags = 0x%x\n",
	       dwrq->length, extra,
	       dwrq->flags);

	for (index = 0; index <= 3; index++) {
		if (wlandev->wep_keys[index].size != 0)
			acxlog(L_IOCTL,
			       "index = %ld, size = %d, key at 0x%p\n",
			       wlandev->wep_keys[index].index,
			       wlandev->wep_keys[index].size,
			       wlandev->wep_keys[index].key);
	}
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(wlandev, &flags);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	if (wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ ) {
		/* ok, let's pretend it's supported, but print a
		 * warning message
		 * FIXME: should be removed once it's definitely working. */
		acxlog(L_STD, "Warning: WEP support might not be supported in Ad-Hoc mode yet!\n");
		/* return -EOPNOTSUPP; */
	}

	if (wlandev->wep_enabled == 0)
	{
		dwrq->flags = IW_ENCODE_DISABLED;
	}
	else
	{
		dwrq->flags =
			(wlandev->wep_restricted == 1) ? IW_ENCODE_RESTRICTED : IW_ENCODE_OPEN;

		dwrq->length =
		    wlandev->wep_keys[wlandev->wep_current_index].size;

		memcpy(extra,
			     wlandev->wep_keys[wlandev->wep_current_index].key,
			     wlandev->wep_keys[wlandev->wep_current_index].size);
	}

	/* set the current index */
	dwrq->flags |=
	    wlandev->wep_keys[wlandev->wep_current_index].index + 1;

	acxlog(L_IOCTL, "len = %d, key = %p, flags = 0x%x\n",
	       dwrq->length, dwrq->pointer,
	       dwrq->flags);

	return 0;
}

static inline int acx100_ioctl_set_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Set 802.11 Power Save flags = 0x%04x\n", vwrq->flags);
	if (vwrq->disabled) {
		wlandev->ps_wakeup_cfg &= ~PS_CFG_ENABLE;
		wlandev->set_mask |= GETSET_POWER_80211;
		return -EINPROGRESS;
	}
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		UINT16 ps_timeout = (vwrq->value * 1024) / 1000;

		if (ps_timeout > 255)
			ps_timeout = 255;
		acxlog(L_IOCTL, "setting PS timeout value to %d time units due to %dus\n", ps_timeout, vwrq->value);
		wlandev->ps_hangover_period = ps_timeout;
	} else if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_PERIOD) {
		UINT16 ps_periods = vwrq->value / 1000000;

		if (ps_periods > 255)
			ps_periods = 255;
		acxlog(L_IOCTL, "setting PS period value to %d periods due to %dus\n", ps_periods, vwrq->value);
		wlandev->ps_listen_interval = ps_periods;
		wlandev->ps_wakeup_cfg &= ~PS_CFG_WAKEUP_MODE_MASK;
		wlandev->ps_wakeup_cfg |= PS_CFG_WAKEUP_EACH_ITVL;
	}
	switch (vwrq->flags & IW_POWER_MODE) {
		/* FIXME: are we doing the right thing here? */
		case IW_POWER_UNICAST_R:
			wlandev->ps_options &= ~PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_MULTICAST_R:
			wlandev->ps_options |= PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_ALL_R:
			wlandev->ps_options |= PS_OPT_STILL_RCV_BCASTS;
			break;
		case IW_POWER_ON:
			break;
		default:
			acxlog(L_IOCTL, "unknown PS mode\n");
			return -EINVAL;
	}
	wlandev->ps_wakeup_cfg |= PS_CFG_ENABLE;
	wlandev->set_mask |= GETSET_POWER_80211;
	return -EINPROGRESS;
			
}

static inline int acx100_ioctl_get_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	acxlog(L_IOCTL, "Get 802.11 Power Save flags = 0x%04x\n", vwrq->flags);
	vwrq->disabled = ((wlandev->ps_wakeup_cfg & PS_CFG_ENABLE) == 0);
	if (vwrq->disabled)
		return 0;
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		vwrq->value = wlandev->ps_hangover_period * 1000 / 1024;
		vwrq->flags = IW_POWER_TIMEOUT;
	} else {
		vwrq->value = wlandev->ps_listen_interval * 1000000;
		vwrq->flags = IW_POWER_PERIOD|IW_POWER_RELATIVE;
	}
	if (wlandev->ps_options & PS_OPT_STILL_RCV_BCASTS)
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	vwrq->flags = IW_TXPOW_DBM;
	vwrq->disabled = 0;
	vwrq->fixed = 0;
	vwrq->value = wlandev->tx_level_dbm;

	acxlog(L_IOCTL, "Get transmit power => %d dBm\n", wlandev->tx_level_dbm);

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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	if (vwrq->value == 255)
		wlandev->tx_level_auto = 1;
	else
	{
		wlandev->tx_level_dbm = vwrq->value <= 20 ? vwrq->value : 20;
		wlandev->tx_level_auto = 0;
	}
	acxlog(L_IOCTL, "Set transmit power = %d dBm\n", wlandev->tx_level_dbm);
	wlandev->set_mask |= GETSET_TXPOWER;
	acx100_unlock(wlandev, &flags);
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

		dwrq->length = sizeof(struct iw_range);
		memset(range, 0, sizeof(struct iw_range));

		range->min_pmp = 0;
		range->max_pmp = 5000000;
		range->min_pmt = 0;
		range->max_pmt = 65535 * 1000;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_ALL_R;

		range->we_version_compiled = WIRELESS_EXT;
		range->we_version_source = 0x9;

		range->retry_capa = IW_RETRY_LIMIT;
		range->retry_flags = IW_RETRY_LIMIT;
		range->min_retry = 1;
		range->max_retry = 255;

		range->r_time_flags = IW_RETRY_LIFETIME;
		range->min_r_time = 0;
		range->max_r_time = 65535; /* FIXME: lifetime ranges and orders of magnitude are strange?? */

		range->sensitivity = 0x3f;

		range->max_qual.qual = 100;
		range->max_qual.level = 100;
		range->max_qual.noise = 100;
		/* FIXME: better values */
		range->avg_qual.qual = 90;
		range->avg_qual.level = 40;
		range->avg_qual.noise = 10;
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	/* FIXME : consider spinlock here */
	strcpy(extra, wlandev->nick);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	if(dwrq->length > IW_ESSID_MAX_SIZE + 1) {
		result = -E2BIG;
		goto end_unlock;
	}

	/* extra includes trailing \0, so it's ok */
	strcpy(wlandev->nick, extra);
	result = 0;

end_unlock:
	acx100_unlock(wlandev, &flags);
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	__u16 type = vwrq->flags & IW_RETRY_TYPE;
	__u16 modifier = vwrq->flags & IW_RETRY_MODIFIER;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	/* return the short retry number by default */
	if (type == IW_RETRY_LIFETIME) {
		vwrq->flags = IW_RETRY_LIFETIME;
		vwrq->value = wlandev->msdu_lifetime;
	} else if (modifier == IW_RETRY_MAX) {
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
		vwrq->value = wlandev->long_retry;
	} else {
		vwrq->flags = IW_RETRY_LIMIT;
		if (wlandev->long_retry != wlandev->short_retry)
			vwrq->flags |= IW_RETRY_MIN;
		vwrq->value = wlandev->short_retry;
	}
	acx100_unlock(wlandev, &flags);
	/* can't be disabled */
	vwrq->disabled = 0;
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	if (!vwrq) {
		result = -EFAULT;
		goto end;
	}
	if (vwrq->disabled) {
		result = -EINVAL;
		goto end;
	}
	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	if (IW_RETRY_LIMIT == (vwrq->flags & IW_RETRY_TYPE)) {
		printk("current retry limits: short %ld long %ld\n", wlandev->short_retry, wlandev->long_retry);
                if (vwrq->flags & IW_RETRY_MAX) {
                        wlandev->long_retry = vwrq->value;
                } else if (vwrq->flags & IW_RETRY_MIN) {
                        wlandev->short_retry = vwrq->value;
                } else {
                        /* no modifier: set both */
                        wlandev->long_retry = vwrq->value;
                        wlandev->short_retry = vwrq->value;
                }
		printk("new retry limits: short %ld long %ld\n", wlandev->short_retry, wlandev->long_retry);
		wlandev->set_mask |= GETSET_RETRY;
		result = -EINPROGRESS;
	}
	else
	if (vwrq->flags & IW_RETRY_LIFETIME) {
		wlandev->msdu_lifetime = vwrq->value;
		printk("new MSDU lifetime: %d\n", wlandev->msdu_lifetime);
		wlandev->set_mask |= SET_MSDU_LIFETIME;
		result = -EINPROGRESS;
	}
	acx100_unlock(wlandev, &flags);
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

static unsigned char *reg_domain_strings[] =
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
	unsigned char **entry;

	printk("Domain/Country  Channels  Setting\n");
	for (i = 0, entry = reg_domain_strings; *entry; i++, entry++)
		printk("%s      %d\n", *entry, i+1);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	if ((*extra < 1) || (*extra > sizeof(reg_domain_ids))) {
		result = -EINVAL;
		goto end_unlock;
	}
	wlandev->reg_dom_id = reg_domain_ids[*extra - 1];
	wlandev->set_mask |= GETSET_REG_DOMAIN;
	result = -EINPROGRESS;

end_unlock:
	acx100_unlock(wlandev, &flags);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int i;

	for (i=1; i <= 7; i++)
		if (reg_domain_ids[i-1] == wlandev->reg_dom_id)
		{
			acxlog(L_STD, "regulatory domain is currently set to %d (0x%x): %s\n", i, wlandev->reg_dom_id, reg_domain_strings[i-1]);
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	char *descr = NULL;

	if ((*extra < 0) || (*extra > 2))
		return -EINVAL;

	wlandev->preamble_mode = *extra;
	switch (*extra) {
		case 0:
			descr = "off";
			wlandev->preamble_flag = 0;
			break;
		case 1:
			descr = "on";
			wlandev->preamble_flag = 1;
			break;
		case 2:
			descr = "auto (peer capability dependent)";

			/* associated to a station? */
			if (wlandev->station_assoc.bssid[0] != 0x00)
				wlandev->preamble_flag = wlandev->station_assoc.caps & IEEE802_11_MGMT_CAP_SHORT_PRE;
			break;
	}
	printk("new Short Preamble setting: %s\n", descr);

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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	char *descr = NULL;

	switch(wlandev->preamble_mode) {
		case 0:
			descr = "off";
			break;
		case 1:
			descr = "on";
			break;
		case 2:
			descr = "auto (peer capability dependent)";
			break;
	}
	printk("current Short Preamble setting: %s\n", descr);

	*extra = wlandev->preamble_mode;

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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	printk("current antenna value: 0x%02X\n", wlandev->antenna);
	printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	printk("Tx antenna selection seems to be bit 5 (0x20)\n");
	wlandev->antenna = *extra;
	printk("new antenna value: 0x%02X\n", wlandev->antenna);
	wlandev->set_mask |= GETSET_ANTENNA;

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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	printk("current antenna value: 0x%02X\n", wlandev->antenna);
	printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	printk("Tx antenna selection seems to be bit 5 (0x20)\n");

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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	printk("current antenna value: 0x%02X\n", wlandev->antenna);
	/* better keep the separate operations atomic */
	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	wlandev->antenna &= 0x3f;
	wlandev->antenna |= (*extra << 6);
	wlandev->set_mask |= GETSET_ANTENNA;
	acx100_unlock(wlandev, &flags);
	printk("new antenna value: 0x%02X\n", wlandev->antenna);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;
	printk("current antenna value: 0x%02X\n", wlandev->antenna);
	/* better keep the separate operations atomic */
	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	wlandev->antenna &= 0xdf;
	wlandev->antenna |= ((*extra &= 0x01) << 5);
	wlandev->set_mask |= GETSET_ANTENNA;
	acx100_unlock(wlandev, &flags);
	printk("new antenna value: 0x%02X\n", wlandev->antenna);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	int *parms = (int*)extra;
	int enable = parms[0] > 0;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	FN_ENTER;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	wlandev->monitor = parms[0];
	printk("setting monitor to: 0x%02X\n", wlandev->monitor);

	switch (parms[0])
	{
	case 0:
		wlandev->netdev->type = ARPHRD_ETHER;
		break;
	case 1:
		wlandev->netdev->type = ARPHRD_IEEE80211_PRISM;
		break;
	case 2:
		wlandev->netdev->type = ARPHRD_IEEE80211;
		break;
	}

	if (wlandev->monitor)
		wlandev->monitor_setting = 0x02; /* don't decrypt default key only, override decryption mechanism */
	else
		wlandev->monitor_setting = 0x00; /* don't decrypt default key only, don't override decryption */

	wlandev->set_mask |= SET_RXCONFIG | SET_WEP_OPTIONS;

	if (enable)
	{
		wlandev->channel = parms[1];
		wlandev->set_mask |= GETSET_RX;
	}
	acx100_unlock(wlandev, &flags);
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
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;
	unsigned long flags;
	client_t client;
	int err;
	int result = -EINVAL;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	acx100_transmit_disassoc(&client, wlandev);
	acx100_unlock(wlandev, &flags);
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;

	printk("current ED threshold value: %d\n", wlandev->ed_threshold);
	wlandev->ed_threshold = (unsigned char)*extra;
	printk("new ED threshold value: %d\n", (unsigned char)*extra);
	wlandev->set_mask |= GETSET_ED_THRESH;

	return 0;
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
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}

	printk("current CCA value: 0x%02X\n", wlandev->cca);
	wlandev->cca = (unsigned char)*extra;
	printk("new CCA value: 0x%02X\n", (unsigned char)*extra);
	wlandev->set_mask |= GETSET_CCA;
	acx100_unlock(wlandev, &flags);
	result = -EINPROGRESS;

end:
	return result;
}

static inline int acx100_ioctl_set_led_power(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	unsigned long flags;
	int err;
	int result = -EINVAL;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	printk("current power LED status: %d\n", wlandev->led_power);
	wlandev->led_power = (unsigned char)*extra;
	printk("new power LED status: %d\n", (unsigned char)*extra);
	wlandev->set_mask |= GETSET_LED_POWER;

	acx100_unlock(wlandev, &flags);
	result = -EINPROGRESS;

end:
	return result;
}

static inline int acx100_ioctl_set_mac_address(struct net_device *dev, struct iw_request_info *info, struct iw_param *vwrq, char *extra)
{
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	unsigned long flags;
	UINT8 *mac = (unsigned char *)extra;
	UINT8 *a;
	int err;
	int result = -EINVAL;

	if ((err = acx100_lock(wlandev, &flags))) {
		result = err;
		goto end;
	}
	a = dev->dev_addr;
	printk("current MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
			a[0], a[1], a[2], a[3], a[4], a[5]);
	
	acx100_copy_mac_address(wlandev->dev_addr, mac);
	acx100_copy_mac_address(dev->dev_addr, wlandev->dev_addr);

	printk("new MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
			a[0], a[1], a[2], a[3], a[4], a[5]);
	
	acx100_unlock(wlandev, &flags);
	result = 0;

end:
	return result;
}

#if WIRELESS_EXT >= 13
#warning "(NOT a warning!) Compile info: choosing to use code infrastructure for newer wireless extension interface version (>= 13)"
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
	(iw_handler) NULL,			/* SIOCSIWSENS */
	(iw_handler) NULL,			/* SIOCGIWSENS */
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
	(iw_handler) acx100_ioctl_set_mac_address,
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



/*================================================================*/
/* Main function						  */
/*================================================================*/
/*----------------------------------------------------------------
* acx100_ioctl_main
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
int acx100_ioctl_main(netdevice_t * dev, struct ifreq *ifr, int cmd)
{
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;
	int result = 0;
#if WIRELESS_EXT < 13
	struct iwreq *iwr = (struct iwreq *)ifr;
#endif

	acxlog(L_IOCTL, "%s cmd = 0x%04X\n", __func__, cmd);

	/* This is the way it is done in the orinoco driver.
	 * Check to see if device is present.
	 */
	if (! netif_device_present(dev))
	{
		return -ENODEV;
	}

	switch (cmd) {
/* WE 13 and higher will use acx100_ioctl_handler_def */
#if WIRELESS_EXT < 13
#warning "(NOT a warning!) Compile info: choosing to use code infrastructure for older wireless extension interface version (< 13)"
#warning "This is untested, please report if it works for you"
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

	/* case SIOCSIWSENS: FIXME */
	/* case SIOCGIWSENS: FIXME */

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
		result = acx100_ioctl_set_scan(iwr, wlandev);
		break;

	case SIOCGIWSCAN:
		/* get list of stations found during scan */
		result = acx100_ioctl_get_scan(iwr, wlandev);
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
		result = acx100_ioctl_get_essid(dev, NULL, &(iwr->u.essid),
						NULL);
		break;

	case SIOCSIWNICKN:
		result = acx100_ioctl_set_nick(dev, NULL, &(iwr->u.data),
					       NULL);
		break;

	case SIOCGIWNICKN:
		result = acx100_ioctl_get_nick(dev, NULL, &(iwr->u.data),
					       NULL);
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
		/* set encoding token & mode */
		result = acx100_ioctl_set_encode(dev, NULL, &(iwr->u.encoding),
						 NULL);
		break;

	case SIOCGIWENCODE:
		/* get encoding token & mode */

		result = acx100_ioctl_get_encode(dev, NULL, &(iwr->u.encoding),
						 NULL);
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
		
	case ACX100_IOCTL_SET_MAC:
		acx100_ioctl_set_mac_address(dev, NULL, NULL, iwr->u.name);
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

	if (wlandev->open && wlandev->set_mask)
		acx100_update_card_settings(wlandev, 0, 0, 0);

#if THIS_LEADS_TO_CRASHES
	if (wlandev->mode != 2 && reinit == 1) {
		if (result = acx100_lock(wlandev, &flags))
			return result;

		if (!acx100_set_beacon_template(wlandev)) {
			acxlog(L_BINSTD,
			       "acx100_set_beacon_template returns error\n");
			result = -EFAULT;
		}

		if (!acx100_set_probe_response_template(wlandev)) {
			acxlog(L_BINSTD,
			       "acx100_set_probe_response_template returns error\n");
			result = -EFAULT;
		}

		acx100_client_sta_list_init();

		acx100_unlock(wlandev, &flags);
	}
#endif

	return result;
}
