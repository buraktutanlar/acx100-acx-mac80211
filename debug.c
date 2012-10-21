/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2012
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

#if defined CONFIG_DEBUG_FS && !defined ACX_NO_DEBUG_FILES

#define pr_fmt(fmt) "acx.%s: " fmt, __func__

#include <linux/version.h>
#include <linux/wireless.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <net/mac80211.h>

#include "acx.h"
#include "pci.h"
#include "mem.h"
#include "merge.h"
#include "usb.h"
#include "utils.h"
#include "cmd.h"
#include "cardsetting.h"
#include "main.h"
#include "boot.h"
#include "debug.h"

enum file_index {
	INFO, DIAG, EEPROM, PHY, DEBUG,
	SENSITIVITY, TX_LEVEL, ANTENNA, REG_DOMAIN,
};
static const char *const dbgfs_files[] = {
	[INFO]		= "info",
	[DIAG]		= "diag",
	[EEPROM]	= "eeprom",
	[PHY]		= "phy",
	[DEBUG]		= "debug",
	[SENSITIVITY]	= "sensitivity",
	[TX_LEVEL]	= "tx_level",
	[ANTENNA]	= "antenna",
	[REG_DOMAIN]	= "reg_domain",
};
BUILD_BUG_DECL(dbgfs_files__VS__enum_REG_DOMAIN,
	ARRAY_SIZE(dbgfs_files) != REG_DOMAIN + 1);

static struct dentry *acx_dbgfs_dir;

static int acx_dbgfs_show_diag(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	ssize_t len = 0, partlen;
	u32 temp1, temp2;
	u8 *st, *st_end;
#ifdef __BIG_ENDIAN
	u8 *st2;
#endif
	fw_stats_t *fw_stats;
	char *part_str = NULL;
	fw_stats_tx_t *tx = NULL;
	fw_stats_rx_t *rx = NULL;
	fw_stats_dma_t *dma = NULL;
	fw_stats_irq_t *irq = NULL;
	fw_stats_wep_t *wep = NULL;
	fw_stats_pwr_t *pwr = NULL;
	fw_stats_mic_t *mic = NULL;
	fw_stats_aes_t *aes = NULL;
	fw_stats_event_t *evt = NULL;


	acx_sem_lock(adev);

	if (IS_PCI(adev))
		acxpci_dbgfs_diag_output(file, adev);
	else if (IS_MEM(adev))
		acxmem_dbgfs_diag_output(file, adev);

	seq_printf(file,
		     "\n"
		     "** network status **\n"
		     "dev_state_mask 0x%04X\n"
		     "mode %u, channel %u, "
		     "reg_dom_id 0x%02X, reg_dom_chanmask 0x%04X, ",
		     adev->dev_state_mask,
		     adev->mode, adev->channel,
		     adev->reg_dom_id, adev->reg_dom_chanmask);

	seq_printf(file,
		     "ESSID \"%s\", essid_active %d, essid_len %d, "
		     "essid_for_assoc \"%s\", nick \"%s\"\n"
		     "WEP ena %d, restricted %d, idx %d\n",
		     adev->essid, adev->essid_active, (int)adev->essid_len,
		     adev->essid_for_assoc, adev->nick,
		     adev->wep_enabled, adev->wep_restricted,
		     adev->wep_current_index);
	seq_printf(file, "dev_addr  " MACSTR "\n", MAC(adev->dev_addr));
	seq_printf(file, "bssid     " MACSTR "\n", MAC(adev->bssid));

	seq_printf(file, "tx_queue len: %d\n", skb_queue_len(&adev->tx_queue));

	seq_printf(file, "\n" "** PHY status **\n"
		"tx_enabled %d, tx_level_dbm %d, tx_level_val %d,\n "
		/* "tx_level_auto %d\n" */
		"sensitivity %d, antenna[0,1] 0x%02X 0x%02X, ed_threshold %d, cca %d, preamble_mode %d\n"
		"rate_basic 0x%04X, rate_oper 0x%04X\n"
		"rts_threshold %d, frag_threshold %d, short_retry %d, long_retry %d\n"
		"msdu_lifetime %d, listen_interval %d, beacon_interval %d\n",
		adev->tx_enabled, adev->tx_level_dbm, adev->tx_level_val,
		/* adev->tx_level_auto, */
		adev->sensitivity, adev->antenna[0], adev->antenna[1],
		adev->ed_threshold,
		adev->cca, adev->preamble_mode, adev->rate_basic,
		adev->rate_oper, adev->rts_threshold,
		adev->frag_threshold, adev->short_retry, adev->long_retry,
		adev->msdu_lifetime, adev->listen_interval,
		adev->beacon_interval);

	seq_printf(file,
		"\n"
		"** Firmware **\n"
		"NOTE: version dependent statistics layout, "
		"please report if you suspect wrong parsing!\n"
		"\n" "version \"%s\"\n", adev->firmware_version);

	fw_stats = kzalloc(sizeof(*fw_stats), GFP_KERNEL);
	if (!fw_stats) {

		return 0;
	}
	st = (u8 *) fw_stats;

	part_str = "statistics query command";

	if (OK != acx_interrogate(adev, st, ACX1xx_IE_FIRMWARE_STATISTICS))
		goto fw_stats_end;

	st += sizeof(u16);
	len = *(u16 *) st;

	if (len > sizeof(*fw_stats)) {
		seq_printf(file,
			"firmware version with bigger fw_stats struct detected\n"
			"(%zu vs. %zu), please report\n", len,
			sizeof(fw_stats_t));
		if (len > sizeof(*fw_stats)) {
			seq_printf(file, "struct size exceeded allocation!\n");
			len = sizeof(*fw_stats);
		}
	}
	st += sizeof(u16);
	st_end = st - 2 * sizeof(u16) + len;

#ifdef __BIG_ENDIAN
	/* let's make one bold assumption here:
	 * (hopefully!) *all* statistics fields are u32 only,
	 * thus if we need to make endianness corrections
	 * we can simply do them in one go, in advance */
	st2 = (u8 *) fw_stats;
	for (temp1 = 0; temp1 < len; temp1 += 4, st2 += 4)
		*(u32 *) st2 = le32_to_cpu(*(u32 *) st2);
#endif

	part_str = "Rx/Tx";

	/* directly at end of a struct part? --> no error! */
	if (st == st_end)
		goto fw_stats_end;

	tx = (fw_stats_tx_t *) st;
	st += sizeof(fw_stats_tx_t);
	rx = (fw_stats_rx_t *) st;
	st += sizeof(fw_stats_rx_t);
	partlen = sizeof(fw_stats_tx_t) + sizeof(fw_stats_rx_t);

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);

		/* our parsing doesn't quite match this firmware yet,
		 * log failure */
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = rx->rx_aci_events;
		temp2 = rx->rx_aci_resets;
	}

	seq_printf(file,
		"%s:\n"
		"  tx_desc_overfl %u\n"
		"  rx_OutOfMem %u, rx_hdr_overfl %u, rx_hw_stuck %u\n"
		"  rx_dropped_frame %u, rx_frame_ptr_err %u, rx_xfr_hint_trig %u\n"
		"  rx_aci_events %u, rx_aci_resets %u\n",
		part_str,
		tx->tx_desc_of,
		rx->rx_oom,
		rx->rx_hdr_of,
		rx->rx_hw_stuck,
		rx->rx_dropped_frame,
		rx->rx_frame_ptr_err, rx->rx_xfr_hint_trig, temp1, temp2);

	part_str = "DMA";

	if (st == st_end)
		goto fw_stats_end;

	dma = (fw_stats_dma_t *) st;
	partlen = sizeof(fw_stats_dma_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  rx_dma_req %u, rx_dma_err %u, tx_dma_req %u, tx_dma_err %u\n",
		part_str,
		dma->rx_dma_req,
		dma->rx_dma_err, dma->tx_dma_req, dma->tx_dma_err);

	part_str = "IRQ";

	if (st == st_end)
		goto fw_stats_end;

	irq = (fw_stats_irq_t *) st;
	partlen = sizeof(fw_stats_irq_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  cmd_cplt %u, fiq %u\n"
		"  rx_hdrs %u, rx_cmplt %u, rx_mem_overfl %u, rx_rdys %u\n"
		"  irqs %u, tx_procs %u, decrypt_done %u\n"
		"  dma_0_done %u, dma_1_done %u, tx_exch_complet %u\n"
		"  commands %u, rx_procs %u, hw_pm_mode_changes %u\n"
		"  host_acks %u, pci_pm %u, acm_wakeups %u\n",
		part_str,
		irq->cmd_cplt,
		irq->fiq,
		irq->rx_hdrs,
		irq->rx_cmplt,
		irq->rx_mem_of,
		irq->rx_rdys,
		irq->irqs,
		irq->tx_procs,
		irq->decrypt_done,
		irq->dma_0_done,
		irq->dma_1_done,
		irq->tx_exch_complet,
		irq->commands,
		irq->rx_procs,
		irq->hw_pm_mode_changes,
		irq->host_acks, irq->pci_pm, irq->acm_wakeups);

	part_str = "WEP";

	if (st == st_end)
		goto fw_stats_end;

	wep = (fw_stats_wep_t *) st;
	partlen = sizeof(fw_stats_wep_t);
	st += partlen;

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = wep->wep_pkt_decrypt;
		temp2 = wep->wep_decrypt_irqs;
	}

	seq_printf(file,
		"%s:\n"
		"  wep_key_count %u, wep_default_key_count %u, dot11_def_key_mib %u\n"
		"  wep_key_not_found %u, wep_decrypt_fail %u\n"
		"  wep_pkt_decrypt %u, wep_decrypt_irqs %u\n",
		part_str,
		wep->wep_key_count,
		wep->wep_default_key_count,
		wep->dot11_def_key_mib,
		wep->wep_key_not_found,
		wep->wep_decrypt_fail, temp1, temp2);

	part_str = "power";

	if (st == st_end)
		goto fw_stats_end;

	pwr = (fw_stats_pwr_t *) st;
	partlen = sizeof(fw_stats_pwr_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  tx_start_ctr %u, no_ps_tx_too_short %u\n"
		"  rx_start_ctr %u, no_ps_rx_too_short %u\n"
		"  lppd_started %u\n"
		"  no_lppd_too_noisy %u, no_lppd_too_short %u, no_lppd_matching_frame %u\n",
		part_str,
		pwr->tx_start_ctr,
		pwr->no_ps_tx_too_short,
		pwr->rx_start_ctr,
		pwr->no_ps_rx_too_short,
		pwr->lppd_started,
		pwr->no_lppd_too_noisy,
		pwr->no_lppd_too_short, pwr->no_lppd_matching_frame);

	part_str = "MIC";

	if (st == st_end)
		goto fw_stats_end;

	mic = (fw_stats_mic_t *) st;
	partlen = sizeof(fw_stats_mic_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  mic_rx_pkts %u, mic_calc_fail %u\n",
		part_str, mic->mic_rx_pkts, mic->mic_calc_fail);

	part_str = "AES";

	if (st == st_end)
		goto fw_stats_end;

	aes = (fw_stats_aes_t *) st;
	partlen = sizeof(fw_stats_aes_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  aes_enc_fail %u, aes_dec_fail %u\n"
		"  aes_enc_pkts %u, aes_dec_pkts %u\n"
		"  aes_enc_irq %u, aes_dec_irq %u\n",
		part_str,
		aes->aes_enc_fail,
		aes->aes_dec_fail,
		aes->aes_enc_pkts,
		aes->aes_dec_pkts, aes->aes_enc_irq, aes->aes_dec_irq);

	part_str = "event";

	if (st == st_end)
		goto fw_stats_end;

	evt = (fw_stats_event_t *) st;
	partlen = sizeof(fw_stats_event_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  heartbeat %u, calibration %u\n"
		"  rx_mismatch %u, rx_mem_empty %u, rx_pool %u\n"
		"  oom_late %u\n"
		"  phy_tx_err %u, tx_stuck %u\n",
		part_str,
		evt->heartbeat,
		evt->calibration,
		evt->rx_mismatch,
		evt->rx_mem_empty,
		evt->rx_pool,
		evt->oom_late, evt->phy_tx_err, evt->tx_stuck);

	if (st < st_end)
		goto fw_stats_bigger;

	goto fw_stats_end;

	fw_stats_fail:
	st -= partlen;
	seq_printf(file,
		"failed at %s part (size %zu), offset %zu (struct size %zu), "
		"please report\n", part_str, partlen,
		((void *) st - (void *) fw_stats), len);

	fw_stats_bigger:
	for (; st < st_end; st += 4)
		seq_printf(file, "UNKN%3d: %u\n", (int) ((void *) st
				- (void *) fw_stats), *(u32 *) st);

	fw_stats_end:
	kfree(fw_stats);

	acx_sem_unlock(adev);

	return 0;
}

/* Writing to acx_diag triggers different actions for debugging */
static ssize_t acx_dbgfs_write_diag(acx_device_t *adev, struct file *file,
                                   const char __user *ubuf, size_t count, loff_t *ppos)
{

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned int val;
	size_t size, len;


	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = (unsigned int) simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count == size)
		ret = count;
	else
		goto exit_unlock;

	logf1(L_ANY, "acx_diag: 0x%04x\n", val);

	/* Execute operation */
	if (val == ACX_DIAG_OP_RECALIB) {
		logf0(L_ANY, "ACX_DIAG_OP_RECALIB: Scheduling immediate radio recalib\n");
		adev->recalib_time_last_success = jiffies - RECALIB_PAUSE * 60 * HZ;
		acx_schedule_task(adev, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_PROCESS_TX_RX) {
		logf0(L_ANY, "ACX_DIAG_OP_PROCESS_TX_RX: Scheduling immediate Rx, Tx processing\n");

		if (IS_PCI(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_COMPLETE);
		else if (IS_MEM(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_DATA);

		SET_BIT(adev->irq_reason, HOST_INT_TX_COMPLETE);
		acx_schedule_task(adev, 0);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_REINIT_TX_BUF) {
		if (IS_MEM(adev)) {
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF\n");
			acxmem_init_acx_txbuf2(adev);
		} else
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF: Only valid for mem device\n");
	}
	/* Unknown */
	else
		logf1(L_ANY, "Unknown command: 0x%04x\n", val);

exit_unlock:
	acx_sem_unlock(adev);

	return ret;
}

static int acx_dbgfs_show_acx(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t*) file->private;


	acx_sem_lock(adev);

	seq_printf(file,
		"acx driver version:\t\t%s (git: %s)\n"
		"Wireless extension version:\t%s\n"
		"chip name:\t\t\t%s (0x%08X)\n"
		"radio type:\t\t\t0x%02X\n"
		"form factor:\t\t\t0x%02X\n"
		     "EEPROM version:\t\t\t0x%02X\n"
		"firmware version:\t\t%s (0x%08X)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown",
		STRING(WIRELESS_EXT),
		adev->chip_name, adev->firmware_id,
		adev->radio_type,
		adev->form_factor,
		adev->eeprom_version,
		adev->firmware_version, adev->firmware_numver);

	acx_sem_unlock(adev);

	return 0;
}

static int acx_dbgfs_show_eeprom(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int length;
	char *buf, *p;


	acx_sem_lock(adev);

	if (IS_PCI(adev) || IS_MEM(adev))
		buf = acx_proc_eeprom_output(&length, adev);
	else
		goto out;

	for (p = buf; p < buf + length; p++)
	     seq_putc(file, *p);

	kfree(buf);
out:
	acx_sem_unlock(adev);

	return 0;
}

static int acx_dbgfs_show_phy(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int i;
	char *buf, *p;
	/* OW Hopefully enough */
	const int buf_size = 1024*64;


	acx_sem_lock(adev);

	buf = kmalloc(buf_size, GFP_KERNEL);
	/*
	   if (RADIO_11_RFMD != adev->radio_type) {
	   pr_info("sorry, not yet adapted for radio types "
	   "other than RFMD, please verify "
	   "PHY size etc. first!\n");
	   goto end;
	   }
	 */

	/* The PHY area is only 0x80 bytes long; further pages after that
	 * only have some page number registers with altered value,
	 * all other registers remain the same. */
	p = buf;
	for (i = 0; i < 0x80; i++) {
		acx_read_phy_reg(adev, i, p++);
		seq_putc(file, *p);
	}

	kfree(buf);

	acx_sem_unlock(adev);

	return 0;
}

static int acx_dbgfs_show_debug(struct seq_file *file, void *v)
{
	/* No sem locking required, since debug is global for all devices */
	seq_printf(file, "acx_debug: 0x%04x\n", acx_debug);
	return 0;
}

static ssize_t acx_dbgfs_write_debug(acx_device_t *adev, struct file *file,
                                    const char __user *ubuf, size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	/* No sem locking required, since debug is global for all devices */

	if (count == size) {
		ret = count;
		acx_debug = val;
	}

	log(L_ANY, "acx_debug=0x%04x\n", acx_debug);

	return ret;
}

static int acx_dbgfs_show_sensitivity(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	acx_sem_lock(adev);

	acx_get_sensitivity(adev);
	seq_printf(file, "acx_sensitivity: %d\n", adev->sensitivity);

	acx_sem_unlock(adev);

	return 0;
}

static ssize_t acx_dbgfs_write_sensitivity(acx_device_t *adev, struct file *file,
                                          const char __user *ubuf, size_t count, loff_t *ppos)

{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;


	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_sensitivity(adev, val);
	logf1(L_ANY, "acx_sensitivity=%d\n", adev->sensitivity);

out:
	acx_sem_unlock(adev);

	return ret;
}

static int acx_dbgfs_show_tx_level(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	acx_sem_lock(adev);

	acx1xx_get_tx_level(adev);
	seq_printf(file, "tx_level_dbm: %d\n", adev->tx_level_dbm);

	acx_sem_unlock(adev);

	return 0;
}

static ssize_t acx_dbgfs_write_tx_level(acx_device_t *adev, struct file *file,
                                          const char __user *ubuf, size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;


	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	logf1(L_ANY, "tx_level_val=%d\n", adev->tx_level_val);
	acx1xx_set_tx_level(adev, val);

out:
	acx_sem_unlock(adev);

	return ret;
}

static int acx_dbgfs_show_reg_domain(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	acx_sem_lock(adev);

	acx_get_reg_domain(adev);
	seq_printf(file, "reg_dom_id: 0x%02x\n", adev->reg_dom_id);

	acx_sem_unlock(adev);

	return 0;
}

static ssize_t acx_dbgfs_write_reg_domain(acx_device_t *adev, struct file *file,
                                         const char __user *ubuf, size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;


	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_reg_domain(adev, val);

out:
	acx_sem_unlock(adev);

	return ret;
}

static int acx_dbgfs_show_antenna(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;


	acx_sem_lock(adev);

	acx1xx_get_antenna(adev);
	seq_printf(file, "antenna[0,1]: 0x%02x 0x%02x\n",
		adev->antenna[0], adev->antenna[1]);

	acx_sem_unlock(adev);

	return 0;
}

static ssize_t acx_dbgfs_write_antenna(acx_device_t *adev, struct file *file,
                                      const char __user *ubuf, size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	u8 val0, val1;
	size_t size, len;


	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	val0 = (u8) (val & 0xFF);
	val1 = (u8) ((val >> 8) & 0xFF);
	acx1xx_set_antenna(adev, val0, val1);

out:
	acx_sem_unlock(adev);

	return ret;
}

acx_dbgfs_show_t *const acx_dbgfs_show_funcs[] = {
	acx_dbgfs_show_acx,
	acx_dbgfs_show_diag,
	acx_dbgfs_show_eeprom,
	acx_dbgfs_show_phy,
	acx_dbgfs_show_debug,
	acx_dbgfs_show_sensitivity,
	acx_dbgfs_show_tx_level,
	acx_dbgfs_show_antenna,
	acx_dbgfs_show_reg_domain,
};

acx_dbgfs_write_t *const acx_dbgfs_write_funcs[] = {
	NULL,
	acx_dbgfs_write_diag,
	NULL,
	NULL,
	acx_dbgfs_write_debug,
	acx_dbgfs_write_sensitivity,
	acx_dbgfs_write_tx_level,
	acx_dbgfs_write_antenna,
	acx_dbgfs_write_reg_domain,
};
BUILD_BUG_DECL(acx_proc_show_funcs__VS__acx_proc_write_funcs,
	ARRAY_SIZE(acx_dbgfs_show_funcs) != ARRAY_SIZE(acx_dbgfs_write_funcs));

static int acx_dbgfs_open(struct inode *inode, struct file *file)
{
	size_t fidx = (size_t) inode->i_private;
	struct acx_device *adev = (struct acx_device *)
		file->f_path.dentry->d_parent->d_inode->i_private;

	switch (fidx) {
	case INFO:
	case DIAG:
	case EEPROM:
	case PHY:
	case DEBUG:
	case SENSITIVITY:
	case TX_LEVEL:
	case ANTENNA:
	case REG_DOMAIN:
		pr_devel("opening filename=%s fmode=%o fidx=%d adev=%p\n",
			dbgfs_files[fidx], file->f_mode, (int)fidx, adev);
		break;
	default:
		pr_err("unknown file @ %d: %s\n", (int)fidx,
			file->f_path.dentry->d_name.name);
		return -ENOENT;
	}
	return single_open(file, acx_dbgfs_show_funcs[fidx], adev);
}

static ssize_t acx_dbgfs_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	/* retrieve file-index and adev from private fields */
	size_t fidx = (size_t) file->f_path.dentry->d_inode->i_private;
	struct acx_device *adev = (struct acx_device *)
		file->f_path.dentry->d_parent->d_inode->i_private;

	switch (fidx) {
	case INFO:
	case DIAG:
	case EEPROM:
	case PHY:
	case DEBUG:
	case SENSITIVITY:
	case TX_LEVEL:
	case ANTENNA:
	case REG_DOMAIN:
		pr_devel("opening filename=%s fmode=%o fidx=%d adev=%p\n",
			dbgfs_files[fidx], file->f_mode, (int)fidx, adev);
		break;
	default:
		pr_err("unknown file @ %d: %s\n", (int)fidx,
			file->f_path.dentry->d_name.name);
		return -ENOENT;
	}

	return (acx_dbgfs_write_funcs[fidx])(adev, file, buf, count, ppos);
}

static const struct file_operations acx_fops = {
	.read		= seq_read,
	.write		= acx_dbgfs_write,
	.open		= acx_dbgfs_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
	.llseek		= noop_llseek,
#endif
};

int acx_debugfs_add_adev(struct acx_device *adev)
{
	size_t i;
	int fmode;
	struct dentry *file;
	const char *devname = wiphy_name(adev->hw->wiphy);
	struct dentry *acx_dbgfs_devdir
		= debugfs_create_dir(devname, acx_dbgfs_dir);

	if (!acx_dbgfs_devdir) {
		pr_err("debugfs_create_dir() failed\n");
		return -ENOMEM;
	}
	pr_info("adev:%p nm:%s dirp:%p\n", adev, devname,
		acx_dbgfs_devdir);

	if (acx_dbgfs_devdir->d_inode->i_private) {
		/* this shouldnt happen */
		pr_err("dentry->d_inode->i_private already set: %p\n",
			acx_dbgfs_devdir->d_inode->i_private);
		goto fail;
	}
	/* save adev in dir's private field */
	acx_dbgfs_devdir->d_inode->i_private = (void*) adev;

	for (i = 0; i < ARRAY_SIZE(dbgfs_files); i++) {

		fmode = (acx_dbgfs_write_funcs[i])
			? 0644 : 0444;
		/* save file-index in in file's private field */
		file = debugfs_create_file(dbgfs_files[i], fmode,
					acx_dbgfs_devdir,
					(void*) i, &acx_fops);
		if (!file)
			goto fail;
	}
	adev->debugfs_dir = acx_dbgfs_devdir;
	return 0;
fail:
	debugfs_remove_recursive(acx_dbgfs_devdir);
	return -ENOENT;
}

void acx_debugfs_remove_adev(struct acx_device *adev)
{
	debugfs_remove_recursive(adev->debugfs_dir);
	pr_info("%s %p\n", wiphy_name(adev->hw->wiphy),
		adev->debugfs_dir);
	adev->debugfs_dir = NULL;
}


int __init acx_debugfs_init(void)
{
	acx_dbgfs_dir = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!acx_dbgfs_dir)
		return -ENOMEM;

	return 0;
}

void __exit acx_debugfs_exit(void)
{
	debugfs_remove_recursive(acx_dbgfs_dir);
}

#endif /* CONFIG_DEBUG_FS && ! ACX_NO_DEBUG_FILES */
