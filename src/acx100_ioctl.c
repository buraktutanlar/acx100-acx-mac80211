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
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <p80211msg.h>
#include <p80211ioctl.h>
#include <acx100.h>
#include <acx100_conv.h>
#include <p80211netdev.h>
#include <p80211req.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <acx100mgmt.h>

/* About the locking:
 *  I only locked the device whenever calls to the hardware are made or
 *  parts of the wlandevice struct are modified, otherwise no locking is
 *  performed. I don't now if this is safe, we'll see.
 */

static char *shortversion = WLAN_RELEASE_SUB;

const long acx100_channel_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484
};

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
static inline int acx100_ioctl_set_freq(struct iwreq *iwr, wlandevice_t *wlandev)
{
	int channel = -1;
	int mult = 1;
	unsigned long flags;

	acx100_lock(wlandev, &flags);

	acxlog(L_IOCTL, "Set Frequency <= %i (%i)\n",
		iwr->u.freq.m, iwr->u.freq.e);

	if (iwr->u.freq.e == 0 && iwr->u.freq.m <= 1000) {
		/* Setting by channel number */
		channel = iwr->u.freq.m;
	} else {
		/* If setting by frequency, convert to a channel */
		int i;

		for (i = 0; i < (6 - iwr->u.freq.e); i++)
			mult *= 10;

		for (i = 0; i <= 13; i++)
			if (iwr->u.freq.m ==
			    acx100_channel_freq[i] * mult)
				channel = i + 1;
	}

	if (channel > 14) {
		acx100_unlock(wlandev, &flags);
		return -EFAULT;
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
static inline int acx100_ioctl_set_mode(struct iwreq *iwr, wlandevice_t *wlandev)
{
	acxlog(L_IOCTL, "Set Mode <= %i\n", iwr->u.mode);

	if (iwr->u.mode == IW_MODE_ADHOC)
		wlandev->mode = 0;
	else if (iwr->u.mode == IW_MODE_INFRA)
		wlandev->mode = 2;
	else {
		return -EOPNOTSUPP;
	}

	wlandev->set_mask |= GETSET_MODE;

	return 0;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_waplist
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
static inline int acx100_ioctl_get_waplist(struct iwreq *iwr, wlandevice_t *hw)
{
	unsigned long flags;

	acx100_lock(hw, &flags);

	if (hw->macmode != WLAN_MACMODE_ESS_AP /* 3 */ )
		return -EOPNOTSUPP;

	hw->unknown0x2350 = ISTATUS_5_UNKNOWN;
	acx100_scan_chan(hw);

	while (!(acx100_read_reg16(hw, ACX100_STATUS) & 0x2000)) {
		/* FIXME: urks, busy loop! */
	};

	acxlog(L_IOCTL, "after site survey status = %d\n",
	       hw->iStatus);

	if (hw->iStatus == ISTATUS_5_UNKNOWN) {
		acx100_set_status(hw, hw->unknown0x2350);
		hw->unknown0x2350 = 0;
	}

	if (iwr->u.data.flags == SIOCGIWAPLIST) {
		/* The variables being used in this ioctl,
		 * despite being localised in local brackets,
		 * still take up a lot of stack space,
		 * thus still possibly leading to IRQ handler
		 * stack overflow. They should probably be
		 * dynamically allocated instead. */
		struct ap {
			int size;
			char essid[32];
			int channel;
			char address[6];

			int var_71c;
			int var_718;
		} var_74c[IW_MAX_AP];

		int i = 0;

		if (hw->iStable != 0) {

			for (; i < hw->iStable; i++) {

				var_74c[i].channel =
				    hw->val0x126c[i].channel;
				var_74c[i].size =
				    hw->val0x126c[i].size;
				memcpy(&(var_74c[i].essid),
				       hw->val0x126c[i].essid,
				       var_74c[i].size);
				memcpy(var_74c[i].address,
				       hw->val0x126c[i].address,
				       WLAN_BSSID_LEN);

				var_74c[i].var_71c = ((hw->val0x126c[i].cap >> 1) ^ 1) & 1;	/* IBSS capability flag */
				var_74c[i].var_718 = hw->val0x126c[i].cap & 0x10;	/* Privacy/WEP capability flag */
			}
		}

		iwr->u.data.length =
		    hw->iStable * sizeof(struct ap);
		if (copy_to_user
		    (iwr->u.data.pointer, &var_74c,
		     iwr->u.data.length) != 0) {
			return -EFAULT;
		}

	} else if (iwr->u.data.pointer != 0) {

		struct ap_addr {
			sa_family_t sa_family;
			char sa_data[6];
		} var_94c[IW_MAX_AP];

		if (hw->iStable != 0) {

			int i;

			for (i = 0; i < hw->iStable; i++) {

				memcpy(var_94c[i].sa_data,
				       hw->val0x126c[i].address,
				       WLAN_BSSID_LEN);
				var_94c[i].sa_family = 1;	/* FIXME: AF_LOCAL ?? */
			}
		}


		iwr->u.data.length = hw->iStable;
		if (copy_to_user
		    (iwr->u.data.pointer, &var_94c,
		     hw->iStable * sizeof(struct ap_addr)) != 0) {
			acx100_unlock(hw, &flags);
			return -EFAULT;
		}
	}

	acx100_unlock(hw, &flags);

	return 0;
}

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
static inline int acx100_ioctl_set_essid(struct iwreq *iwr, wlandevice_t *wlandev)
{
	int len = iwr->u.data.length;

	acxlog(L_IOCTL, "Set ESSID <= %s\n", iwr->u.data.pointer);

	if (len <= 0) {
		goto end;
	}

	/* ESSID disabled? */
	if (iwr->u.data.flags == 0)
	{
		wlandev->essid_active = 0;
	}
	else
	{
		len =
		    len >
		    WLAN_SSID_MAXLEN ? WLAN_SSID_MAXLEN : len;

		copy_from_user(wlandev->essid, iwr->u.essid.pointer,
			       len);
		wlandev->essid_active = 1;
	}

	wlandev->set_mask |= GETSET_ESSID;

end:
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
static inline int acx100_ioctl_set_rate(struct iwreq *iwr, wlandevice_t *hw)
{
	int result = 0;
	unsigned long flags;

	acx100_lock(hw, &flags);

	if (iwr->u.bitrate.fixed == 1) {

		switch (iwr->u.bitrate.value) {

		case 1000000:	/* 1Mbps */
			hw->bitrateval = 10;
			break;

		case 2000000:	/* 2Mbps */
			hw->bitrateval = 20;
			break;


		case 5500000:	/* 5.5Mbps */
			hw->bitrateval = 55;
			break;

		case 11000000:	/* 11Mbps */
			hw->bitrateval = 110;
			break;

		case 22000000:	/* 22Mbps */
			hw->bitrateval = 220;
			break;

		default:
			result = -EINVAL;
			break;
		}

	} else if (iwr->u.bitrate.fixed == 2) {

		switch (iwr->u.bitrate.value) {

		case 0:
			hw->bitrateval = 10;
			break;

		case 1:
			hw->bitrateval = 20;
			break;

		case 2:
			hw->bitrateval = 55;
			break;

		case 3:
			hw->bitrateval = 183;
			break;

		case 4:
			hw->bitrateval = 110;
			break;

		case 5:
			hw->bitrateval = 238;
			break;

		case 6:
			hw->bitrateval = 220;
			break;

		default:
			result = -EINVAL;
			break;
		}

	} else {
		result = -EOPNOTSUPP;
	}

	acxlog(L_IOCTL,
	       "rate = %d, fixed = 0x%x, disabled = 0x%x, flags = 0x%x\n",
	       iwr->u.bitrate.value, iwr->u.bitrate.fixed,
	       iwr->u.bitrate.disabled, iwr->u.bitrate.flags);

	acxlog(L_IOCTL, "Tx rate = %d\n", hw->bitrateval);

	acx100_unlock(hw, &flags);

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
static inline int acx100_ioctl_get_rate(struct iwreq *iwr, wlandevice_t *hw)
{
	/* FIXME: maybe bitrateval is the value we *wanted*, but not the
	 * value it actually chose automatically. Needs verification and
	 * perhaps fixing. */
	iwr->u.bitrate.value = hw->bitrateval * 100000;
	iwr->u.bitrate.fixed = 2;		/* FIXME? */
	iwr->u.bitrate.disabled = 0;
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
static inline int acx100_ioctl_set_encode(struct iwreq *iwr, wlandevice_t *hw)
{
	int index;
	unsigned long flags;

	acx100_lock(hw, &flags);

	acxlog(L_IOCTL,
	       "Set Encoding flags = 0x%04x, size = %i, key: %s\n",
	       iwr->u.encoding.flags, iwr->u.encoding.length,
	       iwr->u.encoding.pointer ? "set" : "No key");

	if (hw->mode == 0 /* Ad-Hoc */ )
	{
		/* ok, let's pretend it's supported, but print a
		 * warning message
		 * FIXME: should be removed once it's definitely working. */
		acxlog(L_STD, "Warning: WEP support might not be supported in Ad-Hoc mode yet!\n");
		/* return -EOPNOTSUPP; */
	}

	index = iwr->u.encoding.flags & IW_ENCODE_INDEX;

	if (iwr->u.encoding.length != 0) {

		if (iwr->u.encoding.flags & IW_ENCODE_OPEN) {

			hw->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
			hw->wep_restricted = 0;

		} else if (iwr->u.encoding.
			   flags & IW_ENCODE_RESTRICTED) {

			hw->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
			hw->wep_restricted = 1;
		}

		if (index == 0) {

			index = hw->wep_current_index;

			if (iwr->u.encoding.length < 5) {
				acx100_unlock(hw, &flags);
				return -EINVAL;
			}

			memset(hw->wep_keys[index].key, 0, 256);

			if (iwr->u.encoding.length < 13) {

				hw->wep_keys[index].size = 5; /* 5*8 == 40bit, WEP64 */
			} else if (iwr->u.encoding.length < 29) {

				hw->wep_keys[index].size = 13; /* 13*8 == 104bit, WEP128 */
			} else {

				hw->wep_keys[index].size = 29; /* 29*8 == 232, WEP256 */
			}
			copy_from_user(hw->wep_keys[index].key,
				       iwr->u.encoding.pointer,
				       hw->wep_keys[index].size);

		} else if (--index <= 3) {

			if (iwr->u.encoding.length < 5) {
				acx100_unlock(hw, &flags);
				return -EINVAL;
			}

			memset(hw->wep_keys[index].key, 0, 256);
			hw->wep_keys[index].index = index;

			if (iwr->u.encoding.length < 13) {

				hw->wep_keys[index].size = 5; /* 5*8 == 40bit, WEP64 */

			} else if (iwr->u.encoding.length < 29) {

				hw->wep_keys[index].size = 13; /* 13*8 == 104bit, WEP128 */

			} else {

				hw->wep_keys[index].size = 29; /* 29*8 == 232, WEP256 */
			}
			copy_from_user(hw->wep_keys[index].key,
				       iwr->u.encoding.pointer,
				       hw->wep_keys[index].size);

		} else {

			acx100_unlock(hw, &flags);
			return -EINVAL;
		}

		hw->wep_enabled = 1;

	} else if (index == 0) {

		if (iwr->u.encoding.flags & IW_ENCODE_DISABLED)
			hw->wep_enabled = 0;

	} else if (--index <= 3) {

		hw->wep_current_index = index;

	} else {

		acx100_unlock(hw, &flags);
		return -EINVAL;
	}

	/* set flag to make sure the card WEP settings get updated */
	hw->set_mask |= GETSET_WEP;

	/* V3CHANGE: dbg msg not present */
	acxlog(L_IOCTL, "len = %d, key at 0x%p, flags = 0x%x\n",
	       iwr->u.encoding.length, iwr->u.encoding.pointer,
	       iwr->u.encoding.flags);

	for (index = 0; index <= 3; index++) {
		if (hw->wep_keys[index].size != 0)
			acxlog(L_IOCTL,
			       "index = %ld, size = %d, key at 0x%p\n",
			       hw->wep_keys[index].index,
			       hw->wep_keys[index].size,
			       hw->wep_keys[index].key);
	}

	acx100_unlock(hw, &flags);

	return 0;
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
static inline int acx100_ioctl_get_encode(struct iwreq *iwr, wlandevice_t *wlandev)
{
	if (wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ ) {
		/* ok, let's pretend it's supported, but print a
		 * warning message
		 * FIXME: should be removed once it's definitely working. */
		acxlog(L_STD, "Warning: WEP support might not be supported in Ad-Hoc mode yet!\n");
		/* return -EOPNOTSUPP; */
	}

	if (wlandev->wep_enabled == 0)
	{
		iwr->u.encoding.flags = IW_ENCODE_DISABLED;
	}
	else
	{
		iwr->u.encoding.flags =
			(wlandev->wep_restricted == 1) ? IW_ENCODE_RESTRICTED : IW_ENCODE_OPEN;

		iwr->u.encoding.length =
		    wlandev->wep_keys[wlandev->wep_current_index].size;

		copy_to_user(iwr->u.encoding.pointer,
			     wlandev->wep_keys[wlandev->wep_current_index].key,
			     wlandev->wep_keys[wlandev->wep_current_index].size);
	}

	/* set the current index */
	iwr->u.encoding.flags |=
	    wlandev->wep_keys[wlandev->wep_current_index].index + 1;

	acxlog(L_IOCTL, "len = %d, key = %p, flags = 0x%x\n",
	       iwr->u.encoding.length, iwr->u.encoding.pointer,
	       iwr->u.encoding.flags);

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
static inline int acx100_ioctl_get_txpow(struct iwreq *iwr, wlandevice_t *hw)
{
	struct iw_param *rrq = &iwr->u.txpower;

	rrq->flags = IW_TXPOW_DBM;
	rrq->disabled = 0;
	rrq->fixed = 0;
	rrq->value = hw->tx_level_dbm;

	acxlog(L_IOCTL, "Get transmit power => %d dBm\n", hw->tx_level_dbm);

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
static inline int acx100_ioctl_set_txpow(struct iwreq *iwr, wlandevice_t *hw)
{
	struct iw_param *rrq = &iwr->u.txpower;

	hw->tx_level_dbm = rrq->value <= 20 ? rrq->value : 20;
	acxlog(L_IOCTL, "Set transmit power = %d dBm\n", hw->tx_level_dbm);
	hw->set_mask |= GETSET_TXPOWER;

	return 0;
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
static inline int acx100_ioctl_get_range(struct iwreq *iwr, wlandevice_t *hw)
{
	if (iwr->u.data.pointer != NULL) {
		struct iw_range parm_range;

		iwr->u.data.length = sizeof(struct iw_range);
		memset(&parm_range, 0, sizeof(struct iw_range));

		parm_range.we_version_compiled = WIRELESS_EXT;
		parm_range.we_version_source = 0x9;
		parm_range.retry_capa = IW_RETRY_LIMIT;
		parm_range.retry_flags = IW_RETRY_LIMIT;
		parm_range.min_retry = IW_RETRY_LIMIT;

		parm_range.sensitivity = 0x3f;
		parm_range.max_qual.qual = 100;
		parm_range.max_qual.level = 100;
		parm_range.max_qual.noise = 100;
		/* FIXME: better values */
		parm_range.avg_qual.qual = 90;
		parm_range.avg_qual.level = 40;
		parm_range.avg_qual.noise = 10;


		if (copy_to_user(iwr->u.data.pointer, &parm_range,
		     sizeof(struct iw_range)) != 0) {
			return -EFAULT;
		}
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
	static struct iw_priv_args iwp[] = {
#ifdef DEBUG
	{ cmd : SIOCIWFIRSTPRIV + 0x0,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_debug"},
#endif
	{ cmd : SIOCIWFIRSTPRIV + 0x1,
		set_args : 0,
		get_args : 0,
		name : "list_reg_domain"},
	{ cmd : SIOCIWFIRSTPRIV + 0x2,
		set_args : 0,
		get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
		name : "get_reg_domain"},
	{ cmd : SIOCIWFIRSTPRIV + 0x3,
		set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_reg_domain"},
	{ cmd : SIOCIWFIRSTPRIV + 0x4,
		set_args : 0,
		get_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
		name : "get_s_preamble"},
	{ cmd : SIOCIWFIRSTPRIV + 0x5,
		set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_s_preamble"},
	{ cmd : SIOCIWFIRSTPRIV + 0x6,
		set_args : 0,
		get_args : 0,
		name : "get_antenna"},
	{ cmd : SIOCIWFIRSTPRIV + 0x7,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_antenna"},
	{ cmd : SIOCIWFIRSTPRIV + 0x8,
		set_args : IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 2,
		get_args : 0,
		name : "set_retry"},
	{ cmd : SIOCIWFIRSTPRIV + 0x9,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_ed"},
	{ cmd : SIOCIWFIRSTPRIV + 0xa,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_cca"},
	{ cmd : SIOCIWFIRSTPRIV + 0xb,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		get_args : 0,
		name : "monitor"},
	{ cmd : SIOCIWFIRSTPRIV + 0xc,
		set_args : 0,
		get_args : 0,
		name : "fw"},
	{ cmd : SIOCIWFIRSTPRIV + 0xd,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_rx_ant"},
	{ cmd : SIOCIWFIRSTPRIV + 0xe,
		set_args : IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		get_args : 0,
		name : "set_tx_ant"},
	{ cmd : SIOCIWFIRSTPRIV + 0xf,
		set_args : 0,
		get_args : 0,
		name : "test"}
	};
	int result = 0;

	if (iwr->u.data.pointer != 0) {
		result =
		    verify_area(VERIFY_WRITE, iwr->u.data.pointer,
				sizeof(iwp));
		if (result != 0)
			return result;

		iwr->u.data.length = sizeof(iwp) / sizeof(iwp[0]);
		if (copy_to_user(iwr->u.data.pointer, iwp, sizeof(iwp)) !=
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
static inline int acx100_ioctl_get_nick(wlandevice_t *hw, struct iw_point *ip)
{
	/* copied from orinoco.c */
	char nickbuf[IW_ESSID_MAX_SIZE+1];

	/* FIXME : consider spinlock here */
	memcpy(nickbuf,hw->nick,IW_ESSID_MAX_SIZE+1);
	/* FIXME : consider spinlock here */

	ip->length = strlen(nickbuf)+1;

	if (copy_to_user(ip->pointer,nickbuf,sizeof(nickbuf)))
		return -EFAULT;

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
static inline int acx100_ioctl_set_nick(wlandevice_t *hw, struct iw_point *ip)
{
	char nickbuf[IW_ESSID_MAX_SIZE+1];
	int result = 0;

	if(ip->length > IW_ESSID_MAX_SIZE) {
		result = -E2BIG;
		goto end;
	}

	memset(nickbuf, 0, sizeof(nickbuf));

	if (copy_from_user(nickbuf, ip->pointer, ip->length)) {
		result = -EFAULT;
		goto end;
	}

	memcpy(hw->nick, nickbuf, sizeof(hw->nick));

end:
	return result;
}

/*----------------------------------------------------------------
* acx100_ioctl_get_fw_stats
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
static inline int acx100_ioctl_get_fw_stats(wlandevice_t *hw)
{
	//UINT tmp[(ACX100_RID_FIRMWARE_STATISTICS_LEN + 8)/4];
	struct {
	UINT val0x0; //hdr;
	UINT val0x4;
	UINT tx_desc_of;
	UINT rx_oom;
	UINT rx_hdr_of;
	UINT rx_hdr_use_next;
	UINT rx_dropped_frame;
	UINT rx_frame_ptr_err;

	UINT rx_dma_req;//val0x1c
	UINT rx_dma_err;//val0x20
	UINT tx_dma_req;
	UINT tx_dma_err;//val0x28

	UINT cmd_cplt;
	UINT fiq;
	UINT rx_hdrs;//val0x34
	UINT rx_cmplt;//val0x38
	UINT rx_mem_of;//val0x3c
	UINT rx_rdys;
	UINT irqs;
	UINT acx_trans_procs;
	UINT decrypt_done;//val0x48
	UINT dma_0_done;
	UINT dma_1_done;
	UINT tx_exch_complet;
	UINT commands;
	UINT acx_rx_procs;
	UINT hw_pm_mode_changes;
	UINT host_acks;
	UINT pci_pm;
	UINT acm_wakeups;

	UINT wep_key_count;
	UINT wep_default_key_count;
	UINT dot11_def_key_mib;
	UINT wep_key_not_found;
	UINT wep_decrypt_fail;

	} tmp;
//	int i;
	unsigned long flags;

	if (acx100_lock(hw, &flags)) {
		return 0;
	}

	acx100_interrogate(hw, &tmp, ACX100_RID_FIRMWARE_STATISTICS);

	acx100_unlock(hw, &flags);

	printk("tx_desc_of %d, rx_oom %d, rx_hdr_of %d, rx_hdr_use_next %d\n",
		tmp.tx_desc_of, tmp.rx_oom, tmp.rx_hdr_of, tmp.rx_hdr_use_next);
	printk("rx_dropped_frame %d, rx_frame_ptr_err %d, rx_dma_req %d\n",
		tmp.rx_dropped_frame, tmp.rx_frame_ptr_err, tmp.rx_dma_req);
	printk("rx_dma_err %d, tx_dma_req %d, tx_dma_err %d, cmd_cplt %d, fiq %d\n",
		tmp.rx_dma_err, tmp.tx_dma_req, tmp.tx_dma_err, tmp.cmd_cplt, tmp.fiq);
	printk("rx_hdrs %d, rx_cmplt %d, rx_mem_of %d, rx_rdys %d, irqs %d\n",
		tmp.rx_hdrs, tmp.rx_cmplt, tmp.rx_mem_of, tmp.rx_rdys, tmp.irqs);
	printk("acx_trans_procs %d, decrypt_done %d, dma_0_done %d, dma_1_done %d\n",
		tmp.acx_trans_procs, tmp.decrypt_done, tmp.dma_0_done, tmp.dma_1_done);
	printk("tx_exch_complet %d, commands %d, acx_rx_procs %d\n",
		tmp.tx_exch_complet, tmp.commands, tmp.acx_rx_procs);
	printk("hw_pm_mode_changes %d, host_acks %d, pci_pm %d, acm_wakeups %d\n",
		tmp.hw_pm_mode_changes, tmp.host_acks, tmp.pci_pm, tmp.acm_wakeups);
	printk("wep_key_count %d, wep_default_key_count %d, dot11_def_key_mib %d\n",
		tmp.wep_key_count, tmp.wep_default_key_count, tmp.dot11_def_key_mib);
	printk("wep_key_not_found %d, wep_decrypt_fail %d\n",
		tmp.wep_key_not_found, tmp.wep_decrypt_fail);

	return 0;
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
static inline int acx100_ioctl_unknown11(wlandevice_t *hw)
{
	unsigned long flags;
	client_t client;

	if (acx100_lock(hw, &flags))
		return 0;
	transmit_disassoc(&client,hw);
	acx100_unlock(hw, &flags);

	return 0;
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
static inline int acx100_ioctl_wlansniff(wlandevice_t *hw, struct iwreq *iwr)
{
	int *parms = (int*) iwr->u.name;
	int enable = parms[0] > 0;

	hw->monitor = parms[0];

	switch (parms[0])
	{
	case 0:
		hw->netdev->type = ARPHRD_ETHER;
		break;
	case 1:
		hw->netdev->type = ARPHRD_IEEE80211_PRISM;
		break;
	case 2:
		hw->netdev->type = ARPHRD_IEEE80211;
		break;
	}

	if (hw->monitor)
		hw->monitor_setting = 0x02; /* don't decrypt default key only, override decryption mechanism */
	else
		hw->monitor_setting = 0x00; /* don't decrypt default key only, don't override decryption */

	hw->set_mask |= SET_RXCONFIG | SET_WEP_OPTIONS;

	if (enable)
	{
		hw->channel = parms[1];
		hw->set_mask |= GETSET_RX;
	}

	return 0;
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
static inline int acx100_ioctl_get_antenna(wlandevice_t *hw)
{
	printk("current antenna value: 0x%02X\n", hw->antenna);
	printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	printk("Tx antenna selection seems to be bit 5 (0x20)\n");

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
static inline int acx100_ioctl_set_antenna(wlandevice_t *hw, struct iwreq *iwr)
{
	UINT8 val = *( (UINT8 *) iwr->u.name );

	printk("current antenna value: 0x%02X\n", hw->antenna);
	printk("Rx antenna selection seems to be bit 6 (0x40)\n");
	printk("Tx antenna selection seems to be bit 5 (0x20)\n");
	hw->antenna = val;
	printk("new antenna value: 0x%02X\n", hw->antenna);
	hw->set_mask |= GETSET_ANTENNA;

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
static inline int acx100_ioctl_set_rx_antenna(wlandevice_t *hw, struct iwreq *iwr)
{
	UINT8 val = *( (UINT8 *) iwr->u.name );
	unsigned long flags;

	printk("current antenna value: 0x%02X\n", hw->antenna);
	/* better keep the separate operations atomic */
	acx100_lock(hw, &flags);
	hw->antenna &= 0x3f;
	hw->antenna |= (val << 6);
	hw->set_mask |= GETSET_ANTENNA;
	acx100_unlock(hw, &flags);
	printk("new antenna value: 0x%02X\n", hw->antenna);

	return 0;
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
static inline int acx100_ioctl_set_tx_antenna(wlandevice_t *hw, struct iwreq *iwr)
{
	UINT8 val = *( (UINT8 *) iwr->u.name );
	unsigned long flags;

	printk("current antenna value: 0x%02X\n", hw->antenna);
	/* better keep the separate operations atomic */
	acx100_lock(hw, &flags);
	hw->antenna &= 0xdf;
	hw->antenna |= ((val &= 0x01) << 5);
	hw->set_mask |= GETSET_ANTENNA;
	acx100_unlock(hw, &flags);
	printk("new antenna value: 0x%02X\n", hw->antenna);

	return 0;
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
static inline int acx100_ioctl_set_retry(wlandevice_t *hw, struct iwreq *iwr)
{
	char *val = (char *)iwr->u.name;

	printk("current short retry limit value: %ld, current long retry limit value: %ld\n", hw->short_retry, hw->long_retry);
	hw->short_retry = val[0];
	hw->long_retry = val[1];
	printk("new short retry limit value: %ld, new long retry limit value: %ld\n", hw->short_retry, hw->long_retry);

	hw->set_mask |= GETSET_RETRY;

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
static inline int acx100_ioctl_get_short_preamble(wlandevice_t *hw, struct iwreq *iwr)
{
	char *descr = NULL;

	switch(hw->preamble_mode) {
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

	*( (int *) iwr->u.name ) = hw->preamble_mode;

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
static inline int acx100_ioctl_set_short_preamble(wlandevice_t *hw, struct iwreq *iwr)
{
	int val = *( (int *) iwr->u.name );
	char *descr = NULL;

	if ((val < 0) || (val > 2))
		return -EINVAL;

	hw->preamble_mode = val;
	switch(val) {
		case 0:
			descr = "off";
			hw->preamble_flag = 0;
			break;
		case 1:
			descr = "on";
			hw->preamble_flag = 1;
			break;
		case 2:
			descr = "auto (peer capability dependent)";

			/* associated to a station? */
			if (hw->station_assoc.address[0] != 0x00)
				hw->preamble_flag = WLAN_GET_MGMT_CAP_INFO_SHORT(hw->station_assoc.cap);
			break;
	}
	printk("new Short Preamble setting: %s\n", descr);

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
static inline int acx100_ioctl_set_ed_threshold(wlandevice_t *hw, struct iwreq *iwr)
{
	int val = *( (int *) iwr->u.name );

	printk("current ED threshold value: 0x%02X\n", hw->ed_threshold);
	hw->ed_threshold = (unsigned char)val;
	printk("new ED threshold value: 0x%02X\n", (unsigned char)val);
	hw->set_mask |= GETSET_ED_THRESH;

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
static inline int acx100_ioctl_set_cca(wlandevice_t *hw, struct iwreq *iwr)
{
	int val = *( (int *) iwr->u.name );
	unsigned long flags;

	acx100_lock(hw, &flags);

	printk("current CCA value: 0x%02X\n", hw->cca);
	hw->cca = (unsigned char)val;
	printk("new CCA value: 0x%02X\n", (unsigned char)val);
	hw->set_mask |= GETSET_CCA;

	acx100_unlock(hw, &flags);

	return 0;
}

static unsigned char reg_domain_ids[] = {0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41};

static unsigned char *reg_domain_strings[] =
{ "FCC (USA)        (1-11)",
  "DOC/IC (Canada)  (1-11)",
	/* BTW: WLAN use in ETSI is regulated by
	 * ETSI standard EN 300 328-2 V1.1.2 */
  "ETSI (Europe)    (1-13)",
  "Spain           (10-11)",
  "France          (10-13)",
  "MKK (Japan)        (14)",
  "MKK1             (1-14)"
};

void acx100_set_reg_domain(wlandevice_t *wlandev, unsigned char reg_dom_id)
{
	int i;

	for (i = 0; i < 7; i++)
		if (reg_domain_ids[i] == reg_dom_id)
			break;

	if (i == 7)
	{
		acxlog(L_STD, "invalid regulatory domain specified, falling back to FCC (USA)!\n");
		i = 0;
	}
	acxlog(L_STD, "setting regulatory domain to %d (0x%x): %s\n", i+1, reg_domain_ids[i], reg_domain_strings[i]);
	wlandev->reg_dom_id = reg_domain_ids[i];
}

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
static inline int acx100_ioctl_list_reg_domain(wlandevice_t *hw)
{
	int i;

	printk("Domain/Country  Channels  Setting\n");
	for (i=0; i < 7; i++)
		printk("%s      %d\n", reg_domain_strings[i], i+1);
	return 0;
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
static inline int acx100_ioctl_get_reg_domain(wlandevice_t *hw, struct iwreq *iwr)
{
	int i;

	for (i=1; i <= 7; i++)
		if (reg_domain_ids[i-1] == hw->reg_dom_id)
		{
			acxlog(L_STD, "regulatory domain is currently set to %d (0x%x): %s\n", i, hw->reg_dom_id, reg_domain_strings[i-1]);
			*(iwr->u.name) = i;
			break;
		}

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
static inline int acx100_ioctl_set_reg_domain(wlandevice_t *hw, struct iwreq *iwr)
{
	unsigned char val = *(iwr->u.name);

	if ((val < 1) || (val > 7))
		return -EINVAL;

	hw->reg_dom_id = reg_domain_ids[val-1];
	hw->set_mask |= GETSET_REG_DOMAIN;

	return 0;
}

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
#ifdef DEBUG
static inline int acx100_ioctl_set_debug(wlandevice_t *hw, struct iwreq *iwr)
{
	int val = *( (int *) iwr->u.name );
	unsigned long flags;

	/* This is maybe a bit over the top, but lets keep it consistent */
	acx100_lock(hw, &flags);

	acxlog(L_STD, "setting debug to 0x%04X\n", val);
	debug = val;

	acx100_unlock(hw, &flags);
	return 0;

}
#endif

/*================================================================*/
/* Main function						  */
/*================================================================*/
/*----------------------------------------------------------------
* acx100_ioctl
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
int acx100_ioctl(netdevice_t * dev, struct ifreq *ifr, int cmd)
{
	wlandevice_t *hw;
	int mult, channel;
	int result;
	struct iwreq *iwr;

	hw = (wlandevice_t *) dev->priv;
	iwr = (struct iwreq *) ifr;
	result = 0;
	mult = 1;
	channel = -1;

	acxlog(L_IOCTL|L_STATE, "<acx100_ioctl: UNVERIFIED. NONV3.> cmd = 0x%04X\n",
	       cmd);

	/* This is the way it is done in the orinoco driver.
	 * Check to see if device is present.
	 */
	if (! netif_device_present(dev))
	{
		return -ENODEV;
	}

	switch (cmd) {
	case SIOCGIWNAME:
		/* get name == wireless protocol */
		acxlog(L_IOCTL, "Get Name => %s\n", shortversion);
		strcpy(iwr->u.name, shortversion);
		break;

	case SIOCSIWFREQ:
		/* set channel/frequency (Hz)
		   data can be frequency or channel :
		   0-1000 = channel
		   > 1000 = frequency in Hz */

		result = acx100_ioctl_set_freq(iwr, hw);
		break;

	case SIOCGIWFREQ:
		/* get channel/frequency (Hz) */

		iwr->u.freq.e = 0;
		iwr->u.freq.m = hw->channel;
		break;

	case SIOCSIWMODE:
		/* set operation mode */

		result = acx100_ioctl_set_mode(iwr, hw);
		break;

	case SIOCGIWMODE:
		acxlog(L_IOCTL, "Get Mode => %ld\n", hw->macmode);

		if (hw->iStatus != ISTATUS_4_ASSOCIATED)
		/* if (!hw->ifup) */
		{ /* card not up yet, so for now indicate the mode we want,
		     not the one we are in */
			if (hw->mode == 0)
				iwr->u.mode = IW_MODE_ADHOC;
			else if (hw->mode == 2)
				iwr->u.mode = IW_MODE_INFRA;
		}
		else
		{
			if (hw->macmode == WLAN_MACMODE_NONE /* 0 */ )
				iwr->u.mode = IW_MODE_ADHOC;
			else if (hw->macmode == WLAN_MACMODE_ESS_STA /* 2 */ )
				iwr->u.mode = IW_MODE_INFRA;
		}
		break;

	case SIOCGIWAP:
		/* get access point MAC address */

		acxlog(L_IOCTL, "Get MAC address\n");
		/* as seen in Aironet driver, airo.c */
		memcpy(iwr->u.ap_addr.sa_data, hw->bssid, WLAN_BSSID_LEN);
		iwr->u.ap_addr.sa_family = ARPHRD_ETHER;
		break;


	case SIOCGIWAPLIST:
		/* get list of access points in range */

		result = acx100_ioctl_get_waplist(iwr, hw);
		break;

	case SIOCSIWESSID:
		/* set ESSID (network name) */

		result = acx100_ioctl_set_essid(iwr, hw);
		break;


	case SIOCGIWESSID:
		/* get ESSID */

		acxlog(L_IOCTL, "Get ESSID => %s\n", hw->essid);

		iwr->u.essid.flags = hw->essid_active;
		if (hw->essid_active)
		{
			iwr->u.essid.length = strlen(hw->essid) + 1;
			copy_to_user(iwr->u.essid.pointer, hw->essid,
				     iwr->u.essid.length);
		}
		break;


	case SIOCSIWRATE:
		/* set default bit rate (bps) */

		result = acx100_ioctl_set_rate(iwr, hw);
		break;

	case SIOCGIWRATE:
		/* get default bit rate (bps) */
		
		result = acx100_ioctl_get_rate(iwr, hw);
		break;

	case SIOCSIWENCODE:
		/* set encoding token & mode */

		result = acx100_ioctl_set_encode(iwr, hw);
		break;

	case SIOCGIWENCODE:
		/* get encoding token & mode */

		result = acx100_ioctl_get_encode(iwr, hw);
		break;

	case SIOCGIWTXPOW:
		/* get tx power */

		result = acx100_ioctl_get_txpow(iwr, hw);
		break;

	case SIOCSIWTXPOW:
		/* set tx power */

		result = acx100_ioctl_set_txpow(iwr, hw);
		break;

	case SIOCGIWRANGE:
		/* Get range of parameters */

		result = acx100_ioctl_get_range(iwr, hw);
		break;

	case SIOCGIWPRIV:
		result = acx100_ioctl_get_iw_priv(iwr);
		break;

	case SIOCSIWNICKN:
		result = acx100_ioctl_set_nick(hw,&iwr->u.data);
		break;
	case SIOCGIWNICKN:
		result = acx100_ioctl_get_nick(hw,&iwr->u.data);
		break;

#ifdef DEBUG
	case SIOCIWFIRSTPRIV + 0x0:
		acx100_ioctl_set_debug(hw, iwr);
		break;
#endif
		
	case SIOCIWFIRSTPRIV + 0x1:
		acx100_ioctl_list_reg_domain(hw);
		break;
		
	case SIOCIWFIRSTPRIV + 0x2:
		acx100_ioctl_get_reg_domain(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x3:
		acx100_ioctl_set_reg_domain(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x4:
		acx100_ioctl_get_short_preamble(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x5:
		acx100_ioctl_set_short_preamble(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x6:
		acx100_ioctl_get_antenna(hw);
		break;
		
	case SIOCIWFIRSTPRIV + 0x7:
		acx100_ioctl_set_antenna(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x8:
		acx100_ioctl_set_retry(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0x9:
		acx100_ioctl_set_ed_threshold(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0xa:
		acx100_ioctl_set_cca(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0xb:	/* set sniff (monitor) mode */
		acxlog(L_IOCTL, "%s: IWPRIV monitor\n", dev->name);

		/* can only be done by admin */
		if (!capable(CAP_NET_ADMIN)) {
			result = -EPERM;
			break;
		}
		result = acx100_ioctl_wlansniff(hw, iwr);
		break;

	case SIOCIWFIRSTPRIV + 0xc:
		acx100_ioctl_get_fw_stats(hw);
		break;
		
	case SIOCIWFIRSTPRIV + 0xd:
		acx100_ioctl_set_rx_antenna(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0xe:
		acx100_ioctl_set_tx_antenna(hw, iwr);
		break;
		
	case SIOCIWFIRSTPRIV + 0xf:
		acx100_ioctl_unknown11(hw);
		break;
		
	default:
		acxlog(L_IOCTL, "wireless ioctl 0x%04X queried but not implemented yet!\n", cmd);
		result = -EOPNOTSUPP;
		break;
	}

	if (hw->open && hw->set_mask)
		acx100_update_card_settings(hw, 0, 0, 0);

#if THIS_LEADS_TO_CRASHES
	if (hw->mode != 2 && reinit == 1) {
		acx100_lock(hw, &flags);

		acxlog(L_STD, "ioctl reinit\n");
		if (!acx100_set_beacon_template(hw)) {
			acxlog(L_BINSTD,
			       "acx100_set_beacon_template returns error\n");
			result = -EFAULT;
		}

		if (!acx100_set_probe_response_template(hw)) {
			acxlog(L_BINSTD,
			       "acx100_set_probe_response_template returns error\n");
			result = -EFAULT;
		}

		acx_client_sta_list_init();

		acx100_unlock(hw, &flags);
	}
#endif

	return result;
}

