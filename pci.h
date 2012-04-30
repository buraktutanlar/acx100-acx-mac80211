
/* purpose of this file is to aid in unification of appropriate
 * functions from mem.c and pci.c into merge.c.  It borrows fn-sigs
 * from pci.c, and changes them from static to public to suppress 
 * warnings like:

 warning: 'acxpci_op_stop' declared 'static' but never defined [-Wunused-function]
 Later, we'll drop it.
*/

#define STATick /* ick - suppress static, and thus a raft of warnings
		   let linker find the fns in mem.o, pci.o */

// Logging
STATick void acxpci_log_rxbuffer(const acx_device_t * adev);
STATick void acxpci_log_txbuffer(acx_device_t * adev);

int acxpci_create_hostdesc_queues(acx_device_t * adev);
//= STATick int acxpci_create_rx_host_desc_queue(acx_device_t * adev);
STATick int acxpci_create_tx_host_desc_queue(acx_device_t * adev);

void acxpci_create_desc_queues(acx_device_t * adev, u32 tx_queue_start, u32 rx_queue_start);
STATick void acxpci_create_rx_desc_queue(acx_device_t * adev, u32 rx_queue_start);
STATick void acxpci_create_tx_desc_queue(acx_device_t * adev, u32 tx_queue_start);

//= void acxpci_free_desc_queues(acx_device_t * adev);
STATick void acxpci_delete_dma_regions(acx_device_t * adev);
STATick inline void acxpci_free_coherent(struct pci_dev *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle);
//= STATick void *acxpci_allocate(acx_device_t * adev, size_t size, dma_addr_t * phy, const char *msg);

// Firmware, EEPROM, Phy
int acxpci_upload_radio(acx_device_t * adev);
int acxpci_read_eeprom_byte(acx_device_t * adev, u32 addr, u8 * charbuf);
// int acxpci_s_write_eeprom(acx_device_t * adev, u32 addr, u32 len, const u8 * charbuf);
STATick inline void acxpci_read_eeprom_area(acx_device_t * adev);
int acxpci_read_phy_reg(acx_device_t * adev, u32 reg, u8 * charbuf);
int acxpci_write_phy_reg(acx_device_t * adev, u32 reg, u8 value);
STATick int acxpci_write_fw(acx_device_t * adev, const firmware_image_t *fw_image, u32 offset);
STATick int acxpci_validate_fw(acx_device_t * adev, const firmware_image_t *fw_image, u32 offset);
STATick int acxpci_upload_fw(acx_device_t * adev);
// STATick void acx_show_card_eeprom_id(acx_device_t * adev);

// CMDs (Control Path)
int acxpci_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd, void *buffer, unsigned buflen, unsigned cmd_timeout, const char *cmdstr);
// coplied to merge.c
// STATick inline void acxpci_write_cmd_type_status(acx_device_t * adev, u16 type, u16 status);
STATick u32 acxpci_read_cmd_type_status(acx_device_t *adev);
STATick inline void acxpci_init_mboxes(acx_device_t * adev);

// Init, Configuration (Control Path)
int acxpci_reset_dev(acx_device_t * adev);
//= STATick int acxpci_verify_init(acx_device_t * adev);
STATick void acxpci_reset_mac(acx_device_t * adev);
STATick void acxpci_up(struct ieee80211_hw *hw);

// Other (Control Path)

// Proc, Debug
int acxpci_proc_diag_output(struct seq_file *file, acx_device_t *adev);
char *acxpci_proc_eeprom_output(int *len, acx_device_t * adev);

// Rx Path
STATick void acxpci_process_rxdesc(acx_device_t * adev);

// Tx Path
tx_t *acxpci_alloc_tx(acx_device_t * adev);
void *acxpci_get_txbuf(acx_device_t * adev, tx_t * tx_opaque);
void acxpci_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len, struct ieee80211_tx_info *info, struct sk_buff *skb);
unsigned int acxpci_tx_clean_txdesc(acx_device_t * adev);
void acxpci_clean_txdesc_emergency(acx_device_t * adev);
//= STATick inline txdesc_t *acxpci_get_txdesc(acx_device_t * adev, int index);
//= STATick inline txdesc_t *acxpci_advance_txdesc(acx_device_t * adev, txdesc_t * txdesc, int inc);
STATick txhostdesc_t *acxpci_get_txhostdesc(acx_device_t * adev, txdesc_t * txdesc);

// Irq Handling, Timer
STATick void acxpci_irq_enable(acx_device_t * adev);
STATick void acxpci_irq_disable(acx_device_t * adev);
void acxpci_irq_work(struct work_struct *work);
// STATick irqreturn_t acxpci_interrupt(int irq, void *dev_id);
irqreturn_t acx_interrupt(int irq, void *dev_id);
STATick void acxpci_handle_info_irq(acx_device_t * adev);
void acxpci_set_interrupt_mask(acx_device_t * adev);

// Mac80211 Ops
STATick int acxpci_op_start(struct ieee80211_hw *hw);
STATick void acxpci_op_stop(struct ieee80211_hw *hw);

// Helpers
void acxpci_power_led(acx_device_t * adev, int enable);
// INLINE_IO int acxpci_adev_present(acx_device_t *adev);

// Ioctls
int acx111pci_ioctl_info(struct net_device *ndev, struct iw_request_info *info, struct iw_param *vwrq, char *extra);
int acx100pci_ioctl_set_phy_amp_bias(struct net_device *ndev, struct iw_request_info *info, struct iw_param *vwrq, char *extra);

// Driver, Module
STATick int __devinit acxpci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
STATick void __devexit acxpci_remove(struct pci_dev *pdev);
#ifdef CONFIG_PM
STATick int acxpci_e_suspend(struct pci_dev *pdev, pm_message_t state);
STATick int acxpci_e_resume(struct pci_dev *pdev);
#endif

int __init acxpci_init_module(void);
void __exit acxpci_cleanup_module(void);

// make available to merge.c
static inline txdesc_t *acxpci_get_txdesc(acx_device_t * adev, int index)
{
	return (txdesc_t *) (((u8 *) adev->txdesc_start) +
			     index * adev->txdesc_size);
}

static inline 
txdesc_t *acxpci_advance_txdesc(acx_device_t *adev, txdesc_t *txdesc,
				int inc)
{
	return (txdesc_t *) (((u8 *) txdesc) + inc * adev->txdesc_size);
}

