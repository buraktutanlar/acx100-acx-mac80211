#ifndef _ACX_MAIN_H_
#define _ACX_MAIN_H_

/* Minutes to wait until next radio recalibration: */
#define RECALIB_PAUSE	5


int acx_setup_modes(acx_device_t *adev);
void acx_init_task_scheduler(acx_device_t *adev);
void acx_after_interrupt_task(acx_device_t *adev);
void acx_log_irq(u16 irqtype);
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);

void acx_timer(unsigned long address);
void acx_start(acx_device_t *adev);

int acx_op_add_interface(struct ieee80211_hw *ieee, struct ieee80211_VIF *vif);
void acx_op_remove_interface(struct ieee80211_hw *hw, struct ieee80211_VIF *vif);
int acx_op_config(struct ieee80211_hw *hw, u32 changed);
void acx_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                             struct ieee80211_bss_conf *info, u32 changed);
int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
                   struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                   struct ieee80211_key_conf *key);
void acx_op_configure_filter(struct ieee80211_hw *hw,
                             unsigned int changed_flags,
                             unsigned int *total_flags, u64 multicast);

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, u16 queue,
                const struct ieee80211_tx_queue_params *params);
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params);
#endif
int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set);
int acx_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			 struct ieee80211_tx_queue_stats *stats)
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
int acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#else
void acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#endif


#endif
