#ifndef _ACX_CARDSETTING_H_
#define _ACX_CARDSETTING_H_

/* Maximum tx_power assumed from ieee80211_ops->config / IEEE80211_CONF_CHANGE_POWER */
#define TX_CFG_MAX_DBM_POWER	20

int acx111_get_feature_config(acx_device_t *adev, u32 *feature_options,
                              u32 *data_flow_options);
int acx111_set_feature_config(acx_device_t *adev, u32 feature_options,
                              u32 data_flow_options, unsigned int mode);
int acx111_feature_off(acx_device_t *adev, u32 f, u32 d);
int acx111_feature_on(acx_device_t *adev, u32 f, u32 d);
int acx111_feature_set(acx_device_t *adev, u32 f, u32 d);

int acx_set_channel(acx_device_t *adev, u8 channel, int freq);
void acx_get_sensitivity(acx_device_t *adev);
void acx_set_sensitivity(acx_device_t *adev, u8 sensitivity);
void acx_update_sensitivity(acx_device_t *adev);
void acx_get_reg_domain(acx_device_t *adev);
void acx_set_reg_domain(acx_device_t *adev, u8 domain_id);
void acx_update_reg_domain(acx_device_t *adev);
int acx1xx_set_tx_level_dbm(acx_device_t *adev, int level_dbm);
int acx1xx_update_tx_level_dbm(acx_device_t *adev);
int acx1xx_get_tx_level(acx_device_t *adev);
int acx1xx_set_tx_level(acx_device_t *adev, u8 level_val);
int acx1xx_update_tx_level(acx_device_t *adev);
int acx1xx_get_antenna(acx_device_t *adev);
int acx1xx_set_antenna(acx_device_t *adev, u8 val0, u8 val1);
int acx1xx_update_antenna(acx_device_t *adev);
int acx1xx_get_station_id(acx_device_t *adev);
int acx1xx_set_station_id(acx_device_t *adev, u8 *new_addr);
int acx1xx_update_station_id(acx_device_t *adev);
int acx1xx_update_ed_threshold(acx_device_t *adev);
int acx1xx_update_cca(acx_device_t *adev);
int acx1xx_update_rate_fallback(acx_device_t *adev);
int acx1xx_update_tx(acx_device_t *adev);
int acx1xx_set_rx_enable(acx_device_t *adev, u8 rx_enabled);
int acx1xx_update_rx(acx_device_t *adev);
int acx1xx_update_retry(acx_device_t *adev);
int acx1xx_update_msdu_lifetime(acx_device_t *adev);
int acx111_set_recalib_auto(acx_device_t *adev, int enable);
int acx_update_hw_encryption(acx_device_t *adev);
int acx_set_hw_encryption_on(acx_device_t *adev);
int acx_set_hw_encryption_off(acx_device_t *adev);

int acx_set_beacon(acx_device_t *adev, struct sk_buff *beacon);
int acx_set_tim_template(acx_device_t *adev, u8 *data, int len);
int acx_set_probe_request_template(acx_device_t *adev, unsigned char *data, unsigned int len);
u8* acx_beacon_find_tim(struct sk_buff *beacon_skb);

int acx_set_mode(acx_device_t *adev, u16 mode);
int acx_update_mode(acx_device_t *adev);
void acx_set_defaults(acx_device_t *adev);
void acx_update_settings(acx_device_t *adev);

#endif
