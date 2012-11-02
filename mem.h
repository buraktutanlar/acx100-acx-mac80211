
/* this file provides prototypes for functions defined in mem.c that
 * are used by common.c etc; it is part of the internal API.  It also
 * forward declares some of the functions used in mem.c, which reduces
 * the set of forward declarations in mem.c
 */
#ifndef _MEM_H_
#define _MEM_H_

#if defined(CONFIG_ACX_MAC80211_MEM)

#define DUMP_MEM_DEFINED 1 // to insure export of dump* fns too

/* Logging */

#if DUMP_MEM_DEFINED > 0
void acxmem_dump_mem(acx_device_t *adev, u32 start, int length);
#else
inline void acxmem_dump_mem(acx_device_t *adev, u32 start, int length) { }
#endif

void acxmem_copy_to_slavemem(acx_device_t *adev, u32 destination,
			u8 *source, int count);
void acxmem_copy_from_slavemem(acx_device_t *adev, u8 *destination,
			u32 source, int count);
void acxmem_chaincopy_to_slavemem(acx_device_t *adev, u32 destination,
			u8 *source, int count);
void acxmem_chaincopy_from_slavemem(acx_device_t *adev, u8 *destination,
			u32 source, int count);

void acxmem_reset_mac(acx_device_t *adev);
int acxmem_patch_around_bad_spots(acx_device_t *adev);

int acxmem_dbgfs_diag_output(struct seq_file *file, acx_device_t *adev);

tx_t *acxmem_alloc_tx(acx_device_t *adev, unsigned int len);
void acxmem_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);

u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev, int count);

void acxmem_init_acx_txbuf2(acx_device_t *adev);

void acxmem_process_rxdesc(acx_device_t *adev);

int __init acxmem_init_module(void);
void __exit acxmem_cleanup_module(void);


#else /* !CONFIG_ACX_MAC80211_MEM */


static inline void acxmem_dump_mem(acx_device_t *adev, u32 start, int length) { }

static inline void acxmem_copy_to_slavemem(acx_device_t *adev,
		u32 destination, u8 *source, int count)
{ }

static inline void acxmem_copy_from_slavemem(acx_device_t *adev,
		u8 *destination, u32 source, int count)
{ }

static inline void acxmem_chaincopy_to_slavemem(acx_device_t *adev,
		u32 destination, u8 *source, int count)
{ }

static inline void acxmem_chaincopy_from_slavemem(acx_device_t *adev,
		u8 *destination, u32 source, int count)
{ }

static inline void acxmem_reset_mac(acx_device_t *adev)
{ }

static inline int acxmem_dbgfs_diag_output(struct seq_file *file,
		acx_device_t *adev)
{ return 0; }

static inline tx_t *acxmem_alloc_tx(acx_device_t *adev, unsigned int len)
{ return (tx_t*) NULL; }

static inline void acxmem_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{ }

static inline u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev,
		int count)
{ return 0; }

static inline void acxmem_init_acx_txbuf2(acx_device_t *adev)
{ }

static inline int __init acxmem_init_module(void)
{ return 0; }

static inline void __exit acxmem_cleanup_module(void)
{ }

static inline void acxmem_init_mboxes(acx_device_t *adev) { }

static inline void acxmem_process_rxdesc(acx_device_t *adev) { };

#endif /* defined(CONFIG_ACX_MAC80211_MEM) */
#endif /* _MEM_H_ */
