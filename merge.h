#ifndef _MERGE_H_
#define _MERGE_H_

#include <linux/irq.h>
#include "acx.h"

/* these 2 externs are defined in common.c (but we dont have a
 * common.h), so expose them here.  Theyre used in debugfs.c
 */
typedef int acx_proc_show_t(struct seq_file *file, void *v);
typedef ssize_t ((acx_proc_write_t)(struct file *, const char __user *,
				    size_t, loff_t *));

extern acx_proc_show_t *const acx_proc_show_funcs[];
extern acx_proc_write_t *const acx_proc_write_funcs[];

/* debugfs.c API used by common.c */
#if defined CONFIG_DEBUG_FS
int acx_debugfs_add_adev(struct acx_device *adev);
void acx_debugfs_remove_adev(struct acx_device *adev);
int acx_proc_register_entries(struct ieee80211_hw *hw);
int __init acx_debugfs_init(void);
void __exit acx_debugfs_exit(void);
#else
static int acx_debugfs_add_adev(struct acx_device *adev) { return 0; }
static void acx_debugfs_remove_adev(struct acx_device *adev) { }
static int acx_proc_register_entries(struct ieee80211_hw *hw) { return 0; }
static int __init acx_debugfs_init(void)  { return 0; }
static void __exit acx_debugfs_exit(void) { }
#endif /* defined CONFIG_DEBUG_FS */

#if defined CONFIG_ACX_MAC80211_PCI || defined CONFIG_ACX_MAC80211_MEM
# define PCI_OR_MEM
#endif

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_create_desc_queues(acx_device_t *adev, u32 tx_queue_start,
				u32 rx_queue_start),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_create_hostdesc_queues(acx_device_t *adev),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_free_desc_queues(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int _acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	int _acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_irq_enable(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_irq_disable(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	char *acx_proc_eeprom_output(int *length, acx_device_t *adev),
	{ return (char*) NULL; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_up(struct ieee80211_hw *hw),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_set_interrupt_mask(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_show_card_eeprom_id(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	unsigned int acx_tx_clean_txdesc(acx_device_t *adev),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_reset_dev(acx_device_t *adev),
	{ return 0; } )

/* wrappers on acx_upload_radio(adev, filename */
DECL_OR_STUB ( PCI_OR_MEM,
	int acxmem_upload_radio(acx_device_t *adev),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acxpci_upload_radio(acx_device_t *adev),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acxmem_update_queue_indicator(acx_device_t *adev, int txqueue),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void _acx_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
			struct ieee80211_tx_info *info, struct sk_buff *skb),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_irq_work(struct work_struct *work),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_write_fw(acx_device_t *adev, const firmware_image_t *fw_image,
			u32 offset),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_validate_fw(acx_device_t *adev,
			const firmware_image_t *fw_image, u32 offset),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	u32 acx_read_cmd_type_status(acx_device_t *adev),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_write_cmd_type_status(acx_device_t *adev, u16 type,
				u16 status),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_op_start(struct ieee80211_hw *hw),
	{ return 0; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_op_stop(struct ieee80211_hw *hw),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	irqreturn_t acx_interrupt(int irq, void *dev_id),
	{ return (irqreturn_t) NULL; } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_delete_dma_regions(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_power_led(acx_device_t * adev, int enable),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_clean_txdesc_emergency(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_log_rxbuffer(const acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void acx_log_txbuffer(acx_device_t *adev),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void *_acx_get_txbuf(acx_device_t * adev, tx_t * tx_opaque),
	{ return (void*) NULL; } )

//void acx_process_rxdesc(acx_device_t *adev);

#if (defined CONFIG_ACX_MAC80211_PCI || defined CONFIG_ACX_MAC80211_MEM)

void acxmem_update_queue_indicator(acx_device_t *adev, int txqueue);

static inline txdesc_t* acx_get_txdesc(acx_device_t *adev, int index)
{
	return (txdesc_t*) (((u8*) adev->tx.desc_start)
			+ index * adev->tx.desc_size);
}

static inline txdesc_t* acx_advance_txdesc(acx_device_t *adev,
					txdesc_t* txdesc, int inc)
{
	return (txdesc_t*) (((u8*) txdesc)
			+ inc * adev->tx.desc_size);
}

#else /* !(CONFIG_ACX_MAC80211_PCI || CONFIG_ACX_MAC80211_MEM) */

static inline txdesc_t* acx_get_txdesc(acx_device_t *adev, int index)
{ return (txdesc_t*) NULL; }

static inline txdesc_t* acx_advance_txdesc(acx_device_t *adev,
					txdesc_t* txdesc, int inc)
{ return (txdesc_t*) NULL; }

/* empty stub here, real one in merge.c */
#define ACX_FREE_QUEUES(adev, _dir_)


#endif /* !(CONFIG_ACX_MAC80211_PCI || CONFIG_ACX_MAC80211_MEM) */
#endif /* _MERGE_H_ */
