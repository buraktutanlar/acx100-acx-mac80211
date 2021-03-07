/***********************************************************************
 ** Copyright (C) 2003  ACX100 Open Source Project
 **
 ** The contents of this file are subject to the Mozilla Public
 ** License Version 1.1 (the "License"); you may not use this file
 ** except in compliance with the License. You may obtain a copy of
 ** the License at http://www.mozilla.org/MPL/
 **
 ** Software distributed under the License is distributed on an "AS
 ** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 ** implied. See the License for the specific language governing
 ** rights and limitations under the License.
 **
 ** Alternatively, the contents of this file may be used under the
 ** terms of the GNU Public License version 2 (the "GPL"), in which
 ** case the provisions of the GPL are applicable instead of the
 ** above.  If you wish to allow the use of your version of this file
 ** only under the terms of the GPL and not to allow others to use
 ** your version of this file under the MPL, indicate your decision
 ** by deleting the provisions above and replace them with the notice
 ** and other provisions required by the GPL.  If you do not delete
 ** the provisions above, a recipient may use your version of this
 ** file under either the MPL or the GPL.
 ** ---------------------------------------------------------------------
 ** Inquiries regarding the ACX100 Open Source Project can be
 ** made directly to:
 **
 ** acx100-users@lists.sf.net
 ** http://acx100.sf.net
 ** ---------------------------------------------------------------------
 **
 ** Slave memory interface support:
 **
 ** Todd Blumer - SDG Systems
 ** Bill Reese - HP
 ** Eric McCorkle - Shadowsun
 */

#include "acx_debug.h"

#define pr_acx		pr_info
#define pr_acxmem	pr_info

#include <linux/version.h>

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/nl80211.h>

#include <net/iw_handler.h>
#include <net/mac80211.h>

#include <asm/io.h>

#include "acx.h"
#include "merge.h"
#include "debug.h"
#include "mem.h"
#include "cmd.h"
#include "ie.h"
#include "init.h"
#include "utils.h"
#include "cardsetting.h"
#include "rx.h"
#include "tx.h"
#include "main.h"
#include "boot.h"

/*
 * BOM Config
 * ==================================================
 */

/* OW PM for later */
#undef CONFIG_PM

/*
 * non-zero makes it dump the ACX memory to the console then
 * panic when you cat /proc/driver/acx_wlan0_diag
 */
#define DUMP_MEM_DEFINED 1

#define DUMP_MEM_DURING_DIAG 0
#define DUMP_IF_SLOW 0

#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ)

#define MEM_FIRMWARE_IMAGE_FILENAME	"WLANGEN.BIN"
#define MEM_RADIO_IMAGE_FILENAME	"RADIO%02x.BIN"
#define MEM_FIRMWARE_FILENAME_MAXLEN 	16

#define PATCH_AROUND_BAD_SPOTS 1
#define HX4700_FIRMWARE_CHECKSUM 0x0036862e
#define HX4700_ALTERNATE_FIRMWARE_CHECKSUM 0x00368a75

/*
 * BOM Prototypes
 * ... static and also none-static for overview reasons (maybe not best practice ...)
 * ==================================================
 */

#if DUMP_MEM_DEFINED > 0
/* = static  */
void acxmem_dump_mem(acx_device_t *adev, u32 start, int length);
#endif

void acxmem_copy_from_slavemem(acx_device_t *adev, u8 *destination,
		u32 source, int count);

// txhostdesc_t *acxmem_get_txhostdesc(acx_device_t *adev, txdesc_t* txdesc);
static char acxmem_printable(char c);

/*
 * BOM Defines, static vars, etc.
 * ==================================================
 */

#define CARD_EEPROM_ID_SIZE 6
#define REG_ACX_VENDOR_ID 0x900

/* This is the vendor id on the HX4700, anyway */
#define ACX_VENDOR_ID 0x8400104c

/* OW 20090815 #define WLAN_A4FR_MAXLEN_WEP_FCS	(30 + 2312 + 4) */

#define RX_BUFFER_SIZE (sizeof(rxbuffer_t) + 32)

#include "io-acx.h"

/*
 * BOM Logging
 * ==================================================
 */

#include "inlines.h"

#if DUMP_MEM_DEFINED > 0
/* = static  */
void acxmem_dump_mem(acx_device_t *adev, u32 start, int length)
{
	int i;
	u8 buf[16];

	while (length > 0) {
		printk("%04x ", start);
		acxmem_copy_from_slavemem(adev, buf, start, 16);
		for (i = 0; (i < 16) && (i < length); i++) {
			printk("%02x ", buf[i]);
		}
		for (i = 0; (i < 16) && (i < length); i++) {
			printk("%c", acxmem_printable(buf[i]));
		}
		printk("\n");
		start += 16;
		length -= 16;
	}
}
#endif

/*
 * BOM Data Access
 *
 * => See locking comment above
 * ==================================================
 */

/*
 * Copy from slave memory
 *
 * TODO - rewrite using address autoincrement, handle partial words
 */
/* = static */
void acxmem_copy_from_slavemem(acx_device_t *adev, u8 *destination,
			u32 source, int count)
{
	u32 tmp = 0;
	u8 *ptmp = (u8 *) &tmp;

	ACXMEM_WARN_NOT_SPIN_LOCKED;

	/*
	 * Right now I'm making the assumption that the destination is
	 * aligned, but I'd better check.
	 */
	if ((uintptr_t) destination & 3) {
		pr_acx("copy_from_slavemem: warning!  destination not word-aligned!\n");
	}

	while (count >= 4) {
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, source);
		udelay (10);
		*((u32 *) destination) = read_reg32(adev, IO_ACX_SLV_MEM_DATA);
		count -= 4;
		source += 4;
		destination += 4;
	}

	/*
	 * If the word reads above didn't satisfy the count, read one
	 * more word and transfer a byte at a time until the request
	 * is satisfied.
	 */
	if (count) {
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, source);
		udelay (10);
		tmp = read_reg32(adev, IO_ACX_SLV_MEM_DATA);
		while (count--) {
			*destination++ = *ptmp++;
		}
	}

}

/*
 * Copy to slave memory
 *
 * TODO - rewrite using autoincrement, handle partial words
 */
/* = static */
void acxmem_copy_to_slavemem(acx_device_t *adev, u32 destination,
			u8 *source, int count)
{
	u32 tmp = 0;
	u8* ptmp = (u8 *) &tmp;
	static u8 src[512]; /* make static to avoid huge stack objects */

	ACXMEM_WARN_NOT_SPIN_LOCKED;

	/*
	 * For now, make sure the source is word-aligned by copying it
	 * to a word-aligned buffer.  Someday rewrite to avoid the
	 * extra copy.
	 */
	if (count > sizeof(src)) {
		pr_acx("copy_to_slavemem: Warning! buffer overflow!\n");
		count = sizeof(src);
	}
	memcpy(src, source, count);
	source = src;

	while (count >= 4) {
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, destination);
		udelay (10);
		write_reg32(adev, IO_ACX_SLV_MEM_DATA, *((u32 *) source));
		count -= 4;
		source += 4;
		destination += 4;
	}

	/*
	 * If there are leftovers read the next word from the acx and
	 * merge in what they want to write.
	 */
	if (count) {
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, destination);
		udelay (10);
		tmp = read_reg32(adev, IO_ACX_SLV_MEM_DATA);
		while (count--) {
			*ptmp++ = *source++;
		}
		/*
		 * reset address in case we're currently in
		 * auto-increment mode
		 */
		write_reg32(adev, IO_ACX_SLV_MEM_ADDR, destination);
		udelay (10);
		write_reg32(adev, IO_ACX_SLV_MEM_DATA, tmp);
		udelay (10);
	}

}

/*
 * Block copy to slave buffers using memory block chain mode.  Copies
 * to the ACX transmit buffer structure with minimal intervention on
 * our part.  Interrupts should be disabled when calling this.
 */
/* =static */
void acxmem_chaincopy_to_slavemem(acx_device_t *adev, u32 destination,
				u8 *source, int count)
{
	u32 val;
	u32 *data = (u32 *) source;
	static u8 aligned_source[WLAN_A4FR_MAXLEN_WEP_FCS];

	ACXMEM_WARN_NOT_SPIN_LOCKED;

	/*
	 * Warn if the pointers don't look right.  Destination must
	 * fit in [23:5] with zero elsewhere and source should be 32
	 * bit aligned.  This should never happen since we're in
	 * control of both, but I want to know about it if it does.
	 */
	if ((destination & 0x00ffffe0) != destination) {
		pr_acx("chaincopy: destination block 0x%04x not aligned!\n",
			destination);
	}
	if (count > sizeof aligned_source) {
		pr_err("chaincopy_to_slavemem overflow!\n");
		count = sizeof aligned_source;
	}
	if ((uintptr_t) source & 3) {
		memcpy(aligned_source, source, count);
		data = (u32 *) aligned_source;
	}

	/*
	 * SLV_MEM_CTL[17:16] = memory block chain mode with
	 * auto-increment SLV_MEM_CTL[5:2] = offset to data portion =
	 * 1 word
	 */
	val = 2 << 16 | 1 << 2;
	acx_writel (val, adev->iobase + ACX_SLV_MEM_CTL);

	/*
	 * SLV_MEM_CP[23:5] = start of 1st block
	 * SLV_MEM_CP[3:2] = offset to memblkptr = 0
	 */
	val = destination & 0x00ffffe0;
	acx_writel (val, adev->iobase + ACX_SLV_MEM_CP);

	/*
	 * SLV_MEM_ADDR[23:2] = SLV_MEM_CTL[5:2] + SLV_MEM_CP[23:5]
	 */
	val = (destination & 0x00ffffe0) + (1 << 2);
	acx_writel (val, adev->iobase + ACX_SLV_MEM_ADDR);

	/*
	 * Write the data to the slave data register, rounding up to
	 * the end of the word containing the last byte (hence the >
	 * 0)
	 */
	while (count > 0) {
		acx_writel (*data++, adev->iobase + ACX_SLV_MEM_DATA);
		count -= 4;
	}

}

/*
 * Block copy from slave buffers using memory block chain mode.
 * Copies from the ACX receive buffer structures with minimal
 * intervention on our part.  Interrupts should be disabled when
 * calling this.
 */
/* = static  */
void acxmem_chaincopy_from_slavemem(acx_device_t *adev, u8 *destination,
				u32 source, int count)
{
	u32 val;
	u32 *data = (u32 *) destination;
	static u8 aligned_destination[WLAN_A4FR_MAXLEN_WEP_FCS];
	int saved_count = count;

	ACXMEM_WARN_NOT_SPIN_LOCKED;

	/*
	 * Warn if the pointers don't look right.  Destination must
	 * fit in [23:5] with zero elsewhere and source should be 32
	 * bit aligned.  Turns out the network stack sends unaligned
	 * things, so fix them before copying to the ACX.
	 */
	if ((source & 0x00ffffe0) != source) {
		pr_acx("chaincopy: source block 0x%04x not aligned!\n", source);
		acxmem_dump_mem(adev, 0, 0x10000);
	}
	if ((uintptr_t) destination & 3) {
		/* printk ("acx chaincopy: data destination not word aligned!\n"); */
		data = (u32 *) aligned_destination;
		if (count > sizeof aligned_destination) {
			pr_err("chaincopy_from_slavemem overflow!\n");
			count = sizeof aligned_destination;
		}
	}

	/*
	 * SLV_MEM_CTL[17:16] = memory block chain mode with auto-increment
	 * SLV_MEM_CTL[5:2] = offset to data portion = 1 word
	 */
	val = (2 << 16) | (1 << 2);
	acx_writel (val, adev->iobase + ACX_SLV_MEM_CTL);

	/*
	 * SLV_MEM_CP[23:5] = start of 1st block
	 * SLV_MEM_CP[3:2] = offset to memblkptr = 0
	 */
	val = source & 0x00ffffe0;
	acx_writel (val, adev->iobase + ACX_SLV_MEM_CP);

	/*
	 * SLV_MEM_ADDR[23:2] = SLV_MEM_CTL[5:2] + SLV_MEM_CP[23:5]
	 */
	val = (source & 0x00ffffe0) + (1 << 2);
	acx_writel (val, adev->iobase + ACX_SLV_MEM_ADDR);

	/*
	 * Read the data from the slave data register, rounding up to
	 * the end of the word containing the last byte (hence the >
	 * 0)
	 */
	while (count > 0) {
		*data++ = acx_readl (adev->iobase + ACX_SLV_MEM_DATA);
		count -= 4;
	}

	/*
	 * If the destination wasn't aligned, we would have saved it
	 * in the aligned buffer, so copy it where it should go.
	 */
	if ((uintptr_t) destination & 3) {
		memcpy(destination, aligned_destination, saved_count);
	}
}


/*
 * In the generic slave memory access mode, most of the stuff in the
 * txhostdesc_t is unused.  It's only here because the rest of the ACX
 * driver expects it to be since the PCI version uses indirect host
 * memory organization with DMA.  Since we're not using DMA the only
 * use we have for the host descriptors is to store the packets on the
 * way out.
 */


/*
 * BOM Firmware, EEPROM, Phy
 * ==================================================
 */

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */

/*
 * acxmem_issue_cmd_timeo_debug
 *
 * Sends command to fw, extract result
 *
 * OW, 20100630:
 *
 * The mem device is quite sensible to data access operations,
 * therefore we may not sleep during the command handling.
 *
 * This has manifested as problem during sw-scan while if up. The acx
 * got stuck - most probably due to concurrent data access collision.
 *
 * By not sleeping anymore and doing the entire operation completely
 * under spinlock (thus with irqs disabled), the sw scan problem was
 * solved.
 *
 * We can now run repeating sw scans, under load, without that the acx
 * device gets stuck.
 *
 * Also ifup/down works more reliable on the mem device.
 *
 */
#if 0 // acxmem_issue_cmd_timeo_debug()
int
acxmem_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd,
			void *buffer, unsigned buflen,
			unsigned cmd_timeout, const char *cmdstr)
{
	unsigned long start = jiffies;
	const char *devname;
	unsigned counter;
	u16 irqtype;
	u16 cmd_status=-1;
	int i, j;
	u8 *p;

	acxmem_lock_flags;


	acxmem_lock();

	devname = wiphy_name(adev->hw->wiphy);
	if (!devname || !devname[0] || devname[4] == '%')
		devname = "acx";

	log(L_CTL, "cmd:%s, cmd:0x%04X, buflen:%u, timeout:%ums, type:0x%04X)\n",
		cmdstr, cmd, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *)buffer)->type) : -1);

	if (!(adev->dev_state_mask & ACX_STATE_FW_LOADED)) {
		pr_acxmem("%s: firmware is not loaded yet, cannot execute commands!\n",
			devname);
		goto bad;
	}

	if ((acx_debug & L_DEBUG) && (cmd != ACX1xx_CMD_INTERROGATE)) {
		pr_acxmem("input buffer (len=%u):\n", buflen);
		acx_dump_bytes(buffer, buflen);
	}

	/* wait for firmware to become idle for our command submission */
	counter = 199; /* in ms */
	do {
		cmd_status = acx_read_cmd_type_status(adev);
		/* Test for IDLE state */
		if (!cmd_status)
			break;

		udelay(1000);
	} while (likely(--counter));

	if (counter == 0) {
		/* the card doesn't get idle, we're in trouble */
		pr_acxmem("%s: cmd_status is not IDLE: 0x%04X!=0\n",
			devname, cmd_status);
		goto bad;
	} else if (counter < 190) { /* if waited >10ms... */
		log(L_CTL|L_DEBUG, "waited for IDLE %dms. Please report\n",
			199 - counter);
	}

	/* now write the parameters of the command if needed */
	if (buffer && buflen) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
#if CMD_DISCOVERY
		if (cmd == ACX1xx_CMD_INTERROGATE)
		memset_io(adev->cmd_area + 4, 0xAA, buflen);
#endif
		/*
		 * slave memory version
		 */
		acxmem_copy_to_slavemem(adev, (uintptr_t) (adev->cmd_area + 4), buffer, (cmd
				== ACX1xx_CMD_INTERROGATE) ? 4 : buflen);
	}
	/* now write the actual command type */
	acx_write_cmd_type_status(adev, cmd, 0);

	/* clear CMD_COMPLETE bit. can be set only by IRQ handler: */
	CLEAR_BIT(adev->irq_status, HOST_INT_CMD_COMPLETE);

	/* execute command */
	write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_CMD);
	write_flush(adev);

	/* wait for firmware to process command */

	/* Ensure nonzero and not too large timeout.  Also converts
	 * e.g. 100->99, 200->199 which is nice but not essential */
	cmd_timeout = (cmd_timeout - 1) | 1;
	if (unlikely(cmd_timeout> 1199))
		cmd_timeout = 1199;

	/* we schedule away sometimes (timeout can be large) */
	counter = cmd_timeout;
	do {
		irqtype = read_reg16(adev, IO_ACX_IRQ_STATUS_NON_DES);
		if (irqtype & HOST_INT_CMD_COMPLETE) {
			write_reg16(adev, IO_ACX_IRQ_ACK, HOST_INT_CMD_COMPLETE);
			break;
		}

		if (adev->irq_status & HOST_INT_CMD_COMPLETE)
			break;

		udelay(1000);

	} while (likely(--counter));

	/* save state for debugging */
	cmd_status = acx_read_cmd_type_status(adev);

	/* put the card in IDLE state */
	acx_write_cmd_type_status(adev, ACX1xx_CMD_RESET, 0);

	/* Timed out! */
	if (counter == 0) {

		log(L_ANY, "%s: Timed out %s for CMD_COMPLETE. "
			"irq bits:0x%04X irq_status:0x%04X timeout:%dms "
			"cmd_status:%d (%s)\n", devname,
		       (adev->irqs_active) ? "waiting" : "polling",
		       irqtype, adev->irq_status, cmd_timeout,
		       cmd_status, acx_cmd_status_str(cmd_status));
		log(L_ANY,
			"timeout: counter:%d cmd_timeout:%d cmd_timeout-counter:%d\n",
			counter, cmd_timeout, cmd_timeout - counter);

		if (read_reg16(adev, IO_ACX_IRQ_MASK) == 0xffff) {
			log(L_ANY,"firmware probably hosed - reloading: FIXME: Not implmemented\n");
			FIXME();
		}

	} else if (cmd_timeout - counter > 30) { /* if waited >30ms... */
		log(L_CTL|L_DEBUG,
			"%s for CMD_COMPLETE %dms. count:%d. Please report\n",
			(adev->irqs_active) ? "waited" : "polled",
			cmd_timeout - counter, counter);
	}

	logf1(L_CTL, "cmd=%s, buflen=%u, timeout=%ums, type=0x%04X: %s\n",
		cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *) buffer)->type) : -1,
		acx_cmd_status_str(cmd_status)
	);

	if (cmd_status != 1) { /* it is not a 'Success' */

		/* zero out result buffer
		 * WARNING: this will trash stack in case of illegally
		 * large input length! */

		if (buflen > 388) {
			/*
			 * 388 is maximum command length
			 */
			log(L_ANY, "invalid length 0x%08x\n", buflen);
			buflen = 388;
		}
		p = (u8 *) buffer;
		for (i = 0; i < buflen; i += 16) {
			printk("%04x:", i);
			for (j = 0; (j < 16) && (i + j < buflen); j++) {
				printk(" %02x", *p++);
			}
			printk("\n");
		}

		if (buffer && buflen)
			memset(buffer, 0, buflen);
		goto bad;
	}

	/* read in result parameters if needed */
	if (buffer && buflen && (cmd == ACX1xx_CMD_INTERROGATE)) {
		acxmem_copy_from_slavemem(adev, buffer, (uintptr_t) (adev->cmd_area + 4), buflen);
		if (acx_debug & L_DEBUG) {
			log(L_ANY, "output buffer (len=%u): ", buflen);
			acx_dump_bytes(buffer, buflen);
		}
	}

	/* ok: */
	log(L_DEBUG, "%s: took %ld jiffies to complete\n",
		cmdstr, jiffies - start);

	acxmem_unlock();

	return OK;

	bad:
	/* Give enough info so that callers can avoid printing their
	* own diagnostic messages */
	logf1(L_ANY,
		"cmd=%s, buflen=%u, timeout=%ums, type=0x%04X, status=%s: FAILED\n",
		cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *) buffer)->type) : -1,
		acx_cmd_status_str(cmd_status)
	);

	acxmem_unlock();

	return NOT_OK;
}
#endif // acxmem_issue_cmd_timeo_debug()


/*
 * BOM Init, Configure (Control Path)
 * ==================================================
 */

void acxmem_reset_mac(acx_device_t *adev)
{
	int count;
	u32 tmp;

	/* Windows driver (MEM) does some funny things here */

	/* clear bit 0x200 in register 0x2A0 */
	clear_regbits(adev, 0x2A0, 0x200);

	/* Set bit 0x200 in ACX_GPIO_OUT */
	set_regbits(adev, IO_ACX_GPIO_OUT, 0x200);

	/*
	 * read register 0x900 until its value is 0x8400104C,
	 * sleeping in between reads if it's not immediate
	 */
	tmp = read_reg32(adev, REG_ACX_VENDOR_ID);
	count = 500;
	while (count-- && (tmp != ACX_VENDOR_ID)) {
		mdelay (10);
		tmp = read_reg32(adev, REG_ACX_VENDOR_ID);
	}

	/* end what Windows driver does */

	/* middelay=200: Windows driver does sleeps here for a while with this sequence */
	acx_base_reset_mac(adev, 200);

	/* Windows driver sleeps here for a while with this sequence */
	for (count = 0; count < 200; count++)
		udelay(50);

	/* Windows driver writes 0x10000 to register 0x808 here */
	write_reg32(adev, 0x808, 0x10000);
}

static int acxmem_load_firmware(acx_device_t *adev)
{
	char *fw_image_filename = MEM_FIRMWARE_IMAGE_FILENAME;
	char radio_image_filename[MEM_FIRMWARE_FILENAME_MAXLEN];

	snprintf(radio_image_filename, sizeof(radio_image_filename),
	        MEM_RADIO_IMAGE_FILENAME, adev->radio_type);

	return acx_load_firmware(adev, fw_image_filename, radio_image_filename);
}

#if PATCH_AROUND_BAD_SPOTS
/*
 * arm-linux-objdump -d patch.bin, or
 * od -Ax -t x4 patch.bin after finding the bounds
 * of the .text section with arm-linux-objdump -s patch.bin
 */
static const u32 patch[] = {
	0xe584c030, 0xe59fc008, 0xe92d1000, 0xe59fc004, 0xe8bd8000,
	0x0000080c, 0x0000aa68, 0x605a2200, 0x2c0a689c, 0x2414d80a,
	0x2f00689f, 0x1c27d007, 0x06241e7c, 0x2f000e24, 0xe000d1f6,
	0x602e6018, 0x23036468, 0x480203db, 0x60ca6003, 0xbdf0750a,
	0xffff0808
};

int acxmem_patch_around_bad_spots(acx_device_t *adev)
{
	u32 offset;
	int i;

	firmware_image_t *fw_image = adev->fw_image;

	acxmem_lock_flags;

	acxmem_lock();
	/*
	 * Only want to do this if the firmware is exactly what we
	 * expect for an iPaq 4700; otherwise, bad things would ensue.
	 */
	if ((HX4700_FIRMWARE_CHECKSUM == fw_image->chksum)
		|| (HX4700_ALTERNATE_FIRMWARE_CHECKSUM == fw_image->chksum)) {
		/*
		 * Put the patch after the main firmware image.
		 * 0x950c contains the ACX's idea of the end of the
		 * firmware.  Use that location to load ours (which
		 * depends on that location being 0xab58) then update
		 * that location to point to after ours.
		 */

		offset = read_slavemem32(adev, 0x950c);

		log (L_DEBUG, "patching in at 0x%04x\n", offset);

		for (i = 0; i < sizeof(patch) / sizeof(patch[0]); i++) {
			write_slavemem32(adev, offset, patch[i]);
			offset += sizeof(u32);
		}

		/*
		 * Patch the instruction at 0x0804 to branch to our
		 * ARM patch at 0xab58
		 */
		write_slavemem32(adev, 0x0804, 0xea000000 + (0xab58 - 0x0804 - 8) / 4);

		/*
		 * Patch the instructions at 0x1f40 to branch to our
		 * Thumb patch at 0xab74
		 *
		 * 4a00 ldr r2, [pc, #0]
		 * 4710 bx  r2
		 * .data 0xab74+1
		 */
		write_slavemem32(adev, 0x1f40, 0x47104a00);
		write_slavemem32(adev, 0x1f44, 0x0000ab74 + 1);

		/*
		 * Bump the end of the firmware up to beyond our patch.
		 */
		write_slavemem32(adev, 0x950c, offset);

	}
	acxmem_unlock();

	return 0;
}
#else
int acxmem_patch_around_bad_spots(acx_device_t *adev) { return 0; }
#endif

#if 0
static void acxmem_i_set_multicast_list(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;



	acx_lock(adev, flags);

	/* firmwares don't have allmulti capability,
	 * so just use promiscuous mode instead in this case. */
	if (ndev->flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		SET_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
		/* let kernel know in case *we* needed to set promiscuous */
		ndev->flags |= (IFF_PROMISC|IFF_ALLMULTI);
	} else {
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		SET_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
		ndev->flags &= ~(IFF_PROMISC|IFF_ALLMULTI);
	}

	/* cannot update card settings directly here, atomic context */
	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);

	acx_unlock(adev, flags);


}
#endif


/*
 * BOM Other (Control Path)
 * ==================================================
 */

/*
 * BOM Proc, Debug
 * ==================================================
 */
#if 1 // copied to merge, but needs work
int acxmem_dbgfs_diag_output(struct seq_file *file,
			acx_device_t *adev)
{
	const char *rtl, *thd, *ttl;
	txacxdesc_t *txdesc;
	u8 Ctl_8;
	rxacxdesc_t *rxdesc;
	int i;
	u32 tmp, tmp2;
	txacxdesc_t txd;
	rxacxdesc_t rxd;

	acxmem_lock_flags;


	acxmem_lock();

#if DUMP_MEM_DURING_DIAG > 0
	acxmem_dump_mem (adev, 0, 0x10000);
	panic ("dump finished");
#endif

	seq_printf(file, "** Rx buf **\n");
	rxdesc = adev->hw_rx_queue.acxdescinfo.start;
	if (rxdesc)
		for (i = 0; i < RX_CNT; i++) {
			rtl = (i == adev->hw_rx_queue.tail) ? " [tail]" : "";
			Ctl_8 = read_slavemem8(adev, (uintptr_t)
					&(rxdesc->Ctl_8));
			if (Ctl_8 & DESC_CTL_HOSTOWN)
				seq_printf(file, "%02u (%02x) FULL %-10s",
					i, Ctl_8, rtl);
			else
				seq_printf(file, "%02u (%02x) empty%-10s",
					i, Ctl_8, rtl);

			/* seq_printf(file, "\n"); */

			acxmem_copy_from_slavemem(adev, (u8 *) &rxd,
						(uintptr_t) rxdesc, sizeof(rxd));
			seq_printf(file,
				"%0lx: %04x %04x %04x %04x %04x %04x %04x Ctl_8=%04x %04x %04x %04x %04x %04x %04x %04x\n",
				(uintptr_t) rxdesc,
				rxd.pNextDesc.v,
				rxd.HostMemPtr.v,
				rxd.ACXMemPtr.v,
				rxd.rx_time,
				rxd.total_length,
				rxd.WEP_length,
				rxd.WEP_ofs,
				rxd.Ctl_8,
				rxd.rate,
				rxd.error,
				rxd.SNR,
				rxd.RxLevel,
				rxd.queue_ctrl,
				rxd.unknown,
				rxd.unknown2);
			rxdesc++;
		}

	seq_printf(file, "** Tx buf (free %d, Ieee80211 queue: %s) **\n",
		adev->acx_txbuf_free,
		acx_queue_stopped(adev->hw) ? "STOPPED" : "Running");

	seq_printf(file,
		"** Tx buf %d blocks total, %d available, free list head %04x\n",
		adev->acx_txbuf_numblocks, adev->acx_txbuf_blocks_free,
		adev->acx_txbuf_free);

	txdesc = adev->hw_tx_queue[0].acxdescinfo.start;
	if (txdesc) {
		for (i = 0; i < TX_CNT; i++) {
			thd = (i == adev->hw_tx_queue[0].head) ? " [head]" : "";
			ttl = (i == adev->hw_tx_queue[0].tail) ? " [tail]" : "";
			acxmem_copy_from_slavemem(adev, (u8 *) &txd,
						 (uintptr_t) txdesc, sizeof(txd));

			Ctl_8 = read_slavemem8(adev, (uintptr_t) &(txdesc->Ctl_8));
			if (Ctl_8 & DESC_CTL_ACXDONE)
				seq_printf(file, "%02u ready to free (%02X)%-7s%-7s",
						i, Ctl_8, thd, ttl);
			else if (Ctl_8 & DESC_CTL_HOSTOWN)
				seq_printf(file, "%02u available     (%02X)%-7s%-7s",
						i, Ctl_8, thd, ttl);
			else
				seq_printf(file, "%02u busy          (%02X)%-7s%-7s",
						i, Ctl_8, thd, ttl);

			seq_printf(file,
				"%0lx: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %02x %02x %02x %02x "
				"%02x %02x %02x %02x %04x: ", (uintptr_t) txdesc,
				txd.pNextDesc.v, txd.HostMemPtr.v,
				txd.AcxMemPtr.v,
				txd.tx_time, txd.total_length, txd.Reserved,
				txd.dummy[0], txd.dummy[1], txd.dummy[2],
				txd.dummy[3], txd.Ctl_8, txd.Ctl2_8, txd.error,
				txd.ack_failures, txd.rts_failures,
				txd.rts_ok, txd.u.r1.rate,
				txd.u.r1.queue_ctrl, txd.queue_info);

			tmp = read_slavemem32(adev,
					(uintptr_t) & (txdesc->AcxMemPtr));
			seq_printf(file, " %04x: ", tmp);

			/* Output allocated tx-buffer chain */
#if 1
			if (tmp) {
				while ((tmp2 = read_slavemem32(adev, (u32) tmp)) != 0x02000000) {
					tmp2 = tmp2 << 5;
					seq_printf(file, "%04x=%04x,", tmp, tmp2);
					tmp = tmp2;
				}
				seq_printf(file, " %04x=%04x", tmp, tmp2);
			}
#endif
			seq_printf(file, "\n");

#if 0
			u8 buf[0x200];
			int j, k;

			if (txd.AcxMemPtr.v) {
				acxmem_copy_from_slavemem(adev, buf, txd.AcxMemPtr.v, sizeof(buf));
				for (j = 0; (j < txd.total_length) && (j < (sizeof(buf) - 4)); j
						+= 16) {
					seq_printf(file, "    ");
					for (k = 0; (k < 16) && (j + k < txd.total_length); k++) {
						seq_printf(file, " %02x", buf[j + k + 4]);
					}
					seq_printf(file, "\n");
				}
			}
#endif

			txdesc = acx_advance_txacxdesc(adev, txdesc, 1, 0);
		}
	}

#if 1
	/* Tx-buffer list dump */
	seq_printf(file, "\n");
	seq_printf(file, "* Tx-buffer list dump\n");
	seq_printf(file, "acx_txbuf_numblocks=%d, acx_txbuf_blocks_free=%d, \n"
		"acx_txbuf_start==%04x, acx_txbuf_free=%04x, memblocksize=%d\n",
		adev->acx_txbuf_numblocks, adev->acx_txbuf_blocks_free,
		adev->acx_txbuf_start, adev->acx_txbuf_free, adev->memblocksize);

	tmp = adev->acx_txbuf_start;
	for (i = 0; i < adev->acx_txbuf_numblocks; i++) {
		tmp2 = read_slavemem32(adev, (u32) tmp);
		seq_printf(file, "%02d: %04x=%04x,%04x\n", i, tmp, tmp2, tmp2 << 5);

		tmp += adev->memblocksize;
	}
	seq_printf(file, "\n");
	/* --- */
#endif

	seq_printf(file, "\n"
		"** Generic slave data **\n"
		"irq_mask 0x%04x irq on acx 0x%04x\n"

		"txbuf_start 0x%p, txbuf_area_size %zu\n"
		/* OW TODO Add also the acx tx_buf size available */
		"txdesc_size %zu, txdesc_start 0x%p\n"
		"txhostdesc_start 0x%p, txhostdesc_area_size %zu\n"
		"txbuf start 0x%04x, txbuf size %d\n"

		"rxdesc_start 0x%p\n"
		"rxhostdesc_start 0x%p, rxhostdesc_area_size %zu\n"
		"rxbuf_start 0x%p, rxbuf_area_size %zu\n",

		adev->irq_mask,	read_reg32(adev, IO_ACX_IRQ_STATUS_NON_DES),

		adev->hw_tx_queue[0].bufinfo.start, adev->hw_tx_queue[0].bufinfo.size, adev->hw_tx_queue[0].acxdescinfo.size,
		adev->hw_tx_queue[0].acxdescinfo.start, adev->hw_tx_queue[0].hostdescinfo.start,
		adev->hw_tx_queue[0].hostdescinfo.size, adev->acx_txbuf_start,
		adev->acx_txbuf_numblocks * adev->memblocksize,

		adev->hw_rx_queue.acxdescinfo.start,
		adev->hw_rx_queue.hostdescinfo.start, adev->hw_rx_queue.hostdescinfo.size,
		adev->hw_rx_queue.bufinfo.start, adev->hw_rx_queue.bufinfo.size);

	acxmem_unlock();

	return 0;
}
#endif // acxmem_proc_diag_output()

/*
 * BOM Rx Path
 * ==================================================
 */

void acxmem_process_rxdesc(acx_device_t *adev)
{
	rxhostdesc_t *hostdesc;
	rxacxdesc_t *rxdesc;
	unsigned count, tail;
	u32 addr;
	u8 Ctl_8;

	if (unlikely(acx_debug & L_BUFR))
		acx_log_rxbuffer(adev);

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * rx_tail and the full descriptor we're supposed to
	 * handle. */
	tail = adev->hw_rx_queue.tail;
	count = RX_CNT;
	while (1) {
		hostdesc = &adev->hw_rx_queue.hostdescinfo.start[tail];
		rxdesc = &adev->hw_rx_queue.acxdescinfo.start[tail];
		/* advance tail regardless of outcome of the below test */
		tail = (tail + 1) % RX_CNT;

		/*
		 * Unlike the PCI interface, where the ACX can write
		 * directly to the host descriptors, on the slave
		 * memory interface we have to pull these.  All we
		 * really need to do is check the Ctl_8 field in the
		 * rx descriptor on the ACX, which should be
		 * 0x11000000 if we should process it.
		 */
		Ctl_8 = hostdesc->hd.Ctl_16
			= read_slavemem8(adev, (uintptr_t) &(rxdesc->Ctl_8));
					
		if ((Ctl_8 & DESC_CTL_HOSTOWN) && (Ctl_8 & DESC_CTL_ACXDONE))
			break; /* found it! */

		if (unlikely(!--count))
			/* hmm, no luck: all descs empty, bail out */
			goto end;
	}

	/* now process descriptors, starting with the first we figured
	 * out */
	while (1) {
		log(L_BUFR, "rx: tail=%u Ctl_8=%02X\n", tail, Ctl_8);
		/*
		 * If the ACX has CTL_RECLAIM set on this descriptor
		 * there is no buffer associated; it just wants us to
		 * tell it to reclaim the memory.
		 */
		if (!(Ctl_8 & DESC_CTL_RECLAIM)) {
			/*
			 * slave interface - pull data now
			 */
			hostdesc->hd.length = read_slavemem16(adev,
					(uintptr_t) &(rxdesc->total_length));

			/*
			 * hostdesc->data is an rxbuffer_t, which
			 * includes header information, but the length
			 * in the data packet doesn't.  The header
			 * information takes up an additional 12
			 * bytes, so add that to the length we copy.
			 */
			addr = read_slavemem32(adev,
					(uintptr_t) &(rxdesc->ACXMemPtr));
			if (addr) {
				/*
				 * How can &(rxdesc->ACXMemPtr) above
				 * ever be zero?  Looks like we get
				 * that now and then - try to trap it
				 * for debug.
				 */
				if (addr & 0xffff0000) {
					log(L_ANY, "rxdesc 0x%08lx\n",
						(uintptr_t) rxdesc);
					acxmem_dump_mem(adev, 0, 0x10000);
					panic("Bad access!");
				}
				acxmem_chaincopy_from_slavemem(adev, (u8 *)
					hostdesc->data, addr,
					hostdesc->hd.length
					+ (uintptr_t) &((rxbuffer_t *) 0)->hdr_a3);

				acx_process_rxbuf(adev, hostdesc->data);
			}
		} else
			log(L_ANY, "rx reclaim only!\n");

		hostdesc->hd.Status = 0;

		/*
		 * Let the ACX know we're done.
		 */
		CLEAR_BIT (Ctl_8, DESC_CTL_HOSTOWN);
		SET_BIT (Ctl_8, DESC_CTL_HOSTDONE);
		SET_BIT (Ctl_8, DESC_CTL_RECLAIM);
		write_slavemem8(adev, (uintptr_t) &rxdesc->Ctl_8, Ctl_8);

		/*
		 * Now tell the ACX we've finished with the receive
		 * buffer so it can finish the reclaim.
		 */
		write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_RXPRC);

		/* ok, descriptor is handled, now check the next descriptor */
		hostdesc = &adev->hw_rx_queue.hostdescinfo.start[tail];
		rxdesc = &adev->hw_rx_queue.acxdescinfo.start[tail];

		Ctl_8 = hostdesc->hd.Ctl_16 = read_slavemem8(adev, (uintptr_t) &(rxdesc->Ctl_8));

		/* if next descriptor is empty, then bail out */
		if (!(Ctl_8 & DESC_CTL_HOSTOWN) || !(Ctl_8 & DESC_CTL_ACXDONE))
			break;

		tail = (tail + 1) % RX_CNT;
	}
	end:
		adev->hw_rx_queue.tail = tail;

}

static int acxmem_get_txbuf_space_needed(acx_device_t *adev,
					unsigned int len)
{
	int blocks_needed;

	blocks_needed = len / (adev->memblocksize - 4);
	if (len % (adev->memblocksize - 4))
		blocks_needed++;

	return (blocks_needed);
}

/*
 * Return an acx pointer to the next transmit data block.
 */
u32 acxmem_allocate_acx_txbuf_space(acx_device_t *adev, int count)
{
	u32 block, next, last_block;
	int blocks_needed;

	/*
	 * Take 4 off the memory block size to account for the
	 * reserved word at the start of the block.
	 */
	blocks_needed = acxmem_get_txbuf_space_needed(adev, count);

	if (blocks_needed <= adev->acx_txbuf_blocks_free) {
		/*
		 * Take blocks at the head of the free list.
		 */
		last_block = block = adev->acx_txbuf_free;

		/*
		 * Follow block pointers through the requested number
		 * of blocks both to find the new head of the free
		 * list and to set the flags for the blocks
		 * appropriately.
		 */
		while (blocks_needed--) {
			/*
			 * Keep track of the last block of the allocation
			 */
			last_block = adev->acx_txbuf_free;

			/*
			 * Make sure the end control flag is not set.
			 */
			next = read_slavemem32(adev, adev->acx_txbuf_free) & 0x7ffff;
			write_slavemem32(adev, adev->acx_txbuf_free, next);

			/*
			 * Update the new head of the free list
			 */
			adev->acx_txbuf_free = next << 5;
			adev->acx_txbuf_blocks_free--;

		}

		/*
		 * Flag the last block both by clearing out the next
		 * pointer and marking the control field.
		 */
		write_slavemem32(adev, last_block, 0x02000000);

		/*
		 * If we're out of buffers make sure the free list
		 * pointer is NULL
		 */
		if (!adev->acx_txbuf_blocks_free) {
			adev->acx_txbuf_free = 0;
		}
	} else {
		block = 0;
	}

	return block;
}

/*
 * acxmem_l_dealloc_tx
 *
 * Clears out a previously allocatedvoid acxmem_l_dealloc_tx(tx_t
 * *tx_opaque); transmit descriptor.  The ACX can get confused if we
 * skip transmit descriptors in the queue, so when we don't need a
 * descriptor return it to its original state and move the queue head
 * pointer back.
 *
 */
void acxmem_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque) {
	/*
	 * txdesc is the address of the descriptor on the ACX.
	 */
	txacxdesc_t *txdesc = (txacxdesc_t*) tx_opaque;
	txacxdesc_t tmptxdesc;
	int index;

	acxmem_lock_flags;
	acxmem_lock();

	memset (&tmptxdesc, 0, sizeof(tmptxdesc));
	tmptxdesc.Ctl_8 = DESC_CTL_HOSTOWN | DESC_CTL_FIRSTFRAG;
	tmptxdesc.u.r1.rate = 0x0a;

	/*
	 * Clear out all of the transmit descriptor except for the next pointer
	 */
	acxmem_copy_to_slavemem(adev, (uintptr_t) &(txdesc->HostMemPtr),
			(u8 *) &(tmptxdesc.HostMemPtr), sizeof(tmptxdesc)
					- sizeof(tmptxdesc.pNextDesc));

	/*
	 * This is only called immediately after we've allocated, so
	 * we should be able to set the head back to this descriptor.
	 */
	index = ((u8*) txdesc - (u8*) adev->hw_tx_queue[0].acxdescinfo.start) /
		adev->hw_tx_queue[0].acxdescinfo.size;
	pr_info("acx_dealloc: moving head from %d to %d\n",
	        adev->hw_tx_queue[0].head, index);
	adev->hw_tx_queue[0].head = index;

	acxmem_unlock();

}

/*
 * acxmem_l_alloc_tx
 * Actually returns a txdesc_t* ptr
 *
 * FIXME: in case of fragments, should allocate multiple descrs
 * after figuring out how many we need and whether we still have
 * sufficiently many.
 */
 /* OW TODO Align with pci.c */
tx_t *acxmem_alloc_tx(acx_device_t *adev, unsigned int len) {
	struct txacxdesc *txdesc;
	unsigned head;
	u8 ctl8;
	static int txattempts = 0;
	int blocks_needed;
	acxmem_lock_flags;


	acxmem_lock();

	if (unlikely(!adev->hw_tx_queue[0].free)) {
		log(L_ANY, "BUG: no free txdesc left\n");
		/*
		 * Probably the ACX ignored a transmit attempt and now
		 * there's a packet sitting in the queue we think
		 * should be transmitting but the ACX doesn't know
		 * about.  On the first pass, send the ACX a TxProc
		 * interrupt to try moving things along, and if that
		 * doesn't work (ie, we get called again) completely
		 * flush the transmit queue.
		 */
		if (txattempts < 10) {
			txattempts++;
			log(L_ANY, "trying to wake up ACX\n");
			write_reg16(adev, IO_ACX_INT_TRIG, INT_TRIG_TXPRC);
			write_flush(adev);
		} else {
			txattempts = 0;
			log(L_ANY, "flushing transmit queue.\n");
			acx_clean_txdesc_emergency(adev);
		}
		txdesc = NULL;
		goto end;
	}

	/*
	 * Make a quick check to see if there is transmit buffer space
	 * on the ACX.  This can't guarantee there is enough space for
	 * the packet since we don't yet know how big it is, but it
	 * will prevent at least some annoyances.
	 */

	/* OW 20090815
	 * Make a detailed check of required tx_buf blocks, to avoid
	 * 'out of tx_buf' situation.
	 *
	 * The empty tx_buf and reuse trick in acxmem_l_tx_data doen't
	 * wotk well.  I think it confused mac80211, that was
	 * supposing the packet was send, but actually it was
	 * dropped. According to mac80211 dropping should not happen,
	 * altough a bit of dropping seemed to be ok.
	 *
	 * What we do now is simpler and clean I think:
	 * - first check the number of required blocks
	 * - and if there are not enough: Stop the queue and report NOT_OK.
	 *
	 * Reporting NOT_OK here shouldn't be done neither according
	 * to mac80211, but it seems to work better here.
	 */

	blocks_needed=acxmem_get_txbuf_space_needed(adev, len);
	if (!(blocks_needed <= adev->acx_txbuf_blocks_free)) {
		txdesc = NULL;
		log(L_BUFT, "!(blocks_needed <= adev->acx_txbuf_blocks_free), "
			"len=%i, blocks_needed=%i, acx_txbuf_blocks_free=%i: "
			"Stopping queue.\n",
			len, blocks_needed, adev->acx_txbuf_blocks_free);
		acx_stop_queue(adev->hw, NULL);
		goto end;
	}

	head = adev->hw_tx_queue[0].head;
	/*
	 * txdesc points to ACX memory
	 */
	txdesc = acx_get_txacxdesc(adev, head, 0);
	ctl8 = read_slavemem8(adev, (uintptr_t) &(txdesc->Ctl_8));

	/*
	 * If we don't own the buffer (HOSTOWN) it is certainly not
	 * free; however, we may have previously thought we had enough
	 * memory to send a packet, allocated the buffer then gave up
	 * when we found not enough transmit buffer space on the
	 * ACX. In that case, HOSTOWN and ACXDONE will both be set.
	 */

	/* TODO OW Check if this is correct
	 * TODO 20100115 Changed to DESC_CTL_ACXDONE_HOSTOWN like in pci.c
	 */
	if (unlikely(DESC_CTL_HOSTOWN != (ctl8 & DESC_CTL_ACXDONE_HOSTOWN))) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
		log(L_ANY, "BUG: tx_head:%d Ctl8:0x%02X - failed to find free txdesc\n",
			head, ctl8);
		txdesc = NULL;
		goto end;
	}

	/* Needed in case txdesc won't be eventually submitted for tx */
	write_slavemem8(adev, (uintptr_t) &(txdesc->Ctl_8), DESC_CTL_ACXDONE_HOSTOWN);

	adev->hw_tx_queue[0].free--;
	log(L_BUFT, "tx: got desc %u, %u remain\n", head, adev->hw_tx_queue[0].free);

	/* returning current descriptor, so advance to next free one */
	adev->hw_tx_queue[0].head = (head + 1) % TX_CNT;

	end:

	acxmem_unlock();


	return (tx_t*) txdesc;
}

/*
 * Return buffer space back to the pool by following the next pointers
 * until we find the block marked as the end.  Point the last block to
 * the head of the free list, then update the head of the free list to
 * point to the newly freed memory.  This routine gets called in
 * interrupt context, so it shouldn't block to protect the integrity
 * of the linked list.  The ISR already holds the lock.
 */
void acxmem_reclaim_acx_txbuf_space(acx_device_t *adev, u32 blockptr)
{
	u32 cur, last, next;

	if ((blockptr >= adev->acx_txbuf_start) &&
		(blockptr <= adev->acx_txbuf_start +
		(adev->acx_txbuf_numblocks - 1)	* adev->memblocksize)) {
		cur = blockptr;
		do {
			last = cur;
			next = read_slavemem32(adev, cur);

			/*
			 * Advance to the next block in this allocation
			 */
			cur = (next & 0x7ffff) << 5;

			/*
			 * This block now counts as free.
			 */
			adev->acx_txbuf_blocks_free++;
		} while (!(next & 0x02000000));

		/*
		 * last now points to the last block of that
		 * allocation.  Update the pointer in that block to
		 * point to the free list and reset the free list to
		 * the first block of the free call.  If there were no
		 * free blocks, make sure the new end of the list
		 * marks itself as truly the end.
		 */
		if (adev->acx_txbuf_free) {
			write_slavemem32(adev, last, adev->acx_txbuf_free >> 5);
		} else {
			write_slavemem32(adev, last, 0x02000000);
		}
		adev->acx_txbuf_free = blockptr;
	}

}


/*
 * Initialize the pieces managing the transmit buffer pool on the ACX.
 * The transmit buffer is a circular queue with one 32 bit word
 * reserved at the beginning of each block.  The upper 13 bits are a
 * control field, of which only 0x02000000 has any meaning.  The lower
 * 19 bits are the address of the next block divided by 32.
 */
void acxmem_init_acx_txbuf(acx_device_t *adev)
{
	/*
	 * acx100_init_memory_pools set up txbuf_start and
	 * txbuf_numblocks for us.  All we need to do is reset the
	 * rest of the bookeeping.
	 */

	adev->acx_txbuf_free = adev->acx_txbuf_start;
	adev->acx_txbuf_blocks_free = adev->acx_txbuf_numblocks;

	/*
	 * Initialization leaves the last transmit pool block without
	 * a pointer back to the head of the list, but marked as the
	 * end of the list.  That's how we want to see it, too, so
	 * leave it alone.  This is only ever called after a firmware
	 * reset, so the ACX memory is in the state we want.
	 */

}

/* Re-initialize tx-buffer list
 */
#if 1 // copied to merge, inappropriately
void acxmem_init_acx_txbuf2(acx_device_t *adev)
{
	int i;
	u32 adr, next_adr;

	adr = adev->acx_txbuf_start;
	for (i = 0; i < adev->acx_txbuf_numblocks; i++) {
		next_adr = adr + adev->memblocksize;

		/* Last block is marked with 0x02000000 */
		if (i == adev->acx_txbuf_numblocks - 1) {
			write_slavemem32(adev, adr, 0x02000000);
		}
		/* Else write pointer to next block */
		else {
			write_slavemem32(adev, adr, (next_adr >> 5));
		}
		adr = next_adr;
	}

	adev->acx_txbuf_free = adev->acx_txbuf_start;
	adev->acx_txbuf_blocks_free = adev->acx_txbuf_numblocks;

}
#endif

#if 0 // acxmem_get_txhostdesc()
static txhostdesc_t*
acxmem_get_txhostdesc(acx_device_t *adev, txacxdesc_t* txdesc)
{
	int index = (u8*) txdesc - (u8*) adev->tx.desc_start;
	if (unlikely(ACX_DEBUG && (index % adev->tx.desc_size))) {
		pr_info("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	index /= adev->tx.desc_size;
	if (unlikely(ACX_DEBUG && (index >= TX_CNT))) {
		pr_info("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	return &adev->tx.host.txstart[index * 2];
}
#endif  // acxmem_get_txhostdesc()

/* OW TODO See if this is usable with mac80211 */
/***********************************************************************
 ** acxmem_i_tx_timeout
 **
 ** Called from network core. Must not sleep!
 */
#if 0 // or mem.c:3242:3: warning: passing argument 1 of 'acx_wake_queue'
      /* from incompatible pointer type [enabled by default] */
static void acxmem_i_tx_timeout(struct net_device *ndev)
{
	acx_device_t *adev = ndev2adev(ndev);
	unsigned long flags;
	unsigned int tx_num_cleaned;



	acx_lock(adev, flags);

	/* clean processed tx descs, they may have been completely full */
	tx_num_cleaned = acxmem_tx_clean_txdesc(adev);

	/* nothing cleaned, yet (almost) no free buffers available?
	 * --> clean all tx descs, no matter which status!!  Note that
	 * I strongly suspect that doing emergency cleaning may
	 * confuse the firmware. This is a last ditch effort to get
	 * ANYTHING to work again...
	 *
	 * TODO: it's best to simply reset & reinit hw from scratch...
	 */
	if ((adev->tx_free <= TX_EMERG_CLEAN) && (tx_num_cleaned == 0)) {
		pr_info("%s: FAILED to free any of the many full tx buffers. "
			"Switching to emergency freeing. "
			"Please report!\n", ndev->name);
		acx_clean_txdesc_emergency(adev);
	}

	if (acx_queue_stopped(ndev) && (ACX_STATUS_4_ASSOCIATED == adev->status))
		acx_wake_queue(ndev, "after tx timeout");

	/* stall may have happened due to radio drift, so recalib radio */
	acx_schedule_task(adev, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* do unimportant work last */
	pr_info("%s: tx timeout!\n", ndev->name);
	adev->stats.tx_errors++;

	acx_unlock(adev, flags);


}
#endif


/*
 * acxmem_handle_info_irq
 */

/* Info mailbox format:
 2 bytes: type
 2 bytes: status
 more bytes may follow
 rumors say about status:
 0x0000 info available (set by hw)
 0x0001 information received (must be set by host)
 0x1000 info available, mailbox overflowed (messages lost) (set by hw)
 but in practice we've seen:
 0x9000 when we did not set status to 0x0001 on prev message
 0x1001 when we did set it
 0x0000 was never seen
 conclusion: this is really a bitfield:
 0x1000 is 'info available' bit
 'mailbox overflowed' bit is 0x8000, not 0x1000
 value of 0x0000 probably means that there are no messages at all
 P.S. I dunno how in hell hw is supposed to notice that messages are lost -
 it does NOT clear bit 0x0001, and this bit will probably stay forever set
 after we set it once. Let's hope this will be fixed in firmware someday
 */

/* OW FIXME Old interrupt handler
 * ---
 */
#if 0 // or mem.c:3579:4: error: implicit declaration of function 'acxmem_log_unusual_irq'
static irqreturn_t acxmem_interrupt(int irq, void *dev_id)
{
	acx_device_t *adev = dev_id;
	unsigned long flags;
	unsigned int irqcount = MAX_IRQLOOPS_PER_JIFFY;
	register u16 irqtype;
	u16 unmasked;



	if (!adev)
		return IRQ_NONE;

	/* LOCKING: can just spin_lock() since IRQs are disabled anyway.
	 * I am paranoid */
	acx_lock(adev, flags);

	unmasked = read_reg16(adev, IO_ACX_IRQ_REASON);
	if (unlikely(0xffff == unmasked)) {
		/* 0xffff value hints at missing hardware,
		 * so don't do anything.
		 * Not very clean, but other drivers do the same... */
		log(L_IRQ, "IRQ type:FFFF - device removed? IRQ_NONE\n");
		goto none;
	}

	/* We will check only "interesting" IRQ types */
	irqtype = unmasked & ~adev->irq_mask;
	if (!irqtype) {
		/* We are on a shared IRQ line and it wasn't our IRQ */
		log(L_IRQ, "IRQ type:%04X, mask:%04X - all are masked, IRQ_NONE\n",
				unmasked, adev->irq_mask);
		goto none;
	}

	/* Done here because IRQ_NONEs taking three lines of log
	 ** drive me crazy */


#define IRQ_ITERATE 1
#if IRQ_ITERATE
	if (jiffies != adev->irq_last_jiffies) {
		adev->irq_loops_this_jiffy = 0;
		adev->irq_last_jiffies = jiffies;
	}

	/* safety condition; we'll normally abort loop below in case
	 * no IRQ type occurred */
	while (likely(--irqcount)) {
#endif
		/* ACK all IRQs ASAP */
		write_reg16(adev, IO_ACX_IRQ_ACK, 0xffff);

		log(L_IRQ, "IRQ type:%04X, mask:%04X, type & ~mask:%04X\n",
				unmasked, adev->irq_mask, irqtype);

		/* Handle most important IRQ types first */

		/* OW 20091123 FIXME Rx path stops under load problem:
		 * Maybe the RX rings fills up to fast, we are missing an irq and
		 * then we are then not getting rx irqs anymore
		 */
		if (irqtype & HOST_INT_RX_DATA) {
			log(L_IRQ, "got Rx_Data IRQ\n");
			acxmem_process_rxdesc(adev);
		}

		if (irqtype & HOST_INT_TX_COMPLETE) {
			log(L_IRQ, "got Tx_Complete IRQ\n");
			/* don't clean up on each Tx complete, wait a
			 * bit unless we're going towards full, in
			 * which case we do it immediately, too
			 * (otherwise we might lockup with a full Tx
			 * buffer if we go into acxmem_clean_txdesc()
			 * at a time when we won't wakeup the net
			 * queue in there for some reason...) */
			if (adev->tx_free <= TX_START_CLEAN) {
#if TX_CLEANUP_IN_SOFTIRQ
				acx_schedule_task(adev, ACX_AFTER_IRQ_TX_CLEANUP);
#else
				acxmem_tx_clean_txdesc(adev);
#endif
			}
		}

		/* Less frequent ones */
		if (irqtype & (0 | HOST_INT_CMD_COMPLETE | HOST_INT_INFO
				| HOST_INT_SCAN_COMPLETE)) {
			if (irqtype & HOST_INT_CMD_COMPLETE) {
				log(L_IRQ, "got Command_Complete IRQ\n");
				/* save the state for the running issue_cmd() */
				SET_BIT(adev->irq_status, HOST_INT_CMD_COMPLETE);
			}
			if (irqtype & HOST_INT_INFO) {
				acx_handle_info_irq(adev);
			}
			if (irqtype & HOST_INT_SCAN_COMPLETE) {
				log(L_IRQ, "got Scan_Complete IRQ\n");
				/* need to do that in process context */
				acx_schedule_task(adev, ACX_AFTER_IRQ_COMPLETE_SCAN);
				/* remember that fw is not scanning anymore */
				SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
			}
		}

		/* These we just log, but either they happen rarely
		 * or we keep them masked out */
		if (irqtype & (0
		/* | HOST_INT_RX_DATA */
		/* | HOST_INT_TX_COMPLETE   */
		| HOST_INT_TX_XFER
		| HOST_INT_RX_COMPLETE
		| HOST_INT_DTIM
		| HOST_INT_BEACON
		| HOST_INT_TIMER
		| HOST_INT_KEY_NOT_FOUND
		| HOST_INT_IV_ICV_FAILURE
		/* | HOST_INT_CMD_COMPLETE  */
		/* | HOST_INT_INFO          */
		| HOST_INT_OVERFLOW
		| HOST_INT_PROCESS_ERROR
		/* | HOST_INT_SCAN_COMPLETE */
		| HOST_INT_FCS_THRESHOLD
		| HOST_INT_UNKNOWN))
		{
			acxmem_log_unusual_irq(irqtype);
		}

#if IRQ_ITERATE
		unmasked = read_reg16(adev, IO_ACX_IRQ_REASON);
		irqtype = unmasked & ~adev->irq_mask;
		/* Bail out if no new IRQ bits or if all are masked out */
		if (!irqtype)
			break;

		if (unlikely(++adev->irq_loops_this_jiffy> MAX_IRQLOOPS_PER_JIFFY)) {
			pr_err("acx: too many interrupts per jiffy!\n");
			/* Looks like card floods us with IRQs! Try to stop that */
			write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
			/* This will short-circuit all future attempts to handle IRQ.
			 * We cant do much more... */
			adev->irq_mask = 0;
			break;
		}
	}
#endif

	/* OW 20091129 TODO Currently breaks mem.c ...
	 * If sleeping is required like for update card settings, this is usefull
	 * For now I replaced sleeping for command handling by mdelays.
 *	if (adev->after_interrupt_jobs){
 *		acx_e_after_interrupt_task(adev);
 *	}
	 */


/* OW TODO */
#if 0
	/* Routine to perform blink with range */
	if (unlikely(adev->led_power == 2))
		update_link_quality_led(adev);
#endif

	/* handled: */
	/* write_flush(adev); - not needed, last op was read anyway */
	acx_unlock(adev, flags);

	return IRQ_HANDLED;

	none:
	acx_unlock(adev, flags);

	return IRQ_NONE;
}
#endif
/* --- */


/*
 * BOM Mac80211 Ops
 * ==================================================
 */

static const struct ieee80211_ops acxmem_hw_ops = {
	.tx		= acx_op_tx,
	.conf_tx	= acx_conf_tx,
	.start		= acx_op_start,
	.stop		= acx_op_stop,
	.config		= acx_op_config,
	.set_key	= acx_op_set_key,
	.get_stats	= acx_op_get_stats,

	.add_interface		= acx_op_add_interface,
	.remove_interface	= acx_op_remove_interface,
	.configure_filter	= acx_op_configure_filter,
	.bss_info_changed	= acx_op_bss_info_changed,

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 1, 0)
	.hw_scan		= acx_op_hw_scan,
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
	.get_tx_stats = acx_e_op_get_tx_stats,
#endif
};


/*
 * BOM Helpers
 * ==================================================
 */

INLINE_IO int acxmem_adev_present(acx_device_t *adev)
{
	/* fast version (accesses the first register,
	 * IO_ACX_SOFT_RESET, which should be safe): */
	return acx_readl(adev->iobase) != 0xffffffff;
}

static char acxmem_printable(char c)
{
	return ((c >= 20) && (c < 127)) ? c : '.';
}

/* OW TODO */
#if 0 // or mem.c:3695:42: error: 'acx_device_t' has no member named 'wstats'
static void update_link_quality_led(acx_device_t *adev)
{
	int qual;

	qual = acx_signal_determine_quality(adev->wstats.qual.level,
			adev->wstats.qual.noise);
	if (qual > adev->brange_max_quality)
		qual = adev->brange_max_quality;

	if (time_after(jiffies, adev->brange_time_last_state_change +
			(HZ/2 - HZ/2 * (unsigned long)qual / adev->brange_max_quality ) )) {
		acx_power_led(adev, (adev->brange_last_state == 0));
		adev->brange_last_state ^= 1; /* toggle */
		adev->brange_time_last_state_change = jiffies;
	}
}
#endif


/*
 * BOM Ioctls
 * ==================================================
 */

/* OW TODO Not used in pci either !? */
#if 0 // or mem.c:3717:5: error: conflicting types for 'acx111pci_ioctl_info'
int acx111pci_ioctl_info(struct ieee80211_hw *hw,
			struct iw_request_info *info,
			struct iw_param *vwrq, char *extra)
{
#if ACX_DEBUG > 1

	acx_device_t *adev = hw2adev(hw);
	rxacxdesc_t *rxdesc;
	txacxdesc_t *txdesc;
	rxhostdesc_t *rxhostdesc;
	txhostdesc_t *txhostdesc;
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	unsigned long flags;
	int i;
	char memmap[0x34];
	char rxconfig[0x8];
	char fcserror[0x8];
	char ratefallback[0x5];

	if (!(acx_debug & (L_IOCTL | L_DEBUG)))
		return OK;
	/* using printk() since we checked debug flag already */

	acx_sem_lock(adev);

	if (!IS_ACX111(adev)) {
		pr_acx("acx111-specific function called "
			"with non-acx111 chip, aborting\n");
		goto end_ok;
	}

	/* get Acx111 Memory Configuration */
	memset(&memconf, 0, sizeof(memconf));
	/* BTW, fails with 12 (Write only) error code.
	 ** Retained for easy testing of issue_cmd error handling :) */
	pr_info("Interrogating queue config\n");
	acx_interrogate(adev, &memconf, ACX1xx_IE_QUEUE_CONFIG);
	pr_info("done with queue config\n");

	/* get Acx111 Queue Configuration */
	memset(&queueconf, 0, sizeof(queueconf));
	pr_info("Interrogating mem config options\n");
	acx_interrogate(adev, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);
	pr_info("done with mem config options\n");

	/* get Acx111 Memory Map */
	memset(memmap, 0, sizeof(memmap));
	pr_info("Interrogating mem map\n");
	acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP);
	pr_info("done with mem map\n");

	/* get Acx111 Rx Config */
	memset(rxconfig, 0, sizeof(rxconfig));
	pr_info("Interrogating rxconfig\n");
	acx_interrogate(adev, &rxconfig, ACX1xx_IE_RXCONFIG);
	pr_info("done with queue rxconfig\n");

	/* get Acx111 fcs error count */
	memset(fcserror, 0, sizeof(fcserror));
	pr_info("Interrogating fcs err count\n");
	acx_interrogate(adev, &fcserror, ACX1xx_IE_FCS_ERROR_COUNT);
	pr_info("done with err count\n");

	/* get Acx111 rate fallback */
	memset(ratefallback, 0, sizeof(ratefallback));
	pr_info("Interrogating rate fallback\n");
	acx_interrogate(adev, &ratefallback, ACX1xx_IE_RATE_FALLBACK);
	pr_info("done with rate fallback\n");

	/* force occurrence of a beacon interrupt */
	/* TODO: comment why is this necessary */
	write_reg16(adev, IO_ACX_HINT_TRIG, HOST_INT_BEACON);

	/* dump Acx111 Mem Configuration */
	pr_acx("dump mem config:\n"
		"data read: %d, struct size: %d\n"
		"Number of stations: %1X\n"
		"Memory block size: %1X\n"
		"tx/rx memory block allocation: %1X\n"
		"count rx: %X / tx: %X queues\n"
		"options %1X\n"
		"fragmentation %1X\n"
		"Rx Queue 1 Count Descriptors: %X\n"
		"Rx Queue 1 Host Memory Start: %X\n"
		"Tx Queue 1 Count Descriptors: %X\n"
		"Tx Queue 1 Attributes: %X\n",
		  memconf.len, (int) sizeof(memconf),
			memconf.no_of_stations,
			memconf.memory_block_size,
			memconf.tx_rx_memory_block_allocation,
			memconf.count_rx_queues, memconf.count_tx_queues,
			memconf.options,
			memconf.fragmentation,
			memconf.rx_queue1_count_descs,
			acx2cpu(memconf.rx_queue1_host_rx_start),
			memconf.tx_queue1_count_descs, memconf.tx_queue1_attributes);

	/* dump Acx111 Queue Configuration */
	pr_acx("dump queue head:\n"
		"data read: %d, struct size: %d\n"
		"tx_memory_block_address (from card): %X\n"
		"rx_memory_block_address (from card): %X\n"
		"rx1_queue address (from card): %X\n"
		"tx1_queue address (from card): %X\n"
		"tx1_queue attributes (from card): %X\n",
			queueconf.len, (int) sizeof(queueconf),
			queueconf.tx_memory_block_address,
			queueconf.rx_memory_block_address,
			queueconf.rx1_queue_address,
			queueconf.tx1_queue_address, queueconf.tx1_attributes);

	/* dump Acx111 Mem Map */
	pr_acx("dump mem map:\n"
		"data read: %d, struct size: %d\n"
		"Code start: %X\n"
		"Code end: %X\n"
		"WEP default key start: %X\n"
		"WEP default key end: %X\n"
		"STA table start: %X\n"
		"STA table end: %X\n"
		"Packet template start: %X\n"
		"Packet template end: %X\n"
		"Queue memory start: %X\n"
		"Queue memory end: %X\n"
		"Packet memory pool start: %X\n"
		"Packet memory pool end: %X\n"
		"iobase: %p\n"
		"iobase2: %p\n",
			*((u16 *) &memmap[0x02]), (int) sizeof(memmap),
			*((u32 *) &memmap[0x04]),
			*((u32 *) &memmap[0x08]),
			*((u32 *) &memmap[0x0C]),
			*((u32 *) &memmap[0x10]),
			*((u32 *) &memmap[0x14]),
			*((u32 *) &memmap[0x18]),
			*((u32 *) &memmap[0x1C]),
			*((u32 *) &memmap[0x20]),
			*((u32 *) &memmap[0x24]),
			*((u32 *) &memmap[0x28]),
			*((u32 *) &memmap[0x2C]),
			*((u32 *) &memmap[0x30]), adev->iobase,
			adev->iobase2);

	/* dump Acx111 Rx Config */
	pr_acx("dump rx config:\n"
		"data read: %d, struct size: %d\n"
		"rx config: %X\n"
		"rx filter config: %X\n",
			*((u16 *) &rxconfig[0x02]),	(int) sizeof(rxconfig),
			*((u16 *) &rxconfig[0x04]),	*((u16 *) &rxconfig[0x06]));

	/* dump Acx111 fcs error */
	pr_acx("dump fcserror:\n"
		"data read: %d, struct size: %d\n"
		"fcserrors: %X\n",
			*((u16 *) &fcserror[0x02]), (int) sizeof(fcserror),
			*((u32 *) &fcserror[0x04]));

	/* dump Acx111 rate fallback */
	pr_acx("dump rate fallback:\n"
		"data read: %d, struct size: %d\n"
		"ratefallback: %X\n",
			*((u16 *) &ratefallback[0x02]),
			(int) sizeof(ratefallback),
			*((u8 *) &ratefallback[0x04]));

	/* protect against IRQ */
	acx_lock(adev, flags);

	/* dump acx111 internal rx descriptor ring buffer */
	rxdesc = adev->hw_rx_queue.desc_start;

	/* loop over complete receive pool */
	if (rxdesc)
		for (i = 0; i < RX_CNT; i++) {
			pr_acx("\ndump internal rxdesc %d:\n"
				"mem pos %p\n"
				"next 0x%X\n"
				"acx mem pointer (dynamic) 0x%X\n"
				"CTL (dynamic) 0x%X\n"
				"Rate (dynamic) 0x%X\n"
				"RxStatus (dynamic) 0x%X\n"
				"Mod/Pre (dynamic) 0x%X\n",
				i,
				rxdesc,
				acx2cpu(rxdesc->pNextDesc),
				acx2cpu(rxdesc->ACXMemPtr),
				rxdesc->Ctl_8, rxdesc->rate,	rxdesc->error, rxdesc->SNR);
			rxdesc++;
		}

		/* dump host rx descriptor ring buffer */

		rxhostdesc = adev->hw_rx_queue.host.rxstart;

		/* loop over complete receive pool */
		if (rxhostdesc)
		for (i = 0; i < RX_CNT; i++) {
			pr_acx("\ndump host rxdesc %d:\n"
				"mem pos %p\n"
				"buffer mem pos 0x%X\n"
				"buffer mem offset 0x%X\n"
				"CTL 0x%X\n"
				"Length 0x%X\n"
				"next 0x%X\n"
				"Status 0x%X\n",
				i,
				rxhostdesc,
				acx2cpu(rxhostdesc->data_phy),
				rxhostdesc->data_offset,
				le16_to_cpu(rxhostdesc->hd.Ctl_16),
				le16_to_cpu(rxhostdesc->hd.length),
				acx2cpu(rxhostdesc->desc_phy_next),
				rxhostdesc->Status);
			rxhostdesc++;
		}

		/* dump acx111 internal tx descriptor ring buffer */
		txdesc = adev->tx.desc_start;

		/* loop over complete transmit pool */
		if (txdesc)
		for (i = 0; i < TX_CNT; i++) {
			pr_acx("\ndump internal txdesc %d:\n"
				"size 0x%X\n"
				"mem pos %p\n"
				"next 0x%X\n"
				"acx mem pointer (dynamic) 0x%X\n"
				"host mem pointer (dynamic) 0x%X\n"
				"length (dynamic) 0x%X\n"
				"CTL (dynamic) 0x%X\n"
				"CTL2 (dynamic) 0x%X\n"
				"Status (dynamic) 0x%X\n"
				"Rate (dynamic) 0x%X\n",
				i,
				(int) sizeof(struct txdesc),
				txdesc,
				acx2cpu(txdesc->pNextDesc),
				acx2cpu(txdesc->AcxMemPtr),
				acx2cpu(txdesc->HostMemPtr),
				le16_to_cpu(txdesc->total_length),
				txdesc->Ctl_8,
				txdesc->Ctl2_8,
				txdesc->error,
				txdesc->u.r1.rate);
			txdesc = acx_advance_txdesc(adev, txdesc, 1);
		}

		/* dump host tx descriptor ring buffer */

		txhostdesc = adev->tx.host.txstart;

		/* loop over complete host send pool */
		if (txhostdesc)
		for (i = 0; i < TX_CNT * 2; i++) {
			pr_acx("\ndump host txdesc %d:\n"
				"mem pos %p\n"
				"buffer mem pos 0x%X\n"
				"buffer mem offset 0x%X\n"
				"CTL 0x%X\n"
				"Length 0x%X\n"
				"next 0x%X\n"
				"Status 0x%X\n",
				i,
				txhostdesc,
				acx2cpu(txhostdesc->data_phy),
				txhostdesc->data_offset,
				le16_to_cpu(txhostdesc->hd.Ctl_16),
				le16_to_cpu(txhostdesc->hd.length),
				acx2cpu(txhostdesc->desc_phy_next),
				le32_to_cpu(txhostdesc->Status));
			txhostdesc++;
		}

		/* write_reg16(adev, 0xb4, 0x4); */

		acx_unlock(adev, flags);
		end_ok:

		acx_sem_unlock(adev);
#endif /* ACX_DEBUG */
	return OK;
}

/*********************************************************************/
int acx100mem_ioctl_set_phy_amp_bias(struct ieee80211_hw *hw,
		struct iw_request_info *info,
		struct iw_param *vwrq, char *extra)
{
	/* OW */
	acx_device_t *adev = hw2adev(hw);
	unsigned long flags;
	u16 gpio_old;

	if (!IS_ACX100(adev)) {
		/* WARNING!!!
		 * Removing this check *might* damage
		 * hardware, since we're tweaking GPIOs here after all!!!
		 * You've been warned...
		 * WARNING!!! */
		pr_acx("sorry, setting bias level for non-acx100 "
			"is not supported yet\n");
		return OK;
	}

	if (*extra > 7) {
		pr_acx("invalid bias parameter, range is 0-7\n");
		return -EINVAL;
	}

	acx_sem_lock(adev);

	/* Need to lock accesses to [IO_ACX_GPIO_OUT]:
	 * IRQ handler uses it to update LED */
	acx_lock(adev, flags);
	gpio_old = read_reg16(adev, IO_ACX_GPIO_OUT);
	write_reg16(adev, IO_ACX_GPIO_OUT,
			(gpio_old & 0xf8ff) | ((u16) *extra << 8));
	acx_unlock(adev, flags);

	log(L_DEBUG, "gpio_old: 0x%04X\n", gpio_old);
	pr_acx("%s: PHY power amplifier bias: old:%d, new:%d\n",
		wiphy_name(adev->hw->wiphy),
		(gpio_old & 0x0700) >> 8, (unsigned char) *extra);

	acx_sem_unlock(adev);

	return OK;
}
#endif


/*
 * BOM Driver, Module
 * ==================================================
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static int __devinit acxmem_probe(struct platform_device *pdev)
#else
static int acxmem_probe(struct platform_device *pdev)
#endif
{
	acx_device_t *adev = NULL;
	const char *chip_name;
	int result = -EIO;
	int i;

	struct resource *iomem;
	unsigned long addr_size = 0;
	u8 chip_type;

	acxmem_lock_flags;

	struct ieee80211_hw *hw;

	/* Alloc ieee80211_hw  */
	hw = acx_alloc_hw(&acxmem_hw_ops);
	if (!hw)
		goto fail_ieee80211_alloc_hw;
	adev = hw2adev(hw);

	/* Driver locking and queue mechanics */
	if(acx_init_mechanics(adev))
		goto fail_init_mechanics;

	/* Slave memory host interface setup */
	SET_IEEE80211_DEV(hw, &pdev->dev);
	adev->pdevmem = pdev;
	adev->bus_dev = &pdev->dev;
	adev->dev_type = DEVTYPE_MEM;

	/** begin board specific inits **/
	platform_set_drvdata(pdev, hw);

	/* chiptype is u8 but id->driver_data is ulong
	 * Works for now (possible values are 1 and 2) */
	chip_type = CHIPTYPE_ACX100;
	/* acx100 and acx111 have different PCI memory regions */
	if (chip_type == CHIPTYPE_ACX100)
		chip_name = "ACX100";
	else if (chip_type == CHIPTYPE_ACX111)
		chip_name = "ACX111";
	else {
		pr_acx("unknown chip type 0x%04X\n", chip_type);
		goto fail_unknown_chiptype;
	}

	pr_acx("found %s-based wireless network card\n", chip_name);
	log(L_ANY, "initial debug setting is 0x%04X\n", acx_debug);

	adev->dev_type = DEVTYPE_MEM;
	adev->chip_type = chip_type;
	adev->chip_name = chip_name;
	adev->io = (CHIPTYPE_ACX100 == chip_type) ? IO_ACX100 : IO_ACX111;

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr_size = iomem->end - iomem->start + 1;
	adev->membase = iomem->start;
	adev->iobase = ioremap(iomem->start, addr_size);
	if (!adev->iobase) {
		result = -ENOMEM;
		dev_err(adev->bus_dev, "Couldn't ioremap\n");
		goto fail_ioremap;
	}

	i = platform_get_irq(pdev, 0);
	if (i < 0)
		return i;
	adev->irq = i;

	log(L_ANY, "found an %s-based wireless network card, "
		"irq:%d, "
		"membase:0x%08lx, mem_size:%ld, "
		"iobase:0x%p",
		chip_name,
		adev->irq,
		adev->membase, addr_size,
		adev->iobase);
	log(L_ANY, "the initial debug setting is 0x%04X\n", acx_debug);

	if (adev->irq == 0) {
		pr_acx("can't use IRQ 0\n");
		goto fail_request_irq;
	}

	log(L_IRQ | L_INIT, "using IRQ %d\n", adev->irq);
	/* request shared IRQ handler */
	if (request_irq(adev->irq, acx_interrupt,
			IRQF_SHARED | IRQF_TRIGGER_FALLING,
			KBUILD_MODNAME,
			adev)) {
		pr_acx("%s: request_irq FAILED\n",
			wiphy_name(adev->hw->wiphy));
		result = -EAGAIN;
		goto fail_request_irq;
	}

	log(L_ANY, "request_irq %d successful\n", adev->irq);

	/* Acx irqs shall be off and are enabled later in acx_up */
	acxmem_lock();
	acx_irq_disable(adev);
	acxmem_unlock();
	/** done with board specific setup **/

	/* --- */
	acx_get_hardware_info(adev);
	if (acxmem_load_firmware(adev))
		goto fail_load_firmware;

	if (acx_reset_on_probe(adev))
		goto fail_reset_on_probe;

	/* Debug fs */
	if (acx_debugfs_add_adev(adev))
		goto fail_debugfs;

	/* Init ieee80211_hw  */
	acx_init_ieee80211(adev, hw);
	hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION)
					| BIT(NL80211_IFTYPE_ADHOC);

	if ((result = ieee80211_register_hw(hw)))
	{
		pr_acx("ieee80211_register_hw() FAILED: %d\n", result);
		goto fail_ieee80211_register_hw;
	}

#if CMD_DISCOVERY
	great_inquisitor(adev);
#endif

	result = OK;
	goto done;


	/* error paths: undo everything in reverse order... */
	fail_ieee80211_register_hw:

	fail_debugfs:

	fail_reset_on_probe:

	fail_load_firmware:
	acx_free_firmware(adev);

	fail_request_irq:
	free_irq(adev->irq, adev);

	fail_ioremap:
	if (adev->iobase)
		iounmap(adev->iobase);

	fail_unknown_chiptype:

	fail_init_mechanics:

	fail_ieee80211_alloc_hw:
	acx_delete_dma_regions(adev);
	platform_set_drvdata(pdev, NULL);
	ieee80211_free_hw(hw);

	done:

	return result;
}

/*
 * acxmem_e_remove
 *
 * Shut device down (if not hot unplugged)
 * and deallocate PCI resources for the acx chip.
 *
 * pdev - ptr to PCI device structure containing info about pci
 * configuration
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static int __devexit acxmem_remove(struct platform_device *pdev)
#else
static int acxmem_remove(struct platform_device *pdev)
#endif
{
	struct ieee80211_hw *hw = (struct ieee80211_hw *)
		platform_get_drvdata(pdev);
	acx_device_t *adev = hw2adev(hw);
	acxmem_lock_flags;



	if (!hw) {
		log(L_DEBUG, "card is unused. Skipping any release code\n");
		goto end_no_lock;
	}

	/* Unregister ieee80211 device */
	log(L_INIT, "removing device %s\n", wiphy_name(adev->hw->wiphy));
	ieee80211_unregister_hw(adev->hw);

	/* If device wasn't hot unplugged... */
	if (acxmem_adev_present(adev)) {

		/* disable both Tx and Rx to shut radio down properly */
		if (test_bit(ACX_FLAG_HW_UP, &adev->flags)) {
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);
			clear_bit(ACX_FLAG_HW_UP, &adev->flags);
		}

#ifdef REDUNDANT
		/* put the eCPU to sleep to save power Halting is not
		 * possible currently, since not supported by all
		 * firmware versions */
		acx_issue_cmd(adev, ACX100_CMD_SLEEP, NULL, 0);
#endif

		acxmem_lock();

		/* disable power LED to save power :-) */
		log(L_INIT, "switching off power LED to save power\n");
		acx_power_led(adev, 0);

		/* stop our eCPU */
		if (IS_ACX111(adev)) {
			/* FIXME: does this actually keep halting the eCPU?
			 * I don't think so...
			 */
			acxmem_reset_mac(adev);
		} else {
			u16 temp;

			/* halt eCPU */
			temp = read_reg16(adev, IO_ACX_ECPU_CTRL) | 0x1;
			write_reg16(adev, IO_ACX_ECPU_CTRL, temp);
			write_flush(adev);
		}

		acxmem_unlock();
	}

	/* Debugfs entries */
	acx_debugfs_remove_adev(adev);

	/* IRQs */
	acxmem_lock();
	acx_irq_disable(adev);
	acxmem_unlock();

	synchronize_irq(adev->irq);

	acx_free_firmware(adev);

	free_irq(adev->irq, adev);

	/* finally, clean up PCI bus state */
	acx_delete_dma_regions(adev);
	if (adev->iobase)
		iounmap(adev->iobase);

	/* remove dev registration */
	platform_set_drvdata(pdev, NULL);

	acx_free_mechanics(adev);

	ieee80211_free_hw(adev->hw);

	pr_acxmem("done\n");

end_no_lock:

	return(0);
}

/*
 * TODO: PM code needs to be fixed / debugged / tested.
 */
#ifdef CONFIG_PM
static int acxmem_e_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	acx_device_t *adev;
	struct ieee80211_hw *hw
		= (struct ieee80211_hw *) platform_get_drvdata(pdev);


	pr_acx("suspend handler is experimental!\n");
	pr_acx("sus: dev %p\n", hw);

	/*	if (!netif_running(ndev))
	 goto end;
	 */

	adev = hw2adev(hw);
	pr_info("sus: adev %p\n", adev);

	acx_sem_lock(adev);

	ieee80211_unregister_hw(hw); /* this one cannot sleep */
	acxmem_s_down(hw);
	/* down() does not set it to 0xffff, but here we really want that */
	write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
	write_reg16(adev, IO_ACX_FEMR, 0x0);
	acx_delete_dma_regions(adev);

	/*
	 * Turn the ACX chip off.
	 */
	/* This should be done by the corresponding platform module, e.g. hx4700_acx.c
	 * hwdata->stop_hw();
	 */

	acx_sem_unlock(adev);


	return OK;
}

static int acxmem_e_resume(struct platform_device *pdev)
{
	struct ieee80211_hw *hw = (struct ieee80211_hw *)
		platform_get_drvdata(pdev);
	acx_device_t *adev;



	pr_acx("resume handler is experimental!\n");
	pr_acx("rsm: got dev %p\n", hw);

	/*	if (!netif_running(ndev))
	 return;
	 */

	adev = hw2adev(hw);
	pr_acx("rsm: got adev %p\n", adev);

	acx_sem_lock(adev);

	/*
	 * Turn on the ACX.
	 */
	/* This should be done by the corresponding platform module, e.g. hx4700_acx.c
	 * hwdata->start_hw();
	 */

	acxmem_complete_hw_reset(adev);

	/*
	 * done by acx_set_defaults for initial startup
	 */
	acx_set_interrupt_mask(adev);

	pr_acx("rsm: bringing up interface\n");
	SET_BIT (adev->set_mask, GETSET_ALL);
	acx_up(hw);
	pr_acx("rsm: acx up done\n");

	/* now even reload all card parameters as they were before
	 * suspend, and possibly be back in the network again already
	 * :-)
	 */
	/* - most settings updated in acx_up() */
	if (ACX_STATE_IFACE_UP & adev->dev_state_mask) {
		adev->set_mask = GETSET_ALL;
		acx_update_card_settings(adev);
		pr_acx("rsm: settings updated\n");
	}

	ieee80211_register_hw(hw);
	pr_acx("rsm: device attached\n");

	acx_sem_unlock(adev);


	return OK;
}
#endif /* CONFIG_PM */


static struct platform_driver acxmem_driver = {
	.driver = {
		.name = "acx-mem",
	},
	.probe = acxmem_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
	.remove = __devexit_p(acxmem_remove),
#else
	.remove = acxmem_remove,
#endif
#ifdef CONFIG_PM
	.suspend = acxmem_e_suspend,
	.resume = acxmem_e_resume
#endif /* CONFIG_PM */

};

/*
 * acxmem_e_init_module
 *
 * Module initialization routine, called once at module load time
 */
int __init acxmem_init_module(void)
{
	int res;

	pr_info("built with CONFIG_ACX_MAC80211_MEM\n");
	pr_acx(IO_COMPILE_NOTE);
	log(L_INIT, ENDIAN_STR
		"acx: Slave-memory module initialized, "
		"waiting for cards to probe...\n");

	res = platform_driver_register(&acxmem_driver);

	return res;
}

/*
 * acxmem_e_cleanup_module
 *
 * Called at module unload time. This is our last chance to
 * clean up after ourselves.
 */
void __exit acxmem_cleanup_module(void)
{
	pr_acxmem("cleanup_module\n");
	platform_driver_unregister(&acxmem_driver);
}

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "ACX Slave Memory Driver" );
MODULE_LICENSE( "GPL" );
