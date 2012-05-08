
/* this file provides prototypes for functions defined in mem.c that
 * are used by common.c etc, and is thus the internal API.  It also
 * forward declares some of the functions used in mem.c, reducing the
 * set of forward declarations in mem.c
 */

#define STATick	/* ick: suppress static, let linker find fns in
		   mem.o pci.o */

#define DUMP_MEM_DEFINED 1 // to insure export of dump* fns too

// Logging

#if DUMP_MEM_DEFINED > 0
void acxmem_dump_mem(acx_device_t *adev, u32 start, int length);
#endif

void acxmem_copy_to_slavemem(acx_device_t *adev, u32 destination,
		u8 *source, int count);
void acxmem_chaincopy_to_slavemem(acx_device_t *adev, u32 destination,
		u8 *source, int count);
void acxmem_chaincopy_from_slavemem(acx_device_t *adev, u8 *destination,
		u32 source, int count);

// Firmware, EEPROM, Phy

int acxmem_upload_radio(acx_device_t *adev);

int acxmem_write_fw(acx_device_t *adev, const firmware_image_t *fw_image,
			u32 offset);
int acxmem_validate_fw(acx_device_t *adev, const firmware_image_t *fw_image,
			u32 offset);

// CMDs (Control Path)

void acxmem_reset_mac(acx_device_t *adev);

// Other (Control Path)

// Proc, Debug
int acxmem_proc_diag_output(struct seq_file *file, acx_device_t *adev);

// Rx Path
// Tx Path
tx_t *acxmem_alloc_tx(acx_device_t *adev, unsigned int len);
void acxmem_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);

u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev, int count);

void acxmem_init_acx_txbuf2(acx_device_t *adev);

void acxmem_update_queue_indicator(acx_device_t *adev, int txqueue);

// Irq Handling, Timer
// Helpers
// Ioctls
// Driver, Module

int __init acxmem_init_module(void);
void __exit acxmem_cleanup_module(void);

void acxmem_write_cmd_type_status(acx_device_t *adev, u16 type, u16 status);
void acxmem_init_mboxes(acx_device_t *adev);
				
