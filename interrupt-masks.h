#ifndef _INTERRRUPT_MASKS_H_
#define _INTERRRUPT_MASKS_H_
/*
 * Define interrupt flag for acx100 & acx111 chips.  Separated to here
 * to gather all pci/mem & 100/111 settings together, to simplify
 * comparison of flags (and possibly increase sharing of handlers).
 * Theyre all static to satisfy sparse, but it doesnt matter, since
 * this is included just once, in merge.c
 */

#include "acx_struct_dev.h"

#define DEVTYPE_MAX	3
#define CHIPTYPE_MAX	3

static u16 interrupt_masks[DEVTYPE_MAX][CHIPTYPE_MAX] = {
	[ DEVTYPE_MEM ] = {
		[ CHIPTYPE_ACX100 ]
		= (u16) ~ (0
			| HOST_INT_RX_DATA
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			/* | HOST_INT_RX_COMPLETE    */
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			/* | HOST_INT_IV_ICV_FAILURE */
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			| HOST_INT_OVERFLOW
			| HOST_INT_PROCESS_ERROR
			| HOST_INT_SCAN_COMPLETE
			/* | HOST_INT_FCS_THRESHOLD  */
			/* | HOST_INT_BEACON_MISSED  */
			),

		[ CHIPTYPE_ACX111 ]
		= (u16)	(0xffff) & ~( 0
			| HOST_INT_RX_DATA
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			/* | HOST_INT_RX_COMPLETE    */
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			| HOST_INT_IV_ICV_FAILURE
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			| HOST_INT_OVERFLOW
			/* | HOST_INT_PROCESS_ERROR  */
			| HOST_INT_SCAN_COMPLETE
			| HOST_INT_FCS_THRESHOLD
			| HOST_INT_UNKNOWN),
	},

	[ DEVTYPE_PCI ] = {
		[ CHIPTYPE_ACX100 ]
		= (u16) ~ (0
			/* | HOST_INT_RX_DATA        */
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			| HOST_INT_RX_COMPLETE
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			/* | HOST_INT_IV_ICV_FAILURE */
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			/* | HOST_INT_OVERFLOW       */
			/* | HOST_INT_PROCESS_ERROR  */
			| HOST_INT_SCAN_COMPLETE
			/* | HOST_INT_FCS_THRESHOLD  */
			/* | HOST_INT_UNKNOWN        */
			),

		[ CHIPTYPE_ACX111 ]
		= (u16) ~ (0
			/* | HOST_INT_RX_DATA        */
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			| HOST_INT_RX_COMPLETE
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			| HOST_INT_IV_ICV_FAILURE
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			/* | HOST_INT_OVERFLOW       */
			/* | HOST_INT_PROCESS_ERROR  */
			| HOST_INT_SCAN_COMPLETE
			| HOST_INT_FCS_THRESHOLD
			/* | HOST_INT_UNKNOWN        */
			),
	}
};

#if (ACX_DEBUG < 2)
inline void interrupt_sanity_checks(void) {}
#else

static const char *devtype_names[] = { "PCI", "USB", "MEM" };
static const char *chiptype_names[] = { "", "ACX100", "ACX111" };

/* defd to textually match #define table in acx-struct-hw (then reordered) */
struct interrupt_desc {
	int flagval;
	char *name;
	char *desc;
};

static const struct interrupt_desc interrupt_descs[] = { 
	{ 0x0001, "HOST_INT_RX_DATA",
	  "IN:  packet rcvd from remote host to device"
	},
	{ 0x0002, "HOST_INT_TX_COMPLETE",
	  "OUT: packet sent from device to remote host"
	},
	{ 0x0004, "HOST_INT_TX_XFER",
	  "OUT: packet sent from host to device"
	},
	{ 0x0008, "HOST_INT_RX_COMPLETE",
	  "IN:  packet rcvd from device to host"
	},
	{ 0x0010, "HOST_INT_DTIM",
	  "no docs"
	},
	{ 0x0020, "HOST_INT_BEACON",
	  "no docs"
	},
	{ 0x0040, "HOST_INT_TIMER",
	  "no docs - in BSD as ACX_DEV_INTF_UNKNOWN1"
	},
	{ 0x0080, "HOST_INT_KEY_NOT_FOUND",
	  "no docs - in BSD as ACX_DEV_INTF_UNKNOWN2"
	},
	{ 0x0100, "HOST_INT_IV_ICV_FAILURE",
	  "no docs"
	},
	{ 0x0200, "HOST_INT_CMD_COMPLETE",
	  "no docs"
	},
	{ 0x0400, "HOST_INT_INFO",
	  "no docs"
	},
	{ 0x0800, "HOST_INT_OVERFLOW",
	  "no docs - in BSD as ACX_DEV_INTF_UNKNOWN3"
	},
	{ 0x1000, "HOST_INT_PROCESS_ERROR",
	  "no docs - in BSD as ACX_DEV_INTF_UNKNOWN4"
	},
	{ 0x2000, "HOST_INT_SCAN_COMPLETE",
	  "no docs"
	},
	{ 0x4000, "HOST_INT_FCS_THRESHOLD",
	  "no docs - in BSD as ACX_DEV_INTF_BOOT ???"
	},
	{ 0x8000, "HOST_INT_UNKNOWN",
	  "no docs - in BSD as ACX_DEV_INTF_UNKNOWN5"
	},
};

/* Show active flags and their desc, or flag diffs vs reference flags.
 * Usually reference flags are 0, so this shows the absolute flags.
 * Otherwize, show the changes, and mark them with ! to show the
 * direction of the difference.
 */
static void interrupt_show_flags(u16 flagval, u16 versus)
{
	int i, mask, flagdiffs;

	/* pr_info("flagval:0x%x versus:0x%x\n", flagval, versus); */

	flagdiffs = flagval ^ versus;

	/* pr_info("flagdiffs:0x%x\n", flagdiffs); */

	if (!flagdiffs) return;

	flagdiffs = ~flagdiffs;	// flags are active low 

	/* pr_info("~flagdiffs:0x%x\n", flagdiffs); */

	for (i = 0; i < 16; i++) {
		mask = 1 << i;
		if (flagdiffs & mask)
			pr_info("%c%-23s # %-.40s\n",
				(flagval & mask ? '!' : ' '),
				interrupt_descs[i].name,
				interrupt_descs[i].desc);
	}
}

static inline void interrupt_sanity_checks(acx_device_t *adev)
{
	int d, c;

	interrupt_show_flags(
		interrupt_masks[adev->dev_type][adev->chip_type], 0);

	return;

	for (d = 0; d < DEVTYPE_MAX; d++) {
		for (c = 0; c < CHIPTYPE_MAX; c++) {

			/* skip non-devices or no-flags (same condition really) */
			if (!interrupt_masks[d][c]) continue;
			if (!chiptype_names[c])	continue;

			pr_info("devtype:%d:%s chip:%d:%s val:0x%x\n", 
				d, devtype_names[d], c, chiptype_names[c],
				interrupt_masks[d][c]);

			interrupt_show_flags(interrupt_masks[d][c], 0);

			/* continue; */

			/* see diffs */
			pr_info("vs devtype:%d:%s chip:%d:%s val:0x%x\n",
				DEVTYPE_PCI,
				devtype_names[DEVTYPE_PCI],
				CHIPTYPE_ACX111,
				chiptype_names[CHIPTYPE_ACX111],
				interrupt_masks[DEVTYPE_PCI][CHIPTYPE_ACX111]);

			interrupt_show_flags(interrupt_masks[d][c],
				interrupt_masks[DEVTYPE_PCI][CHIPTYPE_ACX111]);
		}
	}
}

#endif	/* (ACX_DEBUG < 2) */
#endif	/* _INTERRRUPT_MASKS_H_ */
