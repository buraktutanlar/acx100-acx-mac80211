#ifndef _PCI_H_
#define _PCI_H_

int acxpci_upload_radio(acx_device_t *adev);

int acxpci_write_fw(acx_device_t *adev, const firmware_image_t *fw_image,
                    u32 offset);
int acxpci_validate_fw(acx_device_t *adev, const firmware_image_t *fw_image,
                       u32 offset);
void acxpci_init_mboxes(acx_device_t *adev);

tx_t *acxpci_alloc_tx(acx_device_t *adev, int q);

int acxpci_dbgfs_diag_output(struct seq_file *file, acx_device_t *adev);

#if defined(CONFIG_ACX_MAC80211_PCI)

void acxpci_process_rxdesc(acx_device_t *adev);

void acxpci_reset_mac(acx_device_t *adev);
int acxpci_load_firmware(acx_device_t *adev);

int __init acxpci_init_module(void);
void __exit acxpci_cleanup_module(void);

#else /* !CONFIG_ACX_MAC80211_PCI */

static inline void acxpci_process_rxdesc(acx_device_t *adev) {}

static inline int __init acxpci_init_module(void) { return 0; }
static inline void __exit acxpci_cleanup_module(void) { }

static inline void acxpci_reset_mac(acx_device_t *adev) {}

#endif /* CONFIG_ACX_MAC80211_PCI */
#endif /* _PCI_H_ */
