/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/nl80211.h>
#include <linux/ieee80211.h>

#include <net/mac80211.h>

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "mem.h"
#include "pci.h"

/*
 * BOM Config
 * ==================================================
 */

/*
 * BOM Prototypes
 * ... static and also none-static for overview reasons (maybe not best practice ...)
 * ==================================================
 */

/* Locking */
#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
void acx_lock_unhold(void);
void acx_sem_unhold(void);
void acx_lock_debug(acx_device_t *adev, const char *where);
void acx_unlock_debug(acx_device_t *adev, const char *where);
static inline const char *acx_sanitize_str(const char *s);
#endif

/* Logging */
void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);
char *acx_print_mac(char *buf, const u8 *mac);
void acx_print_mac2(const char *head, const u8 *mac, const char *tail);
void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail);
void acx_dump_bytes(const void *data, int num);
const char *acx_cmd_status_str(unsigned int state);

/* Data Access */
static int acx100_init_memory_pools(acx_device_t *adev, const acx_ie_memmap_t * mmt);
static int acx100_create_dma_regions(acx_device_t *adev);
static int acx111_create_dma_regions(acx_device_t *adev);

/* Firmware, EEPROM, Phy */
MODULE_FIRMWARE("tiacx111");
MODULE_FIRMWARE("tiacx111c16");
MODULE_FIRMWARE("tiacx111r16");

void acx_get_firmware_version(acx_device_t *adev);
void acx_display_hardware_details(acx_device_t *adev);
firmware_image_t *acx_read_fw(struct device *dev, const char *file, u32 * size);
void acx_parse_configoption(acx_device_t *adev, const acx111_ie_configoption_t * pcfg);
int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

/* CMDs (Control Path) */
int acx_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
int acx_configure_debug(acx_device_t *adev, void *pdr, int type, const char *typestr);
static int acx111_get_feature_config(acx_device_t *adev, u32 * feature_options, u32 * data_flow_options);
static int acx111_set_feature_config(acx_device_t *adev, u32 feature_options, u32 data_flow_options, unsigned int mode);
static inline int acx111_feature_off(acx_device_t *adev, u32 f, u32 d);
static inline int acx111_feature_on(acx_device_t *adev, u32 f, u32 d);
static inline int acx111_feature_set(acx_device_t *adev, u32 f, u32 d);
int acx_interrogate_debug(acx_device_t *adev, void *pdr, int type, const char *typestr);
static inline unsigned int acx_rate111to5bits(unsigned int rate);
int acx_cmd_join_bssid(acx_device_t *adev, const u8 *bssid);

/* Configuration (Control Path) */
void acx_set_defaults(acx_device_t *adev);
void acx_update_card_settings(acx_device_t *adev);
void acx_start(acx_device_t *adev);
int acx_net_reset(struct ieee80211_hw *ieee);
int acx_init_mac(acx_device_t *adev);
int acx_setup_modes(acx_device_t *adev);

static int acx_set_mode(acx_device_t *adev, u16 mode);
static int acx_update_mode(acx_device_t *adev);

int acx_selectchannel(acx_device_t *adev, u8 channel, int freq);

static void acx_get_sensitivity(acx_device_t *adev);
static void acx_set_sensitivity(acx_device_t *adev, u8 sensitivity);
static void acx_update_sensitivity(acx_device_t *adev);
static void acx_get_reg_domain(acx_device_t *adev);
static void acx_set_reg_domain(acx_device_t *adev, u8 domain_id);
static void acx_update_reg_domain(acx_device_t *adev);

static int acx1xx_set_tx_level_dbm(acx_device_t *adev, int level_dbm);
static int acx1xx_update_tx_level_dbm(acx_device_t *adev);
static int acx1xx_get_tx_level(acx_device_t *adev);
static int acx1xx_set_tx_level(acx_device_t *adev, u8 level_val);
static int acx1xx_update_tx_level(acx_device_t *adev);

static int acx1xx_get_antenna(acx_device_t *adev);
static int acx1xx_set_antenna(acx_device_t *adev, u8 val0, u8 val1);
static int acx1xx_update_antenna(acx_device_t *adev);
#if 0
static int acx100_set_rx_antenna(acx_device_t *adev, u8 val);
static int acx100_set_tx_antenna(acx_device_t *adev, u8 val);
#endif

static int acx1xx_update_station_id(acx_device_t *adev);
static int acx1xx_set_station_id(acx_device_t *adev, u8 *new_addr);
static int acx1xx_get_station_id(acx_device_t *adev);

static int acx1xx_get_ed_threshold(acx_device_t *adev);
static int acx100_get_ed_threshold(acx_device_t *adev);
static int acx100_update_ed_threshold(acx_device_t *adev);
static int acx1xx_update_ed_threshold(acx_device_t *adev);
#ifdef UNUSED
static int acx1xx_set_ed_threshold(acx_device_t *adev, u8 ed_threshold);
#endif

static int acx1xx_get_cca(acx_device_t *adev);
static int acx100_get_cca(acx_device_t *adev);
static int acx1xx_update_cca(acx_device_t *adev);
static int acx100_update_cca(acx_device_t *adev);
#ifdef UNUSED
static int acx1xx_set_cca(acx_device_t *adev, u8 cca);
#endif

static int acx1xx_update_rate_fallback(acx_device_t *adev);
#ifdef UNUSED
static int acx1xx_get_rate_fallback(acx_device_t *adev);
static int acx1xx_set_rate_fallback(acx_device_t *adev, u8 rate_auto);
#endif

static int acx1xx_update_tx(acx_device_t *adev);
static int acx1xx_set_tx_enable(acx_device_t *adev, u8 tx_enabled);
#ifdef UNUSED
static int acx1xx_set_channel(acx_device_t *adev, u8 channel);
#endif

static int acx1xx_set_rx_enable(acx_device_t *adev, u8 rx_enabled);
static int acx1xx_update_rx(acx_device_t *adev);
static int acx_update_rx_config(acx_device_t *adev);

static int acx1xx_update_retry(acx_device_t *adev);
static int acx1xx_update_msdu_lifetime(acx_device_t *adev);

#ifdef UNUSED
static int acx_update_wep(acx_device_t *adev);
static int acx_update_wep_options(acx_device_t *adev);
static int acx100_update_wep_options(acx_device_t *adev);
#endif

/* Templates */
static int acx_set_beacon(acx_device_t *adev, struct sk_buff *beacon);
static int acx_set_beacon_template(acx_device_t *adev, u8 *data, int len);
static int acx_set_tim_template(acx_device_t *adev, u8 *data, int len);
static int acx_set_probe_response_template(acx_device_t *adev, u8* data, int len);
static u8* acx_beacon_find_tim(struct sk_buff *beacon_skb);

static int acx_init_max_template_generic(acx_device_t *adev, unsigned int len, unsigned int cmd);
static int acx_init_packet_templates(acx_device_t *adev);
static int acx_init_max_null_data_template(acx_device_t *adev);
static int acx_init_max_beacon_template(acx_device_t *adev);
static int acx_init_max_tim_template(acx_device_t *adev);
static int acx_init_max_probe_response_template(acx_device_t *adev);
static int acx_init_max_probe_request_template(acx_device_t *adev);

#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_probe_request_template(acx_device_t *adev);
static int acx_s_set_probe_response_template_off(acx_device_t *adev);
static int acx_s_set_tim_template_off(acx_device_t *adev);
#endif

#if POWER_SAVE_80211
static int acx_s_set_null_data_template(acx_device_t *adev);
#endif

/* Recalibration (Control Path) */
static int acx111_set_recalib_auto(acx_device_t *adev, int enable);
static int acx111_update_recalib_auto(acx_device_t *adev);
static int acx_recalib_radio(acx_device_t *adev);
static void acx_after_interrupt_recalib(acx_device_t *adev);

/* Other (Control Path) */
#if 0
static u8 acx_plcp_get_bitrate_cck(u8 plcp);
static u8 acx_plcp_get_bitrate_ofdm(u8 plcp);
#endif
static void acx_set_sane_reg_domain(acx_device_t *adev, int do_set);
static void acx111_sens_radio_16_17(acx_device_t *adev);
static void acx_update_ratevector(acx_device_t *adev);

#if POWER_SAVE_80211
static void acx_s_update_80211_powersave_mode(acx_device_t *adev)
#endif

/* Proc, Debug */
#ifdef CONFIG_PROC_FS
static int acx_proc_show_diag(struct seq_file *file, void *v);
static ssize_t acx_proc_write_diag(struct file *file,
				const char __user *buf, size_t count,
				loff_t *ppos);
static int acx_proc_show_acx(struct seq_file *file, void *v);
static int acx_proc_show_eeprom(struct seq_file *file, void *v);
static int acx_proc_show_phy(struct seq_file *file, void *v);
static int acx_proc_show_debug(struct seq_file *file, void *v);
static ssize_t acx_proc_write_debug(struct file *file,
				const char __user *buf, size_t count,
				loff_t *ppos);
static int acx_proc_show_sensitivity(struct seq_file *file, void *v);
static ssize_t acx_proc_write_sensitivity(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos);
static int acx_proc_show_tx_level(struct seq_file *file, void *v);
static ssize_t acx111_proc_write_tx_level(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos);
static int acx_proc_show_reg_domain(struct seq_file *file, void *v);
static ssize_t acx_proc_write_reg_domain(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos);
static int acx_proc_show_antenna(struct seq_file *file, void *v);
static ssize_t acx_proc_write_antenna(struct file *file,
				const char __user *buf, size_t count,
				loff_t *ppos);
/*
  obsoleted by debugfs.c
static int acx_proc_open(struct inode *inode, struct file *file);
static void acx_proc_init(void);
int acx_proc_register_entries(struct ieee80211_hw *ieee);
int acx_proc_unregister_entries(struct ieee80211_hw *ieee);
*/
#endif

/* Rx Path */
void acx_process_rxbuf(acx_device_t *adev, rxbuffer_t * rxbuf);
static void acx_rx(acx_device_t *adev, rxbuffer_t *rxbuf);

/* Tx Path */
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
int acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#else
void acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#endif

void acx_tx_work(struct work_struct *work);
void acx_tx_queue_go(acx_device_t *adev);
int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb);
void acx_tx_queue_flush(acx_device_t *adev);
void acx_stop_queue(struct ieee80211_hw *hw, const char *msg);
int acx_queue_stopped(struct ieee80211_hw *ieee);
void acx_wake_queue(struct ieee80211_hw *hw, const char *msg);
tx_t *acx_alloc_tx(acx_device_t *adev, unsigned int len);
static void acx_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);
static void *acx_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
static void acx_tx_data(acx_device_t *adev, tx_t *tx_opaque,
			int len, struct ieee80211_tx_info *ieeectl,
			struct sk_buff *skb);
void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error,
			unsigned int finger,
			struct ieee80211_tx_info *info);
u16 acx111_tx_build_rateset(acx_device_t *adev, txdesc_t *txdesc,
			struct ieee80211_tx_info *info);
void acx111_tx_build_txstatus(acx_device_t *adev,
			struct ieee80211_tx_info *txstatus, u16 r111,
			u8 ack_failures);
u16 acx_rate111_hwvalue_to_bitrate(u16 hw_value);
int acx_rate111_hwvalue_to_rateindex(u16 hw_value);

/* Crypto */
#ifdef UNUSED
static void acx100_set_wepkey(acx_device_t * adev);
static void acx111_set_wepkey(acx_device_t * adev);
static void acx_set_wepkey(acx_device_t * adev);
#endif
static int acx100_init_wep(acx_device_t * adev);

/* OW, 20100704, Obselete, TBC for cleanup */
#if 0
static void acx_keymac_write(acx_device_t * adev, u16 index, const u32 * addr);
int acx_clear_keys(acx_device_t * adev);
int acx_key_write(acx_device_t *adev, u16 index, u8 algorithm, const struct ieee80211_key_conf *key, const u8 *mac_addr);
#endif

/* Irq Handling, Timer */
void acx_init_task_scheduler(acx_device_t *adev);
void acx_after_interrupt_task(acx_device_t *adev);
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);
void acx_log_irq(u16 irqtype);
void acx_timer(unsigned long address);
void acx_set_timer(acx_device_t *adev, int timeout_us);

/* Mac80211 Ops */
int acx_op_config(struct ieee80211_hw *hw, u32 changed);
void acx_op_bss_info_changed(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed);
int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key);
void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast);

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params);
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params);
#endif

int acx_op_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			struct ieee80211_tx_queue_stats *stats);
#endif

int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
		bool set);
static int acx_do_job_update_tim(acx_device_t *adev);

/* Helpers */
void acx_mwait(int ms);
static u8 acx_signal_to_winlevel(u8 rawlevel);
static u8 acx_signal_to_winlevel(u8 rawlevel);
u8 acx_signal_determine_quality(u8 signal, u8 noise);
const char *acx_get_packet_type_string(u16 fc);
/* void great_inquisitor(acx_device_t * adev); */

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);


/*
 * BOM Defines, static vars, etc.
 * ==================================================
 */
#define ACX111_PERCENT(percent) ((percent)/5)

/* Probably a number of acx's intermediate buffers for USB transfers,
 * not to be confused with number of descriptors in tx/rx rings
 * (which are not directly accessible to host in USB devices)
 */
#define USB_RX_CNT 10
#define USB_TX_CNT 10

/* minutes to wait until next radio recalibration: */
#define RECALIB_PAUSE	5

/* Maximum tx_power assumed from ieee80211_ops->config / IEEE80211_CONF_CHANGE_POWER */
#define TX_CFG_MAX_DBM_POWER	20

/* Please keep acx_reg_domain_ids_len in sync... */
const u8 acx_reg_domain_ids[acx_reg_domain_ids_len] =
    { 0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51 };
static const u16 reg_domain_channel_masks[acx_reg_domain_ids_len] =
    { 0x07ff, 0x07ff, 0x1fff, 0x0600, 0x1e00, 0x2000, 0x3fff, 0x01fc };

/* in crda ?? */
const char *const
acx_reg_domain_strings[] = {
	[0] = " 1-11 FCC (USA)",
	[1] = " 1-11 DOC/IC (Canada)",
	/* BTW: WLAN use in ETSI is regulated by ETSI standard EN 300 328-2 V1.1.2 */
	[2] = " 1-13 ETSI (Europe)",
	[3] = "10-11 Spain",
	[4] = "10-13 France",
	[5] = "   14 MKK (Japan)",
	[6] = " 1-14 MKK1",
	[7] = "  3-9 Israel (not all firmware versions)",
	[8] = NULL,	/* needs to remain as last entry */
};

/* FIXME: the lengths given here probably aren't always correct.
 * They should be gradually replaced by proper "sizeof(acx1XX_ie_XXXX)-4",
 * unless the firmware actually expects a different length than the struct length */
static const u16 acx100_ie_len[] = {
	0,
	ACX100_IE_ACX_TIMER_LEN,
	sizeof(acx100_ie_powersave_t) - 4,	/* is that 6 or 8??? */
	ACX1xx_IE_QUEUE_CONFIG_LEN,
	ACX100_IE_BLOCK_SIZE_LEN,
	ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_RATE_FALLBACK_LEN,
	ACX100_IE_WEP_OPTIONS_LEN,
	ACX1xx_IE_MEMORY_MAP_LEN,	/*    ACX1xx_IE_SSID_LEN, */
	0,
	ACX1xx_IE_ASSOC_ID_LEN,
	0,
	ACX111_IE_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_FWREV_LEN,
	ACX1xx_IE_FCS_ERROR_COUNT_LEN,
	ACX1xx_IE_MEDIUM_USAGE_LEN,
	ACX1xx_IE_RXCONFIG_LEN,
	0,
	0,
	sizeof(fw_stats_t) - 4,
	0,
	ACX1xx_IE_FEATURE_CONFIG_LEN,
	ACX111_IE_KEY_CHOOSE_LEN,
	ACX1FF_IE_MISC_CONFIG_TABLE_LEN,
	ACX1FF_IE_WONE_CONFIG_LEN,
	0,
	ACX1FF_IE_TID_CONFIG_LEN,
	0,
	0,
	0,
	ACX1FF_IE_CALIB_ASSESSMENT_LEN,
	ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN,
	ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN,
	ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN,
	0,
	ACX1FF_IE_PACKET_DETECT_THRESH_LEN,
	ACX1FF_IE_TX_CONFIG_OPTIONS_LEN,
	ACX1FF_IE_CCA_THRESHOLD_LEN,
	ACX1FF_IE_EVENT_MASK_LEN,
	ACX1FF_IE_DTIM_PERIOD_LEN,
	0,
	ACX1FF_IE_ACI_CONFIG_SET_LEN,
	0,
	0,
	0,
	0,
	0,
	0,
	ACX1FF_IE_EEPROM_VER_LEN,
};

static const u16 acx100_ie_len_dot11[] = {
	0,
	ACX1xx_IE_DOT11_STATION_ID_LEN,
	0,
	ACX100_IE_DOT11_BEACON_PERIOD_LEN,
	ACX1xx_IE_DOT11_DTIM_PERIOD_LEN,
	ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN,
	ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN,
	ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_IE_DOT11_ED_THRESHOLD_LEN,
	ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0,
};

static const u16 acx111_ie_len[] = {
	0,
	ACX100_IE_ACX_TIMER_LEN,
	sizeof(acx111_ie_powersave_t) - 4,
	ACX1xx_IE_QUEUE_CONFIG_LEN,
	ACX100_IE_BLOCK_SIZE_LEN,
	ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_RATE_FALLBACK_LEN,
	ACX100_IE_WEP_OPTIONS_LEN,
	ACX1xx_IE_MEMORY_MAP_LEN,	/*    ACX1xx_IE_SSID_LEN, */
	0,
	ACX1xx_IE_ASSOC_ID_LEN,
	0,
	ACX111_IE_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_FWREV_LEN,
	ACX1xx_IE_FCS_ERROR_COUNT_LEN,
	ACX1xx_IE_MEDIUM_USAGE_LEN,
	ACX1xx_IE_RXCONFIG_LEN,
	0,
	0,
	sizeof(fw_stats_t) - 4,
	0,
	ACX1xx_IE_FEATURE_CONFIG_LEN,
	ACX111_IE_KEY_CHOOSE_LEN,
	ACX1FF_IE_MISC_CONFIG_TABLE_LEN,
	ACX1FF_IE_WONE_CONFIG_LEN,
	0,
	ACX1FF_IE_TID_CONFIG_LEN,
	0,
	0,
	0,
	ACX1FF_IE_CALIB_ASSESSMENT_LEN,
	ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN,
	ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN,
	ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN,
	0,
	ACX1FF_IE_PACKET_DETECT_THRESH_LEN,
	ACX1FF_IE_TX_CONFIG_OPTIONS_LEN,
	ACX1FF_IE_CCA_THRESHOLD_LEN,
	ACX1FF_IE_EVENT_MASK_LEN,
	ACX1FF_IE_DTIM_PERIOD_LEN,
	0,
	ACX1FF_IE_ACI_CONFIG_SET_LEN,
	0,
	0,
	0,
	0,
	0,
	0,
	ACX1FF_IE_EEPROM_VER_LEN,
};
BUILD_BUG_DECL(acx111_ie_len__VS__acx100_ie_len,
	ARRAY_SIZE(acx111_ie_len) != ARRAY_SIZE(acx100_ie_len));

static const u16 acx111_ie_len_dot11[] = {
	0,
	ACX1xx_IE_DOT11_STATION_ID_LEN,
	0,
	ACX100_IE_DOT11_BEACON_PERIOD_LEN,
	ACX1xx_IE_DOT11_DTIM_PERIOD_LEN,
	ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN,
	ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN,
	ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_IE_DOT11_ED_THRESHOLD_LEN,
	ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0,
};
BUILD_BUG_DECL(acx111_ie_len_dot11__VS__acx100_ie_len_dot11,
	ARRAY_SIZE(acx111_ie_len_dot11) != ARRAY_SIZE(acx100_ie_len_dot11));

/* BOM Rate and channel definition
 * ---
 */

/* We define rates without short-preamble support fo now */

static struct ieee80211_rate acx100_rates[] = {
	{ .bitrate = 10, .hw_value = RATE100_1, },
	{ .bitrate = 20, .hw_value = RATE100_2, },
	{ .bitrate = 55, .hw_value = RATE100_5, },
	{ .bitrate = 110, .hw_value = RATE100_11, },
	{ .bitrate = 220, .hw_value = RATE100_22, },
};

static struct ieee80211_rate acx111_rates[] = {
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

static const u8 bitpos2genframe_txrate[] = {
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

int acx_rate111_hwvalue_to_rateindex(u16 hw_value)
{
	int i, r=-1;

	for (i = 0; i < ARRAY_SIZE(acx111_rates); i++) {
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

/* Proc */
#ifdef CONFIG_PROC_FS

static const char *const proc_files[] = {
	"info", "diag", "eeprom", "phy", "debug",
	"sensitivity", "tx_level", "antenna", "reg_domain",
};

typedef int acx_proc_show_t(struct seq_file *file, void *v);
typedef ssize_t (acx_proc_write_t)(struct file *, const char __user *, size_t, loff_t *);

acx_proc_show_t *const acx_proc_show_funcs[] = {
	acx_proc_show_acx,
	acx_proc_show_diag,
	acx_proc_show_eeprom,
	acx_proc_show_phy,
	acx_proc_show_debug,
	acx_proc_show_sensitivity,
	acx_proc_show_tx_level,
	acx_proc_show_antenna,
	acx_proc_show_reg_domain,
};
BUILD_BUG_DECL(proc_files__VS__acx_proc_show_funcs,
	ARRAY_SIZE(proc_files) != ARRAY_SIZE(acx_proc_show_funcs));

acx_proc_write_t *const acx_proc_write_funcs[] = {
	NULL,
	acx_proc_write_diag,
	NULL,
	NULL,
	acx_proc_write_debug,
	acx_proc_write_sensitivity,
	acx111_proc_write_tx_level,
	acx_proc_write_antenna,
	acx_proc_write_reg_domain,
};
BUILD_BUG_DECL(SHOW, ARRAY_SIZE(acx_proc_show_funcs)
		  != ARRAY_SIZE(acx_proc_write_funcs));

#ifndef CONFIG_DEBUG_FS
/* obsoleted */
static struct file_operations acx_e_proc_ops[ARRAY_SIZE(proc_files)];
#endif

#endif /* CONFIG_PROC_FS */

/*
 * BOM Locking
 * ==================================================
 */
#define DEBUG_TSC 0
#if DEBUG_TSC && defined(CONFIG_X86)
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
#ifdef PARANOID_LOCKING
static unsigned max_lock_time;
static unsigned max_sem_time;

/* Obvious or linux kernel specific derived code follows: */

void acx_lock_unhold()
{
	max_lock_time = 0;
}

void acx_sem_unhold()
{
	max_sem_time = 0;
}

static inline const char *acx_sanitize_str(const char *s)
{
	const char *t = strrchr(s, '/');
	if (t)
		return t + 1;
	return s;
}

void acx_lock_debug(acx_device_t * adev, const char *where)
{
	unsigned int count = 100 * 1000 * 1000;
	TIMESTAMP(lock_start);
	where = acx_sanitize_str(where);
	while (--count) {
		if (!spin_is_locked(&adev->spinlock))
			break;
		cpu_relax();
	}
	if (!count) {
		pr_emerg("LOCKUP: already taken at %s!\n",
		       adev->last_lock);
		BUG();
	}
	adev->last_lock = where;
	adev->lock_time = lock_start;
}

void acx_unlock_debug(acx_device_t * adev, const char *where)
{
#ifdef SMP
	if (!spin_is_locked(&adev->spinlock)) {
		where = acx_sanitize_str(where);
		pr_emerg("STRAY UNLOCK at %s!\n", where);
		BUG();
	}
#endif
	if (acx_debug & L_LOCK) {
		TIMESTAMP(diff);
		diff -= adev->lock_time;
		if (diff > max_lock_time) {
			where = acx_sanitize_str(where);
			pr_notice("max lock hold time %ld CPU ticks from %s "
			       "to %s\n", diff, adev->last_lock, where);
			max_lock_time = diff;
		}
	}
}
#endif /* PARANOID_LOCKING */
#endif

/*
 * BOM Logging
 * ==================================================
 */

/* ----- */
#if ACX_DEBUG > 1

static int acx_debug_func_indent;
#define ACX_DEBUG_FUNC_INDENT_INCREMENT 2
static const char acx_debug_spaces[] = "          " "          ";	/* Nx10 spaces */

void log_fn_enter(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s==> %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);

	acx_debug_func_indent += ACX_DEBUG_FUNC_INDENT_INCREMENT;
}

void log_fn_exit(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	/* OW Handle underflow */
	if (acx_debug_func_indent >= ACX_DEBUG_FUNC_INDENT_INCREMENT)
		acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;
	else
		acx_debug_func_indent = 0;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s<== %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);
}

void log_fn_exit_v(const char *funcname, int v)
{
	int indent;
	TIMESTAMP(d);

	acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s<== %s: %08X\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname, v);
}
#endif /* ACX_DEBUG > 1 */

char* acx_print_mac(char *buf, const u8 *mac)
{
	sprintf(buf, MACSTR, MAC(mac));
	return(buf);
}

void acx_print_mac2(const char *head, const u8 *mac, const char *tail)
{
	pr_info("%s" MACSTR "%s", head, MAC(mac), tail);
}

void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail)
{
	if (acx_debug & level)
		acx_print_mac2(head, mac, tail);
}

void acx_dump_bytes(const void *data, int num)
{
	const u8 *ptr = (const u8 *)data;

	FN_ENTER;

	if (num <= 0) {
		printk("\n");
		return;
	}

	while (num >= 16) {
		printk("%02X %02X %02X %02X %02X %02X %02X %02X "
		       "%02X %02X %02X %02X %02X %02X %02X %02X\n",
		       ptr[0], ptr[1], ptr[2], ptr[3],
		       ptr[4], ptr[5], ptr[6], ptr[7],
		       ptr[8], ptr[9], ptr[10], ptr[11],
		       ptr[12], ptr[13], ptr[14], ptr[15]);
		num -= 16;
		ptr += 16;
	}
	if (num > 0) {
		while (--num > 0)
			printk("%02X ", *ptr++);
		printk("%02X\n", *ptr);
	}

	FN_EXIT0;

}

const char *acx_cmd_status_str(unsigned int state)
{
	static const char *const cmd_error_strings[] = {
		"Idle",
		"Success",
		"Unknown Command",
		"Invalid Information Element",
		"Channel rejected",
		"Channel invalid in current regulatory domain",
		"MAC invalid",
		"Command rejected (read-only information element)",
		"Command rejected",
		"Already asleep",
		"TX in progress",
		"Already awake",
		"Write only",
		"RX in progress",
		"Invalid parameter",
		"Scan in progress",
		"Failed"
	};
	return state < ARRAY_SIZE(cmd_error_strings) ?
	    cmd_error_strings[state] : "?";
}

/*
 * BOM Data Access
 * ==================================================
 */

static int acx100_init_memory_pools(acx_device_t *adev,
				const acx_ie_memmap_t *mmt)
{
	acx100_ie_memblocksize_t MemoryBlockSize;
	acx100_ie_memconfigoption_t MemoryConfigOption;
	int TotalMemoryBlocks;
	int RxBlockNum;
	int TotalRxBlockSize;
	int TxBlockNum;
	int TotalTxBlockSize;

	FN_ENTER;

	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = cpu_to_le16(adev->memblocksize);

	/* Then we alert the card to our decision of block size */
	if (OK != acx_configure(adev, &MemoryBlockSize, ACX100_IE_BLOCK_SIZE))
		goto bad;

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers, i.e.: end-start/size */
	TotalMemoryBlocks =
	    (le32_to_cpu(mmt->PoolEnd) -
	     le32_to_cpu(mmt->PoolStart)) / adev->memblocksize;

	log(L_ANY, "TotalMemoryBlocks=%u (%u bytes)\n",
	    TotalMemoryBlocks, TotalMemoryBlocks * adev->memblocksize);

	/* MemoryConfigOption.DMA_config bitmask:
	   access to ACX memory is to be done:
	   0x00080000   using PCI conf space?!
	   0x00040000   using IO instructions?
	   0x00000000   using memory access instructions
	   0x00020000   using local memory block linked list (else what?)
	   0x00010000   using host indirect descriptors (else host must access ACX memory?)
	 */
	if (IS_PCI(adev)) {
		#if defined(CONFIG_ACX_MAC80211_PCI)
		MemoryConfigOption.DMA_config = cpu_to_le32(0x30000);
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc =
		    cpu2acx(adev->rx.host.phy);
		log(L_DEBUG, "pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
		    acx2cpu(MemoryConfigOption.pRxHostDesc),
		    (long)adev->rx.host.phy);
		#endif
	}
	else if(IS_MEM(adev)) {
		/*
		 * ACX ignores DMA_config for generic slave mode.
		 */
		#if defined(CONFIG_ACX_MAC80211_MEM)
		MemoryConfigOption.DMA_config = 0;
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc = cpu2acx(0);
		log(L_DEBUG, "pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
			acx2cpu(MemoryConfigOption.pRxHostDesc),
			(long)adev->rx.host.phy);
		#endif
	}
	else
		MemoryConfigOption.DMA_config = cpu_to_le32(0x20000);

	/* 50% of the allotment of memory blocks go to tx descriptors */
	TxBlockNum = TotalMemoryBlocks / 2;
	MemoryConfigOption.TxBlockNum = cpu_to_le16(TxBlockNum);

	/* and 50% go to the rx descriptors */
	RxBlockNum = TotalMemoryBlocks - TxBlockNum;
	MemoryConfigOption.RxBlockNum = cpu_to_le16(RxBlockNum);

	/* size of the tx and rx descriptor queues */
	TotalTxBlockSize = TxBlockNum * adev->memblocksize;
	TotalRxBlockSize = RxBlockNum * adev->memblocksize;
	log(L_DEBUG, "TxBlockNum %u RxBlockNum %u TotalTxBlockSize %u "
	    "TotalTxBlockSize %u\n", TxBlockNum, RxBlockNum,
	    TotalTxBlockSize, TotalRxBlockSize);

	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.rx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + 0x1f) & ~0x1f);

	/* align the rx descriptor queue to units of 0x20
	 * and offset it by the tx descriptor queue */
	MemoryConfigOption.tx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + TotalRxBlockSize +
			 0x1f) & ~0x1f);
	log(L_DEBUG, "rx_mem %08X rx_mem %08X\n", MemoryConfigOption.tx_mem,
	    MemoryConfigOption.rx_mem);

	/* alert the device to our decision */
	if (OK !=
	    acx_configure(adev, &MemoryConfigOption,
			    ACX1xx_IE_MEMORY_CONFIG_OPTIONS))
		goto bad;

	/* and tell the device to kick it into gear */
	if (OK != acx_issue_cmd(adev, ACX100_CMD_INIT_MEMORY, NULL, 0))
		goto bad;

#ifdef CONFIG_ACX_MAC80211_MEM
	/*
	 * slave memory interface has to manage the transmit pools for the ACX,
	 * so it needs to know what we chose here.
	 */
	adev->acx_txbuf_start = MemoryConfigOption.tx_mem;
	adev->acx_txbuf_numblocks = MemoryConfigOption.TxBlockNum;
#endif

	FN_EXIT1(OK);
	return OK;
bad:
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/*
 * acx100_s_create_dma_regions
 *
 * Note that this fn messes up heavily with hardware, but we cannot
 * lock it (we need to sleep). Not a problem since IRQs can't happen
 */
/* OLD CODE? - let's rewrite it! */
static int acx100_create_dma_regions(acx_device_t * adev)
{
	acx100_ie_queueconfig_t queueconf;
	acx_ie_memmap_t memmap;
	int res = NOT_OK;
	u32 tx_queue_start, rx_queue_start;

	FN_ENTER;

	/* read out the acx100 physical start address for the queues */
	if (OK != acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	tx_queue_start = le32_to_cpu(memmap.QueueStart);
	rx_queue_start = tx_queue_start + TX_CNT * sizeof(txdesc_t);

	log(L_DEBUG, "initializing Queue Indicator\n");

	memset(&queueconf, 0, sizeof(queueconf));

	/* Not needed for PCI, so we can avoid setting them altogether */
	if (IS_USB(adev)) {
		queueconf.NumTxDesc = USB_TX_CNT;
		queueconf.NumRxDesc = USB_RX_CNT;
	}

	/* calculate size of queues */
	queueconf.AreaSize = cpu_to_le32(TX_CNT * sizeof(txdesc_t) +
					 RX_CNT * sizeof(rxdesc_t) + 8);
	queueconf.NumTxQueues = 1;	/* number of tx queues */
	/* sets the beginning of the tx descriptor queue */
	queueconf.TxQueueStart = memmap.QueueStart;
	/* done by memset: queueconf.TxQueuePri = 0; */
	queueconf.RxQueueStart = cpu_to_le32(rx_queue_start);
	queueconf.QueueOptions = 1;	/* auto reset descriptor */
	/* sets the end of the rx descriptor queue */
	queueconf.QueueEnd =
	    cpu_to_le32(rx_queue_start + RX_CNT * sizeof(rxdesc_t)
	    );
	/* sets the beginning of the next queue */
	queueconf.HostQueueEnd =
	    cpu_to_le32(le32_to_cpu(queueconf.QueueEnd) + 8);
	if (OK != acx_configure(adev, &queueconf, ACX1xx_IE_QUEUE_CONFIG))
		goto fail;

	if (IS_PCI(adev)) {
		/* sets the beginning of the rx descriptor queue,
		 * after the tx descrs */
		if (OK != acx_create_hostdesc_queues(adev))
			goto fail;
		acx_create_desc_queues(adev, tx_queue_start, rx_queue_start);
	}
#ifdef CONFIG_ACX_MAC80211_MEM
	else if (IS_MEM(adev)) {
		/* sets the beginning of the rx descriptor queue,
		 * after the tx descrs */
		adev->acx_queue_indicator = (queueindicator_t *)
			(uintptr_t)le32_to_cpu (queueconf.QueueEnd);

		if (OK != acx_create_hostdesc_queues(adev))
			goto fail;

		acx_create_desc_queues(adev, tx_queue_start, rx_queue_start);
	}
#endif

	if (OK != acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	memmap.PoolStart = cpu_to_le32((le32_to_cpu(memmap.QueueEnd) + 4 +
					0x1f) & ~0x1f);

	if (OK != acx_configure(adev, &memmap, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	if (OK != acx100_init_memory_pools(adev, &memmap))
		goto fail;

	res = OK;
	goto end;

fail:
	acx_mwait(1000);	/* ? */

	if (IS_PCI(adev) || IS_MEM(adev))
		acx_free_desc_queues(adev);
end:
	FN_EXIT1(res);
	return res;
}


/*
 * acx111_s_create_dma_regions
 *
 * Note that this fn messes heavily with hardware, but we cannot
 * lock it (we need to sleep). Not a problem since IRQs can't happen
 */
static int acx111_create_dma_regions(acx_device_t *adev)
{
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	u32 tx_queue_start, rx_queue_start;

	FN_ENTER;

	/* Calculate memory positions and queue sizes */

	/* Set up our host descriptor pool + data pool */
	if (IS_PCI(adev)) {
		if (OK != acx_create_hostdesc_queues(adev))
			goto fail;
	}
	else if (IS_MEM(adev))
		if (OK != acx_create_hostdesc_queues(adev))
			goto fail;

	memset(&memconf, 0, sizeof(memconf));
	/* the number of STAs (STA contexts) to support
	 ** NB: was set to 1 and everything seemed to work nevertheless... */
	memconf.no_of_stations = 1; //cpu_to_le16(ARRAY_SIZE(adev->sta_list));

	/* specify the memory block size. Default is 256 */
	memconf.memory_block_size = cpu_to_le16(adev->memblocksize);
	/* let's use 50%/50% for tx/rx (specify percentage, units of 5%) */
	memconf.tx_rx_memory_block_allocation = ACX111_PERCENT(50);
	/* set the count of our queues
	 ** NB: struct acx111_ie_memoryconfig shall be modified
	 ** if we ever will switch to more than one rx and/or tx queue */
	memconf.count_rx_queues = 1;
	memconf.count_tx_queues = 1;
	/* 0 == Busmaster Indirect Memory Organization, which is what
	 * we want (using linked host descs with their allocated mem).
	 * 2 == Generic Bus Slave */
	/* done by memset: memconf.options = 0; */
	/* let's use 25% for fragmentations and 75% for frame transfers
	 * (specified in units of 5%) */
	memconf.fragmentation = ACX111_PERCENT(75);
	/* Rx descriptor queue config */
	memconf.rx_queue1_count_descs = RX_CNT;
	memconf.rx_queue1_type = 7;	/* must be set to 7 */
	/* done by memset: memconf.rx_queue1_prio = 0; low prio */
	if (IS_PCI(adev)) {
		#if defined(CONFIG_ACX_MAC80211_PCI)
		memconf.rx_queue1_host_rx_start =
		    cpu2acx(adev->rx.host.phy);
		#endif
	}
	else if (IS_MEM(adev)) {
		#if defined(CONFIG_ACX_MAC80211_MEM)
		memconf.rx_queue1_host_rx_start =
			cpu2acx(adev->rx.host.phy);
		#endif
	}

	/* Tx descriptor queue config */
	memconf.tx_queue1_count_descs = TX_CNT;
	/* done by memset: memconf.tx_queue1_attributes = 0; lowest priority */

	/* NB1: this looks wrong: (memconf,ACX1xx_IE_QUEUE_CONFIG),
	 * (queueconf,ACX1xx_IE_MEMORY_CONFIG_OPTIONS) look swapped, eh?
	 * But it is actually correct wrt IE numbers.
	 * NB2: sizeof(memconf) == 28 == 0x1c but
	 *configure(ACX1xx_IE_QUEUE_CONFIG) * writes 0x20 bytes
	 *(because same IE for acx100 uses struct
	 *acx100_ie_queueconfig * which is 4 bytes larger. what a
	 *mess. TODO: clean it up) */
	if (OK != acx_configure(adev, &memconf, ACX1xx_IE_QUEUE_CONFIG))
		goto fail;

	acx_interrogate(adev, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);

	tx_queue_start = le32_to_cpu(queueconf.tx1_queue_address);
	rx_queue_start = le32_to_cpu(queueconf.rx1_queue_address);

	log(L_INIT, "dump queue head (from card):\n"
	    "acx: len: %u\n"
	    "acx: tx_memory_block_address: %X\n"
	    "acx: rx_memory_block_address: %X\n"
	    "acx: tx1_queue address: %X\n"
	    "acx: rx1_queue address: %X\n",
	    le16_to_cpu(queueconf.len),
	    le32_to_cpu(queueconf.tx_memory_block_address),
	    le32_to_cpu(queueconf.rx_memory_block_address),
	    tx_queue_start, rx_queue_start);

	acx_create_desc_queues(adev, tx_queue_start, rx_queue_start);

	FN_EXIT1(OK);
	return OK;

fail:
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_free_desc_queues(adev);

	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/*
 * BOM Firmware, EEPROM, Phy
 * ==================================================
 */

void acx_get_firmware_version(acx_device_t * adev)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;

	FN_ENTER;

	memset(fw.fw_id, 'E', FW_ID_SIZE);
	acx_interrogate(adev, &fw, ACX1xx_IE_FWREV);
	memcpy(adev->firmware_version, fw.fw_id, FW_ID_SIZE);
	adev->firmware_version[FW_ID_SIZE] = '\0';

	log(L_INIT, "Firmware: firmware_version='%s' hw_id=%08X\n",
	    adev->firmware_version, fw.hw_id);

	if (strncmp(fw.fw_id, "Rev ", 4) != 0) {
		pr_info("strange firmware version string "
			"'%s', please report\n", adev->firmware_version);
		adev->firmware_numver = 0x01090407;	/* assume 1.9.4.7 */
	} else {
		num = &fw.fw_id[4];
		while (1) {
			c = *num++;
			if ((c == '.') || (c == '\0')) {
				hexarr[hexidx++] = val;
				if ((hexidx > 3) || (c == '\0')) /* end? */
					break;
				val = 0;
				continue;
			}
			if ((c >= '0') && (c <= '9'))
				c -= '0';
			else
				c = c - 'a' + (char)10;
			val = val * 16 + c;
		}

		adev->firmware_numver = (u32) ((hexarr[0] << 24) |
					       (hexarr[1] << 16)
					       | (hexarr[2] << 8) | hexarr[3]);
		log(L_DEBUG, "firmware_numver 0x%08X\n",
			adev->firmware_numver);
	}
	if (IS_ACX111(adev)) {
		if (adev->firmware_numver == 0x00010011) {
			/* This one does not survive floodpinging */
			pr_info("firmware '%s' is known to be buggy, "
				"please upgrade\n", adev->firmware_version);
		}
	}

	adev->firmware_id = le32_to_cpu(fw.hw_id);

	/* we're able to find out more detailed chip names now */
	switch (adev->firmware_id & 0xffff0000) {
	case 0x01010000:
	case 0x01020000:
		adev->chip_name = "TNETW1100A";
		break;
	case 0x01030000:
		adev->chip_name = "TNETW1100B";
		break;
	case 0x03000000:
	case 0x03010000:
		adev->chip_name = "TNETW1130";
		break;
	case 0x04030000:	/* 0x04030101 is TNETW1450 */
		adev->chip_name = "TNETW1450";
		break;
	default:
		pr_info("unknown chip ID 0x%08X, "
			"please report\n", adev->firmware_id);
		break;
	}
	FN_EXIT0;
}

/*
 * acx_display_hardware_details
 *
 * Displays hw/fw version, radio type etc...
 */
void acx_display_hardware_details(acx_device_t *adev)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
		/* DWL-650+ B1: MAXIM MAX2820 EGM 236 A7NOCH */
		/* USB DWL-120+ flip-antenna version:
		   MAXIM MAX2820 EGM 243 A7NO10
		   (large G logo) W22 B003A P01
		   (reference: W22-P01-B003A) */
		radio_str = "Maxim (MAX2820)";
		break;
	case RADIO_11_RFMD:
		radio_str = "RFMD";
		break;
	case RADIO_15_RALINK:
		radio_str = "Ralink";
		break;
	case RADIO_16_RADIA_RC2422:
		/* WL311v2 indicates that it's a Radia,
                   semi-recognizable label: RC2422(?) */
		radio_str = "Radia (RC2422?)";
		break;
	case RADIO_17_UNKNOWN:
		/* TI seems to have a radio which is
		 * additionally 802.11a capable, too */
		radio_str = "802.11a/b/g radio?! Please report";
		break;
	case RADIO_19_UNKNOWN:
		radio_str = "A radio used by Safecom cards?! Please report";
		break;
	case RADIO_1B_TI_TNETW3422:
		/* ex-Radia (consumed by TI), i.e. likely a RC2422 successor */
		radio_str = "TI (TNETW3422)";
		break;
	default:
		radio_str = "UNKNOWN, please report radio type name!";
		break;
	}

	switch (adev->form_factor) {
	case 0x00:
		form_str = "unspecified";
		break;
	case 0x01:
		form_str = "(mini-)PCI / CardBus";
		break;
	case 0x02:
		form_str = "USB";
		break;
	case 0x03:
		form_str = "Compact Flash";
		break;
	default:
		form_str = "UNKNOWN, please report";
		break;
	}

	pr_info("chipset %s, radio type 0x%02X (%s), "
	       "form factor 0x%02X (%s), EEPROM version 0x%02X, "
	       "uploaded firmware '%s'\n",
	       adev->chip_name, adev->radio_type, radio_str,
	       adev->form_factor, form_str, adev->eeprom_version,
	       adev->firmware_version);

	FN_EXIT0;
}

/*
 * acx_s_read_fw
 *
 * Loads a firmware image
 * Returns:
 *  0:						unable to load file
 *  pointer to firmware:	success
 */
firmware_image_t *acx_read_fw(struct device *dev, const char *file,
				u32 * size)
{
	firmware_image_t *res;
	const struct firmware *fw_entry;

	res = NULL;
	log(L_INIT, "requesting firmware image '%s'\n", file);
	if (!request_firmware(&fw_entry, file, dev)) {
		*size = 8;
		if (fw_entry->size >= 8)
			*size = 8 + le32_to_cpu(*(u32 *) (fw_entry->data + 4));
		if (fw_entry->size != *size) {
			pr_info("firmware size does not match "
				"firmware header: %d != %d, "
				"aborting fw upload\n",
				(int)fw_entry->size, (int)*size);
			goto release_ret;
		}
		res = vmalloc(*size);
		if (!res) {
			pr_info("no memory for firmware "
			       "(%u bytes)\n", *size);
			goto release_ret;
		}
		memcpy(res, fw_entry->data, fw_entry->size);
	      release_ret:
		release_firmware(fw_entry);
		return res;
	}
	pr_info("firmware image '%s' was not provided. "
	       "Check your hotplug scripts\n", file);

	/* checksum will be verified in write_fw, so don't bother here */
	return res;
}

/*
 * Common function to parse ALL configoption struct formats
 * (ACX100 and ACX111; FIXME: how to make it work with ACX100 USB!?!?).
 *
 * FIXME: logging should be removed here and added to a /proc file instead
 */
void acx_parse_configoption(acx_device_t *adev,
			 const acx111_ie_configoption_t *pcfg)
{
	const u8 *pEle;
	struct eeprom_cfg *acfg = &adev->cfgopt;
	int i;
	int is_acx111 = IS_ACX111(adev);

	if (acx_debug & L_DEBUG) {
		pr_info("configoption struct content:\n");
		acx_dump_bytes(pcfg, sizeof(*pcfg));
	}

	if ((is_acx111 && (adev->eeprom_version == 5))
	    || (!is_acx111 && (adev->eeprom_version == 4))
	    || (!is_acx111 && (adev->eeprom_version == 5))) {
		/* these versions are known to be supported */
	} else {
		pr_info("unknown chip and EEPROM version combination (%s, v%d), "
		       "don't know how to parse config options yet. "
		       "Please report\n", is_acx111 ? "ACX111" : "ACX100",
		       adev->eeprom_version);
		return;
	}

	/* first custom-parse the first part which has chip-specific layout */

	pEle = (const u8 *)pcfg;

	pEle += 4;		/* skip (type,len) header */

	memcpy(acfg->NVSv, pEle, sizeof(acfg->NVSv));
	pEle += sizeof(acfg->NVSv);

	pr_info("NVSv: ");
	for (i = 0; i < sizeof(acfg->NVSv); i++) {
		printk("%02X ", acfg->NVSv[i]);
	}
	printk("\n");

	if (is_acx111) {
		acfg->NVS_vendor_offs = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->NVS_vendor_offs);

		acfg->probe_delay = 200;	/* good default value? */
		pEle += 2;	/* FIXME: unknown, value 0x0001 */
	} else {
		memcpy(acfg->MAC, pEle, sizeof(acfg->MAC));
		pEle += sizeof(acfg->MAC);

		acfg->probe_delay = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->probe_delay);
		if ((acfg->probe_delay < 100)
		    || (acfg->probe_delay > 500)) {
			pr_info("strange probe_delay value %d, "
			       "tweaking to 200\n", acfg->probe_delay);
			acfg->probe_delay = 200;
		}
	}

	acfg->eof_memory = le32_to_cpu(*(u32 *) pEle);
	pEle += sizeof(acfg->eof_memory);

	pr_info("NVS_vendor_offs:%04X probe_delay:%d eof_memory:%d\n",
	       acfg->NVS_vendor_offs,
	       acfg->probe_delay, acfg->eof_memory);

	acfg->dot11CCAModes = *pEle++;
	acfg->dot11Diversity = *pEle++;
	acfg->dot11ShortPreambleOption = *pEle++;
	acfg->dot11PBCCOption = *pEle++;
	acfg->dot11ChannelAgility = *pEle++;
	acfg->dot11PhyType = *pEle++;
	acfg->dot11TempType = *pEle++;
	pr_info("CCAModes:%02X Diversity:%02X ShortPreOpt:%02X "
	       "PBCC:%02X ChanAgil:%02X PHY:%02X Temp:%02X\n",
	       acfg->dot11CCAModes,
	       acfg->dot11Diversity,
	       acfg->dot11ShortPreambleOption,
	       acfg->dot11PBCCOption,
	       acfg->dot11ChannelAgility,
	       acfg->dot11PhyType, acfg->dot11TempType);

	/* then use common parsing for next part which has common layout */

	pEle++;			/* skip table_count (6) */

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.  I'm
		 * not sure if these 4 extra bytes are before the
		 * dot11 things above or after, so I'm just going to
		 * guess after.  If someone sees these aren't
		 * reasonable numbers, please fix this.
		 * The area from which the dot11 values above are read
		 * contains: 04 01 01 01 00 05 01 06 00 02 01 02 the 8
		 * dot11 reads above take care of 8 of them, but which
		 * 8...
		 */
		pEle += 4;
	}

	acfg->antennas.type = pEle[0];
	acfg->antennas.len = pEle[1];
	pr_info("AntennaID:%02X Len:%02X Data:",
	       acfg->antennas.type, acfg->antennas.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->antennas.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->power_levels.type = pEle[0];
	acfg->power_levels.len = pEle[1];
	pr_info("PowerLevelID:%02X Len:%02X Data:",
	       acfg->power_levels.type, acfg->power_levels.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->power_levels.list[i] =
		    le16_to_cpu(*(u16 *) & pEle[i * 2 + 2]);
		printk("%04X ", acfg->power_levels.list[i]);
	}
	printk("\n");

	pEle += pEle[1] * 2 + 2;
	acfg->data_rates.type = pEle[0];
	acfg->data_rates.len = pEle[1];
	pr_info("DataRatesID:%02X Len:%02X Data:",
	       acfg->data_rates.type, acfg->data_rates.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->data_rates.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->domains.type = pEle[0];
	acfg->domains.len = pEle[1];

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.
		 * There's an extra byte between this structure and
		 * the next that is not accounted for with this
		 * structure's length.  It's most likely a bug in the
		 * firmware, but we can fix it here by bumping the
		 * length of this field by 1.
		 */
		acfg->domains.len++;
	}

	pr_info("DomainID:%02X Len:%02X Data:",
	       acfg->domains.type, acfg->domains.len);
	for (i = 0; i < acfg->domains.len; i++) {
		acfg->domains.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += acfg->domains.len + 2;
	acfg->product_id.type = pEle[0];
	acfg->product_id.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->product_id.list[i] = pEle[i + 2];

	pr_info("ProductID:%02X Len:%02X Data:%.*s\n",
	       acfg->product_id.type, acfg->product_id.len,
	       acfg->product_id.len,
	       (char *)acfg->product_id.list);

	pEle += pEle[1] + 2;
	acfg->manufacturer.type = pEle[0];
	acfg->manufacturer.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->manufacturer.list[i] = pEle[i + 2];

	pr_info("ManufacturerID:%02X Len:%02X Data:%.*s\n",
	       acfg->manufacturer.type, acfg->manufacturer.len,
	       acfg->manufacturer.len,
	       (char *)acfg->manufacturer.list);
	/*
	pr_info("EEPROM part:\n");
	for (i=0; i<58; i++) {
		printk("%02X =======>  0x%02X\n",
			i, (u8 *)acfg->NVSv[i-2]);
	}
	*/
}

int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_read_phy_reg(adev, reg, charbuf);
	if (IS_USB(adev))
		return acxusb_read_phy_reg(adev, reg, charbuf);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);

	return (NOT_OK);
}

int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_write_phy_reg(adev, reg, value);
	if (IS_USB(adev))
		return acxusb_write_phy_reg(adev, reg, value);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);

	return (NOT_OK);
}

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */
int acx_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param,
		unsigned len, unsigned timeout, const char* cmdstr)
{
	if (IS_PCI(adev))
		return acxpci_issue_cmd_timeo_debug(adev, cmd, param, len,
						timeout, cmdstr);
	if (IS_USB(adev))
		return acxusb_issue_cmd_timeo_debug(adev, cmd, param, len,
						timeout, cmdstr);
	if (IS_MEM(adev))
		return acxmem_issue_cmd_timeo_debug(adev, cmd, param, len,
						timeout, cmdstr);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",  __func__,
		(adev)->dev_type);
	return (NOT_OK);
}

int acx_configure_debug(acx_device_t *adev, void *pdr, int type,
		      const char *typestr)
{
	u16 len;
	int res;
	char msgbuf[255];

	FN_ENTER;

	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	if (unlikely(!len))
		log(L_DEBUG, "%s: zero-length type %s?!\n",
				__func__, typestr);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIGURE, pdr, len + 4);

	sprintf(msgbuf, "%s: %s: type=0x%04X, typestr=%s, len=%u",
			__func__, wiphy_name(adev->ieee->wiphy),
			type, typestr, len);
	if (likely(res == OK))
		log(L_CTL,  "%s: OK\n", msgbuf);
	 else
		log(L_ANY,  "%s: FAILED\n", msgbuf);

	FN_EXIT0;
	return res;
}

static int acx111_get_feature_config(acx_device_t *adev,
			    u32 *feature_options, u32 *data_flow_options)
{
	struct acx111_ie_feature_config feat;

	FN_ENTER;

	if (!IS_ACX111(adev))
		return NOT_OK;

	memset(&feat, 0, sizeof(feat));

	if (OK != acx_interrogate(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	log(L_DEBUG,
	    "acx: got Feature option:0x%X, DataFlow option: 0x%X\n",
	    feat.feature_options, feat.data_flow_options);

	if (feature_options)
		*feature_options = le32_to_cpu(feat.feature_options);
	if (data_flow_options)
		*data_flow_options = le32_to_cpu(feat.data_flow_options);

	FN_EXIT0;
	return OK;
}

static int acx111_set_feature_config(acx_device_t *adev,
			    u32 feature_options, u32 data_flow_options,
			    unsigned int mode
			    /* 0 == remove, 1 == add, 2 == set */ )
{
	struct acx111_ie_feature_config feat;
	int i;

	FN_ENTER;

	if (!IS_ACX111(adev)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	if ((mode < 0) || (mode > 2)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	if (mode != 2)	{
		/* need to modify old data */
		i = acx111_get_feature_config(adev, &feat.feature_options,
				&feat.data_flow_options);
		if (i != OK) {
			printk("%s: acx111_s_get_feature_config: NOT_OK\n",
				__FUNCTION__);
			return i;
		}
	}
	else {
		/* need to set a completely new value */
		feat.feature_options = 0;
		feat.data_flow_options = 0;
	}

	if (mode == 0) {	/* remove */
		CLEAR_BIT(feat.feature_options, cpu_to_le32(feature_options));
		CLEAR_BIT(feat.data_flow_options,
			  cpu_to_le32(data_flow_options));
	} else {		/* add or set */
		SET_BIT(feat.feature_options, cpu_to_le32(feature_options));
		SET_BIT(feat.data_flow_options,
			cpu_to_le32(data_flow_options));
	}

	log(L_DEBUG,
	    "acx: old: feature 0x%08X dataflow 0x%08X. mode: %u\n"
	    "acx: new: feature 0x%08X dataflow 0x%08X\n",
	    feature_options, data_flow_options, mode,
	    le32_to_cpu(feat.feature_options),
	    le32_to_cpu(feat.data_flow_options));

	if (OK != acx_configure(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	FN_EXIT0;
	return OK;
}

static inline int acx111_feature_off(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 0);
}
static inline int acx111_feature_on(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 1);
}
static inline int acx111_feature_set(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 2);
}

int acx_interrogate_debug(acx_device_t *adev, void *pdr, int type,
			const char *typestr)
{
	u16 len;
	int res;

	FN_ENTER;

	/* FIXME: no check whether this exceeds the array yet.
	 * We should probably remember the number of entries... */
	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	log(L_CTL, "%s: (type:%s,len:%u)\n", __func__, typestr, len);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, pdr, len + 4);
	if (unlikely(OK != res)) {
#if ACX_DEBUG
		pr_info("%s: %s: (type:%s) FAILED\n", __func__,
			wiphy_name(adev->ieee->wiphy),
			typestr);
#else
		pr_info("%s: %s: (type:0x%X) FAILED\n", __func__,
			wiphy_name(adev->ieee->wiphy),
			type);
#endif
		/* dump_stack() is already done in issue_cmd() */
	}
	FN_EXIT1(res);
	return res;
}

/* OW TODO Helper for acx_s_cmd_join_bssid below */
/* Looks scary, eh?
** Actually, each one compiled into one AND and one SHIFT,
** 31 bytes in x86 asm (more if uints are replaced by u16/u8) */
static inline unsigned int acx_rate111to5bits(unsigned int rate)
{
	return (rate & 0x7)
	    | ((rate & RATE111_11) / (RATE111_11 / JOINBSS_RATES_11))
	    | ((rate & RATE111_22) / (RATE111_22 / JOINBSS_RATES_22));
}

/*
 * acx_cmd_join_bssid
 *
 * Common code for both acx100 and acx111.
 */
/* NB: does NOT match RATE100_nn but matches ACX[111]_SCAN_RATE_n */
int acx_cmd_join_bssid(acx_device_t *adev, const u8 *bssid)
{
	int res;
        acx_joinbss_t tmp;
        int dtim_interval;
        int i;

        if (mac_is_zero(bssid))
                return OK;

        FN_ENTER;

        dtim_interval = (ACX_MODE_0_ADHOC == adev->mode) ?
                        1 : adev->dtim_interval;

        memset(&tmp, 0, sizeof(tmp));

        for (i = 0; i < ETH_ALEN; i++) {
                tmp.bssid[i] = bssid[ETH_ALEN-1 - i];
        }

        tmp.beacon_interval = cpu_to_le16(adev->beacon_interval);

        /* Basic rate set. Control frame responses (such as ACK or CTS
	 * frames) are sent with one of these rates */
        if (IS_ACX111(adev)) {
                /* It was experimentally determined that rates_basic
		 * can take 11g rates as well, not only rates defined
		 * with JOINBSS_RATES_BASIC111_nnn.  Just use
		 * RATE111_nnn constants... */
                tmp.u.acx111.dtim_interval = dtim_interval;
                tmp.u.acx111.rates_basic = cpu_to_le16(adev->rate_basic);
                log(L_ASSOC, "rates_basic:%04X, rates_supported:%04X\n",
                        adev->rate_basic, adev->rate_oper);
        } else {
                tmp.u.acx100.dtim_interval = dtim_interval;
                tmp.u.acx100.rates_basic =
			acx_rate111to5bits(adev->rate_basic);
                tmp.u.acx100.rates_supported =
			acx_rate111to5bits(adev->rate_oper);
                log(L_ASSOC, "rates_basic:%04X->%02X, "
                        "rates_supported:%04X->%02X\n",
                        adev->rate_basic, tmp.u.acx100.rates_basic,
                        adev->rate_oper, tmp.u.acx100.rates_supported);
        }

        /* Setting up how Beacon, Probe Response, RTS, and PS-Poll
	 * frames will be sent (rate/modulation/preamble) */
        tmp.genfrm_txrate = bitpos2genframe_txrate[lowest_bit(adev->rate_basic)];
        tmp.genfrm_mod_pre = 0;
        /* FIXME: was = adev->capab_short (which was always 0); */

        /* we can use short pre *if* all peers can understand it */
        /* FIXME #2: we need to correctly set PBCC/OFDM bits here too */

        /* we switch fw to STA mode in MONITOR mode, it seems to be
	 * the only mode where fw does not emit beacons by itself but
	 * allows us to send anything (we really want to retain
	 * ability to tx arbitrary frames in MONITOR mode)
	 */
        tmp.macmode = (adev->mode != ACX_MODE_MONITOR
		? adev->mode : ACX_MODE_2_STA);
        tmp.channel = adev->channel;
        tmp.essid_len = adev->essid_len;

        memcpy(tmp.essid, adev->essid, tmp.essid_len);
        res = acx_issue_cmd(adev, ACX1xx_CMD_JOIN, &tmp, tmp.essid_len + 0x11);

        log(L_ASSOC|L_DEBUG, "BSS_Type = %u\n", tmp.macmode);
        acxlog_mac(L_ASSOC|L_DEBUG, "JoinBSSID MAC:", adev->bssid, "\n");

	/* acx_update_capabilities(adev); */
        FN_EXIT0;
        return res;
}

/*
 * BOM Configuration (Control Path)
 * ==================================================
 */

void acx_set_defaults(acx_device_t *adev)
{
	struct eeprom_cfg *acfg = &adev->cfgopt;
	FN_ENTER;

	/* do it before getting settings, prevent bogus channel 0 warning */
	adev->channel = 1;

	/* query some settings from the card.
	 * NOTE: for some settings, e.g. CCA and ED (ACX100!), an initial
	 * query is REQUIRED, otherwise the card won't work correctly! */

	acx1xx_get_antenna(adev);

	acx1xx_get_station_id(adev);
	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);

	acx_get_reg_domain(adev);

	/* Only ACX100 supports ED and CCA */
	if (IS_ACX100(adev)) {
		acx1xx_get_cca(adev);
		acx1xx_get_ed_threshold(adev);
	}

	acx_get_sensitivity(adev);

	/* set our global interrupt mask */
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_set_interrupt_mask(adev);

	adev->led_power = 1;	/* LED is active on startup */
	adev->brange_max_quality = 60;	/* LED blink max quality is 60 */
	adev->brange_time_last_state_change = jiffies;

	/* copy the MAC address we just got from the card into our MAC
	 * address used during current 802.11 session */
	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	MAC_BCAST(adev->ap);

	MAC_COPY(adev->bssid, adev->dev_addr);

	adev->essid_len =
		snprintf(adev->essid, sizeof(adev->essid), "ACXSTA%02X%02X%02X",
			adev->dev_addr[3], adev->dev_addr[4], adev->dev_addr[5]);
	adev->essid_active = 1;

	/* we have a nick field to waste, so why not abuse it
	 * to announce the driver version? ;-) */
	strncpy(adev->nick, "acx " ACX_RELEASE, IW_ESSID_MAX_SIZE);

	if (IS_PCI(adev)) {	/* FIXME: this should be made to apply to USB, too! */
		/* first regulatory domain entry in EEPROM == default
		 * reg. domain */
		adev->reg_dom_id = acfg->domains.list[0];
	} else if(IS_MEM(adev)){
		/* first regulatory domain entry in EEPROM == default
		 * reg. domain */
		adev->reg_dom_id = acfg->domains.list[0];
	}

	/* 0xffff would be better, but then we won't get a "scan
	 * complete" interrupt, so our current infrastructure will
	 * fail: */
	adev->scan_count = 1;
	adev->scan_mode = ACX_SCAN_OPT_ACTIVE;
	adev->scan_duration = 100;
	adev->scan_probe_delay = 200;
	/* reported to break scanning: adev->scan_probe_delay =
	 * acfg->probe_delay; */
	adev->scan_rate = ACX_SCAN_RATE_1;

	adev->mode = ACX_MODE_2_STA;
	adev->listen_interval = 100;
	adev->beacon_interval = DEFAULT_BEACON_INTERVAL;
	adev->dtim_interval = DEFAULT_DTIM_INTERVAL;

	adev->msdu_lifetime = DEFAULT_MSDU_LIFETIME;

	adev->rts_threshold = DEFAULT_RTS_THRESHOLD;
	adev->frag_threshold = 2346;

	/* use standard default values for retry limits */
	adev->short_retry = 7;	/* max. retries for (short) non-RTS packets */
	adev->long_retry = 4;	/* max. retries for long (RTS) packets */

	adev->preamble_mode = 2;	/* auto */
	adev->fallback_threshold = 3;
	adev->stepup_threshold = 10;
	adev->rate_bcast = RATE111_1;
	adev->rate_bcast100 = RATE100_1;
	adev->rate_basic = RATE111_1 | RATE111_2;
	adev->rate_auto = 1;
	if (IS_ACX111(adev))
		adev->rate_oper = RATE111_ALL;
	else
		adev->rate_oper = RATE111_ACX100_COMPAT;

	/* Supported Rates element - the rates here are given in units
	 * of 500 kbit/s, plus 0x80 added. See 802.11-1999.pdf item
	 * 7.3.2.2 */
	acx_update_ratevector(adev);

	/* Get current tx-power setting */
	acx1xx_get_tx_level(adev);

	/* Sensitivity settings */
	if (IS_ACX111(adev))
		/* start with sensitivity level 2 out of 3: */
		adev->sensitivity = 2;

/* #define ENABLE_POWER_SAVE */
#ifdef ENABLE_POWER_SAVE
	adev->ps_wakeup_cfg = PS_CFG_ENABLE | PS_CFG_WAKEUP_ALL_BEAC;
	adev->ps_listen_interval = 1;
	adev->ps_options =
	    PS_OPT_ENA_ENHANCED_PS | PS_OPT_TX_PSPOLL | PS_OPT_STILL_RCV_BCASTS;
	adev->ps_hangover_period = 30;
	adev->ps_enhanced_transition_time = 0;
#else
	adev->ps_wakeup_cfg = 0;
	adev->ps_listen_interval = 0;
	adev->ps_options = 0;
	adev->ps_hangover_period = 0;
	adev->ps_enhanced_transition_time = 0;
#endif
	FN_EXIT0;
}

void acx_start(acx_device_t *adev)
{
	FN_ENTER;

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

	FN_EXIT0;
}


int acx_net_reset(struct ieee80211_hw *ieee)
{
	acx_device_t *adev = ieee2adev(ieee);
	FN_ENTER;
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_reset_dev(adev);
	else
		TODO();

	FN_EXIT0;
	return 0;
}

int acx_init_mac(acx_device_t * adev)
{
	int result = NOT_OK;

	FN_ENTER;

	if (IS_ACX111(adev)) {
		adev->ie_len = acx111_ie_len;
		adev->ie_len_dot11 = acx111_ie_len_dot11;
	} else {
		adev->ie_len = acx100_ie_len;
		adev->ie_len_dot11 = acx100_ie_len_dot11;
	}

	if (IS_PCI(adev)) {
		adev->memblocksize = 256;	/* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxpci_upload_radio(adev);
	}
	else if (IS_MEM(adev)){
		adev->memblocksize = 256; /* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxmem_upload_radio(adev);
	}
	else
		adev->memblocksize = 128;

	if (IS_ACX111(adev)) {
		/* for ACX111, the order is different from ACX100
		   1. init packet templates
		   2. create station context and create dma regions
		   3. init wep default keys
		 */
		if (OK != acx_init_packet_templates(adev))
			goto fail;
		if (OK != acx111_create_dma_regions(adev)) {
			pr_info("%s: acx111_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}
	} else {
		if (OK != acx100_init_wep(adev))
			goto fail;
		if (OK != acx_init_packet_templates(adev))
			goto fail;
		if (OK != acx100_create_dma_regions(adev)) {
			pr_info("%s: acx100_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}
	}

	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	result = OK;
fail:
	if (result)
		pr_info("init_mac() FAILED\n");
	FN_EXIT1(result);
	return result;
}

int acx_setup_modes(acx_device_t *adev)
{
	FN_ENTER;

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
	FN_EXIT0;
	return 0;
}


static int acx_set_mode(acx_device_t *adev, u16 mode)
{
	adev->mode = mode;
	return acx_update_mode(adev);
}

static int acx_update_mode(acx_device_t *adev)
{
	int res = 0;
	FN_ENTER;

	log(L_INIT, "Updating to mode=0x%04x\n", adev->mode);

	switch (adev->mode) {
	case ACX_MODE_2_STA:
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_3_AP:
		if (IS_ACX111(adev)) {
			res += acx111_feature_on(adev, 0,
					FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
		}
		res += acx_update_rx_config(adev);

		acx1xx_set_rx_enable(adev, 1);
		acx1xx_set_tx_enable(adev, 1);

		break;
	case ACX_MODE_MONITOR:
		break;
	case ACX_MODE_OFF:
		res += acx1xx_set_tx_enable(adev, 0);
		res += acx1xx_set_rx_enable(adev, 0);
		break;

	default:
		logf1(L_INIT, "Error: Undefined mode=0x%04x\n",
				adev->mode);
		return NOT_OK;
	}

	FN_EXIT0;
	return res ? NOT_OK : OK;
}

int acx_selectchannel(acx_device_t *adev, u8 channel, int freq)
{
	int res = 0;

	FN_ENTER;

	adev->rx_status.freq = freq;
	adev->rx_status.band = IEEE80211_BAND_2GHZ;

	adev->channel = channel;

	adev->tx_enabled = 1;
	adev->rx_enabled = 1;

	res += acx1xx_update_tx(adev);
	res += acx1xx_update_rx(adev);

	acx_wake_queue(adev->ieee, NULL);

	FN_EXIT0;
	return res ? NOT_OK : OK;
}

static void acx_get_sensitivity(acx_device_t *adev)
{

	if ( (RADIO_11_RFMD == adev->radio_type) ||
		(RADIO_0D_MAXIM_MAX2820 == adev->radio_type) ||
		(RADIO_15_RALINK == adev->radio_type))
	{
		acx_read_phy_reg(adev, 0x30, &adev->sensitivity);
	} else {
		log(L_INIT, "don't know how to get sensitivity "
				"for radio type 0x%02X\n", adev->radio_type);
		return;
	}
	log(L_INIT, "got sensitivity value %u\n", adev->sensitivity);
}

static void acx_set_sensitivity(acx_device_t *adev, u8 sensitivity)
{
	adev->sensitivity = sensitivity;
	acx_update_sensitivity(adev);
}


static void acx_update_sensitivity(acx_device_t *adev)
{
	if (IS_USB(adev) && IS_ACX100(adev)) {
		log(L_ANY, "Updating sensitivity on usb acx100 doesn't work yet.\n");
		return;
	}

	log(L_INIT, "updating sensitivity value: %u\n",
		adev->sensitivity);
	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
	case RADIO_11_RFMD:
	case RADIO_15_RALINK:
		acx_write_phy_reg(adev, 0x30, adev->sensitivity);
		break;
	case RADIO_16_RADIA_RC2422:
	case RADIO_17_UNKNOWN:
		/* TODO: check whether RADIO_1B (ex-Radia!) has same
		 * behaviour */
		acx111_sens_radio_16_17(adev);
		break;
	default:
		log(L_INIT, "don't know how to modify the sensitivity "
			"for radio type 0x%02X\n", adev->radio_type);
	}
}

static void acx_get_reg_domain(acx_device_t *adev)
{
	acx_ie_generic_t dom;

	acx_interrogate(adev, &dom,
			ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
	adev->reg_dom_id = dom.m.bytes[0];
	log(L_INIT, "Got regulatory domain 0x%02X\n", adev->reg_dom_id);
}

static void acx_set_reg_domain(acx_device_t *adev, u8 domain_id)
{
	adev->reg_dom_id = domain_id;
	acx_update_reg_domain(adev);
}

static void acx_update_reg_domain(acx_device_t *adev)
{
	log(L_INIT, "Updating the regulatory domain: 0x%02X\n",
	    adev->reg_dom_id);
	acx_set_sane_reg_domain(adev, 1);
}

static int acx1xx_set_tx_level_dbm(acx_device_t *adev, int level_dbm)
{
	adev->tx_level_dbm = level_dbm;
	return acx1xx_update_tx_level_dbm(adev);
}

static int acx1xx_update_tx_level_dbm(acx_device_t *adev)
{
	u8 level_val;
	/* Number of level of device */
	int numl;
	/* Dbm per level */
	int dpl;
	/* New level reverse */
	int nlr;
	/*  Helper for modulo dpl, ... */
	int helper;

	/* The acx is working with power levels, which shift the
	 * tx-power in partitioned steps and also depending on the
	 * configured regulatory domain.
	 *
	 * The acx111 has five tx_power levels, the acx100 we assume two.
	 *
	 * The acx100 also displays them in co_powerlevels_t config
	 * options. We could use this info for a more precise
	 * matching, but for the time being, we assume there two
	 * levels by default.
	 *
	 * The approach here to set the corresponding tx-power level
	 * here, is to translate the requested tx-power in dbm onto a
	 * scale of 0-20dbm, with an assumed maximum of 20dbm. The
	 * maximum would normally vary depending on the regulatory
	 * domain.
	 *
	 * The the value on the 0-20dbm scale is then matched onto the
	 * available levels.
	 */

	if (adev->tx_level_dbm > TX_CFG_MAX_DBM_POWER) {
		logf1(L_ANY, "Err: Setting tx-power > %d dbm not supported\n",
			TX_CFG_MAX_DBM_POWER);
		return (NOT_OK);
	}

	if (IS_ACX111(adev))
		numl = TX_CFG_ACX111_NUM_POWER_LEVELS;
	else if (IS_ACX100(adev))
		numl = TX_CFG_ACX100_NUM_POWER_LEVELS;
	else
		return (NOT_OK);

	dpl = TX_CFG_MAX_DBM_POWER / numl;

	/* Find closest match */
	nlr = adev->tx_level_dbm / dpl;
	helper = adev->tx_level_dbm % dpl;
	if (helper > dpl - helper)
		nlr++;

	/* Adjust to boundaries (level zero doesn't exists, adjust to 1) */
	if (nlr < 1)
		nlr = 1;
	if (nlr > numl)
		nlr = numl;

	/* Translate to final level_val */
	level_val = numl - nlr + 1;

	/* Inform of adjustments */
	if (nlr * dpl != adev->tx_level_dbm) {
		helper = adev->tx_level_dbm;
		adev->tx_level_dbm = nlr * dpl;
		log(L_ANY, "Tx-power adjusted from %d to %d dbm (tx-power-level: %d)\n", helper, adev->tx_level_dbm, level_val);
	}
	return acx1xx_set_tx_level(adev, level_val);
}

static int acx1xx_get_tx_level(acx_device_t *adev)
{
	struct acx1xx_ie_tx_level tx_level;

	FN_ENTER;

	if (IS_USB(adev)) {
		logf0(L_ANY, "Get tx-level not yet supported on usb\n");
		goto end;
	}

	memset(&tx_level, 0, sizeof(tx_level));

	if (OK != acx_interrogate(adev, &tx_level,
					ACX1xx_IE_DOT11_TX_POWER_LEVEL)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	adev->tx_level_val= tx_level.level;
	log(L_ANY, "Got tx-power-level: %d\n", adev->tx_level_val);
end:
	FN_EXIT0;
	return OK;
}

static int acx1xx_set_tx_level(acx_device_t *adev, u8 level_val)
{
	adev->tx_level_val=level_val;
	return acx1xx_update_tx_level(adev);
}

static int acx1xx_update_tx_level(acx_device_t *adev)
{
	struct acx1xx_ie_tx_level tx_level;

	if (IS_USB(adev)) {
		logf0(L_ANY, "Update tx-level not yet supported on usb\n");
		return OK;
	}

	log(L_ANY, "Updating tx-power-level to: %d\n", adev->tx_level_val);
	memset(&tx_level, 0, sizeof(tx_level));
	tx_level.level=adev->tx_level_val;

	return acx_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);
}

/* OW: Previously included tx-power related functions, kept for documentation */
#if 0
/*
 * FIXME: this should be solved in a general way for all radio types
 * by decoding the radio firmware module,
 * since it probably has some standard structure describing how to
 * set the power level of the radio module which it controls.
 * Or maybe not, since the radio module probably has a function interface
 * instead which then manages Tx level programming :-\
 */
int acx111_set_tx_level(acx_device_t * adev, u8 level_dbm)
{
	struct acx111_ie_tx_level tx_level;

	/* my acx111 card has two power levels in its configoptions (== EEPROM):
	 * 1 (30mW) [15dBm]
	 * 2 (10mW) [10dBm]
	 * For now, just assume all other acx111 cards have the same.
	 * FIXME: Ideally we would query it here, but we first need a
	 * standard way to query individual configoptions easily.
	 * Well, now we have proper cfgopt txpower variables, but this still
	 * hasn't been done yet, since it also requires dBm <-> mW conversion here... */
	if (level_dbm <= 12) {
		tx_level.level = 2;	/* 10 dBm */
		adev->tx_level_dbm = 10;
	} else {
		tx_level.level = 1;	/* 15 dBm */
		adev->tx_level_dbm = 15;
	}
	if (level_dbm != adev->tx_level_dbm)
		log(L_INIT, "only predefined transmission "
		    "power levels are supported at this time: "
		    "adjusted %d dBm to %d dBm\n", level_dbm,
		    adev->tx_level_dbm);

	return acx_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);
}

int acx100pci_set_tx_level(acx_device_t * adev, u8 level_dbm)
{
	/* since it can be assumed that at least the Maxim radio has a
	 * maximum power output of 20dBm and since it also can be
	 * assumed that these values drive the DAC responsible for
	 * setting the linear Tx level, I'd guess that these values
	 * should be the corresponding linear values for a dBm value,
	 * in other words:
	 * calculate the values from that formula:
	 * Y [dBm] = 10 * log (X [mW])
	 * then scale the 0..63 value range onto the 1..100mW range
	 * (0..20 dBm) and you're done...
	 * Hopefully that's ok, but you never know if we're actually
	 * right... (especially since Windows XP doesn't seem to show
	 * actual Tx dBm values :-P) */

	/* NOTE: on Maxim, value 30 IS 30mW, and value 10 IS 10mW - so
	 * the values are EXACTLY mW!!! Not sure about RFMD and
	 * others, though... */
	static const u8 dbm2val_maxim[21] = {
		63, 63, 63, 62,
		61, 61, 60, 60,
		59, 58, 57, 55,
		53, 50, 47, 43,
		38, 31, 23, 13,
		0
	};
	static const u8 dbm2val_rfmd[21] = {
		0, 0, 0, 1,
		2, 2, 3, 3,
		4, 5, 6, 8,
		10, 13, 16, 20,
		25, 32, 41, 50,
		63
	};
	const u8 *table;

	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
		table = &dbm2val_maxim[0];
		break;
	case RADIO_11_RFMD:
	case RADIO_15_RALINK:
		table = &dbm2val_rfmd[0];
		break;
	default:
		pr_info("%s: unknown/unsupported radio type, "
		       "cannot modify tx power level yet!\n",
			wiphy_name(adev->ieee->wiphy));
		return NOT_OK;
	}
	pr_info("%s: changing radio power level to %u dBm (%u)\n",
	       wiphy_name(adev->ieee->wiphy), level_dbm, table[level_dbm]);
	acx_write_phy_reg(adev, 0x11, table[level_dbm]);
	return OK;
}

/* Comment int acx100mem_set_tx_level(acx_device_t *adev, u8
   level_dbm) Otherwise equal with int
   acx100pci_set_tx_level(acx_device_t * adev, u8 level_dbm) */
/*
 * The hx4700 EEPROM, at least, only supports 1 power setting.  The
 * configure routine matches the PA bias with the gain, so just use
 * its default value.  The values are: 0x2b for the gain and 0x03 for
 * the PA bias.  The firmware writes the gain level to the Tx gain
 * control DAC and the PA bias to the Maxim radio's PA bias register.
 * The firmware limits itself to 0 - 64 when writing to the gain
 * control DAC.
 *
 * Physically between the ACX and the radio, higher Tx gain control
 * DAC values result in less power output; 0 volts to the Maxim radio
 * results in the highest output power level, which I'm assuming
 * matches up with 0 in the Tx Gain DAC register.
 *
 * Although there is only the 1 power setting, one of the radio
 * firmware functions adjusts the transmit power level up and down.
 * That function is called by the ACX FIQ handler under certain
 * conditions.
 */
#endif

static int acx1xx_get_antenna(acx_device_t *adev)
{
	int res;
	u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

	FN_ENTER;

	memset(antenna, 0, sizeof(antenna));
	res=acx_interrogate(adev, antenna,
			  ACX1xx_IE_DOT11_CURRENT_ANTENNA);
	adev->antenna[0] = antenna[4];
	adev->antenna[1] = antenna[5];
	log(L_INIT, "Got antenna[0,1]: 0x%02X 0x%02X\n", adev->antenna[0], adev->antenna[1]);

	FN_EXIT0;
	return res;
}

static int acx1xx_set_antenna(acx_device_t *adev, u8 val0, u8 val1)
{
	int res;

	FN_ENTER;

	adev->antenna[0] = val0;
	adev->antenna[1] = val1;
	res=acx1xx_update_antenna(adev);

	FN_EXIT0;
	return res;
}

static int acx1xx_update_antenna(acx_device_t *adev)
{
	int res;
	u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

	FN_ENTER;

	log(L_INIT, "Updating antenna[0,1]: 0x%02X 0x%02X\n",
		adev->antenna[0], adev->antenna[1]);
	memset(antenna, 0, sizeof(antenna));
	antenna[4] = adev->antenna[0];
	antenna[5] = adev->antenna[1];
	res=acx_configure(adev, &antenna,
			ACX1xx_IE_DOT11_CURRENT_ANTENNA);

	FN_EXIT0;
	return res;
}

/* OW: Transfered from the acx-20080210 ioctl calls, but didn't test of verify */
#if 0
/*
 * 0 = antenna1; 1 = antenna2; 2 = full diversity; 3 = partial diversity
 */
static int acx100_set_rx_antenna(acx_device_t *adev, u8 val)
{
	int result;

	FN_ENTER;

	if (val > 3) {
		result = -EINVAL;
		goto end;
	}

	logf1(L_ANY, "old antenna value: 0x%02X\n", adev->antenna[0]);

	acx_sem_lock(adev);

	adev->antenna[0] &= 0x3f;
	SET_BIT(adev->antenna[0], (val << 6));
	logf1(L_ANY, "new antenna value: 0x%02X\n", adev->antenna[0]);

	result = acx1xx_update_antenna(adev);

	acx_sem_unlock(adev);
end:
	FN_EXIT1(result);
	return result;
}

/*
 * Arguments: 0 == antenna1; 1 == antenna2;
 * Could anybody test which antenna is the external one?
 */
static int acx100_set_tx_antenna(acx_device_t *adev, u8 val)
{
	int result;
	u8 val2;

	FN_ENTER;

	if (val > 1) {
		result = -EINVAL;
		goto end;
	}

	logf1(L_ANY, "old antenna value: 0x%02X\n", adev->antenna[0]);

	acx_sem_lock(adev);

	/* swap antenna 1/2 values */
	switch (val) {
	case 0:
		val2 = 1;
		break;
	case 1:
		val2 = 0;
		break;
	default:
		val2=val;
	}
	logf1(L_ANY, "val2=%02d\n", val2);

	adev->antenna[0] &= ~0x30;
	SET_BIT(adev->antenna[0], ((val2 & 0x01) << 5));
	logf1(L_ANY, "new antenna value: 0x%02X\n", adev->antenna[0]);

	result = acx1xx_update_antenna(adev);

	acx_sem_unlock(adev);
end:
	FN_EXIT1(result);
	return result;
}
#endif

#ifdef UNUSED
void acx_update_capabilities(acx_device_t * adev)
{
	u16 cap = 0;

	switch (adev->mode) {
	case ACX_MODE_3_AP:
		SET_BIT(cap, WF_MGMT_CAP_ESS);
		break;
	case ACX_MODE_0_ADHOC:
		SET_BIT(cap, WF_MGMT_CAP_IBSS);
		break;
		/* other types of stations do not emit beacons */
	}

	if (adev->wep_restricted)
		SET_BIT(cap, WF_MGMT_CAP_PRIVACY);

	if (acfg->dot11ShortPreambleOption)
		SET_BIT(cap, WF_MGMT_CAP_SHORT);

	if (acfg->dot11PBCCOption)
		SET_BIT(cap, WF_MGMT_CAP_PBCC);

	if (acfg->dot11ChannelAgility)
		SET_BIT(cap, WF_MGMT_CAP_AGILITY);

	log(L_DEBUG, "caps updated from 0x%04X to 0x%04X\n",
	    adev->capabilities, cap);
	adev->capabilities = cap;
}
#endif

static int acx1xx_get_station_id(acx_device_t *adev)
{
	u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
	const u8 *paddr;
	int i, res;

	FN_ENTER;

	res=acx_interrogate(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);
	paddr = &stationID[4];
	for (i = 0; i < ETH_ALEN; i++) {
		/* we copy the MAC address (reversed in the card) to
		 * the netdevice's MAC address, and on ifup it will be
		 * copied into iwadev->dev_addr */
		adev->dev_addr[ETH_ALEN - 1 - i] = paddr[i];
	}

	log(L_INIT, "Got station_id: " MACSTR "\n", MAC(adev->dev_addr));

	FN_EXIT0;
	return res;
}

static int acx1xx_set_station_id(acx_device_t *adev, u8 *new_addr)
{
	int res;

	FN_ENTER;

	MAC_COPY(adev->dev_addr, new_addr);
	res=acx1xx_update_station_id(adev);

	FN_EXIT0;
	return res;
}

static int acx1xx_update_station_id(acx_device_t *adev)
{
	u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
	u8 *paddr;
	int i, res;

	FN_ENTER;

	log(L_INIT, "Updating station_id to: " MACSTR "\n",
		MAC(adev->dev_addr));

	paddr = &stationID[4];
	for (i = 0; i < ETH_ALEN; i++) {
		/* copy the MAC address we obtained when we noticed
		 * that the ethernet iface's MAC changed to the card
		 * (reversed in the card!) */
		paddr[i] = adev->dev_addr[ETH_ALEN - 1 - i];
	}
	res=acx_configure(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);

	FN_EXIT0;
	return res;
}

static int acx1xx_get_ed_threshold(acx_device_t *adev)
{
	int res = NOT_OK;

	FN_ENTER;

	if (IS_ACX100(adev)) {
		res=acx100_get_ed_threshold(adev);
	} else {
		log(L_INIT, "acx111 doesn't support ED\n");
		adev->ed_threshold = 0;
	}

	log(L_INIT, "Got Energy Detect (ED) threshold %u\n",
	    adev->ed_threshold);

	FN_EXIT0;
	return res;
}

static int acx100_get_ed_threshold(acx_device_t *adev)
{
	int res;
	u8 ed_threshold[4 + ACX100_IE_DOT11_ED_THRESHOLD_LEN];

	FN_ENTER;
	memset(ed_threshold, 0, sizeof(ed_threshold));
	res=acx_interrogate(adev, ed_threshold,
			  ACX100_IE_DOT11_ED_THRESHOLD);
	adev->ed_threshold = ed_threshold[4];

	FN_EXIT0;
	return res;
}

#ifdef UNUSED
static int acx1xx_set_ed_threshold(acx_device_t *adev, u8 ed_threshold)
{
	int res;

	FN_ENTER;
	adev->ed_threshold=ed_threshold;
	res=acx1xx_update_ed_threshold(adev);

	FN_EXIT0;
	return res;
}
#endif

static int acx1xx_update_ed_threshold(acx_device_t *adev)
{
	int res=NOT_OK;

	FN_ENTER;
	log(L_INIT, "Updating the Energy Detect (ED) threshold: %u\n",
	    adev->ed_threshold);

	if (IS_ACX100(adev))
		res=acx100_update_ed_threshold(adev);
	else
		log(L_INIT, "acx111 doesn't support ED threshold\n");

	FN_EXIT0;
	return res;
}

static int acx100_update_ed_threshold(acx_device_t *adev)
{
	int res;
	u8 ed_threshold[4 + ACX100_IE_DOT11_ED_THRESHOLD_LEN];

	FN_ENTER;
	memset(ed_threshold, 0, sizeof(ed_threshold));
	ed_threshold[4] = adev->ed_threshold;
	res=acx_configure(adev, &ed_threshold,
			ACX100_IE_DOT11_ED_THRESHOLD);

	FN_EXIT0;
	return res;
}

static int acx1xx_get_cca(acx_device_t *adev)
{
	int res = NOT_OK;

	FN_ENTER;
	if (IS_ACX100(adev))
		acx100_get_cca(adev);
	else {
		log(L_INIT, "acx111 doesn't support CCA\n");
		adev->cca = 0;
	}
	log(L_INIT, "Got Channel Clear Assessment (CCA) value %u\n",
		adev->cca);

	FN_EXIT0;
	return res;
}

static int acx100_get_cca(acx_device_t *adev)
{
	int res;
	u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];
	FN_ENTER;

	memset(cca, 0, sizeof(cca));
	res = acx_interrogate(adev, cca,
			ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
	adev->cca = cca[4];

	FN_EXIT0;
	return res;
}

#ifdef UNUSED
static int acx1xx_set_cca(acx_device_t *adev, u8 cca)
{
	int res;

	FN_ENTER;

	adev->cca=cca;
	res=acx1xx_update_cca(adev);

	FN_EXIT0;
	return res;
}
#endif

static int acx1xx_update_cca(acx_device_t *adev)
{
	int res = NOT_OK;

	FN_ENTER;
	log(L_INIT, "Updating the Channel Clear Assessment (CCA) value: "
			"0x%02X\n", adev->cca);
	if (IS_ACX100(adev))
		res = acx100_update_cca(adev);
	else
		log(L_INIT, "acx111 doesn't support CCA\n");

	FN_EXIT0;
	return res;
}

static int acx100_update_cca(acx_device_t *adev)
{
	int res;
	u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];

	FN_ENTER;

	memset(cca, 0, sizeof(cca));
	cca[4] = adev->cca;
	res = acx_configure(adev, &cca,
			ACX1xx_IE_DOT11_CURRENT_CCA_MODE);

	FN_EXIT0;
	return res;
}

#ifdef UNUSED
static int acx1xx_get_rate_fallback(acx_device_t *adev)
{
	int res = NOT_OK;
	u8 rate[4 + ACX1xx_IE_RATE_FALLBACK_LEN];

	FN_ENTER;
	memset(rate, 0, sizeof(rate));
	res=acx_interrogate(adev, &rate,
			ACX1xx_IE_RATE_FALLBACK);
	adev->rate_auto = rate[4];

	FN_EXIT0;
	return res;
}

static int acx1xx_set_rate_fallback(acx_device_t *adev, u8 rate_auto)
{
	int res;
	FN_ENTER;
	adev->rate_auto = rate_auto;
	res = acx1xx_update_rate_fallback(adev);
	FN_EXIT0;
	return res;
}
#endif

static int acx1xx_update_rate_fallback(acx_device_t *adev)
{
	int res;
	u8 rate[4 + ACX1xx_IE_RATE_FALLBACK_LEN];

	FN_ENTER;
	/* configure to not do fallbacks when not in auto rate mode */
	rate[4] = (adev->rate_auto) /* adev->txrate_fallback_retries */
		? 1 : 0;
	log(L_INIT, "Updating Tx fallback to %u retries\n", rate[4]);

	res = acx_configure(adev, &rate, ACX1xx_IE_RATE_FALLBACK);
	FN_EXIT0;
	return res;
}

#ifdef UNUSED
static int acx1xx_set_channel(acx_device_t *adev, u8 channel)
{
	int res;
	FN_ENTER;
	adev->channel = channel;
	res = acx1xx_update_tx(adev);
	FN_EXIT0;
	return res;
}
#endif

static int acx1xx_set_tx_enable(acx_device_t *adev, u8 tx_enabled)
{
	int res;
	FN_ENTER;
	adev->tx_enabled = tx_enabled;
	res = acx1xx_update_tx(adev);
	FN_EXIT0;
	return res;
}

static int acx1xx_update_tx(acx_device_t *adev)
{
	int res;
	FN_ENTER;

	log(L_XFER, "Updating TX: %s, channel=%d\n",
		adev->tx_enabled ? "enable" : "disable", adev->channel);

	if (adev->tx_enabled)
		res = acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX,
				&adev->channel, 1);
	else
		res = acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);

	FN_EXIT0;
	return res;
}

static int acx1xx_set_rx_enable(acx_device_t *adev, u8 rx_enabled)
{
	int res;
	FN_ENTER;
	adev->rx_enabled = rx_enabled;
	res = acx1xx_update_rx(adev);
	FN_EXIT0;
	return res;
}

static int acx1xx_update_rx(acx_device_t *adev)
{
	int res;
	FN_ENTER;

	log(L_XFER, "Updating RX: %s, channel=%d\n",
		adev->rx_enabled ? "enable" : "disable", adev->channel);

	if (adev->rx_enabled)
		res = acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
				&adev->channel, 1);
	else
		res = acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);

	FN_EXIT0;
	return res;
}

static int acx1xx_update_retry(acx_device_t *adev)
{
	int res;
	u8 short_retry[4 + ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN];
	u8 long_retry[4 + ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN];

	FN_ENTER;

	log(L_INIT, "Updating the short retry limit: %u, "
		"long retry limit: %u\n",
		adev->short_retry, adev->long_retry);

	short_retry[0x4] = adev->short_retry;
	long_retry[0x4] = adev->long_retry;
	res = acx_configure(adev, &short_retry,
			ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT);
	res += acx_configure(adev, &long_retry,
			ACX1xx_IE_DOT11_LONG_RETRY_LIMIT);

	FN_EXIT0;
	return res;
}

static int acx1xx_update_msdu_lifetime(acx_device_t *adev)
{
	int res = NOT_OK;
	u8 xmt_msdu_lifetime[4 + ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN];
	FN_ENTER;

	log(L_INIT, "Updating the tx MSDU lifetime: %u\n",
		adev->msdu_lifetime);

	*(u32 *) &xmt_msdu_lifetime[4] = cpu_to_le32(
		(u32) adev->msdu_lifetime);
	res = acx_configure(adev, &xmt_msdu_lifetime,
	                ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME);
	FN_EXIT0;
	return res;
}

static int acx_update_rx_config(acx_device_t *adev)
{
	int res;
	struct {
		u16 id;
		u16 len;
		u16 rx_cfg1;
		u16 rx_cfg2;
	} ACX_PACKED cfg;

	FN_ENTER;

	switch (adev->mode) {
	case ACX_MODE_MONITOR:
		adev->rx_config_1 = (u16) (0
		   /* | RX_CFG1_INCLUDE_RXBUF_HDR  */
		   /* | RX_CFG1_FILTER_SSID        */
		   /* | RX_CFG1_FILTER_BCAST       */
		   /* | RX_CFG1_RCV_MC_ADDR1       */
		   /* | RX_CFG1_RCV_MC_ADDR0       */
		   /* | RX_CFG1_FILTER_ALL_MULTI   */
		   /* | RX_CFG1_FILTER_BSSID       */
		   /* | RX_CFG1_FILTER_MAC         */
		   | RX_CFG1_RCV_PROMISCUOUS
		   | RX_CFG1_INCLUDE_FCS
		   /* | RX_CFG1_INCLUDE_PHY_HDR    */
		   );
		adev->rx_config_2 = (u16) (0
		   | RX_CFG2_RCV_ASSOC_REQ
		   | RX_CFG2_RCV_AUTH_FRAMES
		   | RX_CFG2_RCV_BEACON_FRAMES
		   | RX_CFG2_RCV_CONTENTION_FREE
		   | RX_CFG2_RCV_CTRL_FRAMES
		   | RX_CFG2_RCV_DATA_FRAMES
		   | RX_CFG2_RCV_BROKEN_FRAMES
		   | RX_CFG2_RCV_MGMT_FRAMES
		   | RX_CFG2_RCV_PROBE_REQ
		   | RX_CFG2_RCV_PROBE_RESP
		   | RX_CFG2_RCV_ACK_FRAMES
		   | RX_CFG2_RCV_OTHER
		   );
		break;
	default:
		adev->rx_config_1 = (u16) (0
		   /* | RX_CFG1_INCLUDE_RXBUF_HDR  */
		   /* | RX_CFG1_FILTER_SSID        */
		   /* | RX_CFG1_FILTER_BCAST       */
		   /* | RX_CFG1_RCV_MC_ADDR1       */
		   /* | RX_CFG1_RCV_MC_ADDR0       */
		   /* | RX_CFG1_FILTER_ALL_MULTI   */
		   /* | RX_CFG1_FILTER_BSSID       */
		   | RX_CFG1_FILTER_MAC
		   /* | RX_CFG1_RCV_PROMISCUOUS    */
		   /* | RX_CFG1_INCLUDE_FCS        */
		   /* | RX_CFG1_INCLUDE_PHY_HDR    */
		   );
		adev->rx_config_2 = (u16) (0
		   | RX_CFG2_RCV_ASSOC_REQ
		   | RX_CFG2_RCV_AUTH_FRAMES
		   | RX_CFG2_RCV_BEACON_FRAMES
		   | RX_CFG2_RCV_CONTENTION_FREE
		   | RX_CFG2_RCV_CTRL_FRAMES
		   | RX_CFG2_RCV_DATA_FRAMES
		   /*| RX_CFG2_RCV_BROKEN_FRAMES   */
		   | RX_CFG2_RCV_MGMT_FRAMES
		   | RX_CFG2_RCV_PROBE_REQ
		   | RX_CFG2_RCV_PROBE_RESP
		   /*| RX_CFG2_RCV_ACK_FRAMES*/
		   | RX_CFG2_RCV_OTHER
		   );
		break;
	}

	adev->rx_config_1 |= RX_CFG1_INCLUDE_RXBUF_HDR;

	if ((adev->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR)
		|| (adev->firmware_numver >= 0x02000000))
		adev->phy_header_len = IS_ACX111(adev) ? 8 : 4;
	else
		adev->phy_header_len = 0;

	log(L_INIT, "Updating RXconfig to mode=0x%04X,"
		"rx_config_1:2=%04X:%04X\n",
		adev->mode, adev->rx_config_1, adev->rx_config_2);

	cfg.rx_cfg1 = cpu_to_le16(adev->rx_config_1);
	cfg.rx_cfg2 = cpu_to_le16(adev->rx_config_2);
	res = acx_configure(adev, &cfg, ACX1xx_IE_RXCONFIG);

	FN_EXIT0;
	return res;
}

#ifdef UNUSED
static int acx_update_wep(acx_device_t *adev)
{
	int res = NOT_OK;
	ie_dot11WEPDefaultKeyID_t dkey;

	FN_ENTER;

#ifdef DEBUG_WEP
	struct {
		u16 type;
		u16 len;
		u8 val;
	} ACX_PACKED keyindic;
#endif
	log(L_INIT, "updating WEP key settings\n");

	acx_set_wepkey(adev);
	if (adev->wep_enabled) {
		dkey.KeyID = adev->wep_current_index;
		log(L_INIT, "setting WEP key %u as default\n", dkey.KeyID);
		res = acx_configure(adev, &dkey,
				ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);
#ifdef DEBUG_WEP
		keyindic.val = 3;
		acx_configure(adev, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif
	}

	FN_EXIT0;
	return res;
}

static int acx_update_wep_options(acx_device_t *adev)
{
	int res=NOT_OK;

	FN_ENTER;

	if (IS_ACX111(adev))
		log(L_DEBUG, "setting WEP Options for acx111"
			" is not supported\n");
	else
		res = acx100_update_wep_options(adev);

	FN_EXIT0;
	return res;
}

static int acx100_update_wep_options(acx_device_t *adev)
{
	int res=NOT_OK;
	acx100_ie_wep_options_t options;

	FN_ENTER;
	log(L_INIT, "acx100: setting WEP Options\n");

	/* let's choose maximum setting: 4 default keys,
	 * plus 10 other keys: */
	options.NumKeys =
		cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10);
	/* don't decrypt default key only,
	 * don't override decryption: */
	options.WEPOption = 0;
	if (adev->mode == ACX_MODE_3_AP) {
		/* don't decrypt default key only,
		 * override decryption mechanism: */
		options.WEPOption = 2;
	}

	res = acx_configure(adev, &options, ACX100_IE_WEP_OPTIONS);

	FN_EXIT0;
	return res;
}
#endif

static int acx_set_beacon(acx_device_t *adev, struct sk_buff *beacon)
{
	int res;
	u8 *tim_pos;
	int len_wo_tim;
	int len_tim;

	/* The TIM template handling between ACX100 and ACX111 works
	 * differently:
	 *
	 * ACX111: Needs TIM in dedicated template via
	 * ACX1xx_CMD_CONFIG_TIM
	 *
	 * ACX100: Needs TIM included into the beacon, however space
	 * for TIM template needs to be configured during memory-map
	 * setup
	 */

	/* TODO Use pos provided by ieee80211_beacon_get_tim instead */
	tim_pos = acx_beacon_find_tim(beacon);
	if (tim_pos == NULL)
		logf0(L_DEBUG, "No tim contained in beacon skb");

	/* ACX111: If beacon contains tim, only configure
	 * beacon-template until tim
	 */
	if (IS_ACX111(adev) && tim_pos)
		len_wo_tim = tim_pos - beacon->data;
	else
		len_wo_tim = beacon->len;

	res = acx_set_beacon_template(adev, beacon->data, len_wo_tim);
	if (res)
		goto out;

	/* We need to set always a tim template, even if length it
	 * null, since otherwise the acx is not sending fully correct
	 * structured beacons.
	 */
	if (IS_ACX111(adev))
	{
		len_tim = beacon->len - len_wo_tim;
		acx_set_tim_template(adev, tim_pos, len_tim);
	}

	/* BTW acx111 firmware would not send probe responses if probe
	 * request does not have all basic rates flagged by 0x80!
	 * Thus firmware does not conform to 802.11, it should ignore
	 * 0x80 bit in ratevector from STA.  We can 'fix' it by not
	 * using this template and sending probe responses by
	 * hand. TODO --vda */
	res = acx_set_probe_response_template(adev, beacon->data, len_wo_tim);
	if (res)
		goto out;
	/* acx_s_set_probe_response_template_off(adev); */

	/* Needed if generated frames are to be emitted at different
	 * tx rate now */
	logf0(L_ANY, "Redoing cmd_join_bssid() following template cfg\n");
	res = acx_cmd_join_bssid(adev, adev->bssid);

	out:
	return res;
}

/*
 * BOM Templates (Control Path)
 * ==================================================
 */
static int acx_init_max_template_generic(acx_device_t *adev, unsigned int len,
				unsigned int cmd)
{
	int res;
	union {
		acx_template_nullframe_t null;
		acx_template_beacon_t b;
		acx_template_tim_t tim;
		acx_template_probereq_t preq;
		acx_template_proberesp_t presp;
	} templ;

	memset(&templ, 0, len);
	templ.null.size = cpu_to_le16(len - 2);
	res = acx_issue_cmd(adev, cmd, &templ, len);
	return res;
}

static int acx_init_max_null_data_template(acx_device_t *adev)
{
	/* OW hh version:
	 * issue_cmd(cmd:cmd,buflen:26,timeout:50ms,type:0x0018)
	 * mac80211 version: issue_cmd: begin:
	 * (cmd:cmd,buflen:32,timeout:50ms,type:0x001E)
	 *
	 * diff with hh is struct ieee80211_hdr included in
	 * acx_template_nullframe_t, which is bigger, thus size if
	 * bigger
	 */
	return acx_init_max_template_generic(adev,
		sizeof(acx_template_nullframe_t), ACX1xx_CMD_CONFIG_NULL_DATA);
}


static int acx_init_max_beacon_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_beacon_t),
					ACX1xx_CMD_CONFIG_BEACON);
}

static int acx_init_max_tim_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_tim_t),
					ACX1xx_CMD_CONFIG_TIM);
}

static int acx_init_max_probe_response_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_proberesp_t),
					ACX1xx_CMD_CONFIG_PROBE_RESPONSE);
}

static int acx_init_max_probe_request_template(acx_device_t *adev)
{
	return acx_init_max_template_generic(adev,
					sizeof(acx_template_probereq_t),
					ACX1xx_CMD_CONFIG_PROBE_REQUEST);
}

/*
 * acx_set_tim_template
 *
 * FIXME: In full blown driver we will regularly update partial
 * virtual bitmap by calling this function (it can be done by irq
 * handler on each DTIM irq or by timer...)

[802.11 7.3.2.6] TIM information element:
- 1 EID
- 1 Length
1 1 DTIM Count
    indicates how many beacons (including this) appear before next DTIM
    (0=this one is a DTIM)
2 1 DTIM Period
    number of beacons between successive DTIMs
    (0=reserved, 1=all TIMs are DTIMs, 2=every other, etc)
3 1 Bitmap Control
    bit0: Traffic Indicator bit associated with Assoc ID 0 (Bcast AID?)
    set to 1 in TIM elements with a value of 0 in the DTIM Count field
    when one or more broadcast or multicast frames are buffered at the AP.
    bit1-7: Bitmap Offset (logically Bitmap_Offset = Bitmap_Control & 0xFE).
4 n Partial Virtual Bitmap
    Visible part of traffic-indication bitmap.
    Full bitmap consists of 2008 bits (251 octets) such that bit number N
    (0<=N<=2007) in the bitmap corresponds to bit number (N mod 8)
    in octet number N/8 where the low-order bit of each octet is bit0,
    and the high order bit is bit7.
    Each set bit in virtual bitmap corresponds to traffic buffered by AP
    for a specific station (with corresponding AID?).
    Partial Virtual Bitmap shows a part of bitmap which has non-zero.
    Bitmap Offset is a number of skipped zero octets (see above).
    'Missing' octets at the tail are also assumed to be zero.
    Example: Length=6, Bitmap_Offset=2, Partial_Virtual_Bitmap=55 55 55
    This means that traffic-indication bitmap is:
    00000000 00000000 01010101 01010101 01010101 00000000 00000000...
    (is bit0 in the map is always 0 and real value is in Bitmap Control bit0?)
*/
static int acx_set_tim_template(acx_device_t *adev, u8 *data, int len)
{
	acx_template_tim_t templ;
	int res;

	FN_ENTER;

	if (acx_debug & L_DEBUG) {
		logf1(L_ANY, "data, len=%d:\n", len);
		acx_dump_bytes(data, len);
	}

	/* We need to set always a tim template, even with len=0,
	* since otherwise the acx is sending a not 100% well
	* structured beacon (this may not be blocking though, but it's
	* better like this)
	*/
	memset(&templ, 0, sizeof(templ));
	if (data)
		memcpy((u8*) &templ.tim_eid, data, len);
	templ.size = cpu_to_le16(len);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_TIM, &templ,
			sizeof(templ));
	FN_EXIT1(res);
	return res;
}

#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_tim_template_off(acx_device_t *adev)
{
	acx_template_nullframe_t templ;
	int result;

	FN_ENTER;
	memset(&templ, 0, sizeof(templ));
	templ.size = cpu_to_le16(sizeof(templ) - 2);;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_TIM,
			&templ, sizeof(templ));

	FN_EXIT1(result);
	return result;
}
#endif

#if POWER_SAVE_80211
static int acx_s_set_null_data_template(acx_device_t *adev)
{
	struct acx_template_nullframe b;
	int result;

	FN_ENTER;

	/* memset(&b, 0, sizeof(b)); not needed, setting all members */

	b.size = cpu_to_le16(sizeof(b) - 2);
	b.hdr.fc = WF_FTYPE_MGMTi | WF_FSTYPE_NULLi;
	b.hdr.dur = 0;
	MAC_BCAST(b.hdr.a1);
	MAC_COPY(b.hdr.a2, adev->dev_addr);
	MAC_COPY(b.hdr.a3, adev->bssid);
	b.hdr.seq = 0;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_NULL_DATA,
			&b, sizeof(b));

	FN_EXIT1(result);
	return result;
}
#endif

static int acx_set_beacon_template(acx_device_t *adev, u8 *data, int len)
{
	struct acx_template_beacon templ;
	int res;

	FN_ENTER;

	if (acx_debug & L_DEBUG) {
		logf1(L_ANY, "data, len=%d, sizeof(struct"
			"acx_template_beacon)=%d:\n",
			len, (int)sizeof(struct acx_template_beacon));
		acx_dump_bytes(data, len);
	}

	memcpy((u8*) &templ.fc, data, len);
	templ.size = cpu_to_le16(len);

	/* +2: include 'u16 size' field */
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_BEACON, &templ, len+2);

	FN_EXIT1(res);
	return res;
}

static int acx_set_probe_response_template(acx_device_t *adev, u8* data,
					int len)
{
	struct acx_template_proberesp templ;
	int res;

	FN_ENTER;

	memcpy((u8*) &templ.fc, data, len);
	templ.fc = cpu_to_le16(IEEE80211_FTYPE_MGMT
			| IEEE80211_STYPE_PROBE_RESP);

	templ.size = cpu_to_le16(len);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
			&templ, len+2);

	FN_EXIT1(res);
	return res;
}

#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_probe_response_template_off(acx_device_t *adev)
{
	acx_template_nullframe_t templ;
	int result;

	FN_ENTER;
	memset(&templ, 0, sizeof(templ));
	templ.size = cpu_to_le16(sizeof(templ) - 2);;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
			&templ, sizeof(templ));

	FN_EXIT1(result);
	return result;
}
#endif

/*
 * acx_s_init_packet_templates()
 *
 * NOTE: order is very important here, to have a correct memory
 * layout!  init templates: max Probe Request (station mode), max NULL
 * data, max Beacon, max TIM, max Probe Response.
 */
static int acx_init_packet_templates(acx_device_t *adev)
{
	acx_ie_memmap_t mm;	/* ACX100 only */
	int result = NOT_OK;

	FN_ENTER;

	log(L_DEBUG | L_INIT, "initializing max packet templates\n");

	if (OK != acx_init_max_probe_request_template(adev))
		goto failed;

	if (OK != acx_init_max_null_data_template(adev))
		goto failed;

	if (OK != acx_init_max_beacon_template(adev))
		goto failed;

	if (OK != acx_init_max_tim_template(adev))
		goto failed;

	if (OK != acx_init_max_probe_response_template(adev))
		goto failed;

	if (IS_ACX111(adev)) {
		/* ACX111 doesn't need the memory map magic below, and
		 * the other templates will be set later (acx_start) */
		result = OK;
		goto success;
	}

	/* ACX100 will have its TIM template set,
	 * and we also need to update the memory map */

	if (OK != acx_set_tim_template(adev, NULL, 0))
		goto failed_acx100;

	log(L_DEBUG, "sizeof(memmap) = %d bytes\n", (int)sizeof(mm));

	if (OK != acx_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	mm.QueueStart = cpu_to_le32(le32_to_cpu(mm.PacketTemplateEnd) + 4);
	if (OK != acx_configure(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	result = OK;
	goto success;

failed_acx100:
	log(L_DEBUG | L_INIT,
	    /* "cb=0x%X\n" */
	    "acx: ACXMemoryMap:\n"
	    "acx: .CodeStart=0x%X\n"
	    "acx: .CodeEnd=0x%X\n"
	    "acx: .WEPCacheStart=0x%X\n"
	    "acx: .WEPCacheEnd=0x%X\n"
	    "acx: .PacketTemplateStart=0x%X\n"
	    "acx: .PacketTemplateEnd=0x%X\n",
	    /* len, */
	    le32_to_cpu(mm.CodeStart),
	    le32_to_cpu(mm.CodeEnd),
	    le32_to_cpu(mm.WEPCacheStart),
	    le32_to_cpu(mm.WEPCacheEnd),
	    le32_to_cpu(mm.PacketTemplateStart),
	    le32_to_cpu(mm.PacketTemplateEnd));

failed:
	pr_info("%s: %s() FAILED\n",
		wiphy_name(adev->ieee->wiphy), __func__);

success:
	FN_EXIT1(result);
	return result;
}

#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_probe_request_template(acx_device_t *adev)
{
	struct acx_template_probereq probereq;
	char *p;
	int res;
	int frame_len;

	FN_ENTER;

	memset(&probereq, 0, sizeof(probereq));

	probereq.fc = WF_FTYPE_MGMTi | WF_FSTYPE_PROBEREQi;
	MAC_BCAST(probereq.da);
	MAC_COPY(probereq.sa, adev->dev_addr);
	MAC_BCAST(probereq.bssid);

	p = probereq.variable;
	p = wlan_fill_ie_ssid(p, adev->essid_len, adev->essid);
	p = wlan_fill_ie_rates(p, adev->rate_supported_len,
			adev->rate_supported);
	p = wlan_fill_ie_rates_ext(p, adev->rate_supported_len,
			adev->rate_supported);
	frame_len = p - (char*)&probereq;
	probereq.size = cpu_to_le16(frame_len - 2);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_REQUEST,
			&probereq, frame_len);
	FN_EXIT0;
	return res;
}
#endif

/*
 * BOM Recalibration (Control Path)
 * ==================================================
 */
static int acx111_set_recalib_auto(acx_device_t *adev, int enable)
{
	adev->recalib_auto=enable;
	return(acx111_update_recalib_auto(adev));
}

static int acx111_update_recalib_auto(acx_device_t *adev)
{
	acx111_cmd_radiocalib_t cal;

	if (!IS_ACX111(adev)) {
		log(L_INIT, "Firmware auto radio-recalibration"
			" not supported on acx100.\n");
		return(-1);
	}

	if (adev->recalib_auto) {
		log(L_INIT, "Enabling firmware auto radio-recalibration.\n");
		/* automatic recalibration, choose all methods: */
		cal.methods = cpu_to_le32(0x8000000f);
		/* automatic recalibration every 60 seconds (value in TUs)
		 * I wonder what the firmware default here is? */
		cal.interval = cpu_to_le32(58594);
	} else {
		log(L_INIT, "Disabling firmware auto radio-recalibration.\n");
		cal.methods = 0;
		cal.interval = 0;
	}

	return acx_issue_cmd_timeo(adev, ACX111_CMD_RADIOCALIB,
			&cal, sizeof(cal),
			CMD_TIMEOUT_MS(100));
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

/*
 * BOM Other (Control Path)
 * ==================================================
 */
#if POWER_SAVE_80211
static void acx_s_update_80211_powersave_mode(acx_device_t * adev)
{
	/* merge both structs in a union to be able to have common code */
	union {
		acx111_ie_powersave_t acx111;
		acx100_ie_powersave_t acx100;
	} pm;

	/* change 802.11 power save mode settings */
	log(L_INIT, "updating 802.11 power save mode settings: "
	    "wakeup_cfg 0x%02X, listen interval %u, "
	    "options 0x%02X, hangover period %u, "
	    "enhanced_ps_transition_time %u\n",
	    adev->ps_wakeup_cfg, adev->ps_listen_interval,
	    adev->ps_options, adev->ps_hangover_period,
	    adev->ps_enhanced_transition_time);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "Previous PS mode settings: wakeup_cfg 0x%02X, "
	    "listen interval %u, options 0x%02X, "
	    "hangover period %u, "
	    "enhanced_ps_transition_time %u, beacon_rx_time %u\n",
	    pm.acx111.wakeup_cfg,
	    pm.acx111.listen_interval,
	    pm.acx111.options,
	    pm.acx111.hangover_period,
	    IS_ACX111(adev) ?
	    pm.acx111.enhanced_ps_transition_time
	    : pm.acx100.enhanced_ps_transition_time,
	    IS_ACX111(adev) ? pm.acx111.beacon_rx_time : (u32) - 1);
	pm.acx111.wakeup_cfg = adev->ps_wakeup_cfg;
	pm.acx111.listen_interval = adev->ps_listen_interval;
	pm.acx111.options = adev->ps_options;
	pm.acx111.hangover_period = adev->ps_hangover_period;
	if (IS_ACX111(adev)) {
		pm.acx111.beacon_rx_time = cpu_to_le32(adev->ps_beacon_rx_time);
		pm.acx111.enhanced_ps_transition_time =
		    cpu_to_le32(adev->ps_enhanced_transition_time);
	} else {
		pm.acx100.enhanced_ps_transition_time =
		    cpu_to_le16(adev->ps_enhanced_transition_time);
	}
	acx_configure(adev, &pm, ACX1xx_IE_POWER_MGMT);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	acx_mwait(40);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	log(L_INIT, "power save mode change %s\n",
	    (pm.acx111.
	     wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
	/* FIXME: maybe verify via PS_CFG_PENDING bit here
	 * that power save mode change was successful. */
	/* FIXME: we shouldn't trigger a scan immediately after
	 * fiddling with power save mode (since the firmware is sending
	 * a NULL frame then). */
}
#endif

/* TODO Verify these functions: translation rxbuffer.phy_plcp_signal to rate_idx */
#if 0

/** Rate values **/
#define ACX_CCK_RATE_1MB            0
#define ACX_CCK_RATE_2MB            1
#define ACX_CCK_RATE_5MB            2
#define ACX_CCK_RATE_11MB           3
#define ACX_OFDM_RATE_6MB           4
#define ACX_OFDM_RATE_9MB           5
#define ACX_OFDM_RATE_12MB          6
#define ACX_OFDM_RATE_18MB          7
#define ACX_OFDM_RATE_24MB          8
#define ACX_OFDM_RATE_36MB          9
#define ACX_OFDM_RATE_48MB          10
#define ACX_OFDM_RATE_54MB          11

static u8 acx_plcp_get_bitrate_cck(u8 plcp)
{
        switch (plcp) {
        case 0x0A:
                return ACX_CCK_RATE_1MB;
        case 0x14:
                return ACX_CCK_RATE_2MB;
        case 0x37:
                return ACX_CCK_RATE_5MB;
        case 0x6E:
                return ACX_CCK_RATE_11MB;
        }
        return 0;
}

/* Extract the bitrate out of an OFDM PLCP header. */
static u8 acx_plcp_get_bitrate_ofdm(u8 plcp)
{
        switch (plcp & 0xF) {
        case 0xB:
                return ACX_OFDM_RATE_6MB;
        case 0xF:
                return ACX_OFDM_RATE_9MB;
        case 0xA:
                return ACX_OFDM_RATE_12MB;
        case 0xE:
                return ACX_OFDM_RATE_18MB;
        case 0x9:
                return ACX_OFDM_RATE_24MB;
        case 0xD:
                return ACX_OFDM_RATE_36MB;
        case 0x8:
                return ACX_OFDM_RATE_48MB;
        case 0xC:
                return ACX_OFDM_RATE_54MB;
        }
        return 0;
}
#endif

static void acx_set_sane_reg_domain(acx_device_t *adev, int do_set)
{
	unsigned mask;

	unsigned int i;

	for (i = 0; i < sizeof(acx_reg_domain_ids); i++)
		if (acx_reg_domain_ids[i] == adev->reg_dom_id)
			break;

	if (sizeof(acx_reg_domain_ids) == i) {
		log(L_INIT, "Invalid or unsupported regulatory domain"
			" 0x%02X specified, falling back to FCC (USA)!"
			" Please report if this sounds fishy!\n",
			adev->reg_dom_id);
		i = 0;
		adev->reg_dom_id = acx_reg_domain_ids[i];

		/* since there was a mismatch, we need to force updating */
		do_set = 1;
	}

	if (do_set) {
		acx_ie_generic_t dom;
		memset(&dom, 0, sizeof(dom));

		dom.m.bytes[0] = adev->reg_dom_id;
		acx_configure(adev, &dom, ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
	}

	adev->reg_dom_chanmask = reg_domain_channel_masks[i];

	mask = (1 << (adev->channel - 1));
	if (!(adev->reg_dom_chanmask & mask)) {
		/* hmm, need to adjust our channel to reside within domain */
		mask = 1;
		for (i = 1; i <= 14; i++) {
			if (adev->reg_dom_chanmask & mask) {
				pr_info("%s: Adjusting the selected channel from %d "
					"to %d due to the new regulatory domain\n",
					wiphy_name(adev->ieee->wiphy), adev->channel, i);
				adev->channel = i;
				break;
			}
			mask <<= 1;
		}
	}
}

static void acx111_sens_radio_16_17(acx_device_t *adev)
{
	u32 feature1, feature2;

	if ((adev->sensitivity < 1) || (adev->sensitivity > 3)) {
		pr_info("%s: invalid sensitivity setting (1..3), "
		       "setting to 1\n", wiphy_name(adev->ieee->wiphy));
		adev->sensitivity = 1;
	}
	acx111_get_feature_config(adev, &feature1, &feature2);
	CLEAR_BIT(feature1, FEATURE1_LOW_RX | FEATURE1_EXTRA_LOW_RX);
	if (adev->sensitivity > 1)
		SET_BIT(feature1, FEATURE1_LOW_RX);
	if (adev->sensitivity > 2)
		SET_BIT(feature1, FEATURE1_EXTRA_LOW_RX);
	acx111_feature_set(adev, feature1, feature2);
}

/*
 * acx_l_update_ratevector
 *
 * Updates adev->rate_supported[_len] according to rate_{basic,oper}
 */
static void acx_update_ratevector(acx_device_t *adev)
{
	u16 bcfg = adev->rate_basic;
	u16 ocfg = adev->rate_oper;
	u8 *supp = adev->rate_supported;
	const u8 *dot11 = acx_bitpos2ratebyte;

	FN_ENTER;

	while (ocfg) {
		if (ocfg & 1) {
			*supp = *dot11;
			if (bcfg & 1) {
				*supp |= 0x80;
			}
			supp++;
		}
		dot11++;
		ocfg >>= 1;
		bcfg >>= 1;
	}
	adev->rate_supported_len = supp - adev->rate_supported;
	if (acx_debug & L_ASSOC) {
		pr_info("new ratevector: ");
		acx_dump_bytes(adev->rate_supported, adev->rate_supported_len);
	}
	FN_EXIT0;
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

/*
 * BOM Proc, Debug
 * ==================================================
 */

#if defined(CONFIG_PROC_FS) || defined(CONFIG_DEBUGC_FS)

static int acx_proc_show_diag(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	ssize_t len = 0, partlen;
	u32 temp1, temp2;
	u8 *st, *st_end;
#ifdef __BIG_ENDIAN
	u8 *st2;
#endif
	fw_stats_t *fw_stats;
	char *part_str = NULL;
	fw_stats_tx_t *tx = NULL;
	fw_stats_rx_t *rx = NULL;
	fw_stats_dma_t *dma = NULL;
	fw_stats_irq_t *irq = NULL;
	fw_stats_wep_t *wep = NULL;
	fw_stats_pwr_t *pwr = NULL;
	fw_stats_mic_t *mic = NULL;
	fw_stats_aes_t *aes = NULL;
	fw_stats_event_t *evt = NULL;

	FN_ENTER;
	acx_sem_lock(adev);

	if (IS_PCI(adev))
		acxpci_proc_diag_output(file, adev);
	else if (IS_MEM(adev))
		acxmem_proc_diag_output(file, adev);

	seq_printf(file,
		     "\n"
		     "** network status **\n"
		     "dev_state_mask 0x%04X\n"
		     "mode %u, channel %u, "
		     "reg_dom_id 0x%02X, reg_dom_chanmask 0x%04X, ",
		     adev->dev_state_mask,
		     adev->mode, adev->channel,
		     adev->reg_dom_id, adev->reg_dom_chanmask);

	seq_printf(file,
		     "ESSID \"%s\", essid_active %d, essid_len %d, "
		     "essid_for_assoc \"%s\", nick \"%s\"\n"
		     "WEP ena %d, restricted %d, idx %d\n",
		     adev->essid, adev->essid_active, (int)adev->essid_len,
		     adev->essid_for_assoc, adev->nick,
		     adev->wep_enabled, adev->wep_restricted,
		     adev->wep_current_index);
	seq_printf(file, "dev_addr  " MACSTR "\n", MAC(adev->dev_addr));
	seq_printf(file, "bssid     " MACSTR "\n", MAC(adev->bssid));
	seq_printf(file, "ap_filter " MACSTR "\n", MAC(adev->ap));

	seq_printf(file, "tx_queue len: %d\n", skb_queue_len(&adev->tx_queue));

	seq_printf(file, "\n" "** PHY status **\n"
		"tx_enabled %d, tx_level_dbm %d, tx_level_val %d,\n "
		/* "tx_level_auto %d\n" */
		"sensitivity %d, antenna[0,1] 0x%02X 0x%02X, ed_threshold %d, cca %d, preamble_mode %d\n"
		"rate_basic 0x%04X, rate_oper 0x%04X\n"
		"rts_threshold %d, frag_threshold %d, short_retry %d, long_retry %d\n"
		"msdu_lifetime %d, listen_interval %d, beacon_interval %d\n",
		adev->tx_enabled, adev->tx_level_dbm, adev->tx_level_val,
		/* adev->tx_level_auto, */
		adev->sensitivity, adev->antenna[0], adev->antenna[1],
		adev->ed_threshold,
		adev->cca, adev->preamble_mode, adev->rate_basic,
		adev->rate_oper, adev->rts_threshold,
		adev->frag_threshold, adev->short_retry, adev->long_retry,
		adev->msdu_lifetime, adev->listen_interval,
		adev->beacon_interval);

	seq_printf(file,
		"\n"
		"** Firmware **\n"
		"NOTE: version dependent statistics layout, "
		"please report if you suspect wrong parsing!\n"
		"\n" "version \"%s\"\n", adev->firmware_version);

	fw_stats = kzalloc(sizeof(*fw_stats), GFP_KERNEL);
	if (!fw_stats) {
		FN_EXIT1(0);
		return 0;
	}
	st = (u8 *) fw_stats;

	part_str = "statistics query command";

	if (OK != acx_interrogate(adev, st, ACX1xx_IE_FIRMWARE_STATISTICS))
		goto fw_stats_end;

	st += sizeof(u16);
	len = *(u16 *) st;

	if (len > sizeof(*fw_stats)) {
		seq_printf(file,
			"firmware version with bigger fw_stats struct detected\n"
			"(%zu vs. %zu), please report\n", len,
			sizeof(fw_stats_t));
		if (len > sizeof(*fw_stats)) {
			seq_printf(file, "struct size exceeded allocation!\n");
			len = sizeof(*fw_stats);
		}
	}
	st += sizeof(u16);
	st_end = st - 2 * sizeof(u16) + len;

#ifdef __BIG_ENDIAN
	/* let's make one bold assumption here:
	 * (hopefully!) *all* statistics fields are u32 only,
	 * thus if we need to make endianness corrections
	 * we can simply do them in one go, in advance */
	st2 = (u8 *) fw_stats;
	for (temp1 = 0; temp1 < len; temp1 += 4, st2 += 4)
		*(u32 *) st2 = le32_to_cpu(*(u32 *) st2);
#endif

	part_str = "Rx/Tx";

	/* directly at end of a struct part? --> no error! */
	if (st == st_end)
		goto fw_stats_end;

	tx = (fw_stats_tx_t *) st;
	st += sizeof(fw_stats_tx_t);
	rx = (fw_stats_rx_t *) st;
	st += sizeof(fw_stats_rx_t);
	partlen = sizeof(fw_stats_tx_t) + sizeof(fw_stats_rx_t);

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);

		/* our parsing doesn't quite match this firmware yet,
		 * log failure */
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = rx->rx_aci_events;
		temp2 = rx->rx_aci_resets;
	}

	seq_printf(file,
		"%s:\n"
		"  tx_desc_overfl %u\n"
		"  rx_OutOfMem %u, rx_hdr_overfl %u, rx_hw_stuck %u\n"
		"  rx_dropped_frame %u, rx_frame_ptr_err %u, rx_xfr_hint_trig %u\n"
		"  rx_aci_events %u, rx_aci_resets %u\n",
		part_str,
		tx->tx_desc_of,
		rx->rx_oom,
		rx->rx_hdr_of,
		rx->rx_hw_stuck,
		rx->rx_dropped_frame,
		rx->rx_frame_ptr_err, rx->rx_xfr_hint_trig, temp1, temp2);

	part_str = "DMA";

	if (st == st_end)
		goto fw_stats_end;

	dma = (fw_stats_dma_t *) st;
	partlen = sizeof(fw_stats_dma_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  rx_dma_req %u, rx_dma_err %u, tx_dma_req %u, tx_dma_err %u\n",
		part_str,
		dma->rx_dma_req,
		dma->rx_dma_err, dma->tx_dma_req, dma->tx_dma_err);

	part_str = "IRQ";

	if (st == st_end)
		goto fw_stats_end;

	irq = (fw_stats_irq_t *) st;
	partlen = sizeof(fw_stats_irq_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  cmd_cplt %u, fiq %u\n"
		"  rx_hdrs %u, rx_cmplt %u, rx_mem_overfl %u, rx_rdys %u\n"
		"  irqs %u, tx_procs %u, decrypt_done %u\n"
		"  dma_0_done %u, dma_1_done %u, tx_exch_complet %u\n"
		"  commands %u, rx_procs %u, hw_pm_mode_changes %u\n"
		"  host_acks %u, pci_pm %u, acm_wakeups %u\n",
		part_str,
		irq->cmd_cplt,
		irq->fiq,
		irq->rx_hdrs,
		irq->rx_cmplt,
		irq->rx_mem_of,
		irq->rx_rdys,
		irq->irqs,
		irq->tx_procs,
		irq->decrypt_done,
		irq->dma_0_done,
		irq->dma_1_done,
		irq->tx_exch_complet,
		irq->commands,
		irq->rx_procs,
		irq->hw_pm_mode_changes,
		irq->host_acks, irq->pci_pm, irq->acm_wakeups);

	part_str = "WEP";

	if (st == st_end)
		goto fw_stats_end;

	wep = (fw_stats_wep_t *) st;
	partlen = sizeof(fw_stats_wep_t);
	st += partlen;

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = wep->wep_pkt_decrypt;
		temp2 = wep->wep_decrypt_irqs;
	}

	seq_printf(file,
		"%s:\n"
		"  wep_key_count %u, wep_default_key_count %u, dot11_def_key_mib %u\n"
		"  wep_key_not_found %u, wep_decrypt_fail %u\n"
		"  wep_pkt_decrypt %u, wep_decrypt_irqs %u\n",
		part_str,
		wep->wep_key_count,
		wep->wep_default_key_count,
		wep->dot11_def_key_mib,
		wep->wep_key_not_found,
		wep->wep_decrypt_fail, temp1, temp2);

	part_str = "power";

	if (st == st_end)
		goto fw_stats_end;

	pwr = (fw_stats_pwr_t *) st;
	partlen = sizeof(fw_stats_pwr_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  tx_start_ctr %u, no_ps_tx_too_short %u\n"
		"  rx_start_ctr %u, no_ps_rx_too_short %u\n"
		"  lppd_started %u\n"
		"  no_lppd_too_noisy %u, no_lppd_too_short %u, no_lppd_matching_frame %u\n",
		part_str,
		pwr->tx_start_ctr,
		pwr->no_ps_tx_too_short,
		pwr->rx_start_ctr,
		pwr->no_ps_rx_too_short,
		pwr->lppd_started,
		pwr->no_lppd_too_noisy,
		pwr->no_lppd_too_short, pwr->no_lppd_matching_frame);

	part_str = "MIC";

	if (st == st_end)
		goto fw_stats_end;

	mic = (fw_stats_mic_t *) st;
	partlen = sizeof(fw_stats_mic_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  mic_rx_pkts %u, mic_calc_fail %u\n",
		part_str, mic->mic_rx_pkts, mic->mic_calc_fail);

	part_str = "AES";

	if (st == st_end)
		goto fw_stats_end;

	aes = (fw_stats_aes_t *) st;
	partlen = sizeof(fw_stats_aes_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  aes_enc_fail %u, aes_dec_fail %u\n"
		"  aes_enc_pkts %u, aes_dec_pkts %u\n"
		"  aes_enc_irq %u, aes_dec_irq %u\n",
		part_str,
		aes->aes_enc_fail,
		aes->aes_dec_fail,
		aes->aes_enc_pkts,
		aes->aes_dec_pkts, aes->aes_enc_irq, aes->aes_dec_irq);

	part_str = "event";

	if (st == st_end)
		goto fw_stats_end;

	evt = (fw_stats_event_t *) st;
	partlen = sizeof(fw_stats_event_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  heartbeat %u, calibration %u\n"
		"  rx_mismatch %u, rx_mem_empty %u, rx_pool %u\n"
		"  oom_late %u\n"
		"  phy_tx_err %u, tx_stuck %u\n",
		part_str,
		evt->heartbeat,
		evt->calibration,
		evt->rx_mismatch,
		evt->rx_mem_empty,
		evt->rx_pool,
		evt->oom_late, evt->phy_tx_err, evt->tx_stuck);

	if (st < st_end)
		goto fw_stats_bigger;

	goto fw_stats_end;

	fw_stats_fail:
	st -= partlen;
	seq_printf(file,
		"failed at %s part (size %zu), offset %zu (struct size %zu), "
		"please report\n", part_str, partlen,
		((void *) st - (void *) fw_stats), len);

	fw_stats_bigger:
	for (; st < st_end; st += 4)
		seq_printf(file, "UNKN%3d: %u\n", (int) ((void *) st
				- (void *) fw_stats), *(u32 *) st);

	fw_stats_end:
	kfree(fw_stats);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

/*
 * A write on acx_diag executes different operations for debugging
 */
static ssize_t acx_proc_write_diag(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
	acx_device_t *adev = (acx_device_t *) pde->data;

	ssize_t ret = -EINVAL;
	char *after;
	unsigned int val;
	size_t size;

	FN_ENTER;
	acx_sem_lock(adev);

	val = (unsigned int) simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count == size)
		ret = count;
	else
		goto exit_unlock;

	logf1(L_ANY, "acx_diag: 0x%04x\n", val);

	/* Execute operation */
	if (val == ACX_DIAG_OP_RECALIB) {
		logf0(L_ANY, "ACX_DIAG_OP_RECALIB: Scheduling immediate radio recalib\n");
		adev->recalib_time_last_success = jiffies - RECALIB_PAUSE * 60 * HZ;
		acx_schedule_task(adev, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_PROCESS_TX_RX) {
		logf0(L_ANY, "ACX_DIAG_OP_PROCESS_TX_RX: Scheduling immediate Rx, Tx processing\n");

		if (IS_PCI(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_COMPLETE);
		else if (IS_MEM(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_DATA);

		SET_BIT(adev->irq_reason, HOST_INT_TX_COMPLETE);
		acx_schedule_task(adev, 0);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_REINIT_TX_BUF) {
		if (IS_MEM(adev)) {
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF\n");
			acxmem_init_acx_txbuf2(adev);
		} else
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF: Only valid for mem device\n");
	}
	/* Unknown */
	else
		logf1(L_ANY, "Unknown command: 0x%04x\n", val);

exit_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

/*
 * acx_e_read_proc_XXXX
 * Handle our /proc entry
 *
 * Arguments:
 *	standard kernel read_proc interface
 * Returns:
 *	number of bytes written to buf
 * Side effects:
 *	none
 */
static int acx_proc_show_acx(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t*) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	seq_printf(file,
		"acx driver version:\t\t%s (git: %s)\n"
		"Wireless extension version:\t%s\n"
		"chip name:\t\t\t%s (0x%08X)\n"
		"radio type:\t\t\t0x%02X\n"
		"form factor:\t\t\t0x%02X\n"
		     "EEPROM version:\t\t\t0x%02X\n"
		"firmware version:\t\t%s (0x%08X)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown",
		STRING(WIRELESS_EXT),
		adev->chip_name, adev->firmware_id,
		adev->radio_type,
		adev->form_factor,
		adev->eeprom_version,
		adev->firmware_version, adev->firmware_numver);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static int acx_proc_show_eeprom(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int length;
	char *buf, *p;

	FN_ENTER;
	acx_sem_lock(adev);

	if (IS_PCI(adev) || IS_MEM(adev))
		buf = acx_proc_eeprom_output(&length, adev);
	else
		goto out;

	for (p = buf; p < buf + length; p++)
	     seq_putc(file, *p);

	kfree(buf);
out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static int acx_proc_show_phy(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int i;
	char *buf, *p;
	/* OW Hopefully enough */
	const int buf_size = 1024*64;

	FN_ENTER;
	acx_sem_lock(adev);

	buf = kmalloc(buf_size, GFP_KERNEL);
	/*
	   if (RADIO_11_RFMD != adev->radio_type) {
	   pr_info("sorry, not yet adapted for radio types "
	   "other than RFMD, please verify "
	   "PHY size etc. first!\n");
	   goto end;
	   }
	 */

	/* The PHY area is only 0x80 bytes long; further pages after that
	 * only have some page number registers with altered value,
	 * all other registers remain the same. */
	p = buf;
	for (i = 0; i < 0x80; i++) {
		acx_read_phy_reg(adev, i, p++);
		seq_putc(file, *p);
	}

	kfree(buf);

	acx_sem_unlock(adev);
	FN_EXIT0;

	return 0;
}

static int acx_proc_show_debug(struct seq_file *file, void *v)
{
	FN_ENTER;
	/* No sem locking required, since debug is global for all devices */

	seq_printf(file, "acx_debug: 0x%04x\n", acx_debug);

	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_debug(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val = simple_strtoul(buf, &after, 0);
	size_t size = after - buf + 1;

	FN_ENTER;
	/* No sem locking required, since debug is global for all devices */

	if (count == size) {
		ret = count;
		acx_debug = val;
	}

	log(L_ANY, "%s: acx_debug=0x%04x\n", __func__, acx_debug);

	FN_EXIT0;
	return ret;
}

static int acx_proc_show_sensitivity(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx_get_sensitivity(adev);
	seq_printf(file, "acx_sensitivity: %d\n", adev->sensitivity);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_sensitivity(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)

{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val;
	size_t size;

	FN_ENTER;
	acx_sem_lock(adev);

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_sensitivity(adev, val);
	logf1(L_ANY, "acx_sensitivity=%d\n", adev->sensitivity);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

static int acx_proc_show_tx_level(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx1xx_get_tx_level(adev);
	seq_printf(file, "tx_level_dbm: %d\n", adev->tx_level_dbm);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx111_proc_write_tx_level(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val;
	size_t size;

	FN_ENTER;
	acx_sem_lock(adev);

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	logf1(L_ANY, "tx_level_val=%d\n", adev->tx_level_val);
	acx1xx_set_tx_level(adev, val);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

static int acx_proc_show_reg_domain(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx_get_reg_domain(adev);
	seq_printf(file, "reg_dom_id: 0x%02x\n", adev->reg_dom_id);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_reg_domain(struct file *file,
					const char __user *buf,
					size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val;
	size_t size;

	FN_ENTER;
	acx_sem_lock(adev);

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_reg_domain(adev, val);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}


static int acx_proc_show_antenna(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx1xx_get_antenna(adev);
	seq_printf(file, "antenna[0,1]: 0x%02x 0x%02x\n",
		adev->antenna[0], adev->antenna[1]);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_antenna(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val;
	u8 val0, val1;
	size_t size;

	FN_ENTER;
	acx_sem_lock(adev);

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	val0 = (u8) (val & 0xFF);
	val1 = (u8) ((val >> 8) & 0xFF);
	acx1xx_set_antenna(adev, val0, val1);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

#if 0 /* now in debugfs.c */
static int acx_proc_open(struct inode *inode, struct file *file)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		if (!strcmp(proc_files[i],
			    file->f_path.dentry->d_name.name))
			break;
	}
	/* log(L_ANY, "%s: proc filename=%s\n", __func__, proc_files[i]); */

	return single_open(file, acx_proc_show_funcs[i], PDE(inode)->data);
}

static void acx_proc_init(void)
{
	int i;

	/* acx_e_proc_ops init */
	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		acx_e_proc_ops[i].owner = THIS_MODULE;
		acx_e_proc_ops[i].open = acx_proc_open;
		acx_e_proc_ops[i].read = seq_read;
		acx_e_proc_ops[i].llseek = seq_lseek;
		acx_e_proc_ops[i].release = single_release;
		acx_e_proc_ops[i].write = acx_proc_write_funcs[i];
	}
}

int acx_proc_register_entries(struct ieee80211_hw *hw)
{
	acx_device_t *adev = ieee2adev(hw);
	char procbuf[80];
	char procbuf2[80];
	int i;
	struct proc_dir_entry *pe;

	FN_ENTER;

	/* Sub-dir for this acx_phy[0-9] instance */

	/* I tried to create a /proc/driver/acx sub-dir in acx_proc_init()
	 * to put the phy[0-9] into, but for some bizarre reason the proc-fs
	 * refuses then to create the phy[0-9] dirs in /proc/driver/acx !?
	 * It only works, if /proc/driver/acx is created here in
	 * acx_proc_register_entries().
	 * ... Anyway, we should swap to sysfs.
	 */
	snprintf(procbuf2, sizeof(procbuf2), "driver/acx_%s",
		wiphy_name(adev->ieee->wiphy));

	proc_mkdir(procbuf2, NULL);

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		snprintf(procbuf, sizeof(procbuf), "%s/%s",
			procbuf2, proc_files[i]);
		log(L_INIT, "creating proc entry /proc/%s\n", procbuf);

		/* Read-only */
		if (acx_proc_write_funcs[i] == NULL)
			pe = proc_create(procbuf, 0444, NULL,
					&acx_e_proc_ops[i]);
		/* Read-Write */
		else
			pe = proc_create(procbuf, 0644, NULL,
					&acx_e_proc_ops[i]);

		if (!pe) {
			pr_info("cannot register proc entry /proc/%s\n",
				procbuf);
			return NOT_OK;
		}
		pe->data = adev;
	}
	FN_EXIT0;
	return OK;
}

int acx_proc_unregister_entries(struct ieee80211_hw *hw)
{
	acx_device_t *adev = ieee2adev(hw);
	char procbuf[80];
	char procbuf2[80];
	int i;

	FN_ENTER;

	/* Subdir for this acx instance */
	snprintf(procbuf2, sizeof(procbuf2), "driver/acx_%s",
		wiphy_name(adev->ieee->wiphy));

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		snprintf(procbuf, sizeof(procbuf), "%s/%s", procbuf2,
			proc_files[i]);
		log(L_INIT, "removing proc entry /proc/%s\n", procbuf);
		remove_proc_entry(procbuf, NULL);
	}
	remove_proc_entry(procbuf2, NULL);

	FN_EXIT0;
	return OK;
}
#else /* now in debugfs.c */
void acx_proc_init(void) { }	/* stub */
#endif /* now in debugfs.c */

#endif /* defined(CONFIG_PROC_FS) || defined(CONFIG_DEBUGC_FS) */

/*
 * BOM Rx Path
 * ==================================================
 */

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
	buflen = RXBUF_BYTES_USED(rxbuf) - ((u8*)w_hdr - (u8*)rxbuf);

	/* Allocate our skb */
	skb = dev_alloc_skb(buflen + 2);
	if (!skb) {
		pr_info("skb allocation FAILED\n");
		goto out;
	}

	skb_reserve(skb, 2);
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

	status->flag = 0;

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
 * BOM Tx Path
 * ==================================================
 */

/* TODO: consider defining OP_TX_RET_TYPE, OP_TX_RET_VAL in
 * acx_compat, and hiding this #if/else.  OTOH, inclusion doesnt care
 * about old kernels
 */
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
int
#else
void
#endif
acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	acx_device_t *adev = ieee2adev(hw);

	skb_queue_tail(&adev->tx_queue, skb);

	ieee80211_queue_work(adev->ieee, &adev->tx_work);

	if (skb_queue_len(&adev->tx_queue) >= ACX_TX_QUEUE_MAX_LENGTH)
		acx_stop_queue(adev->ieee, NULL);

	#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
	return NETDEV_TX_OK;
	#else
	return;
	#endif
}

void acx_tx_work(struct work_struct *work)
{
	acx_device_t *adev = container_of(work, struct acx_device, tx_work);

	FN_ENTER;

	acx_sem_lock(adev);

	if (unlikely(!adev))
		goto out;

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP)))
		goto out;

	if (unlikely(!adev->initialized))
		goto out;

	acx_tx_queue_go(adev);
out:
	acx_sem_unlock(adev);
	FN_EXIT0;
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
			acx_stop_queue(adev->ieee, NULL);
			skb_queue_head(&adev->tx_queue, skb);
			goto out;
		} else if (ret < 0) {
			logf0(L_BUF, "Other ERR: (Card was removed ?!):"
				" Stop queue. Dealloc skb.\n");
			acx_stop_queue(adev->ieee, NULL);
			dev_kfree_skb(skb);
			goto out;
		}

		/* Keep a few free descs between head and tail of tx
		 * ring. It is not absolutely needed, just feels
		 * safer */
		if (adev->tx_free < TX_STOP_QUEUE) {
			logf1(L_BUF, "Tx_free<TX_STOP_QUEUE (%u tx desc left):"
				" Stop queue.\n", adev->tx_free);
			acx_stop_queue(adev->ieee, NULL);
			goto out;
		}
	}
out:
	return;
}

int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb)
{
	tx_t *tx;
	void *txbuf;
	struct ieee80211_tx_info *ctl;
	ctl = IEEE80211_SKB_CB(skb);

	tx = acx_alloc_tx(adev, skb->len);

	if (unlikely(!tx)) {
		logf0(L_BUFT, "No tx available\n");
		return (-EBUSY);
	}

	txbuf = acx_get_txbuf(adev, tx);

	if (unlikely(!txbuf)) {
		/* Card was removed */
		logf0(L_BUF, "Txbuf==NULL. (Card was removed ?!):"
			" Stop queue. Dealloc skb.\n");

		/* OW only USB implemented */
		acx_dealloc_tx(adev, tx);
		return (-ENXIO);
	}

	/* OW, 20100930: FIXME: Is this required for mem. txbuf is actually not containing to the data
	 * for the, but actually "addr = acxmem_allocate_acx_txbuf_space in acxmem_tx_data().
	 */

	memcpy(txbuf, skb->data, skb->len);

	acx_tx_data(adev, tx, skb->len, ctl, skb);

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

		ieee80211_tx_status(adev->ieee, skb);
	}
}

void acx_stop_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_stop_queues(hw);
	if (msg)
		log(L_BUFT, "tx: stop queue %s\n", msg);
	FN_EXIT0;
}

int acx_queue_stopped(struct ieee80211_hw *ieee)
{
	return ieee80211_queue_stopped(ieee, 0);
}

void acx_wake_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_wake_queues(hw);
	if (msg)
		log(L_BUFT, "tx: wake queue %s\n", msg);
	FN_EXIT0;
}

/*
 * OW Included skb->len to check required blocks upfront in
 * acx_l_alloc_tx This should perhaps also go into pci and usb ?
 */
tx_t* acx_alloc_tx(acx_device_t *adev, unsigned int len)
{
	if (IS_PCI(adev))
		return acxpci_alloc_tx(adev);
	if (IS_USB(adev))
		return acxusb_alloc_tx(adev);
	if (IS_MEM(adev))
		return acxmem_alloc_tx(adev, len);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);
	return (NULL);
}

static void acx_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_USB(adev))
		acxusb_dealloc_tx(tx_opaque);
	if (IS_MEM(adev))
		acxmem_dealloc_tx (adev, tx_opaque);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);
	return;
}

static void* acx_get_txbuf(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_get_txbuf(adev, tx_opaque);
	if (IS_USB(adev))
		return acxusb_get_txbuf(adev, tx_opaque);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);
	return (NULL);
}

static void acx_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
	struct ieee80211_tx_info *ieeectl, struct sk_buff *skb)
{
	if (IS_PCI(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_USB(adev))
		return acxusb_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_MEM(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb);

	log(L_ANY, "%s: Unsupported dev_type=%i\n",
		__func__, (adev)->dev_type);
	return;
}

u16 acx111_tx_build_rateset(acx_device_t *adev, txdesc_t *txdesc,
			struct ieee80211_tx_info *info)
{
	int i;

	char tmpstr[256];
	struct ieee80211_rate *tmpbitrate;
	int tmpcount;

	u16 rateset = 0;

	int debug = acx_debug & L_BUFT;

	if (debug) {
		#if defined(CONFIG_ACX_MAC80211_PCI) || \
			defined(CONFIG_ACX_MAC80211_MEM)
		i = ((u8*) txdesc - (u8*) adev->tx.desc_start)
			/ adev->tx.desc_size;
		sprintf(tmpstr, "txdesc=%i: rates in info"
			"[bitrate,hw_value,count]: ", i);
		#endif
	}

	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		if (info->control.rates[i].idx < 0)
			break;

		tmpbitrate = &adev->ieee->wiphy->bands[info->band]
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
					wiphy_name(adev->ieee->wiphy));

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
			wiphy_name(adev->ieee->wiphy), error, finger, err);
	else
		log(log_level, "%s: tx error 0x%02X, buf %02u!\n",
			wiphy_name(adev->ieee->wiphy), error, finger);
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


/*
 * BOM Crypto
 * ==================================================
 */
#ifdef UNUSED
static void acx100_set_wepkey(acx_device_t *adev)
{
	ie_dot11WEPDefaultKey_t dk;
	int i;

	for (i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++) {
		if (adev->wep_keys[i].size != 0) {
			log(L_INIT, "setting WEP key: %d with "
				"total size: %d\n", i,
				(int)adev->wep_keys[i].size);
			dk.action = 1;
			dk.keySize = adev->wep_keys[i].size;
			dk.defaultKeyNum = i;
			memcpy(dk.key, adev->wep_keys[i].key, dk.keySize);
			acx_configure(adev, &dk,
				ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE);
		}
	}
}

static void acx111_set_wepkey(acx_device_t * adev)
{
	acx111WEPDefaultKey_t dk;
	int i;

	for (i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++) {
		if (adev->wep_keys[i].size != 0) {
			log(L_INIT, "setting WEP key: %d with "
				"total size: %d\n", i,
				(int)adev->wep_keys[i].size);
			memset(&dk, 0, sizeof(dk));
			dk.action = cpu_to_le16(1);
			/* "add key"; yes, that's a 16bit value */
			dk.keySize = adev->wep_keys[i].size;

			/* are these two lines necessary? */
			dk.type = 0;	/* default WEP key */
			dk.index = 0;	/* ignored when setting default key */

			dk.defaultKeyNum = i;
			memcpy(dk.key, adev->wep_keys[i].key, dk.keySize);
			acx_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &dk,
				sizeof(dk));
		}
	}
}

static void acx_set_wepkey(acx_device_t * adev)
{
	if (IS_ACX111(adev))
		acx111_set_wepkey(adev);
	else
		acx100_set_wepkey(adev);
}
#endif

/*
 * acx100_s_init_wep
 *
 * FIXME: this should probably be moved into the new card settings
 * management, but since we're also modifying the memory map layout
 * here due to the WEP key space we want, we should take care...
 */
static int acx100_init_wep(acx_device_t * adev)
{
	/* acx100_ie_wep_options_t options;
	 * ie_dot11WEPDefaultKeyID_t dk;
	 */
	acx_ie_memmap_t pt;
	int res = NOT_OK;

	FN_ENTER;

	if (OK != acx_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP))
		goto fail;

	log(L_DEBUG, "CodeEnd:%X\n", pt.CodeEnd);

	pt.WEPCacheStart = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);
	pt.WEPCacheEnd = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);

	if (OK != acx_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP))
		goto fail;

/* OW: This disables WEP by not configuring the WEP cache and leaving
 * WEPCacheStart=WEPCacheEnd.
 *
 * When doing the crypto by mac80211 it is required, that the acx is
 * not doing any WEP crypto himself. Otherwise TX "WEP key not found"
 * errors occure.
 *
 * By disabling WEP using WEPCacheStart=WEPCacheStart the acx not
 * trying any own crypto anymore. All crypto (including WEP) is pushed
 * to mac80211 for the moment.
 *
 */
#if 0

	/* let's choose maximum setting: 4 default keys, plus 10 other keys: */
	options.NumKeys = cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10);
	options.WEPOption = 0x00;

	log(L_ASSOC, "writing WEP options\n");
	acx_configure(adev, &options, ACX100_IE_WEP_OPTIONS);

	acx100_set_wepkey(adev);

	if (adev->wep_keys[adev->wep_current_index].size != 0) {
		log(L_ASSOC, "setting active default WEP key number: %d\n",
			adev->wep_current_index);
		dk.KeyID = adev->wep_current_index;
		acx_configure(adev, &dk,
			ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);	/* 0x1010 */
	}
	/* FIXME!!! wep_key_struct is filled nowhere! But adev
	 * is initialized to 0, and we don't REALLY need those keys either */
/*		for (i = 0; i < 10; i++) {
		if (adev->wep_key_struct[i].len != 0) {
			MAC_COPY(wep_mgmt.MacAddr, adev->wep_key_struct[i].addr);
			wep_mgmt.KeySize = cpu_to_le16(adev->wep_key_struct[i].len);
			memcpy(&wep_mgmt.Key, adev->wep_key_struct[i].key, le16_to_cpu(wep_mgmt.KeySize));
			wep_mgmt.Action = cpu_to_le16(1);
			log(L_ASSOC, "writing WEP key %d (len %d)\n", i, le16_to_cpu(wep_mgmt.KeySize));
			if (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &wep_mgmt, sizeof(wep_mgmt))) {
				adev->wep_key_struct[i].index = i;
			}
		}
	}
*/

	/* now retrieve the updated WEPCacheEnd pointer... */
	if (OK != acx_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		pr_info("%s: ACX1xx_IE_MEMORY_MAP read #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
#endif

	/* ...and tell it to start allocating templates at that location */
	/* (no endianness conversion needed) */
	pt.PacketTemplateStart = pt.WEPCacheEnd;

	if (OK != acx_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		pr_info("%s: ACX1xx_IE_MEMORY_MAP write #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
	res = OK;

fail:
	FN_EXIT1(res);
	return res;
}

/* OW, 20100704, Obselete, TBC for cleanup */
#if 0
static void acx_keymac_write(acx_device_t *adev, u16 index, const u32 *addr)
{
	/* for keys 0-3 there is no associated mac address */
	if (index < 4)
		return;

	index -= 4;
	if (1) {
		TODO();
/*
                bcm43xx_shm_write32(bcm,
                                    BCM43xx_SHM_HWMAC,
                                    index * 2,
                                    cpu_to_be32(*addr));
                bcm43xx_shm_write16(bcm,
                                    BCM43xx_SHM_HWMAC,
                                    (index * 2) + 1,
                                    cpu_to_be16(*((u16 *)(addr + 1))));
*/
	} else {
		if (index < 8)
			TODO();	/* Put them in the macaddress filter */
		else
			TODO();
			/* Put them BCM43xx_SHM_SHARED, stating index 0x0120.
			   Keep in mind to update the count of keymacs in 0x003 */
	}
}

int acx_clear_keys(acx_device_t *adev)
{
	static const u32 zero_mac[2] = { 0 };
	unsigned int i, j, nr_keys = 54;
	u16 offset;

	/* FixMe:Check for Number of Keys available */

/*        assert(nr_keys <= ARRAY_SIZE(adev->key)); */

	for (i = 0; i < nr_keys; i++) {
		adev->key[i].enabled = 0;
		/* returns for i < 4 immediately */
		acx_keymac_write(adev, i, zero_mac);
/*
                bcm43xx_shm_write16(adev, BCM43xx_SHM_SHARED,
                                    0x100 + (i * 2), 0x0000);
*/
		for (j = 0; j < 8; j++) {
			offset =
			    adev->security_offset + (j * 4)
				+ (i * ACX_SEC_KEYSIZE);
/*
                        bcm43xx_shm_write16(bcm, BCM43xx_SHM_SHARED,
                                            offset, 0x0000);
*/
		}
	}
	return 1;
}

int acx_key_write(acx_device_t *adev, u16 index, u8 algorithm,
		const struct ieee80211_key_conf *key, const u8 *mac_addr)
{
	int result;

	FN_ENTER;

/*       log(L_IOCTL, "set encoding flags=0x%04X, size=%d, key: %s\n",
 *                       dwrq->flags, dwrq->length, extra ? "set" : "No key");
 */

/*	acx_sem_lock(adev); */

	/* index = (dwrq->flags & IW_ENCODE_INDEX) - 1; */
	if (key->keylen > 0) {

		/* if index is 0 or invalid, use default key */
		if (index > 3)
			index = (int) adev->wep_current_index;

		if ((algorithm == ACX_SEC_ALGO_WEP) ||
			  (algorithm == ACX_SEC_ALGO_WEP104)) {

			switch (key->keylen) {
			case 40 / 8:
				/* WEP 40-bit =
				 40-bit  entered key + 24 bit IV = 64-bit */
				/* adev->wep_keys[index].size = 13; */

				adev->wep_keys[index].size = 5;
				break;

			case 104 / 8:
				/* WEP 104-bit =
				 104-bit entered key + 24-bit IV = 128-bit */
				/* adev->wep_keys[index].size = 29; */
				adev->wep_keys[index].size = 13;
				break;

			case 128 / 8:
				/* WEP 128-bit =
				 128-bit entered key + 24 bit IV = 152-bit */
				adev->wep_keys[index].size = 29;
				break;

			default:
				adev->wep_keys[index].size = 0;
				return -EINVAL; /* shouldn't happen */
			}

			memset(adev->wep_keys[index].key, 0,
				sizeof(adev->wep_keys[index].key));
			memcpy(adev->wep_keys[index].key, key->key,
				key->keylen);

			adev->wep_current_index = index;

		} else {
			/* set transmit key */
			if (index <= 3)
				adev->wep_current_index = index;
		}

	}

	adev->wep_enabled = ((algorithm == ACX_SEC_ALGO_WEP)
			|| (algorithm == ACX_SEC_ALGO_WEP104));

/*
        adev->wep_enabled = !(dwrq->flags & IW_ENCODE_DISABLED);

        if (algorithm & IW_ENCODE_OPEN) {
                adev->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
                adev->wep_restricted = 0;

        } else if (algorithm & IW_ENCODE_RESTRICTED) {
                adev->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
                adev->wep_restricted = 1;
        }
*/
/*	adev->auth_alg = algorithm; */
	/* set flag to make sure the card WEP settings get updated */

	/* OW
	 if (adev->wep_enabled) {
		SET_BIT(adev->set_mask, GETSET_WEP);
		acx_s_update_card_settings(adev);
		acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	 }
  */

	/*
        log(L_IOCTL, "len=%d, key at 0x%p, flags=0x%X\n",
                dwrq->length, extra, dwrq->flags);
        for (index = 0; index <= 3; index++) {
                if (adev->wep_keys[index].size) {
                        log(L_IOCTL,    "acx: index=%d, size=%d, key at 0x%p\n",
                                adev->wep_keys[index].index,
                                (int) adev->wep_keys[index].size,
                                adev->wep_keys[index].key);
                }
        }
*/

	result = -EINPROGRESS;
/*	acx_sem_unlock(adev); */

	FN_EXIT1(result);
	return result;
}
#endif

/*
 * BOM Irq Handling, Timer
 * ==================================================
 */
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
	FN_ENTER;

	if (!adev->after_interrupt_jobs || !adev->initialized)
		goto end_no_lock;	/* no jobs to do */

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
		pr_info("%s: Jobs still to be run: 0x%02X\n",
			__func__, adev->after_interrupt_jobs);
		adev->after_interrupt_jobs = 0;
	}
end_no_lock:
	FN_EXIT0;
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

	FN_ENTER;

	FIXME();
	/* We need calibration and stats gather tasks to perform here */

	FN_EXIT0;
}

/*
 * acx_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 */
void acx_set_timer(acx_device_t *adev, int timeout_us)
{
	FN_ENTER;

	log(L_DEBUG | L_IRQ, "%s(%u ms)\n", __func__, timeout_us / 1000);
	if (!(adev->dev_state_mask & ACX_STATE_IFACE_UP)) {
		pr_info("attempt to set the timer "
		       "when the card interface is not up!\n");
		goto end;
	}

	/* first check if the timer was already initialized, THEN modify it */
	if (adev->mgmt_timer.function) {
		mod_timer(&adev->mgmt_timer,
			  jiffies + (timeout_us * HZ / 1000000));
	}
end:
	FN_EXIT0;
}

/*
 * BOM Mac80211 Ops
 * ==================================================
 */
int acx_debugfs_add_adev(struct acx_device *adev);
void acx_debugfs_remove_adev(struct acx_device *adev);

int acx_op_add_interface(struct ieee80211_hw *ieee, struct ieee80211_vif *vif)
{
	acx_device_t *adev = ieee2adev(ieee);
	int err = -EOPNOTSUPP;

	u8 *mac_vif;
	char mac[] = MACSTR; // approximate max length

	int vif_type;

	FN_ENTER;
	acx_sem_lock(adev);

	vif_type = vif->type;
	adev->vif_type = vif_type;
	log(L_ANY, "vif_type=%04X\n", vif_type);

	if (vif_type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor++;
	else if (adev->vif_operating)
		goto out_unlock;

	adev->vif_operating = 1;
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
	adev->vif = vif->vif;
	mac_vif = vif->mac_addr;
#else
	adev->vif = vif;
	mac_vif = vif->addr;
#endif

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

		acx1xx_set_station_id(adev, mac_vif);
		SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	}

	acx_update_mode(adev);
	acx_debugfs_add_adev(adev);

	pr_info("Virtual interface added (type: 0x%08X, MAC: %s)\n",
		adev->vif_type,	acx_print_mac(mac, vif->addr));

	err = 0;

out_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}

void acx_op_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	acx_device_t *adev = ieee2adev(hw);

	char mac[] = MACSTR; // approximate max length

	FN_ENTER;
	acx_sem_lock(adev);
	acx_debugfs_remove_adev(adev);

	if (vif->type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor--;
	else {
		adev->vif_operating = 0;
		adev->vif=NULL;
	}

	acx_set_mode(adev, ACX_MODE_OFF);

	log(L_DEBUG, "vif_operating=%d, vif->type=%d\n",
		adev->vif_operating, vif->type);

	log(L_ANY, "Virtual interface removed: type=%d, MAC=%s\n",
		vif->type, acx_print_mac(mac, vif->addr));

	acx_sem_unlock(adev);
	FN_EXIT0;
}

/* FUNCTION_GREP_RESET
 * The function_grep script can get confused with multiple "{"" opening braces
 * due e.g. due to #ifdefs. This tag reset the parser state of the script.
 */

int acx_op_config(struct ieee80211_hw *hw, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);
	struct ieee80211_conf *conf = &hw->conf;

	u32 changed_not_done = changed;

	FN_ENTER;
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
	FN_EXIT0;
	return 0;
}

/* Find position of TIM IE */
static u8* acx_beacon_find_tim(struct sk_buff *beacon_skb)
{
	u8 *p1, *p2, *tim;
	int len1;

	struct wlan_ie_base {
		u8 eid;
		u8 len;
	} __attribute__ ((packed));
	struct wlan_ie_base *ie;

	p1 = beacon_skb->data;
	len1 = beacon_skb->len;
	p2 = p1;
	p2 += offsetof(struct ieee80211_mgmt, u.beacon.variable);

	tim = p2;
	while (tim < p1 + len1) {
		ie = (struct wlan_ie_base*) tim;
		if (ie->eid == WLAN_EID_TIM)
			break;
		tim += ie->len + 2;
	}
	if (tim >= p1 + len1) {
		logf0(L_ANY, "No TIM IE found\n");
		return NULL;
	}
	return tim;
}

void acx_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);

	int err = -ENODEV;

	struct sk_buff *beacon;

	FN_ENTER;
	acx_sem_lock(adev);

	logf1(L_DEBUG, "changed=%04X\n", changed);

	if (!adev->vif_operating)
		goto end_sem_unlock;

	if (changed & BSS_CHANGED_BSSID) {
		MAC_COPY(adev->bssid, info->bssid);
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

		acx_set_beacon(adev, beacon);
		adev->beacon_interval = info->beacon_int;

		dev_kfree_skb(beacon);
	}
	err = 0;

end_sem_unlock:
	acx_sem_unlock(adev);
	FN_EXIT1(err);
	return;
}

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key)
{
	struct acx_device *adev = ieee2adev(hw);
	/* unsigned long flags; */
	u8 algorithm;
	/* u16 index; */
	int err = -EINVAL;

	FN_ENTER;
	acx_sem_lock(adev);

	/* OW Mac80211 SW crypto support:
	 *
	 * For the moment we do all crypto in sw with mac80211.
	 * Cpu cycles are cheap, and acx100 can do only WEP in hw anyway.
	 * TODO WEP hw support can still be added later, if required.
	 */

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        switch (key->alg) {
#else
	switch (key->cipher) {
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_WEP:
                if (key->keylen == 5) {
                    algorithm = ACX_SEC_ALGO_WEP;
                    log(L_INIT, "%s: algorithm=%i: %s\n",
			    __func__, algorithm, "ACX_SEC_ALGO_WEP");
                } else {
                    algorithm = ACX_SEC_ALGO_WEP104;
                    log(L_INIT, "%s: algorithm=%i: %s\n",
			    __func__, algorithm, "ACX_SEC_ALGO_WEP104");
                }
                /* OW Let's try WEP in mac80211 sw */
                err = -EOPNOTSUPP;
                break;
#else
	case WLAN_CIPHER_SUITE_WEP40:
	        algorithm = ACX_SEC_ALGO_WEP;
                log(L_INIT, "%s: algorithm=%i: %s\n",
			__func__, algorithm, "ACX_SEC_ALGO_WEP");
                err = -EOPNOTSUPP;
                break;

        case WLAN_CIPHER_SUITE_WEP104:
                algorithm = ACX_SEC_ALGO_WEP104;
                log(L_INIT, "%s: algorithm=%i: %s\n",
			__func__, algorithm, "ACX_SEC_ALGO_WEP104");
                err = -EOPNOTSUPP;
                break;
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_TKIP:
#else
	case WLAN_CIPHER_SUITE_TKIP:
#endif
	        algorithm = ACX_SEC_ALGO_TKIP;
	        log(L_INIT, "%s: algorithm=%i: %s\n",
			__func__, algorithm, "ACX_SEC_ALGO_TKIP");
	        err = -EOPNOTSUPP;
	        break;

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_CCMP:
#else
	case WLAN_CIPHER_SUITE_CCMP:
#endif
		algorithm = ACX_SEC_ALGO_AES;
		log(L_INIT, "%s: algorithm=%i: %s\n",
			__func__, algorithm, "ACX_SEC_ALGO_AES");
		err = -EOPNOTSUPP;
		break;

	default:
		FIXME();
		algorithm = ACX_SEC_ALGO_NONE;
		err = -EOPNOTSUPP;
	}

	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;

	/* OW Everything below this lines, doesn't matter anymore for the moment. */
#if 0
	index = (u8) (key->keyidx);
	if (index >= ARRAY_SIZE(adev->key))
		goto out;

	acx_lock(adev, flags);

	switch (cmd) {
	case SET_KEY:
		err = acx_key_write(adev, index, algorithm, key, addr);
		if (err != -EINPROGRESS)
			goto out_unlock;

		key->hw_key_idx = index;

		/* CLEAR_BIT(key->flags, IEEE80211_KEY_FORCE_SW_ENCRYPT); */
		/* if (CHECK_BIT(key->flags, IEEE80211_KEY_DEFAULT_TX_KEY))
		 adev->default_key_idx = index;*/

		SET_BIT(key->flags, IEEE80211_KEY_FLAG_GENERATE_IV);
		adev->key[index].enabled = 1;
		err = 0;

		break;

	case DISABLE_KEY:
		adev->key[index].enabled = 0;
		err = 0;
		break;
		/* case ENABLE_COMPRESSION:
		 case DISABLE_COMPRESSION:
		 err = 0;
		 break; */
	}

	out_unlock:
	acx_unlock(adev, flags);

	if (adev->wep_enabled) {
		SET_BIT(adev->set_mask, GETSET_WEP);
		acx_update_card_settings(adev);
		/* acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG); */
	}
out:
	FN_EXIT0;
	return err;
#endif
}

void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;

	acx_sem_lock(adev);

	logf1(L_DEBUG, "1: changed_flags=0x%08x, *total_flags=0x%08x\n",
		changed_flags, *total_flags);

	/* OWI TODO: Set also FIF_PROBE_REQ ? */
	*total_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);

	logf1(L_DEBUG, "2: *total_flags=0x%08x\n", *total_flags);

	acx_sem_unlock(adev);
	FN_EXIT0;
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
	FN_ENTER;
	acx_sem_lock(adev);
    /* TODO */
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;
	acx_sem_lock(adev);

	memcpy(stats, &adev->ieee_stats, sizeof(*stats));

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set)
{
	acx_device_t *adev = ieee2adev(hw);

	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_TIM);

	return 0;
}

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

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			 struct ieee80211_tx_queue_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);
	int err = -ENODEV;

	FN_ENTER;
	acx_sem_lock(adev);

	stats->len = 0;
	stats->limit = TX_CNT;
	stats->count = 0;

	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}
#endif

/*
 * BOM Helpers
 * ==================================================
 */

/*
 * Basically a mdelay/msleep with logging
 */
void acx_mwait(int ms)
{
	FN_ENTER;
	msleep(ms);
	FN_EXIT0;
}

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
		"MGMT/Disassoc", "MGMT/Authen", "MGMT/Deauthen"
	};
	static const char * const ctl_arr[] = {
		"CTL/PSPoll", "CTL/RTS", "CTL/CTS", "CTL/Ack", "CTL/CFEnd",
		"CTL/CFEndCFAck"
	};
	static const char * const data_arr[] = {
		"DATA/DataOnly", "DATA/Data CFAck", "DATA/Data CFPoll",
		"DATA/Data CFAck/CFPoll", "DATA/Null", "DATA/CFAck",
		"DATA/CFPoll", "DATA/CFAck/CFPoll"
	};
	const char *str;
	u8 fstype = (IEEE80211_FCTL_STYPE & fc) >> 4;
	u8 ctl;

	switch (IEEE80211_FCTL_FTYPE & fc) {
	case IEEE80211_FTYPE_MGMT:
		if (fstype < ARRAY_SIZE(mgmt_arr))
			str = mgmt_arr[fstype];
		else
			str = "MGMT/UNKNOWN";
		break;
	case IEEE80211_FTYPE_CTL:
		ctl = fstype - 0x0a;
		if (ctl < ARRAY_SIZE(ctl_arr))
			str = ctl_arr[ctl];
		else
			str = "CTL/UNKNOWN";
		break;
	case IEEE80211_FTYPE_DATA:
		if (fstype < ARRAY_SIZE(data_arr))
			str = data_arr[fstype];
		else
			str = "DATA/UNKNOWN";
		break;
	default:
		str = "UNKNOWN";
		break;
	}
	return str;
}

#if CMD_DISCOVERY
void great_inquisitor(acx_device_t * adev)
{
	static struct {
		u16 type;
		u16 len;
		/* 0x200 was too large here: */
		u8 data[0x100 - 4];
	} ACX_PACKED ie;
	u16 type;

	FN_ENTER;

	/* 0..0x20, 0x1000..0x1020 */
	for (type = 0; type <= 0x1020; type++) {
		if (type == 0x21)
			type = 0x1000;
		ie.type = cpu_to_le16(type);
		ie.len = cpu_to_le16(sizeof(ie) - 4);
		acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, &ie, sizeof(ie));
	}
	FN_EXIT0;
}
#endif


/*
 * BOM Driver, Module
 * ==================================================
 */
int __init acx_debugfs_init(void);
void __init acx_debugfs_exit(void);

static int __init acx_init_module(void)
{
	int r1, r2, r3;

	acx_struct_size_check();

	/* ACX_GIT_VERSION can be an empty string, if something went
	   wrong before on Makefile/shell level. We trap this here
	   ... since trapping empty macro strings in cpp seems not
	   possible (didn't find how ) !? */
	pr_info("acx-mac80211, version: %s (git: %s)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown");

	pr_info("this driver is still EXPERIMENTAL\n"
	       "acx: please read the README file and/or "
	       "go to http://acx100.sourceforge.net/wiki for "
	       "further information\n");

	r1 = r2 = r3 = -EINVAL;

#if defined(CONFIG_ACX_MAC80211_PCI)
	pr_info("built with CONFIG_ACX_MAC80211_PCI\n");
	r1 = acxpci_init_module();
#endif

#if defined(CONFIG_ACX_MAC80211_USB)
	pr_info("built with CONFIG_ACX_MAC80211_USB\n");
	r2 = acxusb_init_module();
#endif

#if defined(CONFIG_ACX_MAC80211_MEM)
	pr_info("built with CONFIG_ACX_MAC80211_MEM\n");
	r3 = acxmem_init_module();
#endif

	if (r3 && r2 && r1) {		/* all three failed! */
		pr_info("r1_pci=%i, r2_usb=%i, r3_mem=%i\n", r1, r2, r3);
		return -EINVAL;
	}

	acx_proc_init();
	acx_debugfs_init();

	/* return success if at least one succeeded */
	return 0;
}

void acx_debugfs_exit(void);
static void __exit acx_cleanup_module(void)
{
	/* TODO Check, that interface isn't still up */
	acx_debugfs_exit();

#if defined(CONFIG_ACX_MAC80211_PCI)
	acxpci_cleanup_module();
#endif

#if defined(CONFIG_ACX_MAC80211_USB)
	acxusb_cleanup_module();
#endif

#if defined(CONFIG_ACX_MAC80211_MEM)
	acxmem_cleanup_module();
#endif

}

/*
 * BOM Module
 * ==================================================
 */

module_init(acx_init_module)
module_exit(acx_cleanup_module)

#if ACX_DEBUG

/* will add __read_mostly later */
unsigned int acx_debug = ACX_DEFAULT_MSG;
/* parameter is 'debug', corresponding var is acx_debug */
module_param_named(debug, acx_debug, uint, 0644);
MODULE_PARM_DESC(debug, "Debug level mask (see L_xxx constants)");

static const char *flag_names[] = {
	"L_LOCK", "L_INIT", "L_IRQ", "L_ASSOC", "L_FUNC", "L_XFER",
	"L_DATA", "L_DEBUG", "L_IOCTL", "L_CTL", "L_BUFR", "L_XFER_BEACON",
	"L_BUFT", "L_USBRXTX", "L_BUF",
};

static int acx_debug_flag_get(char *buf, const struct kernel_param *kp)
{
	int i, len;
	char *p = buf; // 1 page preallocated (I think - it didnt crash !!)

	for (i = 0; i < ARRAY_SIZE(flag_names); i++) {
		if (acx_debug & 1 << i)
			len = sprintf(p, "bit %d:%s = 1\n", i, flag_names[i]);
		else
			len = sprintf(p, "bit %d:%s = 0\n", i, flag_names[i]);
		pr_info("%s", p);
		p += len;
	}
	return p - buf;
}

static int acx_debug_flag_set(const char *val, const struct kernel_param *kp)
{
        TODO();
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static struct kernel_param_ops acx_debug_flag_ops = {
        .get = acx_debug_flag_get,
        .set = acx_debug_flag_set,
};

module_param_cb(debugflags, &acx_debug_flag_ops, "str", 0644);
#endif

#endif

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
/* USB had this: MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>"); */
MODULE_AUTHOR("ACX100 Open Source Driver development team");
MODULE_DESCRIPTION
    ("Driver for TI ACX1xx based wireless cards (CardBus/PCI/USB)");

MODULE_VERSION(ACX_RELEASE);
