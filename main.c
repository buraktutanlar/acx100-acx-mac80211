#include "acx_debug.h"

#include <linux/etherdevice.h>
#include <net/mac80211.h>

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "mem.h"
#include "pci.h"
#include "cmd.h"
#include "ie.h"
#include "utils.h"
#include "cardsetting.h"
#include "tx.h"
#include "main.h"
#include "debug.h"

/*
 * General Mac-related Definitions
 * ==================================================
 */

/* We define rates without short-preamble support fo now */

static struct ieee80211_rate acx100_rates[] = {
	{ .bitrate = 10, .hw_value = RATE100_1, },
	{ .bitrate = 20, .hw_value = RATE100_2, },
	{ .bitrate = 55, .hw_value = RATE100_5, },
	{ .bitrate = 110, .hw_value = RATE100_11, },
	{ .bitrate = 220, .hw_value = RATE100_22, },
};

struct ieee80211_rate acx111_rates[] = {
	{ .bitrate = 10, .hw_value = RATE111_1, },
	{ .bitrate = 20, .hw_value = RATE111_2, },
	{ .bitrate = 55, .hw_value = RATE111_5, },
	{ .bitrate = 60, .hw_value = RATE111_6, },
	{ .bitrate = 90, .hw_value = RATE111_9, },
	{ .bitrate = 110, .hw_value = RATE111_11, },
	{ .bitrate = 120, .hw_value = RATE111_12, },
	{ .bitrate = 180, .hw_value = RATE111_18, },
	{ .bitrate = 240, .hw_value = RATE111_24, },
	{ .bitrate = 360, .hw_value = RATE111_36, },
	{ .bitrate = 480, .hw_value = RATE111_48, },
	{ .bitrate = 540, .hw_value = RATE111_54, },
};
const int acx111_rates_sizeof=ARRAY_SIZE(acx111_rates);

static struct ieee80211_channel channels[] = {
	{ .center_freq = 2412, .hw_value = 1, },
	{ .center_freq = 2417, .hw_value = 2, },
	{ .center_freq = 2422, .hw_value = 3, },
	{ .center_freq = 2427, .hw_value = 4, },
	{ .center_freq = 2432, .hw_value = 5, },
	{ .center_freq = 2437, .hw_value = 6, },
	{ .center_freq = 2442, .hw_value = 7, },
	{ .center_freq = 2447, .hw_value = 8, },
	{ .center_freq = 2452, .hw_value = 9, },
	{ .center_freq = 2457, .hw_value = 10, },
	{ .center_freq = 2462, .hw_value = 11, },
	{ .center_freq = 2467, .hw_value = 12, },
	{ .center_freq = 2472, .hw_value = 13, },
	{ .center_freq = 2484, .hw_value = 14, },
};

static struct ieee80211_supported_band acx100_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx100_rates,
	.n_bitrates	= ARRAY_SIZE(acx100_rates),
};

static struct ieee80211_supported_band acx111_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx111_rates,
	.n_bitrates	= ARRAY_SIZE(acx111_rates),
};

const u8 bitpos2genframe_txrate[] = {
	[0] = 10,		/*  1 Mbit/s */
	[1] = 20,		/*  2 Mbit/s */
	[2] = 55,		/*  5*5 Mbit/s */
	[3] = 0x0B,		/*  6 Mbit/s */
	[4] = 0x0F,		/*  9 Mbit/s */
	[5] = 110,		/* 11 Mbit/s */
	[6] = 0x0A,		/* 12 Mbit/s */
	[7] = 0x0E,		/* 18 Mbit/s */
	[8] = 220,		/* 22 Mbit/s */
	[9] = 0x09,		/* 24 Mbit/s */
	[10] = 0x0D,		/* 36 Mbit/s */
	[11] = 0x08,		/* 48 Mbit/s */
	[12] = 0x0C,		/* 54 Mbit/s */
	[13] = 10,		/*  1 Mbit/s, should never happen */
	[14] = 10,		/*  1 Mbit/s, should never happen */
	[15] = 10,		/*  1 Mbit/s, should never happen */
};

const u8 acx_bitpos2ratebyte[] = {
	DOT11RATEBYTE_1,
	DOT11RATEBYTE_2,
	DOT11RATEBYTE_5_5,
	DOT11RATEBYTE_6_G,
	DOT11RATEBYTE_9_G,
	DOT11RATEBYTE_11,
	DOT11RATEBYTE_12_G,
	DOT11RATEBYTE_18_G,
	DOT11RATEBYTE_22,
	DOT11RATEBYTE_24_G,
	DOT11RATEBYTE_36_G,
	DOT11RATEBYTE_48_G,
	DOT11RATEBYTE_54_G,
};

const u8 acx_bitpos2rate100[] = {
	[0] = RATE100_1,
	[1] = RATE100_2,
	[2] = RATE100_5,
	[3] = RATE100_2,	/* should not happen */
	[4] = RATE100_2,	/* should not happen */
	[5] = RATE100_11,
	[6] = RATE100_2,	/* should not happen */
	[7] = RATE100_2,	/* should not happen */
	[8] = RATE100_22,
	[9] = RATE100_2,	/* should not happen */
	[10] = RATE100_2,	/* should not happen */
	[11] = RATE100_2,	/* should not happen */
	[12] = RATE100_2,	/* should not happen */
	[13] = RATE100_2,	/* should not happen */
	[14] = RATE100_2,	/* should not happen */
	[15] = RATE100_2,	/* should not happen */
};
BUILD_BUG_DECL(Rates, ARRAY_SIZE(acx_bitpos2rate100)
		   != ARRAY_SIZE(bitpos2genframe_txrate));

int acx_setup_modes(acx_device_t *adev)
{
	int i;



	for (i=0; i<ARRAY_SIZE(channels); i++)
		channels[i].max_power = TX_CFG_MAX_DBM_POWER;

	if (IS_ACX100(adev)) {
		adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
			&acx100_band_2GHz;
	} else {
		if (IS_ACX111(adev))
			adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
				&acx111_band_2GHz;
		else {
			logf0(L_ANY, "Error: Unknown device");
			return -1;
		}
	}

	return 0;
}

/*
 * Task Handling
 * ==================================================
 */

static int acx_do_job_update_tim(acx_device_t *adev)
{
	int ret;
	struct sk_buff *beacon;
	u16 tim_offset;
	u16 tim_length;

#if CONFIG_ACX_MAC80211_VERSION > KERNEL_VERSION(2, 6, 32)
	beacon = ieee80211_beacon_get_tim(adev->ieee, adev->vif, &tim_offset,
			&tim_length);
#else
	beacon = ieee80211_beacon_get(adev->ieee, adev->vif);
	if (!beacon)
		goto out;

	tim_offset = acx_beacon_find_tim(beacon) - beacon->data;
out:
#endif
	if (!beacon) {
		logf0(L_ANY, "Error: beacon==NULL");
		return NOT_OK;
	}

	if (IS_ACX111(adev)) {
		ret = acx_set_tim_template(adev, beacon->data + tim_offset,
				beacon->len - tim_offset);
	}

	dev_kfree_skb(beacon);

	return (ret);
}


static int acx_recalib_radio(acx_device_t *adev)
{
	if (IS_ACX100(adev)) {
		logf0(L_INIT, "acx100: Doing radio re-calibration.\n");
		/* On ACX100, we need to recalibrate the radio
		 * by issuing a GETSET_TX|GETSET_RX */

		/* (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX,
		   NULL, 0)) && (OK == acx_s_issue_cmd(adev,
		   ACX1xx_CMD_DISABLE_RX, NULL, 0)) && */
		if ((acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX, &adev->channel,
						1) == OK)
			&& (acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
						&adev->channel, 1) == OK))
			return OK;

		return NOT_OK;
	} else {
		logf0(L_INIT, "acx111: Enabling auto radio re-calibration.\n");
		return(acx111_set_recalib_auto(adev, 1));
	}

}

static void acx_after_interrupt_recalib(acx_device_t *adev)
{
	int res;

	/* this helps with ACX100 at least; hopefully ACX111 also does
	 * a recalibration here */

	/* clear flag beforehand, since we want to make sure it's
	 * cleared; then only set it again on specific
	 * circumstances */
	CLEAR_BIT(adev->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* better wait a bit between recalibrations to prevent
	 * overheating due to torturing the card into working too long
	 * despite high temperature (just a safety measure) */
	if (adev->recalib_time_last_success
	    && time_before(jiffies, adev->recalib_time_last_success
			   + RECALIB_PAUSE * 60 * HZ)) {
		if (adev->recalib_msg_ratelimit <= 4) {
			logf1(L_ANY, "%s: less than " STRING(RECALIB_PAUSE)
			       " minutes since last radio recalibration, "
			       "not recalibrating (maybe the card is too hot?)\n",
			       wiphy_name(adev->ieee->wiphy));
			adev->recalib_msg_ratelimit++;
			if (adev->recalib_msg_ratelimit == 5)
				logf0(L_ANY, "disabling the above message until next recalib\n");
		}
		return;
	}

	adev->recalib_msg_ratelimit = 0;

	/* note that commands sometimes fail (card busy), so only
	 * clear flag if we were fully successful */
	res = acx_recalib_radio(adev);
	if (res == OK) {
		pr_info("%s: successfully recalibrated radio\n",
		       wiphy_name(adev->ieee->wiphy));
		adev->recalib_time_last_success = jiffies;
		adev->recalib_failure_count = 0;
	} else {
		/* failed: resubmit, but only limited amount of times
		 * within some time range to prevent endless loop */

		adev->recalib_time_last_success = 0;	/* we failed */

		/* if some time passed between last attempts, then
		 * reset failure retry counter to be able to do next
		 * recalib attempt */
		if (time_after
		    (jiffies, adev->recalib_time_last_attempt + 5 * HZ))
			adev->recalib_failure_count = 0;

		if (adev->recalib_failure_count < 5) {
			/* increment inside only, for speedup of
			 * outside path */
			adev->recalib_failure_count++;
			adev->recalib_time_last_attempt = jiffies;
			acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
	}
}



void acx_init_task_scheduler(acx_device_t *adev)
{
	/* configure task scheduler */
#if defined(CONFIG_ACX_MAC80211_PCI)
	if (IS_PCI(adev)) {
		pr_info("device IS_PCI\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_USB)
	if (IS_USB(adev)) {
		pr_info("device IS_USB\n");
		INIT_WORK(&adev->irq_work, acxusb_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_MEM)
	if (IS_MEM(adev)) {
		pr_info("device IS_MEM\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif

	logf0(L_ANY, "Unhandled adev device type!\n");
	BUG();

	/* OW TODO Interrupt handling ... */
	/* OW In case of of tasklet ... but workqueues seem to be prefered
	 tasklet_init(&adev->interrupt_tasklet,
	 (void(*)(unsigned long)) acx_interrupt_tasklet,
	 (unsigned long) adev);
	 */

}

void acx_after_interrupt_task(acx_device_t *adev)
{


	if (!adev->after_interrupt_jobs || !adev->initialized)
		return;	/* no jobs to do */

	/* we see lotsa tx errors */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_RADIO_RECALIB) {
		logf0(L_DEBUG, "Schedule CMD_RADIO_RECALIB\n");
		acx_after_interrupt_recalib(adev);
	}

	/* 1) we detected that no Scan_Complete IRQ came from fw, or
	 * 2) we found too many STAs */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_STOP_SCAN) {
		log(L_IRQ, "sending a stop scan cmd...\n");

		/* OW Scanning is done by mac80211 */
#if 0
		acx_unlock(adev, flags);
		acx_issue_cmd(adev, ACX1xx_CMD_STOP_SCAN, NULL, 0);
		acx_lock(adev, flags);
		/* HACK: set the IRQ bit, since we won't get a scan
		 * complete IRQ any more on ACX111 (works on ACX100!),
		 * since _we_, not a fw, have stopped the scan */
		SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
#endif
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_CMD_STOP_SCAN);
	}

	/* either fw sent Scan_Complete or we detected that no
	 * Scan_Complete IRQ came from fw. Finish scanning, pick join
	 * partner if any */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_COMPLETE_SCAN) {
		/* + scan kills current join status - restore it
		 *   (do we need it for STA?) */
		/* + does it happen only with active scans?
		 *   active and passive scans? ALL scans including
		 *   background one? */
		/* + was not verified that everything is restored
		 *   (but at least we start to emit beacons again) */
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_COMPLETE_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_RESTART_SCAN) {
		log(L_IRQ, "sending a start_scan cmd...\n");
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_RESTART_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_UPDATE_TIM) {
		log(L_IRQ, "ACX_AFTER_IRQ_UPDATE_TIM\n");
		acx_do_job_update_tim(adev);
		CLEAR_BIT(adev->after_interrupt_jobs,
			ACX_AFTER_IRQ_UPDATE_TIM);
	}

	/* others */
	if(adev->after_interrupt_jobs)
	{
		pr_info("Jobs still to be run: 0x%02X\n",
			adev->after_interrupt_jobs);
		adev->after_interrupt_jobs = 0;
	}

}

void acx_log_irq(u16 irqtype)
{
	pr_info("%s: got: ", __func__);

	if (irqtype & HOST_INT_RX_DATA)
		pr_info("Rx_Data,");

	if (irqtype & HOST_INT_TX_COMPLETE)
		pr_info("Tx_Complete,");

	if (irqtype & HOST_INT_TX_XFER)
		pr_info("Tx_Xfer,");

	if (irqtype & HOST_INT_RX_COMPLETE)
		pr_info("Rx_Complete,");

	if (irqtype & HOST_INT_DTIM)
		pr_info("DTIM,");

	if (irqtype & HOST_INT_BEACON)
		pr_info("Beacon,");

	if (irqtype & HOST_INT_TIMER)
		log(L_IRQ, "Timer,");

	if (irqtype & HOST_INT_KEY_NOT_FOUND)
		pr_info("Key_Not_Found,");

	if (irqtype & HOST_INT_IV_ICV_FAILURE)
		pr_info("IV_ICV_Failure (crypto),");

	if (irqtype & HOST_INT_CMD_COMPLETE)
		pr_info("Cmd_Complete,");

	if (irqtype & HOST_INT_INFO)
		pr_info("Info,");

	if (irqtype & HOST_INT_OVERFLOW)
		pr_info("Overflow,");

	if (irqtype & HOST_INT_PROCESS_ERROR)
		pr_info("Process_Error,");

	if (irqtype & HOST_INT_SCAN_COMPLETE)
		pr_info("Scan_Complete,");

	if (irqtype & HOST_INT_FCS_THRESHOLD)
		pr_info("FCS_Threshold,");

	if (irqtype & HOST_INT_UNKNOWN)
		pr_info("Unknown,");

	pr_info(": IRQ(s)\n");
}

/*
 * acx_schedule_task
 *
 * Schedule the call of the after_interrupt method after leaving
 * the interrupt context.
 */
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag)
{
	SET_BIT(adev->after_interrupt_jobs, set_flag);
	ieee80211_queue_work(adev->ieee, &adev->irq_work);
}

/*
* acx_i_timer
*/
void acx_timer(unsigned long address)
{
	/* acx_device_t *adev = (acx_device_t *) address; */



	FIXME();
	/* We need calibration and stats gather tasks to perform here */


}

/*
 * acx_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 */
void acx_set_timer(acx_device_t *adev, int timeout_us)
{


	log(L_DEBUG | L_IRQ, "(%u ms)\n", timeout_us / 1000);
	if (!(adev->dev_state_mask & ACX_STATE_IFACE_UP)) {
		pr_info("attempt to set the timer "
		       "when the card interface is not up!\n");
		return;
	}

	/* first check if the timer was already initialized, THEN modify it */
	if (adev->mgmt_timer.function) {
		mod_timer(&adev->mgmt_timer,
			  jiffies + (timeout_us * HZ / 1000000));
	}

}

void acx_start(acx_device_t *adev)
{


	log(L_INIT, "Updating initial settings\n");

	acx1xx_update_station_id(adev);

	acx1xx_update_rate_fallback(adev);
	acx1xx_update_tx_level(adev);
	acx1xx_update_antenna(adev);

	acx1xx_update_ed_threshold(adev);
	acx1xx_update_cca(adev);

	acx1xx_update_tx(adev);
	acx1xx_update_rx(adev);

	acx1xx_update_retry(adev);
	acx1xx_update_msdu_lifetime(adev);
	acx_update_reg_domain(adev);

	acx_update_mode(adev);

	/* For the acx100, we leave the firmware sensitivity and it
	   doesn't support auto recalib, so don't set it */
	if (IS_ACX111(adev)) {
		acx_update_sensitivity(adev);
		acx111_set_recalib_auto(adev, 1);
	}


}



/*
 * BOM Mac80211 Ops
 * ==================================================
 */

int acx_op_add_interface(struct ieee80211_hw *ieee, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(ieee);
	int err = -EOPNOTSUPP;

	u8 *mac_vif;
	char mac[MACSTR_SIZE];

	int vif_type;


	acx_sem_lock(adev);

	vif_type = vif->type;
	adev->vif_type = vif_type;
	log(L_ANY, "vif_type=%04X\n", vif_type);

	if (vif_type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor++;
	else if (adev->vif_operating)
		goto out_unlock;

	adev->vif_operating = 1;
	adev->vif = VIF_vif(vif);
	mac_vif = VIF_addr(vif);

	switch (adev->vif_type) {
	case NL80211_IFTYPE_AP:
		log(L_ANY, "NL80211_IFTYPE_AP\n");
		adev->mode = ACX_MODE_3_AP;
		break;

	case NL80211_IFTYPE_ADHOC:
		log(L_ANY, "NL80211_IFTYPE_ADHOC\n");
		adev->mode = ACX_MODE_0_ADHOC;
		break;

	case NL80211_IFTYPE_STATION:
		log(L_ANY, "NL80211_IFTYPE_STATION\n");
		adev->mode = ACX_MODE_2_STA;
		break;

	case NL80211_IFTYPE_MONITOR:
		logf0(L_ANY, "NL80211_IFTYPE_MONITOR\n");
		break;

	case NL80211_IFTYPE_WDS:
		logf0(L_ANY, "NL80211_IFTYPE_WDS: Not implemented\n");
		goto out_unlock;

	default:
		logf1(L_ANY, "Unknown adev->vif_type=%d\n", adev->vif_type);

		goto out_unlock;
		break;
	}

	/* Reconfigure mac-address globally, affecting all vifs */
	if (!mac_is_equal(mac_vif, adev->dev_addr)) {
		memcpy(adev->dev_addr, mac_vif, ETH_ALEN);
		memcpy(adev->bssid, mac_vif, ETH_ALEN);
		acx1xx_set_station_id(adev, mac_vif);
	}

	acx_update_mode(adev);

	logf0(L_ANY, "Redoing cmd_join_bssid() after add_interface\n");
	acx_cmd_join_bssid(adev, adev->bssid);

	pr_info("Virtual interface added (type: 0x%08X, MAC: %s)\n",
		adev->vif_type,	acx_print_mac(mac, mac_vif));

	err = 0;

out_unlock:
	acx_sem_unlock(adev);

	return err;
}

void acx_op_remove_interface(struct ieee80211_hw *hw, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(hw);

	char mac[MACSTR_SIZE];
	u8 *mac_vif;


	acx_sem_lock(adev);

	mac_vif = VIF_addr(vif);

	if (vif->type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor--;
	else {
		adev->vif_operating = 0;
		adev->vif = NULL;
	}

	acx_set_mode(adev, ACX_MODE_OFF);

	log(L_DEBUG, "vif_operating=%d, vif->type=%d\n",
		adev->vif_operating, vif->type);

	log(L_ANY, "Virtual interface removed: type=%d, MAC=%s\n",
		vif->type, acx_print_mac(mac, mac_vif));

	acx_sem_unlock(adev);

}

int acx_op_config(struct ieee80211_hw *hw, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);
	struct ieee80211_conf *conf = &hw->conf;

	u32 changed_not_done = changed;


	acx_sem_lock(adev);

	if (!adev->initialized)
		goto end_sem_unlock;

	logf1(L_DEBUG, "changed=%08X\n", changed);

	/* Tx-Power
	 * power_level: requested transmit power (in dBm)
	 */
	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_POWER: %d\n",
			conf->power_level);
		acx1xx_set_tx_level_dbm(adev, conf->power_level);
	}

	/* IEEE80211_CONF_CHANGE_CHANNEL */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		changed_not_done &= ~IEEE80211_CONF_CHANGE_CHANNEL;

		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_CHANNEL, "
			"channel->hw_value=%i\n", conf->channel->hw_value);

		if (conf->channel->hw_value == adev->channel)
			goto change_channel_done;

		acx_selectchannel(adev, conf->channel->hw_value,
				conf->channel->center_freq);
	}
change_channel_done:
	if (changed_not_done)
		logf1(L_DEBUG, "changed_not_done=%08X\n", changed_not_done);

end_sem_unlock:
	acx_sem_unlock(adev);

	return 0;
}

void acx_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);

	int err = -ENODEV;

	struct sk_buff *beacon;


	acx_sem_lock(adev);

	logf1(L_DEBUG, "changed=%04X\n", changed);

	if (!adev->vif_operating)
		goto end_sem_unlock;

	if (changed & BSS_CHANGED_BSSID) {
		MAC_COPY(adev->bssid, info->bssid);

		logf0(L_ANY, "Redoing join following bssid update\n");
		acx_cmd_join_bssid(adev, adev->bssid);
	}

	/* BOM BSS_CHANGED_BEACON */
	if (changed & BSS_CHANGED_BEACON) {

		/* TODO Use ieee80211_beacon_get_tim instead */
		beacon = ieee80211_beacon_get(hw, vif);
		if (!beacon) {
			logf0(L_ANY,
				"Error: BSS_CHANGED_BEACON: skb_tmp==NULL");
			return;
		}

		adev->beacon_interval = info->beacon_int;
		acx_set_beacon(adev, beacon);

		dev_kfree_skb(beacon);
	}
	err = 0;

end_sem_unlock:
	acx_sem_unlock(adev);

	return;
}

static int acx111_set_key_type(acx_device_t *adev, acx111WEPDefaultKey_t *key,
                               struct ieee80211_key_conf *mac80211_key,
                               const u8 *addr)
{
	switch (mac80211_key->cipher) {
#if 0
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_WEP_DEFAULT;
		else
			key->type = KEY_WEP_ADDR;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_TKIP_MIC_GROUP;
		else
			key->type = KEY_TKIP_MIC_PAIRWISE;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
#endif
	case WLAN_CIPHER_SUITE_CCMP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_AES_GROUP;
		else
			key->type = KEY_AES_PAIRWISE;

		break;
	default:
		log(L_INIT, "Unknown key cipher 0x%x", mac80211_key->cipher);
		return -EOPNOTSUPP;
	}

	return 0;
}


static int acx111_set_key(acx_device_t *adev, enum set_key_cmd cmd,
                          const u8 *addr, struct ieee80211_key_conf *key)
{
	int ret = -1;
	acx111WEPDefaultKey_t dk;

	memset(&dk, 0, sizeof(dk));

	switch (cmd) {
	case SET_KEY:
		dk.action = cpu_to_le16(KEY_ADD_OR_REPLACE);
		break;
	case DISABLE_KEY:
		dk.action = cpu_to_le16(KEY_REMOVE);
		break;
	default:
		log(L_INIT, "Unsupported key cmd 0x%x", cmd);
		break;
	}

	ret = acx111_set_key_type(adev, &dk, key, addr);
	if (ret < 0) {
		log(L_INIT, "Set KEY type failed");
		return ret;
	}

	memcpy(dk.MacAddr, addr, ETH_ALEN);

	dk.keySize = key->keylen;
	dk.defaultKeyNum = key->keyidx; /* ignored when setting default key */
	dk.index = 0;

	memcpy(dk.key, key->key, dk.keySize);

	ret = acx_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &dk, sizeof(dk));

	return ret;
}

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
                   struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                   struct ieee80211_key_conf *key)
{
	struct acx_device *adev = ieee2adev(hw);
	u8 algorithm;
	int ret=0;

	const u8 *addr;
	static const u8 bcast_addr[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	acx_sem_lock(adev);

	addr = sta ? sta->addr : bcast_addr;

	log(L_DEBUG, "cmd=%d\n", cmd);
	log(L_DEBUG, "addr=" MACSTR, MAC(addr));
	log(L_DEBUG, "key->: cipher=%08x, icv_len=%d, iv_len=%d, hw_key_idx=%d, "
			"flags=%02x, keyidx=%d, keylen=%d\n", key->cipher, key->icv_len,
	        key->iv_len, key->hw_key_idx, key->flags, key->keyidx,
	        key->keylen);
	if (acx_debug & L_DEBUG)
		hexdump("key->: key", key->key, key->keylen);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        switch (key->alg) {
#else
	switch (key->cipher) {
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_WEP:
                if (key->keylen == 5) {
                    algorithm = ACX_SEC_ALGO_WEP;
                    log(L_INIT, "algorithm=%i: %s\n",
			    algorithm, "ACX_SEC_ALGO_WEP");
                } else {
                    algorithm = ACX_SEC_ALGO_WEP104;
                    log(L_INIT, "algorithm=%i: %s\n", "ACX_SEC_ALGO_WEP104");
                }

		acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#else
	case WLAN_CIPHER_SUITE_WEP40:
	        algorithm = ACX_SEC_ALGO_WEP;
                log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_WEP");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;

        case WLAN_CIPHER_SUITE_WEP104:
                algorithm = ACX_SEC_ALGO_WEP104;
                log(L_INIT, "algorithm=%i: %s\n",
			algorithm, "ACX_SEC_ALGO_WEP104");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_TKIP:
#else
	case WLAN_CIPHER_SUITE_TKIP:
#endif
	        algorithm = ACX_SEC_ALGO_TKIP;
	        log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_TKIP");

	        acx_set_hw_encryption_off(adev);
	        ret = -EOPNOTSUPP;
	        break;

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_CCMP:
#else
	case WLAN_CIPHER_SUITE_CCMP:
#endif
		algorithm = ACX_SEC_ALGO_AES;
		log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_AES");

		acx_set_hw_encryption_on(adev);
		acx111_set_key(adev, cmd, addr, key);
		ret = 0;
		break;

	default:
		algorithm = ACX_SEC_ALGO_NONE;

		acx_set_hw_encryption_off(adev);
		ret = 0;
		break;
	}

	acx_sem_unlock(adev);
	return ret;
}

void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast)
{
	acx_device_t *adev = ieee2adev(hw);



	acx_sem_lock(adev);

	logf1(L_DEBUG, "1: changed_flags=0x%08x, *total_flags=0x%08x\n",
		changed_flags, *total_flags);

	/* OWI TODO: Set also FIF_PROBE_REQ ? */
	*total_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);

	logf1(L_DEBUG, "2: *total_flags=0x%08x\n", *total_flags);

	acx_sem_unlock(adev);

}

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params)
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params)
#endif
{
	acx_device_t *adev = ieee2adev(hw);

	acx_sem_lock(adev);
    /* TODO */
	acx_sem_unlock(adev);

	return 0;
}

int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set)
{
	acx_device_t *adev = ieee2adev(hw);

	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_TIM);

	return 0;
}


int acx_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);


	acx_sem_lock(adev);

	memcpy(stats, &adev->ieee_stats, sizeof(*stats));

	acx_sem_unlock(adev);

	return 0;
}


#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			 struct ieee80211_tx_queue_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);
	int err = -ENODEV;


	acx_sem_lock(adev);

	stats->len = 0;
	stats->limit = TX_CNT;
	stats->count = 0;

	acx_sem_unlock(adev);

	return err;
}
#endif



/* TODO: consider defining OP_TX_RET_TYPE, OP_TX_RET_VAL in
 * acx_compat, and hiding this #if/else.  OTOH, inclusion doesnt care
 * about old kernels
 */
OP_TX_RET_TYPE acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	acx_device_t *adev = ieee2adev(hw);

	skb_queue_tail(&adev->tx_queue, skb);

	ieee80211_queue_work(adev->ieee, &adev->tx_work);

	if (skb_queue_len(&adev->tx_queue) >= ACX_TX_QUEUE_MAX_LENGTH)
		acx_stop_queue(adev->ieee, NULL);

	return OP_TX_RET_OK;
}

