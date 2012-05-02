#include <linux/interrupt.h>

irqreturn_t acx_interrupt(int irq, void *dev_id);

// static int acx_upload_radio(acx_device_t *adev);
int acxmem_upload_radio(acx_device_t *adev);
int acxpci_upload_radio(acx_device_t *adev);

int acx_create_hostdesc_queues(acx_device_t *adev);

void acx_log_rxbuffer(const acx_device_t *adev);
void acx_log_txbuffer(acx_device_t *adev);

void acx_op_stop(struct ieee80211_hw *hw);
int acx_op_start(struct ieee80211_hw *hw);

void acx_handle_info_irq(acx_device_t *adev);

// temporary ?? may go static after all users are in merge.c
void *acx_allocate(acx_device_t *adev, size_t size,
		   dma_addr_t *phy, const char *msg);

void acx_free_desc_queues(acx_device_t *adev);

int acxx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);

void acx_irq_enable(acx_device_t *adev);
void acx_irq_disable(acx_device_t *adev);

int acx_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
char *acx_proc_eeprom_output(int *length, acx_device_t *adev);

void acx_up(struct ieee80211_hw *hw);

void acx_set_interrupt_mask(acx_device_t *adev);

void acx_show_card_eeprom_id(acx_device_t *adev);

void acx_create_rx_desc_queue(acx_device_t *adev, u32 rx_queue_start);
void acx_create_tx_desc_queue(acx_device_t *adev, u32 rx_queue_start);

unsigned int acx_tx_clean_txdesc(acx_device_t *adev);

static inline txdesc_t* acx_get_txdesc(acx_device_t *adev, int index)
{
	return (txdesc_t*) (((u8*) adev->txdesc_start)
			+ index * adev->txdesc_size);
}

static inline txdesc_t* acx_advance_txdesc(acx_device_t *adev,
					txdesc_t* txdesc, int inc)
{
	return (txdesc_t*) (((u8*) txdesc)
			+ inc * adev->txdesc_size);
}

void _acx_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
		struct ieee80211_tx_info *info, struct sk_buff *skb);

void *_acx_get_txbuf(acx_device_t * adev, tx_t * tx_opaque);
