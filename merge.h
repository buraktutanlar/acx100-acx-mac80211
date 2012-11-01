#ifndef _MERGE_H_
#define _MERGE_H_

#include <linux/irq.h>
#include "acx.h"

int _acx_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd,
		void *buffer, unsigned buflen, unsigned cmd_timeout,
		const char *cmdstr);

#if defined CONFIG_ACX_MAC80211_PCI || defined CONFIG_ACX_MAC80211_MEM
#define PCI_OR_MEM
#endif

DECL_OR_STUB ( PCI_OR_MEM,
               void acx_create_desc_queues(acx_device_t *adev, u32 rx_queue_start,
                                           u32 *tx_queue_start, int num_tx),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	int acx_create_hostdesc_queues(acx_device_t *adev, int num_tx),
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
	unsigned int acx_tx_clean_txdesc(acx_device_t *adev, int queue_id),
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
			struct ieee80211_tx_info *info, struct sk_buff *skb, int queue_id),
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
	void acx_log_txbuffer(acx_device_t *adev, int queue_id),
	{ } )

DECL_OR_STUB ( PCI_OR_MEM,
	void *_acx_get_txbuf(acx_device_t * adev, tx_t * tx_opaque, int queue_id),
	{ return (void*) NULL; } )


#if (defined CONFIG_ACX_MAC80211_PCI || defined CONFIG_ACX_MAC80211_MEM)

static inline txacxdesc_t* acx_get_txacxdesc(acx_device_t *adev, int index, int queue_id)
{
	return (txacxdesc_t*) (((u8*) adev->hw_tx_queue[queue_id].acxdescinfo.start)
			+ index * adev->hw_tx_queue[queue_id].acxdescinfo.size);
}

static inline txacxdesc_t* acx_advance_txacxdesc(acx_device_t *adev,
					txacxdesc_t* txdesc, int inc, int queue_id)
{
	return (txacxdesc_t*) (((u8*) txdesc)
			+ inc * adev->hw_tx_queue[queue_id].acxdescinfo.size);
}

void acx_base_reset_mac(acx_device_t *adev, int middelay);
int acx_get_hardware_info(acx_device_t *adev);

#else /* !(CONFIG_ACX_MAC80211_PCI || CONFIG_ACX_MAC80211_MEM) */

static inline txacxdesc_t* acx_get_txacxdesc(acx_device_t *adev, int index)
{ return (txacxdesc_t*) NULL; }

static inline txacxdesc_t* acx_advance_txacxdesc(acx_device_t *adev,
					txacxdesc_t* txdesc, int inc)
{ return (txacxdesc_t*) NULL; }


#endif /* !(CONFIG_ACX_MAC80211_PCI || CONFIG_ACX_MAC80211_MEM) */
#endif /* _MERGE_H_ */
