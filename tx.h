#ifndef _ACX_TX_H_
#define _ACX_TX_H_

void acx_tx_queue_flush(acx_device_t *adev);
void acx_stop_queue(struct ieee80211_hw *hw, const char *msg);
int acx_queue_stopped(struct ieee80211_hw *ieee);
void acx_wake_queue(struct ieee80211_hw *hw, const char *msg);

int acx_rate111_hwvalue_to_rateindex(u16 hw_value);
u16 acx_rate111_hwvalue_to_bitrate(u16 hw_value);
u16 acx111_tx_build_rateset(acx_device_t *adev, txacxdesc_t *txdesc,
			struct ieee80211_tx_info *info);

void acx111_tx_build_txstatus(acx_device_t *adev,
			struct ieee80211_tx_info *txstatus, u16 r111,
			u8 ack_failures);
void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error,
			unsigned int finger,
			struct ieee80211_tx_info *info);

void acx_tx_work(struct work_struct *work);
void acx_tx_queue_go(acx_device_t *adev);

#endif
