/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "acx_debug.h"

#define pr_acx	pr_info

#include <linux/version.h>

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/nl80211.h>
#include <linux/interrupt.h>
#include <net/iw_handler.h>
#include <net/mac80211.h>

#ifdef CONFIG_VLYNQ
#include <linux/vlynq.h>
#endif // CONFIG_VLYNQ

#include "acx.h"
#include "pci.h"
#include "merge.h"
#include "debug.h"
#include "io-acx.h"
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

/* Pick one */
/* #define INLINE_IO static */
#define INLINE_IO static inline

#define FW_NO_AUTO_INCREMENT	1

/*
 * BOM Defines, static vars, etc.
 * ==================================================
 */

/* PCI
 * -----
 */
#ifdef CONFIG_PCI
#define PCI_TYPE		(PCI_USES_MEM | PCI_ADDR0 | PCI_NO_ACPI_WAKE)
#define PCI_ACX100_REGION1		0x01
#define PCI_ACX100_REGION1_SIZE		0x1000	/* Memory size - 4K bytes */
#define PCI_ACX100_REGION2		0x02
#define PCI_ACX100_REGION2_SIZE		0x10000	/* Memory size - 64K bytes */

#define PCI_ACX111_REGION1		0x00
#define PCI_ACX111_REGION1_SIZE		0x2000	/* Memory size - 8K bytes */
#define PCI_ACX111_REGION2		0x01
#define PCI_ACX111_REGION2_SIZE		0x20000	/* Memory size - 128K bytes */

/* Texas Instruments Vendor ID */
#define PCI_VENDOR_ID_TI		0x104c

/* ACX100 22Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1100A	0x8400
#define PCI_DEVICE_ID_TI_TNETW1100B	0x8401

/* ACX111 54Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1130	0x9066

/* PCI Class & Sub-Class code, Network-'Other controller' */
#define PCI_CLASS_NETWORK_OTHERS	0x0280

#define CARD_EEPROM_ID_SIZE 6

#ifndef PCI_D0
/* From include/linux/pci.h */
#define PCI_D0		0
#define PCI_D1		1
#define PCI_D2		2
#define PCI_D3hot	3
#define PCI_D3cold	4
#define PCI_UNKNOWN	5
#define PCI_POWER_ERROR	-1
#endif // !PCI_D0
#endif /* CONFIG_PCI */

#define RX_BUFFER_SIZE (sizeof(rxbuffer_t) + 32)

#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ)

/*
 * BOM Logging
 * ==================================================
 */

#include "inlines.h"

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */

#if 0 // acxpci_issue_cmd_timeo_debug
/*
 * acxpci_issue_cmd_timeo_debug
 *
 * Sends command to fw, extract result
 *
 * NB: we do _not_ take lock inside, so be sure to not touch anything
 * which may interfere with IRQ handler operation
 *
 * TODO: busy wait is a bit silly, so:
 * 1) stop doing many iters - go to sleep after first
 * 2) go to waitqueue based approach: wait, not poll!
 */
int
acxpci_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd,
			void *buffer, unsigned buflen,
			unsigned cmd_timeout, const char *cmdstr)
{
	unsigned long start = jiffies;
	const char *devname;
	unsigned counter;
	u16 irqtype;
	u16 cmd_status=-1;
	unsigned long timeout;



	devname = wiphy_name(adev->hw->wiphy);
	if (!devname || !devname[0] || devname[4] == '%')
		devname = "acx";

	log(L_CTL, "cmd=%s, buflen=%u, timeout=%ums, type=0x%04X\n",
		cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *) buffer)->type) : -1);

	if (!(adev->dev_state_mask & ACX_STATE_FW_LOADED)) {
		pr_acx("%s: firmware is not loaded yet, "
		       "cannot execute commands!\n", devname);
		goto bad;
	}

	if ((acx_debug & L_DEBUG) && (cmd != ACX1xx_CMD_INTERROGATE)) {
		pr_acx("input buffer (len=%u):\n", buflen);
		acx_dump_bytes(buffer, buflen);
	}

	/* wait for firmware to become idle for our command submission */
	timeout = HZ / 5;
	counter = (timeout * 1000 / HZ) - 1;	/* in ms */
	timeout += jiffies;
	do {
		cmd_status = acx_read_cmd_type_status(adev);
		/* Test for IDLE state */
		if (!cmd_status)
			break;
		if (counter % 8 == 0) {
			if (time_after(jiffies, timeout)) {
				counter = 0;
				break;
			}
			/* we waited 8 iterations, no luck. Sleep 8 ms */
			acx_mwait(8);
		}
	} while (likely(--counter));

	if (!counter) {
		/* the card doesn't get idle, we're in trouble */
		pr_acx("%s: cmd_status is not IDLE: 0x%04X!=0\n",
			devname, cmd_status);
		goto bad;
	}
	else if (counter < 190) {	/* if waited >10ms... */
		log(L_CTL | L_DEBUG,
			"waited for IDLE %dms. Please report\n",
			199 - counter);
	}

	/* now write the parameters of the command if needed */
	if (buffer && buflen) {
		/* if it's an INTERROGATE command, just pass the
		 * length of parameters to read, as data */
#if CMD_DISCOVERY
		if (cmd == ACX1xx_CMD_INTERROGATE)
			memset_io(adev->cmd_area + 4, 0xAA, buflen);
#endif
		/* adev->cmd_area points to PCI device's memory, not to RAM! */
		memcpy_toio(adev->cmd_area + 4, buffer,
			    (cmd == ACX1xx_CMD_INTERROGATE) ? 4 : buflen);
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
	if (unlikely(cmd_timeout > 1199))
		cmd_timeout = 1199;

	/* we schedule away sometimes (timeout can be large) */
	counter = cmd_timeout;
	timeout = jiffies + cmd_timeout * HZ / 1000;


	do {
		irqtype = read_reg16(adev, IO_ACX_IRQ_STATUS_NON_DES);
		if (irqtype & HOST_INT_CMD_COMPLETE) {
			write_reg16(adev, IO_ACX_IRQ_ACK,
				    HOST_INT_CMD_COMPLETE);
			break;
		}

		if (adev->irq_status & HOST_INT_CMD_COMPLETE)
			break;

		if (counter % 8 == 0) {
			/* Timeout */
			if (time_after(jiffies, timeout)) {
				counter = -1;
				break;
			}
			/* we waited 8 iterations, no luck. Sleep 8 ms */
			acx_mwait(8);
		}
	} while (likely(--counter));

	/* save state for debugging */
	cmd_status = acx_read_cmd_type_status(adev);

	/* put the card in IDLE state */
	acx_write_cmd_type_status(adev, 0, 0);

	/* Timed out! */
	if (counter == -1) {
		log(L_ANY, "%s: timed out %s for CMD_COMPLETE. "
		       "irq bits:0x%04X irq_status:0x%04X timeout:%dms "
		       "cmd_status:%d (%s)\n", devname,
                       (adev->irqs_active) ? "waiting" : "polling",
		       irqtype, adev->irq_status, cmd_timeout,
		       cmd_status, acx_cmd_status_str(cmd_status));

		log(L_ANY, "timeout: counter:%d cmd_timeout:%d cmd_timeout-counter:%d\n",
			counter, cmd_timeout, cmd_timeout - counter);

	} else if ((cmd_timeout - counter) > 30) {	/* if waited >30ms... */
		log(L_CTL | L_DEBUG,
			"%s for CMD_COMPLETE %dms. count:%d. Please report\n",
			(adev->irqs_active) ? "waited" : "polled",
			cmd_timeout - counter, counter);
	}

	logf1(L_CTL, "%s: cmd=%s, buflen=%u, timeout=%ums, type=0x%04X: %s\n",
		devname, cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *) buffer)->type) : -1,
		acx_cmd_status_str(cmd_status)
	);

	if (1 != cmd_status) {	/* it is not a 'Success' */
		/* zero out result buffer
		 * WARNING: this will trash stack in case of illegally
		 * large input length! */
		if (buffer && buflen)
			memset(buffer, 0, buflen);
		goto bad;
	}

	/* read in result parameters if needed */
	if (buffer && buflen && (cmd == ACX1xx_CMD_INTERROGATE)) {
		/* adev->cmd_area points to PCI device's memory, not to RAM! */
		memcpy_fromio(buffer, adev->cmd_area + 4, buflen);
		if (acx_debug & L_DEBUG) {
			pr_acx("output buffer (len=%u): ", buflen);
			acx_dump_bytes(buffer, buflen);
		}
	}
	/* ok: */
	log(L_DEBUG, "%s: took %ld jiffies to complete\n",
		cmdstr, jiffies - start);


	return OK;

     bad:
	/* Give enough info so that callers can avoid printing their
	 * own diagnostic messages */
	logf1(L_ANY,
		"%s: cmd=%s, buflen=%u, timeout=%ums, type=0x%04X, status=%s: FAILED\n",
		devname, cmdstr, buflen, cmd_timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *) buffer)->type) : -1,
		acx_cmd_status_str(cmd_status)
	);
	/* dump_stack(); */


	return NOT_OK;
}
#endif // acxpci_issue_cmd_timeo_debug

/*
 * BOM Init, Configuration (Control Path)
 * ==================================================
 */


void acxpci_reset_mac(acx_device_t * adev)
{
	/* Vlynq doesn't do this reset sequence; it even crashes if done */
	if (adev->dev_is_vlynq)
		return;

	acx_base_reset_mac(adev, 0);
}

/*
 * BOM Other (Control Path)
 * ==================================================
 */

/* FIXME: update_link_quality_led was a stub - let's comment it and avoid
 * compiler warnings */
/*
static void update_link_quality_led(acx_device_t * adev)
{
	int qual;

	qual =
	    acx_signal_determine_quality(adev->wstats.qual.level,
					 adev->wstats.qual.noise);
	if (qual > adev->brange_max_quality)
		qual = adev->brange_max_quality;

	if (time_after(jiffies, adev->brange_time_last_state_change +
		       (HZ / 2 -
			HZ / 2 * (unsigned long)qual /
			adev->brange_max_quality))) {
		acx_power_led(adev, (adev->brange_last_state == 0));
		adev->brange_last_state ^= 1;	// toggle
		adev->brange_time_last_state_change = jiffies;
	}
}
*/


/*
 * BOM Proc, Debug
 * ==================================================
 */

int acxpci_dbgfs_diag_output(struct seq_file *file, acx_device_t *adev)
{
	const char *rtl, *thd, *ttl;
	rxhostdesc_t *rxhostdesc;
	txacxdesc_t *txdesc;
	int i;
	int queue_id;



	seq_printf(file, "** Rx buf **\n");
	rxhostdesc = adev->hw_rx_queue.hostdescinfo.start;
	if (rxhostdesc)
		for (i = 0; i < RX_CNT; i++) {
			rtl = (i == adev->hw_rx_queue.tail) ? " [tail]" : "";
			if ((rxhostdesc->hd.Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
			    && (rxhostdesc->hd.Status & cpu_to_le32(DESC_STATUS_FULL)))
				seq_printf(file, "%02u FULL%s\n", i, rtl);
			else
				seq_printf(file, "%02u empty%s\n", i, rtl);
			rxhostdesc++;
		}

	for(queue_id=0; queue_id<adev->num_hw_tx_queues; queue_id++){

		seq_printf(file, "** Tx buf (q=%d, free %d, Ieee80211 queue: %s) **\n",
			queue_id, adev->hw_tx_queue[queue_id].free,
			acx_queue_stopped(adev->hw) ? "STOPPED" : "running");

		txdesc = adev->hw_tx_queue[queue_id].acxdescinfo.start;
		if (txdesc)
			for (i = 0; i < TX_CNT; i++) {
				thd = (i == adev->hw_tx_queue[queue_id].head) ? " [head]" : "";
				ttl = (i == adev->hw_tx_queue[queue_id].tail) ? " [tail]" : "";

				if (txdesc->Ctl_8 & DESC_CTL_ACXDONE)
					seq_printf(file, "%02u Ready to free (%02X)%s%s", i, txdesc->Ctl_8,
						thd, ttl);
				else if (txdesc->Ctl_8 & DESC_CTL_HOSTOWN)
					seq_printf(file, "%02u Available     (%02X)%s%s", i, txdesc->Ctl_8,
						thd, ttl);
				else
					seq_printf(file, "%02u Busy          (%02X)%s%s", i, txdesc->Ctl_8,
						thd, ttl);
				seq_printf(file, "\n");

				txdesc = acx_advance_txacxdesc(adev, txdesc, 1, queue_id);
			}
		seq_printf(file,
		           "\n"
		           "** PCI data **\n"
		           "txbuf_start %p, txbuf_area_size %u, txbuf_startphy %08llx\n"
		           "txdesc_size %u, txdesc_start %p\n"
		           "txhostdesc_start %p, txhostdesc_area_size %u, txhostdesc_startphy %08llx\n"
		           "rxdesc_start %p\n"
		           "rxhostdesc_start %p, rxhostdesc_area_size %u, rxhostdesc_startphy %08llx\n"
		           "rxbuf_start %p, rxbuf_area_size %u, rxbuf_startphy %08llx\n",
		           adev->hw_tx_queue[queue_id].bufinfo.start, adev->hw_tx_queue[queue_id].bufinfo.size,
		           (unsigned long long)adev->hw_tx_queue[queue_id].bufinfo.phy,
		           adev->hw_tx_queue[queue_id].acxdescinfo.size, adev->hw_tx_queue[queue_id].acxdescinfo.start,
		           adev->hw_tx_queue[queue_id].hostdescinfo.start, adev->hw_tx_queue[queue_id].hostdescinfo.size,
		           (unsigned long long)adev->hw_tx_queue[queue_id].hostdescinfo.phy,
		           adev->hw_rx_queue.acxdescinfo.start,
		           adev->hw_rx_queue.hostdescinfo.start, adev->hw_rx_queue.hostdescinfo.size,
		           (unsigned long long)adev->hw_rx_queue.hostdescinfo.phy,
		           adev->hw_rx_queue.bufinfo.start, adev->hw_rx_queue.bufinfo.size,
		           (unsigned long long)adev->hw_rx_queue.bufinfo.phy);
	}


	return 0;
}

/*
 * BOM Rx Path
 * ==================================================
 */


/*
 * BOM Tx Path
 * ==================================================
 */

/*
 * acxpci_alloc_tx
 * Actually returns a txdesc_t* ptr
 *
 * FIXME: in case of fragments, should allocate multiple descrs after
 * figuring out how many we need and whether we still have
 * sufficiently many.
 */
tx_t* acxpci_alloc_tx(acx_device_t * adev, int queue_id)
{
	struct txacxdesc *txdesc;
	unsigned head;
	u8 ctl8;



	if (unlikely(!adev->hw_tx_queue[queue_id].free)) {
		pr_acx("BUG: no free txdesc left\n");
		txdesc = NULL;
		goto end;
	}

	head = adev->hw_tx_queue[queue_id].head;
	txdesc = acx_get_txacxdesc(adev, head, queue_id);
	ctl8 = txdesc->Ctl_8;

	/* 2005-10-11: there were several bug reports on this
	 * happening but now cause seems to be understood & fixed */

	/* TODO OW Check if this is correct */
	if (unlikely(DESC_CTL_HOSTOWN != (ctl8 & DESC_CTL_ACXDONE_HOSTOWN))) {
		/* whoops, descr at current index is not free, so
		 * probably ring buffer already full */
		pr_acx("BUG: tx_head:%d Ctl8:0x%02X - failed to find "
		       "free txdesc\n", head, ctl8);
		txdesc = NULL;
		goto end;
	}

	/* Needed in case txdesc won't be eventually submitted for tx */
	txdesc->Ctl_8 = DESC_CTL_ACXDONE_HOSTOWN;

	adev->hw_tx_queue[queue_id].free--;
	log(L_BUFT, "tx: got desc %u, %u remain\n", head, adev->hw_tx_queue[queue_id].free);

	/* returning current descriptor, so advance to next free one */
	adev->hw_tx_queue[queue_id].head = (head + 1) % TX_CNT;
end:


	return (tx_t *) txdesc;
}

void acxpci_process_rxdesc(acx_device_t *adev)
{
	register rxhostdesc_t *hostdesc;
	unsigned count, tail;

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

		/* advance tail regardless of outcome of the below test */
		tail = (tail + 1) % RX_CNT;

		if ((hostdesc->hd.Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		        && (hostdesc->hd.Status & cpu_to_le32(DESC_STATUS_FULL)))
			break; /* found it! */

		if (unlikely(!--count))
			/* hmm, no luck: all descs empty, bail out */
			goto end;
	}

	/* now process descriptors, starting with the first we figured out */
	while (1) {
		log(L_BUF,
		        "rx: tail=%u Ctl_16=%04X Status=%08X\n", tail, hostdesc->hd.Ctl_16, hostdesc->hd.Status);

		acx_process_rxbuf(adev, hostdesc->data);
		hostdesc->hd.Status = 0;

		/* flush all writes before adapter sees CTL_HOSTOWN change */
		wmb();

		/* Host no longer owns this, needs to be LAST */
		CLEAR_BIT(hostdesc->hd.Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));

		/* ok, descriptor is handled, now check the next descriptor */
		hostdesc = &adev->hw_rx_queue.hostdescinfo.start[tail];

		/* if next descriptor is empty, then bail out */
		if (!(hostdesc->hd.Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		        || !(hostdesc->hd.Status & cpu_to_le32(DESC_STATUS_FULL)))
			break;

		tail = (tail + 1) % RX_CNT;
	}

	end:
	adev->hw_rx_queue.tail = tail;
}



/*
 * BOM Irq Handling, Timer
 * ==================================================
 */


/*
 * acxpci_handle_info_irq
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

/*
 * BOM Mac80211 Ops
 * ==================================================
 */

static const struct ieee80211_ops acxpci_hw_ops = {
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
	.set_tim = acx_op_set_tim,
};

/*
 * BOM Helpers
 * ==================================================
 */

INLINE_IO int acxpci_adev_present(acx_device_t *adev)
{
	/* fast version (accesses the first register,
	 * IO_ACX_SOFT_RESET, which should be safe): */
	return acx_readl(adev->iobase) != 0xffffffff;
}


/*
 * BOM Ioctls
 * ==================================================
 */

#if 0	/* acx111pci_ioctl_info() plus */
int acx111pci_ioctl_info(struct net_device *ndev,
			struct iw_request_info *info,
			struct iw_param *vwrq, char *extra)
{
#if ACX_DEBUG > 1
	acx_device_t *adev = ndev2adev(ndev);
	rxdesc_t *rxdesc;
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
	/* BTW, fails with 12 (Write only) error code.  Retained for
	 * easy testing of issue_cmd error handling :) */
	acx_interrogate(adev, &memconf, ACX1xx_IE_QUEUE_CONFIG);

	/* get Acx111 Queue Configuration */
	memset(&queueconf, 0, sizeof(queueconf));
	acx_interrogate(adev, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);

	/* get Acx111 Memory Map */
	memset(memmap, 0, sizeof(memmap));
	acx_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP);

	/* get Acx111 Rx Config */
	memset(rxconfig, 0, sizeof(rxconfig));
	acx_interrogate(adev, &rxconfig, ACX1xx_IE_RXCONFIG);

	/* get Acx111 fcs error count */
	memset(fcserror, 0, sizeof(fcserror));
	acx_interrogate(adev, &fcserror, ACX1xx_IE_FCS_ERROR_COUNT);

	/* get Acx111 rate fallback */
	memset(ratefallback, 0, sizeof(ratefallback));
	acx_interrogate(adev, &ratefallback, ACX1xx_IE_RATE_FALLBACK);

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
	       memconf.len, (int)sizeof(memconf),
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
	       queueconf.len, (int)sizeof(queueconf),
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
	       *((u16 *) & memmap[0x02]), (int)sizeof(memmap),
	       *((u32 *) & memmap[0x04]),
	       *((u32 *) & memmap[0x08]),
	       *((u32 *) & memmap[0x0C]),
	       *((u32 *) & memmap[0x10]),
	       *((u32 *) & memmap[0x14]),
	       *((u32 *) & memmap[0x18]),
	       *((u32 *) & memmap[0x1C]),
	       *((u32 *) & memmap[0x20]),
	       *((u32 *) & memmap[0x24]),
	       *((u32 *) & memmap[0x28]),
	       *((u32 *) & memmap[0x2C]),
	       *((u32 *) & memmap[0x30]), adev->iobase, adev->iobase2);

	/* dump Acx111 Rx Config */
	pr_acx("dump rx config:\n"
	       "data read: %d, struct size: %d\n"
	       "rx config: %X\n"
	       "rx filter config: %X\n",
	       *((u16 *) & rxconfig[0x02]), (int)sizeof(rxconfig),
	       *((u16 *) & rxconfig[0x04]), *((u16 *) & rxconfig[0x06]));

	/* dump Acx111 fcs error */
	pr_acx("dump fcserror:\n"
	       "data read: %d, struct size: %d\n"
	       "fcserrors: %X\n",
	       *((u16 *) & fcserror[0x02]), (int)sizeof(fcserror),
	       *((u32 *) & fcserror[0x04]));

	/* dump Acx111 rate fallback */
	pr_acx("dump rate fallback:\n"
	       "data read: %d, struct size: %d\n"
	       "ratefallback: %X\n",
	       *((u16 *) & ratefallback[0x02]), (int)sizeof(ratefallback),
	       *((u8 *) & ratefallback[0x04]));

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
			       rxdesc->Ctl_8,
			       rxdesc->rate, rxdesc->error, rxdesc->SNR);
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
			       le16_to_cpu(rxhostdesc->length),
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
			       (int)sizeof(struct txdesc),
			       txdesc,
			       acx2cpu(txdesc->pNextDesc),
			       acx2cpu(txdesc->AcxMemPtr),
			       acx2cpu(txdesc->HostMemPtr),
			       le16_to_cpu(txdesc->total_length),
			       txdesc->Ctl_8,
			       txdesc->Ctl2_8, txdesc->error,
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
			       le16_to_cpu(txhostdesc->length),
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


/*******************************************************************/
int acx100pci_ioctl_set_phy_amp_bias(struct net_device *ndev,
				struct iw_request_info *info,
				struct iw_param *vwrq, char *extra)
{
	acx_device_t *adev = ndev2adev(ndev);
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
		    (gpio_old & 0xf8ff) | ((u16) * extra << 8));
	acx_unlock(adev, flags);

	log(L_DEBUG, "gpio_old: 0x%04X\n", gpio_old);
	pr_acx("%s: PHY power amplifier bias: old:%d, new:%d\n",
	       ndev->name, (gpio_old & 0x0700) >> 8, (unsigned char)*extra);

	acx_sem_unlock(adev);

	return OK;
}
#endif	/* acx111pci_ioctl_info() plus */


/*
 * BOM Driver, Module
 * ==================================================
 */

/*
 * acxpci_probe
 *
 * Probe routine called when a PCI device w/ matching ID is found.
 * Here's the sequence:
 *   - Allocate the PCI resources.
 *   - Read the PCMCIA attribute memory to make sure we have a WLAN card
 *   - Reset the MAC
 *   - Initialize the dev and wlan data
 *   - Initialize the MAC
 *
 * pdev	- ptr to pci device structure containing info about pci configuration
 * id	- ptr to the device id entry that matched this device
 */
#ifdef CONFIG_PCI
static int __devinit acxpci_probe(struct pci_dev *pdev,
				const struct pci_device_id *id)
{
	acx111_ie_configoption_t co;
	unsigned long mem_region1 = 0;
	unsigned long mem_region2 = 0;
	unsigned long mem_region1_size;
	unsigned long mem_region2_size;
	unsigned long phymem1;
	unsigned long phymem2;
	void __iomem *mem1 = NULL;
	void __iomem *mem2 = NULL;
	acx_device_t *adev = NULL;
	const char *chip_name;
	int result = -EIO;
	int err;
	u8 chip_type;
	struct ieee80211_hw *hw;

	/* Initialize ieee80211_hw  */
	hw = ieee80211_alloc_hw(sizeof(struct acx_device), &acxpci_hw_ops);
	if (!hw) {
		pr_acx("could not allocate ieee80211 structure %s\n",
		       pci_name(pdev));
		goto fail_ieee80211_alloc_hw;
	}

	adev = hw2adev(hw);
	memset(adev, 0, sizeof(*adev));

	acx_init_ieee80211(adev, hw);
	SET_IEEE80211_DEV(hw, &pdev->dev);
	hw->wiphy->interface_modes =
			BIT(NL80211_IFTYPE_STATION) |
			BIT(NL80211_IFTYPE_ADHOC) |
			BIT(NL80211_IFTYPE_AP);

	/** Set up our private interface **/
	spin_lock_init(&adev->spinlock);	/* initial state: unlocked */
	/* We do not start with downed sem: we want PARANOID_LOCKING to work */
	pr_acx("mutex_init(&adev->mutex); // adev = 0x%px\n", adev);
	mutex_init(&adev->mutex);

	adev->pdev = pdev;
	adev->bus_dev = &pdev->dev;
	adev->dev_type = DEVTYPE_PCI;

/** Finished with private interface **/

/** begin board specific inits **/
	pci_set_drvdata(pdev, hw);

	/* Enable the PCI device */
	if (pci_enable_device(pdev)) {
		pr_acx("pci_enable_device() FAILED\n");
		result = -ENODEV;
		goto fail_pci_enable_device;
	}

	/* enable busmastering (required for CardBus) */
	pci_set_master(pdev);

	/* Specify DMA mask 30-bit. Problem was triggered from
	 * >=2.6.33 on x86_64 */
	adev->bus_dev->coherent_dma_mask = DMA_BIT_MASK(30);

	/* chiptype is u8 but id->driver_data is ulong Works for now
	 * (possible values are 1 and 2) */
	chip_type = (u8) id->driver_data;
	/* acx100 and acx111 have different PCI memory regions */
	if (chip_type == CHIPTYPE_ACX100) {
		chip_name = "ACX100";
		mem_region1 = PCI_ACX100_REGION1;
		mem_region1_size = PCI_ACX100_REGION1_SIZE;

		mem_region2 = PCI_ACX100_REGION2;
		mem_region2_size = PCI_ACX100_REGION2_SIZE;
	} else if (chip_type == CHIPTYPE_ACX111) {
		chip_name = "ACX111";
		mem_region1 = PCI_ACX111_REGION1;
		mem_region1_size = PCI_ACX111_REGION1_SIZE;

		mem_region2 = PCI_ACX111_REGION2;
		mem_region2_size = PCI_ACX111_REGION2_SIZE;
	} else {
		pr_acx("unknown chip type 0x%04X\n", chip_type);
		goto fail_unknown_chiptype;
	}

	/* Figure out our resources
	 *
	 * Request our PCI IO regions
	 */
	err = pci_request_region(pdev, mem_region1, "acx_1");
	if (err) {
		pr_warn("pci_request_region (1/2) FAILED!"
			"No cardbus support in kernel?\n");
		goto fail_request_mem_region1;
	}

	phymem1 = pci_resource_start(pdev, mem_region1);

	err = pci_request_region(pdev, mem_region2, "acx_2");
	if (err) {
		pr_warn("pci_request_region (2/2) FAILED!\n");
		goto fail_request_mem_region2;
	}

	phymem2 = pci_resource_start(pdev, mem_region2);

	/*
	 * We got them? Map them!
	 *
	 * We pass 0 as the third argument to pci_iomap(): it will map
	 * the full region in this case, which is what we want.
	 */

	mem1 = pci_iomap(pdev, mem_region1, 0);
	if (!mem1) {
		pr_warn("ioremap() FAILED\n");
		goto fail_iomap1;
	}

	mem2 = pci_iomap(pdev, mem_region2, 0);
	if (!mem2) {
		pr_warn("ioremap() #2 FAILED\n");
		goto fail_iomap2;
	}

	pr_acx("found an %s-based wireless network card at %s, irq:%d, "
	       "phymem1:0x%lX, phymem2:0x%lX, mem1:0x%p, mem1_size:%ld, "
	       "mem2:0x%p, mem2_size:%ld\n",
	       chip_name, pci_name(pdev), pdev->irq, phymem1, phymem2,
	       mem1, mem_region1_size, mem2, mem_region2_size);
	log(L_ANY, "the initial debug setting is 0x%04X\n", acx_debug);
	adev->chip_type = chip_type;
	adev->chip_name = chip_name;
	adev->io = (CHIPTYPE_ACX100 == chip_type) ? IO_ACX100 : IO_ACX111;
	adev->membase = phymem1;
	adev->iobase = mem1;
	adev->membase2 = phymem2;
	adev->iobase2 = mem2;
	adev->irq = pdev->irq;

	if (adev->irq == 0) {
		pr_acx("can't use IRQ 0\n");
		goto fail_no_irq;
	}

	/* request shared IRQ handler */
	if (request_irq(adev->irq, acx_interrupt, IRQF_SHARED, KBUILD_MODNAME,
			adev)) {
		pr_acx("%s: request_irq FAILED\n", wiphy_name(adev->hw->wiphy));
		result = -EAGAIN;
		goto fail_request_irq;
	}
	log(L_IRQ | L_INIT, "using IRQ %d: OK\n", pdev->irq);

	/* Acx irqs shall be off and are enabled later in acx_up */
	acx_irq_disable(adev);

	/* to find crashes due to weird driver access
	 * to unconfigured interface (ifup) */
	adev->mgmt_timer.function = (void (*)(unsigned long))0x0000dead;


#ifdef NONESSENTIAL_FEATURES
	acx_show_card_eeprom_id(adev);
#endif /* NONESSENTIAL_FEATURES */


	/* PCI setup is finished, now start initializing the card */
	/* ----- */

	acx_init_task_scheduler(adev);

	/* Mac80211 Tx_queue */
	INIT_WORK(&adev->tx_work, acx_tx_work);
	skb_queue_head_init(&adev->tx_queue);

	/* NB: read_reg() reads may return bogus data before
	 * reset_dev(), since the firmware which directly controls
	 * large parts of the I/O registers isn't initialized yet.
	 * acx100 seems to be more affected than acx111 */
	if (OK != acx_reset_dev(adev))
		goto fail_reset_dev;

	if (IS_ACX100(adev)) {
		/* ACX100: configopt struct in cmd mailbox - directly
		 * after reset */
		memcpy_fromio(&co, adev->cmd_area, sizeof(co));
	}

	if (OK != acx_init_mac(adev))
		goto fail_init_mac;

	if (IS_ACX111(adev)) {
		/* ACX111: configopt struct needs to be queried after
		 * full init */
		acx_interrogate(adev, &co, ACX111_IE_CONFIG_OPTIONS);
	}

	/* TODO: merge them into one function, they are called just
	 * once and are the same for pci & usb */
	if (OK != acx_read_eeprom_byte(adev, 0x05, &adev->eeprom_version))
		goto fail_read_eeprom_byte;

	acx_parse_configoption(adev, &co);
	acx_set_defaults(adev); // TODO OW may put this after acx_display_hardware_details(adev);
	acx_get_firmware_version(adev);	/* needs to be after acx_init_mac() */
	acx_display_hardware_details(adev);

	/* Debugfs */
	if (acx_debugfs_add_adev(adev))
		goto fail_debugfs;

	pr_acx("net device %s, driver compiled "
	       "against wireless extensions %d and Linux %s\n",
	       wiphy_name(adev->hw->wiphy), WIRELESS_EXT, UTS_RELEASE);

	/** done with board specific setup **/

	/* need to be able to restore PCI state after a suspend */
#ifdef CONFIG_PM
	pci_save_state(pdev);
#endif

	err = acx_setup_modes(adev);
	if (err) {
		pr_acx("can't setup hwmode\n");
		goto fail_setup_modes;
	}

	err = ieee80211_register_hw(hw);
	if (OK != err) {
		pr_acx("ieee80211_register_hw() FAILED: %d\n", err);
		goto fail_ieee80211_register_hw;
	}
#if CMD_DISCOVERY
	great_inquisitor(adev);
#endif

	result = OK;
	goto done;

	/* error paths: undo everything in reverse order... */

	/* TODO FIXME OW 20100507
	 * Check if reverse doing is correct. e.g. if alloc failed, no dealloc is required !!
	 * => See vlynq probe
	 */

	/* err = ieee80211_register_hw(ieee); */
fail_ieee80211_register_hw:
	ieee80211_unregister_hw(hw);

	/* err = acx_setup_modes(adev) */
fail_setup_modes:

fail_debugfs:

	/* acxpci_read_eeprom_byte(adev, 0x05, &adev->eeprom_version) */
fail_read_eeprom_byte:

	/* acx_init_mac(adev) */
fail_init_mac:

	/* acx_reset_dev(adev) */
fail_reset_dev:

	/* request_irq(adev->irq, acxpci_i_interrupt, IRQF_SHARED, KBUILD_MODNAME, */
fail_request_irq:
	free_irq(adev->irq, adev);

fail_no_irq:
	
	/* pci_iomap(pdev, mem_region2, 0) */
fail_iomap2:
	pci_iounmap(pdev, mem2);

	/* pci_iomap(pdev, mem_region1, 0) */
fail_iomap1:
	pci_iounmap(pdev, mem1);

	/*	err = pci_request_region(pdev, mem_region2, "acx_2"); */
fail_request_mem_region2:
	pci_release_region(pdev, mem_region2);

	/* err = pci_request_region(pdev, mem_region1, "acx_1"); */
fail_request_mem_region1:
	pci_release_region(pdev, mem_region1);

fail_unknown_chiptype:

	/* pci_enable_device(pdev) */
fail_pci_enable_device:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);

	/* OW TODO Check if OK for PM */
#ifdef CONFIG_PM
	pci_set_power_state(pdev, PCI_D3hot);
#endif

	/* ieee80211_alloc_hw */
fail_ieee80211_alloc_hw:
	ieee80211_free_hw(hw);
	
done:

	return result;
}


/*
 * acxpci_remove
 *
 * Shut device down (if not hot unplugged)
 * and deallocate PCI resources for the acx chip.
 *
 * pdev - ptr to PCI device structure containing info about pci configuration
 */
static void __devexit acxpci_remove(struct pci_dev *pdev)
{
	struct ieee80211_hw *hw
		= (struct ieee80211_hw *) pci_get_drvdata(pdev);
	acx_device_t *adev = hw2adev(hw);
	unsigned long mem_region1, mem_region2;



	if (!hw) {
		log(L_DEBUG, "card is unused. Skipping any release code\n");
		return;
	}

	/* Unregister ieee80211 device */
	log(L_INIT, "removing device %s\n", wiphy_name(adev->hw->wiphy));
	ieee80211_unregister_hw(adev->hw);
	CLEAR_BIT(adev->dev_state_mask, ACX_STATE_IFACE_UP);

	/* If device wasn't hot unplugged... */
	if (acxpci_adev_present(adev)) {

		/* Disable both Tx and Rx to shut radio down properly */
		if (adev->initialized) {
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);
			adev->initialized = 0;
		}

#ifdef REDUNDANT
		/* put the eCPU to sleep to save power Halting is not
		 * possible currently, since not supported by all
		 * firmware versions */
		acx_issue_cmd(adev, ACX100_CMD_SLEEP, NULL, 0);
#endif
		/* disable power LED to save power :-) */
		log(L_INIT, "switching off power LED to save power\n");
		acx_power_led(adev, 0);
		/* stop our eCPU */
		if (IS_ACX111(adev)) {
			/* FIXME: does this actually keep halting the
			 * eCPU?  I don't think so...
			 */
			acxpci_reset_mac(adev);
		} else {
			u16 temp;
			/* halt eCPU */
			temp = read_reg16(adev, IO_ACX_ECPU_CTRL) | 0x1;
			write_reg16(adev, IO_ACX_ECPU_CTRL, temp);
			write_flush(adev);
		}

	}

	/* Debugfs */
	acx_debugfs_remove_adev(adev);

	/* IRQs */
	acx_irq_disable(adev);
	synchronize_irq(adev->irq);
	free_irq(adev->irq, adev);

	/* Mem regions */
	if (IS_ACX100(adev)) {
		mem_region1 = PCI_ACX100_REGION1;
		mem_region2 = PCI_ACX100_REGION2;
	} else {
		mem_region1 = PCI_ACX111_REGION1;
		mem_region2 = PCI_ACX111_REGION2;
	}

	/* finally, clean up PCI bus state */
	acx_delete_dma_regions(adev);
	if (adev->iobase)
		iounmap(adev->iobase);
	if (adev->iobase2)
		iounmap(adev->iobase2);
	release_mem_region(pci_resource_start(pdev, mem_region1),
			   pci_resource_len(pdev, mem_region1));
	release_mem_region(pci_resource_start(pdev, mem_region2),
			   pci_resource_len(pdev, mem_region2));
	pci_disable_device(pdev);

	/* remove dev registration */
	pci_set_drvdata(pdev, NULL);

	/* Free netdev (quite late, since otherwise we might get
	 * caught off-guard by a netdev timeout handler execution
	 * expecting to see a working dev...) */
	ieee80211_free_hw(adev->hw);

	/* put device into ACPI D3 mode (shutdown) */
#ifdef CONFIG_PM
	pci_set_power_state(pdev, PCI_D3hot);
#endif

}


/***********************************************************************
** TODO: PM code needs to be fixed / debugged / tested.
*/
#ifdef CONFIG_PM
static int acxpci_e_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct ieee80211_hw *hw = pci_get_drvdata(pdev);
	acx_device_t *adev;


	pr_acx("suspend handler is experimental!\n");
	pr_acx("sus: dev %p\n", hw);

/*	if (!netif_running(ndev))
		goto end;
*/
	adev = hw2adev(hw);
	pr_acx("sus: adev %p\n", adev);

	acx_sem_lock(adev);

	ieee80211_unregister_hw(hw);	/* this one cannot sleep */
	/* OW 20100603 FIXME acx_down(hw); */
	/* down() does not set it to 0xffff, but here we really want that */
	write_reg16(adev, IO_ACX_IRQ_MASK, 0xffff);
	write_reg16(adev, IO_ACX_FEMR, 0x0);
	acx_delete_dma_regions(adev);
	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3hot);

	acx_sem_unlock(adev);

	return OK;
}

static int acxpci_e_resume(struct pci_dev *pdev)
{
	struct ieee80211_hw *hw = pci_get_drvdata(pdev);
	acx_device_t *adev;



	pr_acx("resume handler is experimental!\n");
	pr_acx("rsm: got dev %p\n", hw);


	adev = hw2adev(hw);
	pr_acx("rsm: got adev %p\n", adev);

	acx_sem_lock(adev);

	pci_set_power_state(pdev, PCI_D0);
	pr_acx("rsm: power state PCI_D0 set\n");
	pci_restore_state(pdev);
	pr_acx("rsm: PCI state restored\n");

	if (OK != acx_reset_dev(adev))
		goto end_unlock;
	pr_acx("rsm: device reset done\n");
	if (OK != acx_init_mac(adev))
		goto end_unlock;
	pr_acx("rsm: init MAC done\n");

	acx_up(hw);
	pr_acx("rsm: acx up done\n");

	/* now even reload all card parameters as they were before
	 * suspend, and possibly be back in the network again already
	 * :-) */
	if (ACX_STATE_IFACE_UP & adev->dev_state_mask) {
		adev->set_mask = GETSET_ALL;
		/* acx_update_card_settings(adev); */
		pr_acx("rsm: settings updated\n");
	}
	ieee80211_register_hw(hw);
	pr_acx("rsm: device attached\n");

      end_unlock:
	acx_sem_unlock(adev);
	/* we need to return OK here anyway, right? */

	return OK;
}
#endif /* CONFIG_PM */
#endif /* CONFIG_PCI */

/*
 * Data for init_module/cleanup_module
 */

#if 0 // use later ?
static struct acxpci_device_info acxpci_info_tbl[] __devinitdata = {
        [0] = {
                .part_name      = "acx111",
                .helper_image   = "tiacx1111r16", // probly wrong !!
        },
};
#endif

#ifdef CONFIG_PCI
static DEFINE_PCI_DEVICE_TABLE(acxpci_id_tbl) = {
	{ PCI_VDEVICE(TI, PCI_DEVICE_ID_TI_TNETW1100A),
	  .driver_data = CHIPTYPE_ACX100,
	},
	{ PCI_VDEVICE(TI, PCI_DEVICE_ID_TI_TNETW1100B),
	  .driver_data = CHIPTYPE_ACX100,
	},
	{ PCI_VDEVICE(TI, PCI_DEVICE_ID_TI_TNETW1130),
	 .driver_data = CHIPTYPE_ACX111,
	},
	{ }
};
MODULE_DEVICE_TABLE(pci, acxpci_id_tbl);

static struct pci_driver acxpci_driver = {
	.name		= "acx_pci",
	.id_table	= acxpci_id_tbl,
	.probe		= acxpci_probe,
	.remove		= __devexit_p(acxpci_remove),
#ifdef CONFIG_PM
	.suspend	= acxpci_e_suspend,
	.resume		= acxpci_e_resume
#endif /* CONFIG_PM */
};
/* #else
 * #error "compiled pci.c w/o CONFIG_PCI !!"
 */
#endif /* CONFIG_PCI */


/*
 * VLYNQ support
 */
/* TODO Check section mismatch warning vlynq */

#ifdef CONFIG_VLYNQ
struct vlynq_reg_config {
	u32 offset;
	u32 value;
};

struct vlynq_known {
	u32 chip_id;
	char name[32];
	struct vlynq_mapping rx_mapping[4];
	int irq;
	int irq_type;
	int num_regs;
	struct vlynq_reg_config regs[10];
};

#define CHIP_TNETW1130 0x00000009
#define CHIP_TNETW1350 0x00000029

static struct vlynq_known vlynq_known_devices[] = {
	{
		.chip_id = CHIP_TNETW1130, .name = "TI TNETW1130",
		.rx_mapping = {
			{ .size = 0x22000, .offset = 0xf0000000 },
			{ .size = 0x40000, .offset = 0xc0000000 },
			{ .size = 0x0, .offset = 0x0 },
			{ .size = 0x0, .offset = 0x0 },
		},
		.irq = 0,
		.irq_type = IRQ_TYPE_EDGE_RISING,
		.num_regs = 5,
		.regs = {
			{
				.offset = 0x790,
				.value = (0xd0000000 - PHYS_OFFSET)
			},
			{
				.offset = 0x794,
				.value = (0xd0000000 - PHYS_OFFSET)
			},
			{ .offset = 0x740, .value = 0 },
			{ .offset = 0x744, .value = 0x00010000 },
			{ .offset = 0x764, .value = 0x00010000 },
		},
	},
	{
		.chip_id = CHIP_TNETW1350, .name = "TI TNETW1350",
		.rx_mapping = {
			{ .size = 0x100000, .offset = 0x00300000 },
			{ .size = 0x80000, .offset = 0x00000000 },
			{ .size = 0x0, .offset = 0x0 },
			{ .size = 0x0, .offset = 0x0 },
		},
		.irq = 0,
		.irq_type = IRQ_TYPE_EDGE_RISING,
		.num_regs = 5,
		.regs = {
			{
				.offset = 0x790,
				.value = (0x60000000 - PHYS_OFFSET)
			},
			{
				.offset = 0x794,
				.value = (0x60000000 - PHYS_OFFSET)
			},
			{ .offset = 0x740, .value = 0 },
			{ .offset = 0x744, .value = 0x00010000 },
			{ .offset = 0x764, .value = 0x00010000 },
		},
	},
};

static struct vlynq_device_id acx_vlynq_id[] = {
	{ CHIP_TNETW1130, vlynq_div_auto, 0 },
	/* TNETW1350 not supported by the acx driver, therefore don't claim it anymore
	 * { CHIP_TNETW1350, vlynq_div_auto, 1 },
	 */
	{ 0, 0, 0 },
};


static __devinit int vlynq_probe(struct vlynq_device *vdev,
				 struct vlynq_device_id *id)
{
	int result = -EIO, i;
	u32 addr;
	struct ieee80211_hw *hw;
	acx_device_t *adev = NULL;
	acx111_ie_configoption_t co;
	struct vlynq_mapping mapping[4] = { { 0, }, };
	struct vlynq_known *match = NULL;

	/* Initialize ieee80211_hw  */
	hw = ieee80211_alloc_hw(sizeof(struct acx_device), &acxpci_hw_ops);
	if (!hw) {
		pr_acx("could not allocate ieee80211 structure %s\n",
		       dev_name(&vdev->dev));
		goto fail_vlynq_ieee80211_alloc_hw;
	}

	adev = hw2adev(hw);
	memset(adev, 0, sizeof(*adev));

	acx_init_ieee80211(adev, hw);
	SET_IEEE80211_DEV(hw, &vdev->dev);
	hw->wiphy->interface_modes =
			BIT(NL80211_IFTYPE_STATION)	|
			BIT(NL80211_IFTYPE_ADHOC) |
			BIT(NL80211_IFTYPE_AP);

	/** Set up our private interface **/
	spin_lock_init(&adev->spinlock);	/* initial state: unlocked */
	/* We do not start with downed sem: we want PARANOID_LOCKING to work */
	mutex_init(&adev->mutex);

	adev->vdev = vdev;
	adev->bus_dev = &vdev->dev;
	adev->dev_type = DEVTYPE_PCI;
	adev->dev_is_vlynq = 1;

	/* Finished with private interface */

	/* Begin board specific inits */
	result = vlynq_enable_device(vdev);
	if (result)
		goto fail_vlynq_enable_device;

	vlynq_set_drvdata(vdev, hw);

	match = &vlynq_known_devices[id->driver_data];

	if (!match) {
		result = -ENODEV;
		goto fail_vlynq_known_devices;
	}

	mapping[0].offset = ARCH_PFN_OFFSET << PAGE_SHIFT;
	mapping[0].size = 0x02000000;
	vlynq_set_local_mapping(vdev, vdev->mem_start, mapping);
	vlynq_set_remote_mapping(vdev, 0, match->rx_mapping);

	irq_set_irq_type(vlynq_virq_to_irq(vdev, match->irq), match->irq_type);

	addr = (u32)ioremap(vdev->mem_start, 0x1000);
	if (!addr) {
		pr_err("%s: failed to remap io memory\n",
		       dev_name(&vdev->dev));
		result = -ENXIO;
		goto fail_vlynq_ioremap1;
	}

	for (i = 0; i < match->num_regs; i++)
		iowrite32(match->regs[i].value,
			  (u32 *)(addr + match->regs[i].offset));

	iounmap((void *)addr);

	if (!request_mem_region(vdev->mem_start, vdev->mem_end - vdev->mem_start, "acx")) {
		pr_acx("cannot reserve VLYNQ memory region\n");
		goto fail_vlynq_request_mem_region;
	}

	adev->iobase = ioremap(vdev->mem_start, vdev->mem_end - vdev->mem_start);
	if (!adev->iobase) {
		pr_acx("ioremap() FAILED\n");
		goto fail_vlynq_ioremap2;
	}
	adev->iobase2 = adev->iobase + match->rx_mapping[0].size;
	adev->chip_type = CHIPTYPE_ACX111;
	adev->chip_name = match->name;
	adev->io = IO_ACX111;
	adev->irq = vlynq_virq_to_irq(vdev, match->irq);

	pr_acx("found %s-based wireless network card at %s, irq:%d, "
	       "phymem:0x%x, mem:0x%p\n",
	       match->name, dev_name(&vdev->dev), adev->irq,
	       vdev->mem_start, adev->iobase);
	log(L_ANY, "the initial debug setting is 0x%04X\n", acx_debug);

	if (0 == adev->irq) {
		pr_acx("can't use IRQ 0\n");
		goto fail_vlynq_irq;
	}

	/* request shared IRQ handler */
	if (request_irq
	    (adev->irq, acx_interrupt, IRQF_SHARED, KBUILD_MODNAME, adev)) {
		pr_acx("%s: request_irq FAILED\n", wiphy_name(adev->hw->wiphy));
		result = -EAGAIN;
		goto done;
	}
	log(L_IRQ | L_INIT, "using IRQ %d\n", adev->irq);

	/* Acx irqs shall be off and are enabled later in acx_up */
	acx_irq_disable(adev);

	/* to find crashes due to weird driver access
	 * to unconfigured interface (ifup) */
	adev->mgmt_timer.function = (void (*)(unsigned long))0x0000dead;

	/* PCI setup is finished, now start initializing the card */
	/* ----- */

	acx_init_task_scheduler(adev);

	/* Mac80211 Tx_queue */
	INIT_WORK(&adev->tx_work, acx_tx_work);
	skb_queue_head_init(&adev->tx_queue);

	/* NB: read_reg() reads may return bogus data before
	 * reset_dev(), since the firmware which directly controls
	 * large parts of the I/O registers isn't initialized yet.
	 * acx100 seems to be more affected than acx111 */
	if (OK != acx_reset_dev(adev))
		goto fail_vlynq_reset_dev;

	if (OK != acx_init_mac(adev))
		goto fail_vlynq_init_mac;

	acx_interrogate(adev, &co, ACX111_IE_CONFIG_OPTIONS);
	/* TODO: merge them into one function, they are called just
	 * once and are the same for pci & usb */
	if (OK != acx_read_eeprom_byte(adev, 0x05, &adev->eeprom_version))
		goto fail_vlynq_read_eeprom_version;

	acx_parse_configoption(adev, &co);
	acx_set_defaults(adev);
	acx_get_firmware_version(adev);	/* needs to be after acx_init_mac() */
	acx_display_hardware_details(adev);

	// Debugfs entries
	if (acx_debugfs_add_adev(adev))
		goto fail_debugfs;

	/* Now we have our device, so make sure the kernel doesn't try
	 * to send packets even though we're not associated to a
	 * network yet */

	/* after register_netdev() userspace may start working with
	 * dev (in particular, on other CPUs), we only need to up the
	 * sem */
	/* acx_sem_unlock(adev); */

	pr_acx("net device %s, driver compiled "
	       "against wireless extensions %d and Linux %s\n",
	       wiphy_name(adev->hw->wiphy), WIRELESS_EXT, UTS_RELEASE);

	/** done with board specific setup **/

	result = acx_setup_modes(adev);
	if (result) {
	pr_acx("can't register hwmode\n");
		goto fail_vlynq_setup_modes;
	}

	result = ieee80211_register_hw(adev->hw);
	if (OK != result) {
		pr_acx("ieee80211_register_hw() FAILED: %d\n", result);
		goto fail_vlynq_ieee80211_register_hw;
	}
#if CMD_DISCOVERY
	great_inquisitor(adev);
#endif

	result = OK;
	goto done;

	/* error paths: undo everything in reverse order... */

	fail_vlynq_ieee80211_register_hw:

	fail_vlynq_setup_modes:

	fail_debugfs:

    fail_vlynq_read_eeprom_version:

	fail_vlynq_init_mac:

    fail_vlynq_reset_dev:

	fail_vlynq_irq:
      iounmap(adev->iobase);

    fail_vlynq_ioremap2:
		release_mem_region(vdev->mem_start, vdev->mem_end - vdev->mem_start);

    fail_vlynq_request_mem_region:

    fail_vlynq_ioremap1:

    fail_vlynq_known_devices:
		vlynq_disable_device(vdev);
		vlynq_set_drvdata(vdev, NULL);

    fail_vlynq_enable_device:
		ieee80211_free_hw(hw);

    fail_vlynq_ieee80211_alloc_hw:

	done:


	return result;
}

static void vlynq_remove(struct vlynq_device *vdev)
{
	struct ieee80211_hw *hw = vlynq_get_drvdata(vdev);
	acx_device_t *adev = hw2adev(hw);


	if (!hw) {
		log(L_DEBUG, "card is unused. Skipping any release code\n");
		return;
	}

	/* Unregister ieee80211 device */
	log(L_INIT, "removing device %s\n", wiphy_name(adev->hw->wiphy));
	ieee80211_unregister_hw(adev->hw);
	CLEAR_BIT(adev->dev_state_mask, ACX_STATE_IFACE_UP);

	/* If device wasn't hot unplugged... */
	if (acxpci_adev_present(adev)) {

		/* disable both Tx and Rx to shut radio down properly */
		if (adev->initialized) {
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
			acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);
			adev->initialized = 0;
		}
		/* disable power LED to save power :-) */
		log(L_INIT, "switching off power LED to save power\n");
		acx_power_led(adev, 0);

		/* stop our eCPU */
		/* OW PCI still does something here (although also need to be reviewed). */
	}

	/* Debugfs entries */
	acx_debugfs_remove_adev(adev);

	/* IRQs */
	acx_irq_disable(adev);
	synchronize_irq(adev->irq);
	free_irq(adev->irq, adev);

	/* finally, clean up PCI bus state */
	acx_delete_dma_regions(adev);
	if (adev->iobase)
		iounmap(adev->iobase);
	if (adev->iobase2)
		iounmap(adev->iobase2);
	release_mem_region(vdev->mem_start, vdev->mem_end - vdev->mem_start);

	vlynq_disable_device(vdev);

	/* remove dev registration */
	vlynq_set_drvdata(vdev, NULL);

	/* Free netdev (quite late, since otherwise we might get
	 * caught off-guard by a netdev timeout handler execution
	 * expecting to see a working dev...) */
	ieee80211_free_hw(adev->hw);

}

static struct vlynq_driver vlynq_acx = {
	.name = "acx_vlynq",
	.id_table = acx_vlynq_id,
	.probe = vlynq_probe,
	.remove = __devexit_p(vlynq_remove),
};
#endif /* CONFIG_VLYNQ */


/*
 * acxpci_e_init_module
 *
 * Module initialization routine, called once at module load time
 */
int __init acxpci_init_module(void)
{
	int res;

	pr_info("built with CONFIG_ACX_MAC80211_PCI\n");
	pr_acx(IO_COMPILE_NOTE);
	log(L_INIT, ENDIAN_STR
	    " PCI/VLYNQ module initialized, waiting for cards to probe...\n");

#if defined(CONFIG_PCI)
	res = pci_register_driver(&acxpci_driver);
#elif defined(CONFIG_VLYNQ)
	res = vlynq_register_driver(&vlynq_acx);
#endif

	if (res)
		pr_err("can't register pci/vlynq driver\n");

	return res;
}


/*
 * acxpci_e_cleanup_module
 *
 * Called at module unload time. This is our last chance to
 * clean up after ourselves.
 */
void __exit acxpci_cleanup_module(void)
{


#if defined(CONFIG_PCI)
	pci_unregister_driver(&acxpci_driver);
#elif defined(CONFIG_VLYNQ)
	vlynq_unregister_driver(&vlynq_acx);
#endif
	log(L_INIT,
	    "acxpci: PCI module unloaded\n");

}
