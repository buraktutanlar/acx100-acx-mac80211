
irqreturn_t acx_interrupt(int irq, void *dev_id);

int acx_upload_radio(acx_device_t *adev);
int acxmem_upload_radio(acx_device_t *adev);
int acxpci_upload_radio(acx_device_t *adev);

int acx_create_hostdesc_queues(acx_device_t *adev);

void acx_log_rxbuffer(acx_device_t *adev);
void acx_log_txbuffer(acx_device_t *adev);

void acx_op_stop(struct ieee80211_hw *hw);
int acx_op_start(struct ieee80211_hw *hw);
