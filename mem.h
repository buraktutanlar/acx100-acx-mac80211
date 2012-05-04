
/* purpose of this file is to aid in unification of appropriate
 * functions from mem.c and pci.c into merge.c.  It borrows fn-sigs
 * from pci.c, and changes them from static to public to suppress 
 * warnings like:

 warning: 'acxpci_op_stop' declared 'static' but never defined [-Wunused-function]
 Later, we'll drop it.
*/

#define STATick	/* ick: suppress static, let linker find fns in
		   mem.o pci.o */

#define DUMP_MEM_DEFINED 1 // to insure export of dump* fns too

// Logging
STATick void acxmem_log_rxbuffer(const acx_device_t *adev);
STATick void acxmem_log_txbuffer(acx_device_t *adev);
#if DUMP_MEM_DEFINED > 0
STATick void acxmem_dump_mem(acx_device_t *adev, u32 start, int length);
#endif

STATick void acxmem_copy_from_slavemem(acx_device_t *adev, u8 *destination, u32 source, int count);
STATick void acxmem_copy_to_slavemem(acx_device_t *adev, u32 destination, u8 *source, int count);
STATick void acxmem_chaincopy_to_slavemem(acx_device_t *adev, u32 destination, u8 *source, int count);
STATick void acxmem_chaincopy_from_slavemem(acx_device_t *adev, u8 *destination, u32 source, int count);

int acxmem_create_hostdesc_queues(acx_device_t *adev);
STATick int acxmem_create_rx_host_desc_queue(acx_device_t *adev);
STATick int acxmem_create_tx_host_desc_queue(acx_device_t *adev);
void acxmem_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start);
STATick void acxmem_create_rx_desc_queue(acx_device_t *adev, u32 rx_queue_start);
STATick void acxmem_create_tx_desc_queue(acx_device_t *adev, u32 tx_queue_start);
void acxmem_free_desc_queues(acx_device_t *adev);
STATick void acxmem_delete_dma_regions(acx_device_t *adev);
STATick void *acxmem_allocate(acx_device_t *adev, size_t size, dma_addr_t *phy, const char *msg);

// Firmware, EEPROM, Phy
int acxmem_upload_radio(acx_device_t *adev);
int acxmem_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
#ifdef UNUSED
int acxmem_s_write_eeprom(acx_device_t *adev, u32 addr, u32 len, const u8 *charbuf);
#endif
STATick inline void acxmem_read_eeprom_area(acx_device_t *adev);
int acxmem_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acxmem_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
STATick int acxmem_write_fw(acx_device_t *adev, const firmware_image_t *fw_image, u32 offset);
STATick int acxmem_validate_fw(acx_device_t *adev, const firmware_image_t *fw_image, u32 offset);
STATick int acxmem_upload_fw(acx_device_t *adev);

#if defined(NONESSENTIAL_FEATURES)
STATick void acx_show_card_eeprom_id(acx_device_t *adev);
#endif

// CMDs (Control Path)
int acxmem_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd,
				void *buffer, unsigned buflen,
				unsigned cmd_timeout, const char *cmdstr);

// STATick inline void acxmem_write_cmd_type_status(acx_device_t *adev, u16 type, u16 status);

//= copied to merge.c
//= STATick u32 acxmem_read_cmd_type_status(acx_device_t *adev);
//= STATick inline void acxmem_init_mboxes(acx_device_t *adev);

// Init, Configure (Control Path)
//= int acxmem_reset_dev(acx_device_t *adev);
//= STATick int acxmem_verify_init(acx_device_t *adev);
// STATick int acxmem_complete_hw_reset(acx_device_t *adev);
// STATick void acxmem_reset_mac(acx_device_t *adev);
// STATick void acxmem_up(struct ieee80211_hw *hw);
//STATick void acxmem_i_set_multicast_list(struct net_device *ndev);

// Other (Control Path)

// Proc, Debug
int acxmem_proc_diag_output(struct seq_file *file, acx_device_t *adev);
char *acxmem_proc_eeprom_output(int *len, acx_device_t *adev);

// Rx Path
//= STATick void acxmem_process_rxdesc(acx_device_t *adev);

// Tx Path
tx_t *acxmem_alloc_tx(acx_device_t *adev, unsigned int len);
void acxmem_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);

void *acxmem_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
//= STATick int acxmem_get_txbuf_space_needed(acx_device_t *adev, unsigned int len);
//= STATick u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev, int count);
//= STATick void acxmem_reclaim_acx_txbuf_space(acx_device_t *adev, u32 blockptr);
//= STATick void acxmem_init_acx_txbuf(acx_device_t *adev);
void acxmem_init_acx_txbuf2(acx_device_t *adev);
//= STATick inline txdesc_t *acxmem_get_txdesc(acx_device_t *adev, int index);
STATick inline txdesc_t *acxmem_advance_txdesc(acx_device_t *adev, txdesc_t *txdesc, int inc);
//= STATick txhostdesc_t *acxmem_get_txhostdesc(acx_device_t *adev, txdesc_t *txdesc);

void acxmem_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len, struct ieee80211_tx_info *info, struct sk_buff *skb);
unsigned int acxmem_tx_clean_txdesc(acx_device_t *adev);
void acxmem_clean_txdesc_emergency(acx_device_t *adev);

void acxmem_update_queue_indicator(acx_device_t *adev, int txqueue);
int acx100mem_set_tx_level(acx_device_t *adev, u8 level_dbm);
//STATick void acxmem_i_tx_timeout(struct net_device *ndev);

// Irq Handling, Timer
//= STATick void acxmem_irq_enable(acx_device_t *adev);
//= STATick void acxmem_irq_disable(acx_device_t *adev);
void acxmem_irq_work(struct work_struct *work);
// STATick irqreturn_t acxmem_interrupt(int irq, void *dev_id);
irqreturn_t acx_interrupt(int irq, void *dev_id);
STATick void acxmem_handle_info_irq(acx_device_t *adev);
void acxmem_set_interrupt_mask(acx_device_t *adev);

// Mac80211 Ops
STATick int acxmem_op_start(struct ieee80211_hw *hw);
STATick void acxmem_op_stop(struct ieee80211_hw *hw);

// Helpers
void acxmem_power_led(acx_device_t *adev, int enable);
// INLINE_IO int acxmem_adev_present(acx_device_t *adev);
//= STATick char acxmem_printable(char c);
//STATick void update_link_quality_led(acx_device_t *adev);

// Ioctls
//int acx111pci_ioctl_info(struct ieee80211_hw *hw, struct iw_request_info *info, struct iw_param *vwrq, char *extra);
//int acx100mem_ioctl_set_phy_amp_bias(struct ieee80211_hw *hw, struct iw_request_info *info, struct iw_param *vwrq, char *extra);

// Driver, Module
//= STATick int __devinit acxmem_probe(struct platform_device *pdev);
//= STATick int __devexit acxmem_remove(struct platform_device *pdev);
#ifdef CONFIG_PM
//= STATick int acxmem_e_suspend(struct platform_device *pdev, pm_message_t state);
//= STATick int acxmem_e_resume(struct platform_device *pdev);
#endif
int __init acxmem_init_module(void);
void __exit acxmem_cleanup_module(void);

// for merge of tx_data
u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev, int count);

// for merge of acx_reset_dev
void acxmem_reset_mac(acx_device_t *adev);
u32 acxmem_read_cmd_type_status(acx_device_t *adev);
void acxmem_write_cmd_type_status(acx_device_t *adev, u16 type, u16 status);
void acxmem_init_mboxes(acx_device_t *adev);
