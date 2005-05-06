/* src/acx100_helper.c - helper functions
 *
 * Helper functions used for card hardware initialization etc.
 * (firmware, template configuration, ...), /proc management, ...
 *
 * Functions mostly sorted according to driver initialization order
 *
 * --------------------------------------------------------------------
 *
 * Copyright (C) 2003  ACX100 Open Source Project
 *
 *   The contents of this file are subject to the Mozilla Public
 *   License Version 1.1 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.mozilla.org/MPL/
 *
 *   Software distributed under the License is distributed on an "AS
 *   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *   implied. See the License for the specific language governing
 *   rights and limitations under the License.
 *
 *   Alternatively, the contents of this file may be used under the
 *   terms of the GNU Public License version 2 (the "GPL"), in which
 *   case the provisions of the GPL are applicable instead of the
 *   above.  If you wish to allow the use of your version of this file
 *   only under the terms of the GPL and not to allow others to use
 *   your version of this file under the MPL, indicate your decision
 *   by deleting the provisions above and replace them with the notice
 *   and other provisions required by the GPL.  If you do not delete
 *   the provisions above, a recipient may use your version of this
 *   file under either the MPL or the GPL.
 *
 * --------------------------------------------------------------------
 *
 * Inquiries regarding the ACX100 Open Source Project can be
 * made directly to:
 *
 * acx100-users@lists.sf.net
 * http://acx100.sf.net
 *
 * --------------------------------------------------------------------
 */

/*================================================================*/
/* System Includes */

#ifdef S_SPLINT_S /* some crap that splint needs to not crap out */
#define __signed__ signed
#define __u64 unsigned long long
#define loff_t unsigned long
#define sigval_t unsigned long
#define siginfo_t unsigned long
#define stack_t unsigned long
#define __s64 signed long long
#endif
#include <linux/config.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif
#include <asm/uaccess.h>

#include <linux/pm.h>

#include <linux/highmem.h>

/*================================================================*/
/* Project Includes */

#include <acx.h>

#define RECALIB_PAUSE	5 /* minutes to wait until next radio recalibration */

#if BOGUS
u8 DTIM_count;
#endif

const u8 reg_domain_ids[] =
		{0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51};
/* stupid workaround for the fact that in C the size of an external array
 * cannot be determined from within a second file */
const u8 reg_domain_ids_len = sizeof(reg_domain_ids);
const u16 reg_domain_channel_masks[] =
	{0x07ff, 0x07ff, 0x1fff, 0x0600, 0x1e00, 0x2000, 0x3fff, 0x01fc};

/*----------------------------------------------------------------
    Debugging support
*----------------------------------------------------------------*/
#if ACX_DEBUG

static int acx_debug_func_indent = 0;
#define DEBUG_TSC 0
#define FUNC_INDENT_INCREMENT 2

#if DEBUG_TSC
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

static const char spaces[] = "          " "          "; /* Nx10 spaces */

void log_fn_enter(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces)-1;

	printk("%lx %s==> %s\n",
		d,
		spaces + (sizeof(spaces)-1) - indent,
		funcname
	);

	acx_debug_func_indent += FUNC_INDENT_INCREMENT;
}

void log_fn_exit(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	acx_debug_func_indent -= FUNC_INDENT_INCREMENT;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces)-1;
		
	printk("%lx %s<== %s\n",
		d,
		spaces + (sizeof(spaces)-1) - indent,
		funcname
	);
}

void log_fn_exit_v(const char *funcname, int v)
{
	int indent;
	TIMESTAMP(d);
 
	acx_debug_func_indent -= FUNC_INDENT_INCREMENT;
 
	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces)-1;
 
	printk("%lx %s<== %s: %08x\n",
		d,
		spaces + (sizeof(spaces)-1) - indent,
		funcname,
		v
	);
}
#endif /* ACX_DEBUG */


/* acx_schedule()
 * Make sure to schedule away sometimes, in order to not hog the CPU too much.
 * Remember to not do it in IRQ context, though!
 */
void acx_schedule(long timeout)
{
	FN_ENTER;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(timeout);
	FN_EXIT0();
}

/*----------------------------------------------------------------
Helper: updates short preamble, basic and oper rates, etc,
(removing those unsupported by the peer)
*----------------------------------------------------------------*/
const u8
bitpos2ratebyte[] = {
	DOT11RATEBYTE_1,
	DOT11RATEBYTE_2,
	DOT11RATEBYTE_5_5,
	DOT11RATEBYTE_6_G,
	DOT11RATEBYTE_9_G,
	DOT11RATEBYTE_11,
	DOT11RATEBYTE_12_G,
	DOT11RATEBYTE_18_G,
	DOT11RATEBYTE_22,
	DOT11RATEBYTE_24_G,
	DOT11RATEBYTE_36_G,
	DOT11RATEBYTE_48_G,
	DOT11RATEBYTE_54_G,
};

/*----------------------------------------------------------------
Helper: updates priv->rate_supported[_len] according to rate_{basic,oper}
*----------------------------------------------------------------*/
void
acx_update_dot11_ratevector(wlandevice_t *priv)
{
	u16 bcfg = priv->rate_basic;
	u16 ocfg = priv->rate_oper;
	u8 *supp = priv->rate_supported;
	const u8 *dot11 = bitpos2ratebyte;

	FN_ENTER;

	while (ocfg) {
		if (ocfg & 1) {
			*supp = *dot11;
			if (bcfg & 1) {
				*supp |= 0x80;
			}
			supp++;
		}
		dot11++;
		ocfg>>=1;
		bcfg>>=1;
	}
	priv->rate_supported_len = supp - priv->rate_supported;
#if ACX_DEBUG
	if (debug & L_ASSOC) {
		int i = priv->rate_supported_len;
		printk(KERN_WARNING "new ratevector:");
		supp = priv->rate_supported;
		while (i--)
			printk(" %02x", *supp++);
		printk("\n");
	}
#endif
	FN_EXIT0();
}

#ifdef CONFIG_PROC_FS
/*------------------------------------------------------------------------------
 * acx_proc_output
 * Generate content for our /proc entry
 *
 * Arguments:
 *	buf is a pointer to write output to
 *	priv is the usual pointer to our private struct wlandevice
 * Returns:
 *	number of bytes actually written to buf
 * Side effects:
 *	none
 * Call context:
 *	
 * Status:
 *	should be okay, non-critical
 * Comment:
 *
 *----------------------------------------------------------------------------*/
static int acx_proc_output(char *buf, wlandevice_t *priv)
{
	char *p = buf;
	u16 i;

	FN_ENTER;
	p += sprintf(p,
		"acx100 driver version:\t\t%s\n"
		"Wireless extension version:\t%d\n"
		"chip name:\t\t\t%s (0x%08x)\n"
		"radio type:\t\t\t0x%02x\n" /* TODO: add radio type string from acx_display_hardware_details */
		"form factor:\t\t\t0x%02x\n" /* TODO: add form factor string from acx_display_hardware_details */
		"EEPROM version:\t\t\t0x%02x\n"
		"firmware version:\t\t%s (0x%08x)\n",
		WLAN_RELEASE_SUB,
		WIRELESS_EXT,
		priv->chip_name, priv->firmware_id,
		priv->radio_type,
		priv->form_factor,
		priv->eeprom_version,
		priv->firmware_version, priv->firmware_numver);

	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		struct client *bss = &priv->sta_list[i];
		if (!bss->used) continue;
		p += sprintf(p, "BSS %u BSSID "MACSTR" ESSID %s channel %u "
			"Cap 0x%x SIR %u SNR %u\n",
			i, MAC(bss->bssid), (char*)bss->essid, bss->channel,
			bss->cap_info, bss->sir, bss->snr);
	}
	p += sprintf(p, "status:\t\t\t%u (%s)\n",
			priv->status, acx_get_status_name(priv->status));
	/* TODO: add more interesting stuff (essid, ...) here */
	FN_EXIT1(p - buf);
	return p - buf;
}

static int acx_proc_diag_output(char *buf, wlandevice_t *priv)
{
	char *p = buf;
	unsigned int i;
	TIWLAN_DC *pDc = &priv->dc;
	const struct rxhostdescriptor *pRxDesc;
	const txdesc_t *pTxDesc;
	fw_stats_t *fw_stats;
	const char *rtl, *thd, *ttl;
	unsigned long flags;

	FN_ENTER;

	p += sprintf(p, "*** Rx buf ***\n");
	spin_lock_irqsave(&pDc->rx_lock, flags);
	for (i = 0; i < pDc->rx_pool_count; i++) {
		rtl = (i == pDc->rx_tail) ? " [tail]" : "";
		pRxDesc = &pDc->pRxHostDescQPool[i];
		if ((le16_to_cpu(pRxDesc->Ctl_16) & DESC_CTL_HOSTOWN)
		 && (le32_to_cpu(pRxDesc->Status) & BIT31) )
			p += sprintf(p, "%02u FULL%s\n", i, rtl);
		else
			p += sprintf(p, "%02u empty%s\n", i, rtl);
	}
	spin_unlock_irqrestore(&pDc->rx_lock, flags);
	spin_lock_irqsave(&pDc->tx_lock, flags);
	p += sprintf(p, "*** Tx buf (free %d, Linux netqueue %s) ***\n", priv->TxQueueFree,
				acx_queue_stopped(priv->netdev) ? "STOPPED" : "running");
	pTxDesc = pDc->pTxDescQPool;
	for (i = 0; i < pDc->tx_pool_count; i++) {
		thd = (i == pDc->tx_head) ? " [head]" : "";
		ttl = (i == pDc->tx_tail) ? " [tail]" : "";
		if (pTxDesc->Ctl_8 & DESC_CTL_ACXDONE)
			p += sprintf(p, "%02u DONE   (%02x)%s%s\n", i, pTxDesc->Ctl_8, thd, ttl);
		else
		if (!(pTxDesc->Ctl_8 & DESC_CTL_HOSTOWN))
			p += sprintf(p, "%02u TxWait (%02x)%s%s\n", i, pTxDesc->Ctl_8, thd, ttl);
		else
			p += sprintf(p, "%02u empty  (%02x)%s%s\n", i, pTxDesc->Ctl_8, thd, ttl);
		pTxDesc = GET_NEXT_TX_DESC_PTR(pDc, pTxDesc);
	}
	spin_unlock_irqrestore(&pDc->tx_lock, flags);
	p += sprintf(p,
		"\n"
		"*** network status ***\n"
		"dev_state_mask 0x%04x\n"
		"status %u (%s), "
		"mode %u, channel %u, "
		"reg_dom_id 0x%02X, reg_dom_chanmask 0x%04x, "
		/* "txrate_auto %d, txrate_curr %04x, txrate_cfg %04x, "
		"txrate_fallbacks %d/%d, "
		"txrate_stepups %d/%d, "
		"bss_table_count %d\n" */
		,
		priv->dev_state_mask,
		priv->status, acx_get_status_name(priv->status),
		priv->mode, priv->channel,
		priv->reg_dom_id, priv->reg_dom_chanmask
		/* priv->defpeer.txrate.do_auto, priv->defpeer.txrate.cur, priv->defpeer.txrate.cfg,
		priv->defpeer.txrate.fallback_count, priv->defpeer.txrate.fallback_threshold,
		priv->defpeer.txrate.stepup_count, priv->defpeer.txrate.stepup_threshold,
		priv->bss_table_count */
		);
	p += sprintf(p,
		"ESSID \"%s\", essid_active %d, essid_len %d, essid_for_assoc \"%s\", nick \"%s\"\n"
		"WEP ena %d, restricted %d, idx %d\n",
		priv->essid, priv->essid_active, (int)priv->essid_len,
		priv->essid_for_assoc, priv->nick,
		priv->wep_enabled, priv->wep_restricted,
		priv->wep_current_index);
	p += sprintf(p, "dev_addr  "MACSTR"\n", MAC(priv->dev_addr));
	p += sprintf(p, "bssid     "MACSTR"\n", MAC(priv->bssid));
	p += sprintf(p, "ap_filter "MACSTR"\n", MAC(priv->ap));

	p += sprintf(p,
		"\n"
		"*** PHY status ***\n"
		"tx_disabled %d, tx_level_dbm %d, tx_level_val %d, tx_level_auto %d\n"
		"sensitivity %d, antenna 0x%02x, ed_threshold %d, cca %d, preamble_mode %d\n"
		"rts_threshold %d, short_retry %d, long_retry %d, msdu_lifetime %d, listen_interval %d, beacon_interval %d\n",
		priv->tx_disabled, priv->tx_level_dbm, priv->tx_level_val, priv->tx_level_auto,
		priv->sensitivity, priv->antenna, priv->ed_threshold, priv->cca, priv->preamble_mode,
		priv->rts_threshold, priv->short_retry, priv->long_retry, priv->msdu_lifetime, priv->listen_interval, priv->beacon_interval);

	p += sprintf(p,
		"\n"
		"*** TIWLAN_DC ***\n"
		"ui32ACXTxQueueStart %u, ui32ACXRxQueueStart %u\n"
		"pTxBufferPool %p, TxBufferPoolSize %u, TxBufferPoolPhyAddr %08llx\n"
		"TxDescrSize %u, pTxDescQPool %p, tx_lock %d, tx_pool_count %u\n"
		"pFrameHdrQPool %p, FrameHdrQPoolSize %u, FrameHdrQPoolPhyAddr %08llx\n"
		"pTxHostDescQPool %p, TxHostDescQPoolSize %u, TxHostDescQPoolPhyAddr %08llx\n"
		"pRxDescQPool %p, rx_lock %d, rx_pool_count %d\n"
		"pRxHostDescQPool %p, RxHostDescQPoolSize %u, RxHostDescQPoolPhyAddr %08llx\n"
		"pRxBufferPool %p, RxBufferPoolSize %u, RxBufferPoolPhyAddr %08llx\n",
		pDc->ui32ACXTxQueueStart, pDc->ui32ACXRxQueueStart,
		pDc->pTxBufferPool, pDc->TxBufferPoolSize, (u64)pDc->TxBufferPoolPhyAddr,
		pDc->TxDescrSize, pDc->pTxDescQPool, spin_is_locked(&pDc->tx_lock), pDc->tx_pool_count,
		pDc->pFrameHdrQPool, pDc->FrameHdrQPoolSize, (u64)pDc->FrameHdrQPoolPhyAddr,
		pDc->pTxHostDescQPool, pDc->TxHostDescQPoolSize, (u64)pDc->TxHostDescQPoolPhyAddr,
		pDc->pRxDescQPool, spin_is_locked(&pDc->rx_lock), pDc->rx_pool_count,
		pDc->pRxHostDescQPool, pDc->RxHostDescQPoolSize, (u64)pDc->RxHostDescQPoolPhyAddr,
		pDc->pRxBufferPool, pDc->RxBufferPoolSize, (u64)pDc->RxBufferPoolPhyAddr);

        fw_stats = kmalloc(sizeof(fw_stats_t), GFP_KERNEL);
        if (!fw_stats) {
		FN_EXIT1(0);
                return 0;
        }
	memset(fw_stats, 0, sizeof(fw_stats_t));
	if (OK != acx_interrogate(priv, fw_stats, ACX1xx_IE_FIRMWARE_STATISTICS))
		p += sprintf(p,
			"\n"
			"*** Firmware ***\n"
			"QUERY FAILED!!\n");
	else {
		p += sprintf(p,
			"\n"
			"*** Firmware ***\n"
			"version \"%s\"\n"
			"tx_desc_overfl %u, rx_OutOfMem %u, rx_hdr_overfl %u, rx_hdr_use_next %u\n"
			"rx_dropped_frame %u, rx_frame_ptr_err %u, rx_xfr_hint_trig %u, rx_dma_req %u\n"
			"rx_dma_err %u, tx_dma_req %u, tx_dma_err %u, cmd_cplt %u, fiq %u\n"
			"rx_hdrs %u, rx_cmplt %u, rx_mem_overfl %u, rx_rdys %u, irqs %u\n"
			"acx_trans_procs %u, decrypt_done %u, dma_0_done %u, dma_1_done %u\n",
			priv->firmware_version,
			le32_to_cpu(fw_stats->tx_desc_of), le32_to_cpu(fw_stats->rx_oom), le32_to_cpu(fw_stats->rx_hdr_of), le32_to_cpu(fw_stats->rx_hdr_use_next),
			le32_to_cpu(fw_stats->rx_dropped_frame), le32_to_cpu(fw_stats->rx_frame_ptr_err), le32_to_cpu(fw_stats->rx_xfr_hint_trig), le32_to_cpu(fw_stats->rx_dma_req),
			le32_to_cpu(fw_stats->rx_dma_err), le32_to_cpu(fw_stats->tx_dma_req), le32_to_cpu(fw_stats->tx_dma_err), le32_to_cpu(fw_stats->cmd_cplt), le32_to_cpu(fw_stats->fiq),
			le32_to_cpu(fw_stats->rx_hdrs), le32_to_cpu(fw_stats->rx_cmplt), le32_to_cpu(fw_stats->rx_mem_of), le32_to_cpu(fw_stats->rx_rdys), le32_to_cpu(fw_stats->irqs),
			le32_to_cpu(fw_stats->acx_trans_procs), le32_to_cpu(fw_stats->decrypt_done), le32_to_cpu(fw_stats->dma_0_done), le32_to_cpu(fw_stats->dma_1_done));
		p += sprintf(p,
			"tx_exch_complet %u, commands %u, acx_rx_procs %u\n"
			"hw_pm_mode_changes %u, host_acks %u, pci_pm %u, acm_wakeups %u\n"
			"wep_key_count %u, wep_default_key_count %u, dot11_def_key_mib %u\n"
			"wep_key_not_found %u, wep_decrypt_fail %u\n",
			le32_to_cpu(fw_stats->tx_exch_complet), le32_to_cpu(fw_stats->commands), le32_to_cpu(fw_stats->acx_rx_procs),
			le32_to_cpu(fw_stats->hw_pm_mode_changes), le32_to_cpu(fw_stats->host_acks), le32_to_cpu(fw_stats->pci_pm), le32_to_cpu(fw_stats->acm_wakeups),
			le32_to_cpu(fw_stats->wep_key_count), le32_to_cpu(fw_stats->wep_default_key_count), le32_to_cpu(fw_stats->dot11_def_key_mib),
			le32_to_cpu(fw_stats->wep_key_not_found), le32_to_cpu(fw_stats->wep_decrypt_fail));
	}

        kfree(fw_stats);

	FN_EXIT1(p - buf);
	return p - buf;
}

static int acx_proc_eeprom_output(char *buf, wlandevice_t *priv)
{
	char *p = buf;
	int i;

	FN_ENTER;

	for (i = 0; i < 0x400; i++) {
		acx_read_eeprom_offset(priv, i, p);
		p++;
	}

	FN_EXIT1(p - buf);
	return p - buf;
}

static int acx_proc_phy_output(char *buf, wlandevice_t *priv)
{
	char *p = buf;
	int i;

	FN_ENTER;

	/*
	if (RADIO_RFMD_11 != priv->radio_type) {
		acxlog(L_STD, "Sorry, not yet adapted for radio types other than RFMD, please verify PHY size etc. first!\n");
		goto end;
	}
	*/
	
	/* The PHY area is only 0x80 bytes long; further pages after that
	 * only have some page number registers with altered value,
	 * all other registers remain the same. */
	for (i = 0; i < 0x80; i++) {
		acx_read_phy_reg(priv, i, p);
		p++;
	}
	
	FN_EXIT1(p - buf);
	return p - buf;
}

/*------------------------------------------------------------------------------
 * acx_read_proc
 * Handle our /proc entry
 *
 * Arguments:
 *	standard kernel read_proc interface
 * Returns:
 *	number of bytes written to buf
 * Side effects:
 *	none
 * Call context:
 *	
 * Status:
 *	should be okay, non-critical
 * Comment:
 *
 *----------------------------------------------------------------------------*/
static int acx_read_proc(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *priv = (wlandevice_t *)data;
	/* fill buf */
	int length = acx_proc_output(buf, priv);

	FN_ENTER;
	/* housekeeping */
	if (length <= offset + count)
		*eof = 1;
	*start = buf + offset;
	length -= offset;
	if (length > count)
		length = count;
	if (length < 0)
		length = 0;
	FN_EXIT1(length);
	return length;
}

int acx_read_proc_diag(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *priv = (wlandevice_t *)data;
	/* fill buf */
	int length = acx_proc_diag_output(buf, priv);

	FN_ENTER;
	/* housekeeping */
	if (length <= offset + count)
		*eof = 1;
	*start = buf + offset;
	length -= offset;
	if (length > count)
		length = count;
	if (length < 0)
		length = 0;
	FN_EXIT1(length);
	return length;
}

static int acx_read_proc_eeprom(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *priv = (wlandevice_t *)data;
	/* fill buf */
	int length = acx_proc_eeprom_output(buf, priv);

	FN_ENTER;
	/* housekeeping */
	if (length <= offset + count)
		*eof = 1;
	*start = buf + offset;
	length -= offset;
	if (length > count)
		length = count;
	if (length < 0)
		length = 0;
	FN_EXIT1(length);
	return length;
}

static int acx_read_proc_phy(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *priv = (wlandevice_t *)data;
	/* fill buf */
	int length = acx_proc_phy_output(buf, priv);

	FN_ENTER;
	/* housekeeping */
	if (length <= offset + count)
		*eof = 1;
	*start = buf + offset;
	length -= offset;
	if (length > count)
		length = count;
	if (length < 0)
		length = 0;
	FN_EXIT1(length);
	return length;
}

static const char * const proc_files[] = { "", "_diag", "_eeprom", "_phy" };
static void * const acx_proc_funcs[] = { acx_read_proc, acx_read_proc_diag, acx_read_proc_eeprom, acx_read_proc_phy };
u16 manage_proc_entries(const struct net_device *dev, int remove)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	char procbuf[80];
	int i;

	for (i = 0; i < 4; i++)	{
		sprintf(procbuf, "driver/acx_%s", dev->name);
		strcat(procbuf, proc_files[i]);
		if (!remove) {
			acxlog(L_INIT, "creating /proc entry %s\n", procbuf);
			if (!create_proc_read_entry(procbuf, 0, 0, acx_proc_funcs[i], priv))
				return NOT_OK;
		} else {
			acxlog(L_INIT, "removing /proc entry %s\n", procbuf);
			remove_proc_entry(procbuf, NULL);
		}
	}
	return OK;
}

u16 acx_proc_register_entries(const struct net_device *dev)
{
	return manage_proc_entries(dev, 0);
}

u16 acx_proc_unregister_entries(const struct net_device *dev)
{
	return manage_proc_entries(dev, 1);
}
#endif /* CONFIG_PROC_FS */

/*----------------------------------------------------------------
* acx_reset_mac
*
*
* Arguments:
*	wlandevice: private device that contains card device
* Returns:
*	void
* Side effects:
*	MAC will be reset
* Call context:
*	acx_reset_dev
* STATUS:
*	stable
* Comment:
*	resets onboard acx100 MAC
*----------------------------------------------------------------*/

/* acx_reset_mac()
 * Used to be HwReset()
 * STATUS: should be ok.
 */
void acx_reset_mac(wlandevice_t *priv)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	u16 temp;
#endif

	FN_ENTER;

#if (WLAN_HOSTIF!=WLAN_USB)
	/* halt eCPU */
	temp = acx_read_reg16(priv, IO_ACX_ECPU_CTRL) | 0x1;
	acx_write_reg16(priv, IO_ACX_ECPU_CTRL, temp);

	/* now do soft reset of eCPU */
	temp = acx_read_reg16(priv, IO_ACX_SOFT_RESET) | 0x1;
	acxlog(L_BINSTD, "%s: enable soft reset...\n", __func__);
	acx_write_reg16(priv, IO_ACX_SOFT_RESET, temp);

	/* used to be for loop 65536; do scheduled delay instead */
	acx_schedule(HZ / 100);

	/* now reset bit again */
	acxlog(L_BINSTD, "%s: disable soft reset and go to init mode...\n", __func__);
	/* deassert eCPU reset */
	acx_write_reg16(priv, IO_ACX_SOFT_RESET, temp & ~0x1);

	/* now start a burst read from initial flash EEPROM */
	temp = acx_read_reg16(priv, IO_ACX_EE_START) | 0x1;
	acx_write_reg16(priv, IO_ACX_EE_START, temp);
#endif

	/* used to be for loop 65536; do scheduled delay instead */
	acx_schedule(HZ / 100);

	FN_EXIT0();
}


/*----------------------------------------------------------------
* acx_read_fw
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_read_fw()
 * STATUS: new
 *
 * Loads a firmware image in from a file.
 *
 * This seemed like a good idea at the time. I'm not so sure about it now.
 * perhaps this should be a user mode helper that ioctls the module the firmware?
 *
 * Returns:
 *  0				unable to load file
 *  pointer to firmware		success
 */
#ifdef USE_FW_LOADER_LEGACY
static char * const default_firmware_dir = "/usr/share/acx";
#endif
#ifdef USE_FW_LOADER_26
firmware_image_t* acx_read_fw(struct device *dev, const char *file, u32 *size)
#else
firmware_image_t* acx_read_fw(const char *file, u32 *size)
#endif
{
	firmware_image_t *res = NULL;

#ifdef USE_FW_LOADER_LEGACY
	mm_segment_t orgfs;
	unsigned long page;
	char *buffer;
	struct file *inf;
	int retval;
	u32 offset = 0;
	char *filename;
#endif

#ifdef USE_FW_LOADER_26
	const struct firmware *fw_entry;

	acxlog(L_STD, "Requesting firmware image '%s'\n", file);
	if (!request_firmware(&fw_entry, file, dev)) {
		*size = 8 + le32_to_cpu(*(u32 *)(fw_entry->data + 4));
		if (fw_entry->size != *size) {
			acxlog(L_STD, "firmware size does not match firmware header: %d != %d\n", fw_entry->size, *size);
		}
		res = vmalloc(*size);
		if (!res) {
			acxlog(L_STD, "ERROR: Unable to allocate %u bytes for firmware\n", *size);
			goto ret;
		}
		memcpy(res, fw_entry->data, fw_entry->size);
		release_firmware(fw_entry);
		goto ret;
	}
	acxlog(L_STD, "No firmware image was provided. Check your hotplug scripts\n");
#endif

#ifdef USE_FW_LOADER_LEGACY
	orgfs = get_fs(); /* store original fs */
	set_fs(KERNEL_DS);

	/* Read in whole file then check the size */
	page = __get_free_page(GFP_KERNEL);
	if (0 == page) {
		acxlog(L_STD, "Unable to allocate memory for firmware loading\n");
		goto fail;
	}

	filename = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!filename) {
		acxlog(L_STD, "Unable to allocate memory for firmware loading\n");
		goto fail;
	}
	if (!firmware_dir) {
		firmware_dir = default_firmware_dir;
		acxlog(L_STD, "Attention: no firmware directory specified "
			"via module parameter firmware_dir, using default "
			"firmware directory %s\n", firmware_dir);
	}
	sprintf(filename,"%s/%s", firmware_dir, file);
	acxlog(L_STD, "Reading firmware image '%s'\n", filename);

	buffer = (char*)page;
	
	/* Note that file must be given as absolute path:
	 * a relative path works on first loading,
	 * but any subsequent firmware loading during card
	 * eject/insert will fail, most likely since the first
	 * module loading happens in user space (and thus
	 * filp_open can figure out the absolute path from a
	 * relative path) whereas the card reinsert processing
	 * probably happens in kernel space where you don't have
	 * a current directory to be able to figure out an
	 * absolute path from a relative path... */
	inf = filp_open(filename, O_RDONLY, 0);
	kfree(filename);
	if (OK != IS_ERR(inf)) {
		char *err;

		switch(-PTR_ERR(inf)) {
			case 2: err = "file not found - make sure this EXACT filename is in eXaCtLy this directory!";
				break;
			default:
				err = "unknown error";
				break;
		}
		acxlog(L_STD, "ERROR %ld trying to open firmware image file '%s': %s\n", -PTR_ERR(inf), file, err);
		goto fail;
	}

	if ((NULL == inf->f_op) || (NULL == inf->f_op->read)) {
		acxlog(L_STD, "ERROR: %s does not have a read method\n", file);
		goto fail_close;
	}

	offset = 0;
	do {
		retval = inf->f_op->read(inf, buffer, PAGE_SIZE, &inf->f_pos);

		if (0 > retval) {
			acxlog(L_STD, "ERROR %d reading firmware image file '%s'.\n", -retval, file);
			if (NULL != res)
				vfree(res);
			res = NULL;
		} else if (0 == retval) {
			if (0 == offset) {
				acxlog(L_STD, "ERROR: firmware image file '%s' is empty.\n", file);
			}
		} else if (0 < retval) {
			/* allocate result buffer here if needed,
			 * since we don't want to waste resources/time
			 * (in case file opening/reading fails)
			 * by doing allocation in front of the loop instead. */
			if (NULL == res) {
				*size = 8 + le32_to_cpu(*(u32 *)(4 + buffer));

				res = vmalloc(*size);
				if (NULL == res) {
					acxlog(L_STD, "ERROR: Unable to allocate %u bytes for firmware module loading.\n", *size);
					goto fail_close;
				}
				acxlog(L_STD, "Allocated %u bytes for firmware module loading.\n", *size);
			}
			memcpy((u8*)res + offset, buffer, retval);
			offset += retval;
		}
	} while (0 < retval);

fail_close:
	retval = filp_close(inf, NULL);

	if (retval) {
		acxlog(L_STD, "ERROR %d closing %s\n", -retval, file);
	}

	if ((NULL != res) && (offset != le32_to_cpu(res->size) + 8)) {
		acxlog(L_STD,"Firmware is reporting a different size 0x%08x to read 0x%08x\n", le32_to_cpu(res->size) + 8, offset);
		vfree(res);
		res = NULL;
	}

fail:
	if (page)
		free_page(page);
	set_fs(orgfs);
#endif

ret:
	/* checksum will be verified in write_fw, so don't bother here */

	return res;
}


#if (WLAN_HOSTIF!=WLAN_USB)

#define NO_AUTO_INCREMENT	1

/*----------------------------------------------------------------
* acx_write_fw
* Used to be WriteACXImage
*
* Write the firmware image into the card.
*
* Arguments:
*	priv		wlan device structure
*   apfw_image  firmware image.
*
* Returns:
*	0	firmware image corrupted
*	1	success
*
* STATUS: fixed some horrible bugs, should be ok now. FINISHED.
----------------------------------------------------------------*/

static int acx_write_fw(wlandevice_t *priv, const firmware_image_t *apfw_image, u32 offset)
{
	int len,size;
	u32 sum,v32;
	/* we skip the first four bytes which contain the control sum */
	const u8 *image = (u8*)apfw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = image[0]+image[1]+image[2]+image[3];
	image += 4;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0);

#if NO_AUTO_INCREMENT
	acxlog(L_INIT, "not using auto increment for firmware loading\n");
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0); /* use basic mode */
#else
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 1); /* use autoincrement mode */
#endif

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset); /* configure host indirect memory access address?? */

	len = 0;
	size = le32_to_cpu(apfw_image->size) & (~3);

	while (len < size) {
		v32 = be32_to_cpu(*(u32*)image);
		sum += image[0]+image[1]+image[2]+image[3];
		image += 4;
		len += 4;

		/* this could probably also be done by doing
		 * 32bit write to register priv->io[IO_ACX_SLV_MEM_DATA]...
		 * But maybe there are cards with 16bit interface
		 * only */
#if NO_AUTO_INCREMENT
		acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
#endif
		acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, v32);

		/* reschedule after some time has been spent in this loop!
		 * cond_resched() doesn't seem to work here,
		 * since we're probably uploading the firmware
		 * as a kernel thread, so there was no user thread
		 * kicking off this operation which can have its
		 * time slice expired (and thus having need_resched())
		 */
		if (unlikely(len % 16384 == 8192)) {
			acx_schedule(HZ / 50);
		}
	}

	acxlog(L_STD,"%s: firmware written\n", __func__);

	/* compare our checksum with the stored image checksum */
	return (int)(sum != le32_to_cpu(apfw_image->chksum));
}

/*----------------------------------------------------------------
* acx_validate_fw
* used to be ValidateACXImage
*
* Compare the firmware image given with
* the firmware image written into the card.
*
* Arguments:
*	priv		wlan device structure
*   apfw_image  firmware image.
*
* Returns:
*	NOT_OK	firmware image corrupted or not correctly written
*	OK	success
*
* STATUS: FINISHED.
----------------------------------------------------------------*/

static int acx_validate_fw(wlandevice_t *priv, const firmware_image_t *apfw_image, u32 offset)
{
	u32 v32,w32,sum;
	int len,size;
	int result = OK;
	/* we skip the first four bytes which contain the control sum */
	const u8 *image = (u8*)apfw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = image[0]+image[1]+image[2]+image[3];
	image += 4;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0);

#if NO_AUTO_INCREMENT
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0);
#else
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 1);
#endif

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset);

	len = 0;
	size = le32_to_cpu(apfw_image->size) & (~3);

	while (len < size) {
		v32 = be32_to_cpu(*(u32*)image);
		image += 4;
		len += 4;

#if NO_AUTO_INCREMENT
		acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
#endif
		w32 = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

		if (unlikely(w32 != v32)) {
			acxlog(L_STD, "FATAL: firmware upload: "
			"data parts at offset %d don't match (0x%08x vs. 0x%08x)! "
			"I/O timing issues or defective memory, with DWL-xx0+? "
			"Makefile: ACX_IO_WIDTH=16 should help. Please report!\n",
				len, v32, w32);
			result = NOT_OK;
			break;
		}

		sum += (u8)w32 + (u8)(w32>>8) + (u8)(w32>>16) + (u8)(w32>>24);

		if (unlikely(len % 16384 == 8192)) {
			acx_schedule(HZ / 50);
		}
	}

	/* sum control verification */
	if (result != NOT_OK) {
		if (sum != le32_to_cpu(apfw_image->chksum)) {
			acxlog(L_STD, "FATAL: firmware upload: checksums don't match!\n");
			result = NOT_OK;
		}
	}

	return result;
}

/*----------------------------------------------------------------
* acx_upload_fw
*
*
* Arguments:
*	wlandevice: private device that contains card device
* Returns:
*	NOT_OK: failed
*	OK: success
* Side effects:
*
* Call context:
*	acx_reset_dev
* STATUS:
*	stable
* Comment:
*
*----------------------------------------------------------------*/

static int acx_upload_fw(wlandevice_t *priv)
{
	int res1 = NOT_OK;
	int res2 = NOT_OK;
	firmware_image_t *apfw_image = NULL;
	char *filename;
	int try;
	u32 size;

	FN_ENTER;

	if (priv->chip_type == CHIPTYPE_ACX111) {
		filename = "TIACX111.BIN"; /* combined firmware */
#if DISABLED_USER_SHALL_SYMLINK_IT_TO_TIACX111
		if (OK != acx_check_file(filename)) {
			acxlog(L_INIT, "Firmware: '%s' not found. Trying alternative firmware.\n", filename);
			filename = "FwRad16.bin" /* combined firmware */
			/* FIXME: why radio #16 is hardcoded in above?! */
			if (OK != acx_check_file(filename)) {
				acxlog(L_INIT, "Firmware: '%s' not found. Trying alternative firmware.\n", filename);
				filename = "FW1130.BIN"; /* NON-combined firmware! */
			}
		}
#endif
	} else
		filename = "WLANGEN.BIN";
	
#ifdef USE_FW_LOADER_26
	apfw_image = acx_read_fw(&priv->pdev->dev, filename, &size);
#else
	apfw_image = acx_read_fw(filename, &size);
#endif
	if (NULL == apfw_image) {
		acxlog(L_STD, "acx_read_fw FAILED\n");
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	for (try = 1; try <= 5; try++) {
		res1 = acx_write_fw(priv, apfw_image, 0);
		res2 = acx_validate_fw(priv, apfw_image, 0);

		acxlog(L_DEBUG | L_INIT,
	   		"acx_write_fw (firmware): %d, acx_validate_fw: %d\n", res1, res2);
		if ((OK == res1) && (OK == res2))
			break;
		acxlog(L_STD, "firmware upload attempt #%d FAILED, retrying...\n", try);
		acx_schedule(HZ); /* better wait for a while... */
	}

	vfree(apfw_image);

	if ((OK == res1) && (OK == res2))
		SET_BIT(priv->dev_state_mask, ACX_STATE_FW_LOADED);
	FN_EXIT1(res1 || res2);
	return (res1 || res2);
}
#endif /* (WLAN_HOSTIF!=WLAN_USB) */

#if (WLAN_HOSTIF!=WLAN_USB)
/*----------------------------------------------------------------
* acx_upload_radio
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_upload_radio()
 * STATUS: new
 * Used to upload the appropriate radio module firmware
 * into the card.
 */
int acx_upload_radio(wlandevice_t *priv)
{
	u32 offset;
	acx_ie_memmap_t mm;
	int res = NOT_OK, res1 = NOT_OK, res2 = NOT_OK;
	firmware_image_t *radio_image=NULL;
	acx_cmd_radioinit_t radioinit;
/* FIXME: name RADIOnn.BIN is too generic. We'll need to use
** something like TIACX1nn_nn.BIN for Linux 2.6 style fw upload
** via hotplug */
	char filename[sizeof("RADIOnn.BIN")];
	int try;
	u32 size;

	FN_ENTER;
	acx_interrogate(priv, &mm, ACX1xx_IE_MEMORY_MAP);
	offset = le32_to_cpu(mm.CodeEnd);

	sprintf(filename,"RADIO%02x.BIN", priv->radio_type);
#ifdef USE_FW_LOADER_26
	radio_image = acx_read_fw(&priv->pdev->dev, filename, &size);
#else
	radio_image = acx_read_fw(filename, &size);
#endif

/*
 * 0d = RADIO0d.BIN = Maxim chipset
 * 11 = RADIO11.BIN = RFMD chipset
 * 15 = RADIO15.BIN = Ralink chipset
 * 16 = RADIO16.BIN = Radia chipset
 * 17 = RADIO17.BIN = UNKNOWN chipset
 */

	if (NULL == radio_image) {
		acxlog(L_STD,"WARNING: no suitable radio module (%s) found "
			"to load. No problem in case of a combined firmware, "
			"FATAL when using a separated firmware "
			"(base firmware / radio image).\n", filename);
		res = OK; /* Doesn't need to be fatal, we might be using a combined image */
		goto fail;
	}

	acx_issue_cmd(priv, ACX1xx_CMD_SLEEP, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);

	for (try = 1; try <= 5; try++) {
		res1 = acx_write_fw(priv, radio_image, offset);
		res2 = acx_validate_fw(priv, radio_image, offset);
		acxlog(L_DEBUG | L_INIT, "acx_write_fw (radio): %d, acx_validate_fw: %d\n", res1, res2);
		if ((OK == res1) && (OK == res2))
			break;
		acxlog(L_STD, "radio firmware upload attempt #%d FAILED, retrying...\n", try);
		acx_schedule(HZ); /* better wait for a while... */
	}

	acx_issue_cmd(priv, ACX1xx_CMD_WAKE, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
	radioinit.offset = cpu_to_le32(offset);
	radioinit.len = radio_image->size; /* no endian conversion needed, remains in card CPU area */
	
	vfree(radio_image);
	
	if ((OK != res1) || (OK != res2))
		goto fail;

	/* will take a moment so let's have a big timeout */
	acx_issue_cmd(priv, ACX1xx_CMD_RADIOINIT, &radioinit, sizeof(radioinit), 120000);

	if (OK != acx_interrogate(priv, &mm, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "Error reading memory map\n");
		goto fail;
	}
	res = OK;
fail:
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_verify_init
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_verify_init()
 * ACXWaitForInitComplete()
 * STATUS: should be ok.
 */
static int acx_verify_init(wlandevice_t *priv)
{
	int result = NOT_OK;
	int timer;

	FN_ENTER;

	for (timer = 40; timer > 0; timer--) {
		if (acx_read_reg16(priv, IO_ACX_IRQ_STATUS_NON_DES) & HOST_INT_FCS_THRESHOLD) {
			result = OK;
			acx_write_reg16(priv, IO_ACX_IRQ_ACK, HOST_INT_FCS_THRESHOLD);
			break;
		}

		/* used to be for loop 65535; do scheduled delay instead */
		acx_schedule(HZ / 20); /* HZ / 50 resulted in 24 schedules for ACX100 on my machine, so better schedule away longer for greater efficiency, decrease loop count */
	}

	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_reset_dev
*
*
* Arguments:
*	netdevice that contains the wlandevice priv variable
* Returns:
*	NOT_OK on fail
*	OK on success
* Side effects:
*	device is hard reset
* Call context:
*	acx_probe_pci
* STATUS:
*	stable
* Comment:
*	This resets the acx100 device using low level hardware calls
*	as well as uploads and verifies the firmware to the card
*----------------------------------------------------------------*/

int acx_reset_dev(netdevice_t *dev)
{
	int result = NOT_OK;
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	u16 vala = 0;

	FN_ENTER;

	/* we're doing a reset, so hardware is unavailable */
	priv->hw_unavailable++;
	acxlog(L_INIT, "reset hw_unavailable++\n");
	
	/* reset the device to make sure the eCPU is stopped 
	   to upload the firmware correctly */
	acx_reset_mac(priv);	

	vala = acx_read_reg16(priv, IO_ACX_ECPU_CTRL) & 1;
	if (!vala) {
		acxlog(L_BINSTD, "%s: eCPU already running (%xh)\n", __func__, vala);
		goto fail;
	}

#if WE_DONT_NEED_THAT_DO_WE
	if (acx_read_reg16(priv, IO_ACX_SOR_CFG) & 2) {
		/* eCPU most likely means "embedded CPU" */
		acxlog(L_BINSTD, "%s: eCPU did not start after boot from flash\n", __func__);
		goto fail;
	}

	/* check sense on reset flags */
	if (acx_read_reg16(priv, IO_ACX_SOR_CFG) & 0x10) { 			
		acxlog(L_BINSTD, "%s: eCPU do not start after boot (SOR), is this fatal?\n", __func__);
	}
#endif
	acx_schedule(HZ / 100);

	/* load the firmware */
	if (OK != acx_upload_fw(priv)) {
		acxlog(L_STD, "%s: Failed to upload firmware to the ACX1xx\n", __func__);
		goto fail;
	}

	acx_schedule(HZ / 100);

	/* now start eCPU by clearing bit */
	acxlog(L_BINSTD, "%s: boot up eCPU and wait for complete...\n", __func__);
	acx_write_reg16(priv, IO_ACX_ECPU_CTRL, vala & ~0x1);

	/* wait for eCPU bootup */
	if (OK != acx_verify_init(priv)) {
		acxlog(L_BINSTD,
			   "Timeout waiting for the ACX100 to complete initialization\n");
		goto fail;
	}

	acxlog(L_BINSTD, "%s: Received signal that card is ready to be configured :) (the eCPU has woken up)\n", __func__);

	if (priv->chip_type == CHIPTYPE_ACX111) {
		acxlog(L_BINSTD, "%s: Clean up cmd mailbox access area\n", __func__);
		acx_write_cmd_type_or_status(priv, 0, 1);
		acx_get_cmd_state(priv);
		if (priv->cmd_status) {
			acxlog(L_BINSTD, "Error cleaning cmd mailbox area\n");
			goto fail;
		}
	}

	/* TODO what is this one doing ?? adapt for acx111 */
	if ((OK != acx_read_eeprom_area(priv)) && (CHIPTYPE_ACX100 == priv->chip_type)) {
		/* does "CIS" mean "Card Information Structure"?
		 * If so, then this would be a PCMCIA message...
		 */
		acxlog(L_BINSTD, "CIS error\n");
		goto fail;
	}

	/* reset succeeded, so let's release the hardware lock */
	acxlog(L_INIT, "reset hw_unavailable--\n");
	priv->hw_unavailable--;
	result = OK;
fail:
	FN_EXIT1(result);
	return result;
}
#endif /* (WLAN_HOSTIF!=WLAN_USB) */


/*----------------------------------------------------------------
* acx_init_mboxes
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acxInitializeMailboxes
  STATUS: should be ok.
*/
static void acx_init_mboxes(wlandevice_t *priv)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	u32 cmd_offs, info_offs;

	FN_ENTER;
	acxlog(L_BINDEBUG,
		   "==> Get the mailbox pointers from the scratch pad registers\n");

	cmd_offs = acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS);
	info_offs = acx_read_reg32(priv, IO_ACX_INFO_MAILBOX_OFFS);
	
	acxlog(L_BINDEBUG,
		"CmdMailboxOffset = %x\n"
		"InfoMailboxOffset = %x\n"
		"<== Get the mailbox pointers from the scratch pad registers\n",
		cmd_offs,
		info_offs);
	priv->CommandParameters = priv->iobase2 + cmd_offs + 0x4;
	priv->InfoParameters = priv->iobase2 + info_offs + 0x4;
	acxlog(L_BINDEBUG,
		"CommandParameters = [ 0x%p ]\n"
		"InfoParameters = [ 0x%p ]\n",
		   priv->CommandParameters,
		   priv->InfoParameters);
	FN_EXIT0();
#endif /* (WLAN_HOSTIF!=WLAN_USB) */
}

void acx100_set_wepkey( wlandevice_t *priv )
{
  ie_dot11WEPDefaultKey_t dk;
  int i;
   
  for ( i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++ ) {
    if ( priv->wep_keys[i].size != 0 ) {
      acxlog(L_INIT, "Setting WEP key: %d with size: %d\n", i, priv->wep_keys[i].size);
      dk.action = 1;
      dk.keySize = priv->wep_keys[i].size;
      dk.defaultKeyNum = i;
      memcpy(dk.key, priv->wep_keys[i].key, dk.keySize);
      acx_configure(priv, &dk, ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE);
    }
  }
}
 
void acx111_set_wepkey( wlandevice_t *priv )
{
  acx111WEPDefaultKey_t dk;
  int i;
                                                                                
  for ( i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++ ) {
    if ( priv->wep_keys[i].size != 0 ) {
      acxlog(L_INIT, "Setting WEP key: %d with size: %d\n", i, priv->wep_keys[i].size);
      memset(&dk, 0, sizeof(dk));
      dk.action = cpu_to_le16(1); /* "add key"; yes, that's a 16bit value */
      dk.keySize = priv->wep_keys[i].size;

    /* are these two lines necessary? */
      dk.type = 0;              /* default WEP key */
      dk.index = 0;             /* ignored when setting default key */


      dk.defaultKeyNum = i;
      memcpy(dk.key, priv->wep_keys[i].key, dk.keySize);
      acx_issue_cmd(priv, ACX1xx_CMD_WEP_MGMT, &dk, sizeof(dk), ACX_CMD_TIMEOUT_DEFAULT);
    }
  }
}

/*----------------------------------------------------------------
* acx100_init_wep
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx100_init_wep()
 * STATUS: UNVERIFIED.
 * FIXME: this should probably be moved into the new card settings
 * management, but since we're also modifying the memory map layout here
 * due to the WEP key space we want, we should take care...
 */
int acx100_init_wep(wlandevice_t *priv)
{
/*	int i;
	acx100_cmd_wep_mgmt_t wep_mgmt;           size = 37 bytes */
	acx100_ie_wep_options_t options;
	ie_dot11WEPDefaultKeyID_t dk;
	acx_ie_memmap_t pt;
	int res = NOT_OK;

	FN_ENTER;
	
	if (OK != acx_interrogate(priv, &pt, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "ctlMemoryMapRead FAILED\n");
		goto fail;
	}

	acxlog(L_BINDEBUG, "CodeEnd:%X\n", pt.CodeEnd);

	pt.WEPCacheStart = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);
	pt.WEPCacheEnd   = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);

	if (OK != acx_configure(priv, &pt, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "%s: ctlMemoryMapWrite FAILED\n", __func__);
		goto fail;
	}

	options.NumKeys = cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10); /* let's choose maximum setting: 4 default keys, plus 10 other keys */
	options.WEPOption = 0x00;

	acxlog(L_ASSOC, "%s: writing WEP options.\n", __func__);
	acx_configure(priv, &options, ACX100_IE_WEP_OPTIONS);
	
	acx100_set_wepkey( priv );
	
	if (priv->wep_keys[priv->wep_current_index].size != 0) {
		acxlog(L_ASSOC, "setting active default WEP key number: %d.\n", priv->wep_current_index);
		dk.KeyID = priv->wep_current_index;
		acx_configure(priv, &dk, ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET); /* 0x1010 */
	}
	/* FIXME!!! wep_key_struct is filled nowhere! But priv
	 * is initialized to 0, and we don't REALLY need those keys either */
/*		for (i = 0; i < 10; i++) {
		if (priv->wep_key_struct[i].len != 0) {
			MAC_COPY(wep_mgmt.MacAddr, priv->wep_key_struct[i].addr);
			wep_mgmt.KeySize = cpu_to_le16(priv->wep_key_struct[i].len);
			memcpy(&wep_mgmt.Key, priv->wep_key_struct[i].key, le16_to_cpu(wep_mgmt.KeySize));
			wep_mgmt.Action = cpu_to_le16(1);
			acxlog(L_ASSOC, "writing WEP key %d (len %d).\n", i, le16_to_cpu(wep_mgmt.KeySize));
			if (OK == acx_issue_cmd(priv, ACX1xx_CMD_WEP_MGMT, &wep_mgmt, sizeof(wep_mgmt), ACX_CMD_TIMEOUT_DEFAULT)) {
				priv->wep_key_struct[i].index = i;
			}
		}
	} */

	/* now retrieve the updated WEPCacheEnd pointer... */
	if (OK != acx_interrogate(priv, &pt, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "ctlMemoryMapRead #2 FAILED\n");
		goto fail;
	}
	/* ...and tell it to start allocating templates at that location */
	pt.PacketTemplateStart = pt.WEPCacheEnd; /* no endianness conversion needed */

	if (OK != acx_configure(priv, &pt, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD, "ctlMemoryMapWrite #2 FAILED\n");
		goto fail;
	}
	res = OK;

fail:
	FN_EXIT1(res);
	return res;
}

static int acx_init_max_null_data_template(wlandevice_t *priv)
{
	struct acxp80211_nullframe b;
	int result;

	FN_ENTER;
	memset(&b, 0, sizeof(struct acxp80211_nullframe));
	b.size = cpu_to_le16(sizeof(struct acxp80211_nullframe) - 2);
	result = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_NULL_DATA, &b, sizeof(struct acxp80211_nullframe), ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_init_max_beacon_template
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_init_max_beacon_template()
 * InitMaxACXBeaconTemplate()
 * STATUS: should be ok.
 */
static int acx_init_max_beacon_template(wlandevice_t *priv)
{
	struct acx_template_beacon b;
	int result;

	FN_ENTER;
	memset(&b, 0, sizeof(b));
	b.size = cpu_to_le16(sizeof(b) - 2);
	result = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_BEACON, &b, sizeof(b), ACX_CMD_TIMEOUT_DEFAULT);

	FN_EXIT1(result);
	return result;
}

/* acx_init_max_tim_template()
 * InitMaxACXTIMTemplate()
 * STATUS: should be ok.
 */
static int acx_init_max_tim_template(wlandevice_t *priv)
{
	acx_template_tim_t t;

	memset(&t, 0, sizeof(t));
	t.size = cpu_to_le16(sizeof(t) - 2);
	return acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_TIM, &t, sizeof(t), ACX_CMD_TIMEOUT_DEFAULT);
}

/*----------------------------------------------------------------
* acx_init_max_probe_response_template
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_init_max_probe_response_template()
 * InitMaxACXProbeResponseTemplate()
 * STATUS: should be ok.
 */
static int acx_init_max_probe_response_template(wlandevice_t *priv)
{
	struct acx_template_proberesp pr;
	
	memset(&pr, 0, sizeof(pr));
	pr.size = cpu_to_le16(sizeof(pr) - 2);

	return acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_PROBE_RESPONSE, &pr, sizeof(pr), ACX_CMD_TIMEOUT_DEFAULT);
}


/* acx_init_max_probe_request_template()
 * STATUS: should be ok.
 *
 * NB: I wasn't able to make my acx111 to emit probe requests,
 * even when I hacked this func to produce good template
 * with bcast bssid/da, my own mac in sa, correct ssid and rate vector
 * (I didnt forget to make _active_ scans. I saw bogus Assoc requests
 * with empty ssid instead of Probe Requests. I guess I forgot to
 * initialize fc --vda)
 */
static int acx_init_max_probe_request_template(wlandevice_t *priv)
{
	union {
		acx100_template_probereq_t p100;
		acx111_template_probereq_t p111;
	} pr;
	int res;

	FN_ENTER;
	memset(&pr, 0, sizeof(pr));
	pr.p100.size = cpu_to_le16(sizeof(pr) - 2);
	res = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_PROBE_REQUEST, &pr, sizeof(pr), ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_set_tim_template
* STATUS: should be ok.

[802.11 7.3.2.6] TIM information element:
- 1 EID
- 1 Length
1 1 DTIM Count
    indicates how many beacons (including this) appear before next DTIM
    (0=this one is a DTIM)
2 1 DTIM Period
    number of beacons between successive DTIMs
    (0=reserved, 1=all TIMs are DTIMs, 2=every other, etc)
3 1 Bitmap Control
    bit0: Traffic Indicator bit associated with Assoc ID 0 (Bcast AID?)
    set to 1 in TIM elements with a value of 0 in the DTIM Count field
    when one or more broadcast or multicast frames are buffered at the AP.
    bit1-7: Bitmap Offset (logically Bitmap_Offset = Bitmap_Control & 0xFE).
4 n Partial Virtual Bitmap
    Visible part of traffic-indication bitmap.
    Full bitmap consists of 2008 bits (251 octets) such that bit number N
    (0<=N<=2007) in the bitmap corresponds to bit number (N mod 8)
    in octet number N/8 where the low-order bit of each octet is bit0,
    and the high order bit is bit7.
    Each set bit in virtual bitmap corresponds to traffic buffered by AP
    for a specific station (with corresponding AID?).
    Partial Virtual Bitmap shows a part of bitmap which has non-zero.
    Bitmap Offset is a number of skipped zero octets (see above).
    'Missing' octets at the tail are also assumed to be zero.
    Example: Length=6, Bitmap_Offset=2, Partial_Virtual_Bitmap=55 55 55
    This means that traffic-indication bitmap is:
    00000000 00000000 01010101 01010101 01010101 00000000 00000000...
    (is bit0 in the map is always 0 and real value is in Bitmap Control bit0?)
*----------------------------------------------------------------*/
/* In full blown driver we will regularly update partial virtual bitmap
** by calling this function
** (it can be done by irq handler on each DTIM irq or by timer...) */
static int acx_set_tim_template(wlandevice_t *priv)
{
/* For now, configure smallish test bitmap, all zero ("no pending data") */
	enum { bitmap_size = 5 };

	acx_template_tim_t t;
	int result;

	FN_ENTER;

	memset(&t, 0, sizeof(t));
	t.size = 5 + bitmap_size; /* eid+len+count+period+bmap_ctrl + bmap */
	t.tim_eid = WLAN_EID_TIM;
	t.len = 3 + bitmap_size; /* count+period+bmap_ctrl + bmap */
	result = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_TIM, &t, sizeof(t), ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_fill_beacon_or_proberesp_template
* 
* For frame format info, please see 802.11-1999.pdf item 7.2.3.9 and below!!
*
* STATUS: done
* WARNING/FIXME/TODO: this needs to be called (via SET_TEMPLATES) *whenever*
* *any* of the parameters contained in it change!!!
* fishy status fixed
*
* NB: we use the fact that 
* struct acx_template_proberesp and struct acx_template_beacon are the same
* (well, almost...)
*
* [802.11] Beacon's body consist of these IEs:
* 1 Timestamp
* 2 Beacon interval
* 3 Capability information
* 4 SSID
* 5 Supported rates (up to 8 rates)
* 6 FH Parameter Set (frequency-hopping PHYs only)
* 7 DS Parameter Set (direct sequence PHYs only)
* 8 CF Parameter Set (only if PCF is supported)
* 9 IBSS Parameter Set (ad-hoc only)
*
* Beacon only:
* 10 TIM (AP only) (see 802.11 7.3.2.6)
* 11 Country Information (802.11d)
* 12 FH Parameters (802.11d)
* 13 FH Pattern Table (802.11d)
* ... (?!! did not yet find relevant PDF file... --vda)
* 19 ERP Information (extended rate PHYs)
* 20 Extended Supported Rates (if more than 8 rates)
*
* Proberesp only:
* 10 Country information (802.11d)
* 11 FH Parameters (802.11d)
* 12 FH Pattern Table (802.11d)
* 13-n Requested information elements (802.11d)
* ????
* 18 ERP Information (extended rate PHYs)
* 19 Extended Supported Rates (if more than 8 rates)
*----------------------------------------------------------------*/
static int acx_fill_beacon_or_proberesp_template(wlandevice_t *priv,
					struct acx_template_beacon *templ,
					u16 fc /* in host order! */)
{
	int len;
	char *p;

	FN_ENTER;
	memset(templ, 0, sizeof(*templ));
	MAC_BCAST(templ->da);
	MAC_COPY(templ->sa, priv->dev_addr);
	MAC_COPY(templ->bssid, priv->bssid);

	templ->beacon_interval = cpu_to_le16(priv->beacon_interval);
	acx_update_capabilities(priv);
	templ->cap = cpu_to_le16(priv->capabilities);

	p = templ->variable;
	p = wlan_fill_ie_ssid(p, priv->essid_len, priv->essid);
	p = wlan_fill_ie_rates(p, priv->rate_supported_len, priv->rate_supported);
	p = wlan_fill_ie_ds_parms(p, priv->channel);
	/* NB: should go AFTER tim, but acx seem to keep tim last always */
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);

	switch (priv->mode) {
	case ACX_MODE_0_ADHOC:
		/* ATIM window */
		p = wlan_fill_ie_ibss_parms(p, 0); break;
	case ACX_MODE_3_AP:
		/* TIM IE is set up as separate template */
		break;
	}

	len = p - (char*)templ;
	templ->fc = cpu_to_le16(WF_FTYPE_MGMT | fc);
	/* - 2: do not count 'u16 size' field */
	templ->size = cpu_to_le16(len - 2);
	FN_EXIT1(len);
	return len;
}

/*----------------------------------------------------------------
* acx_set_beacon_template
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_set_beacon_template()
 * SetACXBeaconTemplate()
 * STATUS: FINISHED.
 */
static int acx_set_beacon_template(wlandevice_t *priv)
{
	struct acx_template_beacon bcn;
	unsigned int len;
	int result;

	FN_ENTER;

	len = acx_fill_beacon_or_proberesp_template(priv, &bcn, WF_FSTYPE_BEACON);
	acxlog(L_BINDEBUG, "Beacon length:%i\n", len);

	result = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_BEACON, &bcn, len, ACX_CMD_TIMEOUT_DEFAULT);

	FN_EXIT1(result);

	return result;
}

/*----------------------------------------------------------------
* acx_set_probe_response_template
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* SetACXProbeResponseTemplate()
 * STATUS: ok.
 */
static int acx_set_probe_response_template(wlandevice_t *priv)
{
	struct acx_template_proberesp pr;
	const u8 *pr2;
	int len, result;

	FN_ENTER;

	len = acx_fill_beacon_or_proberesp_template(priv, &pr, WF_FSTYPE_PROBERESP);

	acxlog(L_DATA | L_XFER, "SetProberTemp: cb = %d\n", len);
	pr2 = pr.variable;
	acxlog(L_DATA,
		"sa="MACSTR"\n"
		"BSSID="MACSTR"\n"
		"SetProberTemp: Info1=%02X %02X %02X %02X %02X %02X %02X %02X\n"
		"SetProberTemp: Info2=%02X %02X %02X %02X %02X %02X %02X %02X\n"
		"SetProberTemp: Info3=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		MAC(pr.sa), MAC(pr.bssid),
		pr2[0x00], pr2[0x01], pr2[0x02], pr2[0x03], pr2[0x04], pr2[0x05], pr2[0x06], pr2[0x07],
		pr2[0x08], pr2[0x09], pr2[0x0a], pr2[0x0b], pr2[0x0c], pr2[0x0d], pr2[0x0e], pr2[0x0f],
		pr2[0x10], pr2[0x11], pr2[0x12], pr2[0x13], pr2[0x14], pr2[0x15], pr2[0x16], pr2[0x17]);

	result = acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_PROBE_RESPONSE, &pr, len, ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT1(result);
	return result;
}

/* acx100_init_packet_templates()
 *
 * NOTE: order is very important here, to have a correct memory layout!
 * init templates: max Probe Request (station mode), max NULL data,
 * max Beacon, max TIM, max Probe Response.
 * 
 * acxInitPacketTemplates()
 * STATUS: almost ok, except for struct definitions.
 */
static int acx100_init_packet_templates(wlandevice_t *priv, acx_ie_memmap_t *mm)
{
	/* TODO: either remove len or rework it so that it is really useful */
	/* unsigned int len = 0; */
	int result = NOT_OK;

	FN_ENTER;

#if NOT_WORKING_YET
	/* FIXME: creating the NULL data template breaks
	 * communication right now, needs further testing.
	 * Also, need to set the template once we're joining a network. */
	if (OK != acx_init_max_null_data_template(priv))
		goto failed;
	/* len += sizeof(struct acxp80211_hdr) + 2; */
#endif

	if (OK != acx_init_max_beacon_template(priv))
		goto failed;
	/* len += sizeof(struct acxp80211_beacon_prb_resp_template); */

	/* TODO: beautify code by moving init_tim down just before
	 * set_tim */
	if (OK != acx_init_max_tim_template(priv))
		goto failed;
	/* len += sizeof(struct acx_tim); */

	if (OK != acx_init_max_probe_response_template(priv))
		goto failed;
	/* len += sizeof(struct acxp80211_hdr) + 2; */

	if (OK != acx_set_tim_template(priv))
		goto failed;

	if (OK != acx_interrogate(priv, mm, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD | L_INIT, "memmap req FAILED\n");
		goto failed;
	}

	mm->QueueStart = cpu_to_le32(le32_to_cpu(mm->PacketTemplateEnd) + 4);
	if (OK != acx_configure(priv, mm, ACX1xx_IE_MEMORY_MAP)) {
		acxlog(L_STD | L_INIT, "memmap cfg FAILED\n");
		goto failed;
	}

	result = OK;
	goto success;

failed:
	acxlog(L_BINDEBUG | L_INIT,
		/* "cb =0x%X\n" */
		"pACXMemoryMap->CodeStart= 0x%X\n"
		"pACXMemoryMap->CodeEnd = 0x%X\n"
		"pACXMemoryMap->WEPCacheStart= 0x%X\n"
		"pACXMemoryMap->WEPCacheEnd = 0x%X\n"
		"pACXMemoryMap->PacketTemplateStart= 0x%X\n"
		"pACXMemoryMap->PacketTemplateEnd = 0x%X\n",
		/* len, */
		le32_to_cpu(mm->CodeStart),
		le32_to_cpu(mm->CodeEnd),
		le32_to_cpu(mm->WEPCacheStart),
		le32_to_cpu(mm->WEPCacheEnd),
		le32_to_cpu(mm->PacketTemplateStart),
		le32_to_cpu(mm->PacketTemplateEnd));

success:
	FN_EXIT1(result);
	return result;
}

static int acx111_init_packet_templates(wlandevice_t *priv)
{
	int result = NOT_OK;

	FN_ENTER;

	acxlog(L_BINDEBUG | L_INIT, "%s: Init max packet templates\n", __func__);

	/* FIXME: we only init it but don't set it yet!
	 * Could this cause problems??
	 * (such as non-working beacon Tx!?!?) */
	if (OK != acx_init_max_probe_request_template(priv))
		goto failed;

	if (OK != acx_init_max_null_data_template(priv))
		goto failed;

	if (OK != acx_init_max_beacon_template(priv))
		goto failed;

	if (OK != acx_init_max_tim_template(priv))
		goto failed;

	if (OK != acx_init_max_probe_response_template(priv))
		goto failed;

	/* the other templates will be set later (acx_start) */
	/*
	if (OK != acx_set_tim_template(priv))
		goto failed;*/

	result = OK;
	goto success;

failed:
	acxlog(L_STD | L_INIT, "%s: packet template configuration FAILED\n", __func__);

success:
	FN_EXIT1(result);
	return result;
}

#if UNUSED
/*----------------------------------------------------------------
* acx_set_probe_request_template
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/
/*
 * STATUS: ok.
 */
static void acx100_set_probe_request_template(wlandevice_t *priv)
{
	struct acx100_template_probereq probereq;
	const char *p;
	unsigned int frame_len;
	
	FN_ENTER;

	memset(&probereq, 0, sizeof(probereq));

	probereq.fc = cpu_to_le16(WF_FTYPE_MGMT | WF_FSTYPE_PROBEREQ);
	MAC_BCAST(probereq.da);
	MAC_COPY(probereq.sa, priv->dev_addr);
	MAC_BCAST(probereq.bssid);

	probereq.beacon_interval = cpu_to_le16(priv->beacon_interval);
	acx_update_capabilities(priv);
	probereq.cap = cpu_to_le16(priv->capabilities);

	p = probereq.variable;
	acxlog(L_ASSOC, "SSID = %s, len = %i\n", priv->essid, priv->essid_len);
	p = wlan_fill_ie_ssid(p, priv->essid_len, priv->essid);
	p = wlan_fill_ie_rates(p, priv->rate_supported_len, priv->rate_supported);
	/* FIXME: should these be here or AFTER ds_parms? */
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);
	/* HUH?? who said it must be here? I've found nothing in 802.11! --vda*/
	p = wlan_fill_ie_ds_parms(p, priv->channel);
	frame_len = p - (char*)&probereq;
	probereq.size = frame_len - 2;

	acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_PROBE_REQUEST, &probereq, frame_len, ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT0();
}

static void acx111_set_probe_request_template(wlandevice_t *priv)
{
	struct acx111_template_probereq probereq;
	const char *p;
	int frame_len;
	
	FN_ENTER;

	memset(&probereq, 0, sizeof(probereq));

	probereq.fc = cpu_to_le16(WF_FTYPE_MGMT | WF_FSTYPE_PROBEREQ);
	MAC_BCAST(probereq.da);
	MAC_COPY(probereq.sa, priv->dev_addr);
	MAC_BCAST(probereq.bssid);

	p = probereq.variable;
	p = wlan_fill_ie_ssid(p, priv->essid_len, priv->essid);
	p = wlan_fill_ie_rates(p, priv->rate_supported_len, priv->rate_supported);
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);
	frame_len = p - (char*)&probereq;
	probereq.size = frame_len - 2;

	acx_issue_cmd(priv, ACX1xx_CMD_CONFIG_PROBE_REQUEST, &probereq, frame_len, ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT0();
}
#endif /* UNUSED */


/* FIXME: this should be solved in a general way for all radio types
 * by decoding the radio firmware module,
 * since it probably has some standard structure describing how to
 * set the power level of the radio module which it controls.
 * Or maybe not, since the radio module probably has a function interface
 * instead which then manages Tx level programming :-\
 */
static inline int acx100_set_tx_level(wlandevice_t *priv, u8 level_dbm)
{
	/* since it can be assumed that at least the Maxim radio has a
	 * maximum power output of 20dBm and since it also can be
	 * assumed that these values drive the DAC responsible for
	 * setting the linear Tx level, I'd guess that these values
	 * should be the corresponding linear values for a dBm value,
	 * in other words: calculate the values from that formula:
	 * Y [dBm] = 10 * log (X [mW])
	 * then scale the 0..63 value range onto the 1..100mW range (0..20 dBm)
	 * and you're done...
	 * Hopefully that's ok, but you never know if we're actually
	 * right... (especially since Windows XP doesn't seem to show
	 * actual Tx dBm values :-P) */
#if (WLAN_HOSTIF!=WLAN_USB)
	/* NOTE: on Maxim, value 30 IS 30mW, and value 10 IS 10mW - so the
	 * values are EXACTLY mW!!! Not sure about RFMD and others,
	 * though... */
	static const u8 dbm2val_maxim[21] = {
		63, 63, 63, 62,
		61, 61, 60, 60,
		59, 58, 57, 55,
		53, 50, 47, 43,
		38, 31, 23, 13,
		0
	};
	static const u8 dbm2val_rfmd[21] = {
		 0,  0,  0,  1,
		 2,  2,  3,  3,
		 4,  5,  6,  8,
		10, 13, 16, 20,
		25, 32, 41, 50,
		63
	};
        const u8 *table; 
	
	switch (priv->radio_type) {
		case RADIO_MAXIM_0D:
			table = &dbm2val_maxim[0];
			break;
		case RADIO_RFMD_11:
		case RADIO_RALINK_15:
			table = &dbm2val_rfmd[0];
			break;
		default:
			acxlog(L_STD, "FIXME: unknown/unsupported radio type, cannot modify Tx power level yet!\n");
			return NOT_OK;
	}
	acxlog(L_STD, "changing radio power level to %u dBm (%u)\n", level_dbm, table[level_dbm]);
	acx_write_phy_reg(priv, 0x11, table[level_dbm]);
#endif
	return OK;
}

static inline int acx111_set_tx_level(wlandevice_t *priv, u8 level_dbm)
{
	struct ACX111TxLevel tx_level;

	/* my acx111 card has two power levels in its configoptions (== EEPROM):
	 * 1 (30mW) [15dBm]
	 * 2 (10mW) [10dBm]
	 * For now, just assume all other acx111 cards have the same.
	 * Ideally we would query it here, but we first need a
	 * standard way to query individual configoptions easily. */
	if (level_dbm <= 12) {
		tx_level.level = 2; /* 10 dBm */
		priv->tx_level_dbm = 10;
	} else {
		tx_level.level = 1; /* 15 dBm */
		priv->tx_level_dbm = 15;
	}
	if (level_dbm != priv->tx_level_dbm)
		acxlog(L_INIT, "ACX111 firmware has specific power levels only: adjusted %d dBm to %d dBm!\n", level_dbm, priv->tx_level_dbm);

	if (OK != acx_configure(priv, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL)) {
		acxlog(L_INIT, "Error setting acx111 tx level\n");
		return NOT_OK;
	}
	return OK;
}

/* Returns the current tx level (ACX111) */
static inline u8 acx111_get_tx_level(wlandevice_t *priv)
{
	struct ACX111TxLevel tx_level;

	tx_level.level = 0;
	if (OK != acx_interrogate(priv, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL)) {
		acxlog(L_INIT, "Error getting acx111 tx level\n");
	}
	return tx_level.level;
}

int acx111_get_feature_config(wlandevice_t *priv, u32 *feature_options, u32 *data_flow_options)
{
	struct ACX111FeatureConfig fc;

	if(priv->chip_type != CHIPTYPE_ACX111) {
		return NOT_OK;
	}

	memset(&fc, 0, sizeof(struct ACX111FeatureConfig));

	if (OK != acx_interrogate(priv, &fc, ACX1xx_IE_FEATURE_CONFIG)) {
		acxlog(L_INIT, "Error reading acx111 feature config\n");
		return NOT_OK;
	}
	acxlog(L_DEBUG,
		"Got Feature option: 0x%X\n"
		"Got DataFlow option: 0x%X\n",
		fc.feature_options,
		fc.data_flow_options);

	if (feature_options)
		*feature_options = le32_to_cpu(fc.feature_options);
	if (data_flow_options)
		*data_flow_options = le32_to_cpu(fc.data_flow_options);

	return OK;
}

int acx111_set_feature_config(wlandevice_t *priv, u32 feature_options, u32 data_flow_options, unsigned int mode /* 0 == remove, 1 == add, 2 == set */)
{
	struct ACX111FeatureConfig fc;

	if(priv->chip_type != CHIPTYPE_ACX111) {
		return NOT_OK;
	}

	if ((mode < 0) || (mode > 2))
		return NOT_OK;

	if (mode != 2)
		/* need to modify old data */
		acx111_get_feature_config(priv, &fc.feature_options, &fc.data_flow_options);
	else {
		/* need to set a completely new value */
		fc.feature_options = 0;
		fc.data_flow_options = 0;
	}
		
	if (mode == 0) { /* remove */
		CLEAR_BIT(fc.feature_options, cpu_to_le32(feature_options));
		CLEAR_BIT(fc.data_flow_options, cpu_to_le32(data_flow_options));
	} else { /* add or set */
		SET_BIT(fc.feature_options, cpu_to_le32(feature_options));
		SET_BIT(fc.data_flow_options, cpu_to_le32(data_flow_options));
	}

	acxlog(L_DEBUG,
		"input is feature 0x%X dataflow 0x%X mode %u: setting feature 0x%X dataflow 0x%X\n", feature_options, data_flow_options, mode, le32_to_cpu(fc.feature_options), le32_to_cpu(fc.data_flow_options));

	if (OK != acx_configure(priv, &fc, ACX1xx_IE_FEATURE_CONFIG)) {
		acxlog(L_INIT, "Error setting feature config\n");
		return NOT_OK;
	}

	return OK;
}

int acx_recalib_radio(wlandevice_t *priv)
{
	if (CHIPTYPE_ACX111 == priv->chip_type) {
		acx111_cmd_radiocalib_t cal;
	
		acxlog(L_STD, "recalibrating ACX111 radio. Not tested yet, please report status!!\n");
		cal.methods = cpu_to_le32(0x8000000f); /* automatic recalibration, choose all methods */
		cal.interval = cpu_to_le32(58594); /* automatic recalibration every 60 seconds (value in TUs) FIXME: what is the firmware default here?? */
	
		return acx_issue_cmd(priv, ACX111_CMD_RADIOCALIB, &cal, sizeof(cal), ACX_CMD_TIMEOUT_DEFAULT);
	} else {
		if (/* (OK == acx_issue_cmd(priv, ACX1xx_CMD_DISABLE_TX, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT)) &&
		    (OK == acx_issue_cmd(priv, ACX1xx_CMD_DISABLE_RX, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT)) && */
		    (OK == acx_issue_cmd(priv, ACX1xx_CMD_ENABLE_TX, &(priv->channel), 0x1, ACX_CMD_TIMEOUT_DEFAULT)) &&
		    (OK == acx_issue_cmd(priv, ACX1xx_CMD_ENABLE_RX, &(priv->channel), 0x1, ACX_CMD_TIMEOUT_DEFAULT)) )
			return OK;
		return NOT_OK;
	}
}

/*----------------------------------------------------------------
* acx_cmd_start_scan
* STATUS: should be ok, but struct not classified yet.
*
* Issue scan command to the hardware
*----------------------------------------------------------------*/
static void acx100_scan_chan_p(wlandevice_t *priv, acx100_scan_t *s)
{
	FN_ENTER;
	acx_issue_cmd(priv, ACX1xx_CMD_SCAN, s, sizeof(acx100_scan_t), ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT0();
}

static void acx111_scan_chan_p(wlandevice_t *priv, acx111_scan_t *s)
{
	FN_ENTER;
	acx_issue_cmd(priv, ACX1xx_CMD_SCAN, s, sizeof(acx111_scan_t), ACX_CMD_TIMEOUT_DEFAULT);
	FN_EXIT0();
}

static void acx100_scan_chan(wlandevice_t *priv)
{
	acx100_scan_t s;

	FN_ENTER;
	acxlog(L_INIT, "Starting radio scan\n");

	/* now that we're starting a new scan, reset the number of stations
	 * found in range back to 0.
	 * (not doing so would keep outdated stations in our list,
	 * and if we decide to associate to "any" station, then we'll always
	 * pick an outdated one)
	 * NOTE: we don't do this any more, instead we should probably
	 * go towards a mechanism that kicks outdated entries */
	/* priv->bss_table_count = 0; */
	memset(&s, 0, sizeof(acx100_scan_t));
	s.count = cpu_to_le16(priv->scan_count);
	s.start_chan = cpu_to_le16(1);
	s.flags = cpu_to_le16(0x8000);
	s.max_rate = priv->scan_rate;
	s.options = priv->scan_mode;

	s.chan_duration = cpu_to_le16(priv->scan_duration);
	s.max_probe_delay = cpu_to_le16(priv->scan_probe_delay);

	acx100_scan_chan_p(priv, &s);
	FN_EXIT0();
}

static void acx111_scan_chan(wlandevice_t *priv)
{
	acx111_scan_t s;

	FN_ENTER;
	acxlog(L_INIT, "Starting radio scan\n");

	memset(&s, 0, sizeof(acx111_scan_t));
	s.count = cpu_to_le16(priv->scan_count);
	s.channel_list_select = 0; /* scan every allowed channel */
	/*s.channel_list_select = 1;*/ /* scan given channels */

	s.rate = priv->scan_rate;
	s.options = priv->scan_mode;

	s.chan_duration = cpu_to_le16(priv->scan_duration);
	s.max_probe_delay = cpu_to_le16(priv->scan_probe_delay);
	/*s.modulation = 0x40;*/ /* long preamble ? OFDM ? -> only for active scan */
	s.modulation = 0;
	/*s.channel_list[0] = 6;
	s.channel_list[1] = 4;*/

	acx111_scan_chan_p(priv, &s);
	FN_EXIT0();
}

void acx_cmd_start_scan(wlandevice_t *priv)
{
	if(priv->chip_type == CHIPTYPE_ACX100) {
		acx100_scan_chan(priv);
	} else {
		acx111_scan_chan(priv);
	}
}

/*----------------------------------------------------------------
* acx_update_card_settings
*
* Applies accumulated changes in various priv->xxxx members
* Called by ioctl commit handler, acx_start, acx_set_defaults,
* acx_after_interrupt_task (if IRQ_CMD_UPDATE_CARD_CFG),
* acx_set_status (FIXME: acx_set_status shouldn't do that).
* init = 1 if called from acx_start or acx_set_defaults
*----------------------------------------------------------------*/
static void sens_radio_16_17(wlandevice_t *priv)
{
	u32 feature1,feature2;
	if ((priv->sensitivity < 1) || (priv->sensitivity > 3)) {
		acxlog(L_STD, "invalid sensitivity setting (1..3), setting to 1\n");
		priv->sensitivity = 1;
	}
	acx111_get_feature_config(priv, &feature1, &feature2);
	CLEAR_BIT(feature1, FEATURE1_LOW_RX|FEATURE1_EXTRA_LOW_RX);
	if (priv->sensitivity > 1)
		SET_BIT(feature1, FEATURE1_LOW_RX);
	if (priv->sensitivity > 2)
		SET_BIT(feature1, FEATURE1_EXTRA_LOW_RX);
	acx111_feature_set(priv, feature1, feature2);
}

void acx_update_card_settings(wlandevice_t *priv, int init, int get_all, int set_all)
{
#ifdef BROKEN_LOCKING
	unsigned long flags;
#endif
	unsigned int start_scan = 0;

	FN_ENTER;

#ifdef BROKEN_LOCKING
	if (init) {
		/* cannot use acx_lock() - hw_unavailable is set */
		local_irq_save(flags);
		if (!spin_trylock(&priv->lock)) {
			printk(KERN_EMERG "ARGH! Lock already taken in %s:%d\n", __FILE__, __LINE__);
			local_irq_restore(flags);
			FN_EXIT0();
			return;
		} else {
			printk("Lock taken in %s\n", __func__);
		}
	} else {
		if (acx_lock(priv, &flags)) {
			FN_EXIT0();
			return;
		}
	}
#endif

	if ((0 == init) && (0 == (ACX_STATE_IFACE_UP & priv->dev_state_mask))) {
		acxlog(L_DEBUG, "iface not up, won't update card settings yet!\n");
		goto end;
	}

	if (get_all)
		SET_BIT(priv->get_mask, GETSET_ALL);
	if (set_all)
		SET_BIT(priv->set_mask, GETSET_ALL);
	/* Why not just set masks to 0xffffffff? We can get rid of GETSET_ALL */

	acxlog(L_INIT, "get_mask 0x%08x, set_mask 0x%08x\n",
			priv->get_mask, priv->set_mask);

	if (priv->set_mask & (GETSET_MODE|GETSET_RESCAN|GETSET_WEP)) {
		acxlog(L_INIT, "important setting has been changed --> need to update packet templates, too\n");
		SET_BIT(priv->set_mask, SET_TEMPLATES);
	}

#ifdef WHY_SHOULD_WE_BOTHER /* imagine we were just powered off */
	/* send a disassoc request in case it's required */
	if (priv->set_mask & (GETSET_MODE|GETSET_RESCAN|GETSET_CHANNEL|GETSET_WEP|GETSET_ALL)) {
		if (ACX_MODE_2_STA == priv->mode) {
			if (ACX_STATUS_4_ASSOCIATED == priv->status) {
				acxlog(L_ASSOC, "we were ASSOCIATED - sending disassoc request\n");
				acx_transmit_disassoc(NULL, priv);
				/* FIXME: deauth? */
			}
			/* need to reset some other stuff as well */
			acxlog(L_DEBUG, "resetting bssid\n");
			MAC_ZERO(priv->bssid);
			SET_BIT(priv->set_mask, SET_TEMPLATES|SET_STA_LIST);
			/* FIXME: should start scanning */
			start_scan = 1;
		}
	}
#endif

	if (priv->get_mask & (GETSET_STATION_ID|GETSET_ALL)) {
		u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
		const u8 *paddr;
		unsigned int u;

		acx_interrogate(priv, &stationID, ACX1xx_IE_DOT11_STATION_ID);
		paddr = &stationID[4];
		for (u = 0; u < ETH_ALEN; u++) {
			/* we copy the MAC address (reversed in
			 * the card) to the netdevice's MAC
			 * address, and on ifup it will be
			 * copied into iwpriv->dev_addr */
			priv->netdev->dev_addr[ETH_ALEN - 1 - u] = paddr[u];
		}
		CLEAR_BIT(priv->get_mask, GETSET_STATION_ID);
	}

	if (priv->get_mask & (GETSET_SENSITIVITY|GETSET_ALL)) {
		if ((RADIO_RFMD_11 == priv->radio_type)
		|| (RADIO_MAXIM_0D == priv->radio_type)
		|| (RADIO_RALINK_15 == priv->radio_type)) {
			acx_read_phy_reg(priv, 0x30, &priv->sensitivity);
		} else {
			acxlog(L_STD, "Don't know how to get sensitivity for radio type 0x%02x, please try to add that!\n", priv->radio_type);
			priv->sensitivity = 0;
		}
		acxlog(L_INIT, "Got sensitivity value %u\n", priv->sensitivity);

		CLEAR_BIT(priv->get_mask, GETSET_SENSITIVITY);
	}

	if (priv->get_mask & (GETSET_ANTENNA|GETSET_ALL)) {
		u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

		memset(antenna, 0, sizeof(antenna));
		acx_interrogate(priv, antenna, ACX1xx_IE_DOT11_CURRENT_ANTENNA);
		priv->antenna = antenna[4];
		acxlog(L_INIT, "Got antenna value 0x%02X\n", priv->antenna);
		CLEAR_BIT(priv->get_mask, GETSET_ANTENNA);
	}

	if (priv->get_mask & (GETSET_ED_THRESH|GETSET_ALL)) {
		if (priv->chip_type == CHIPTYPE_ACX100)	{
			u8 ed_threshold[4 + ACX1xx_IE_DOT11_ED_THRESHOLD_LEN];

			memset(ed_threshold, 0, sizeof(ed_threshold));
			acx_interrogate(priv, ed_threshold, ACX1xx_IE_DOT11_ED_THRESHOLD);
			priv->ed_threshold = ed_threshold[4];
		} else {
			acxlog(L_INIT, "acx111 doesn't support ED\n");
			priv->ed_threshold = 0;
		}
		acxlog(L_INIT, "Got Energy Detect (ED) threshold %u\n", priv->ed_threshold);
		CLEAR_BIT(priv->get_mask, GETSET_ED_THRESH);
	}

	if (priv->get_mask & (GETSET_CCA|GETSET_ALL)) {
		if (priv->chip_type == CHIPTYPE_ACX100)	{
			u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];

			memset(cca, 0, sizeof(priv->cca));
			acx_interrogate(priv, cca, ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
			priv->cca = cca[4];
		} else {
			acxlog(L_INIT, "acx111 doesn't support CCA\n");
			priv->cca = 0;
		}
		acxlog(L_INIT, "Got Channel Clear Assessment (CCA) value %u\n", priv->cca);
		CLEAR_BIT(priv->get_mask, GETSET_CCA);
	}

	if (priv->get_mask & (GETSET_REG_DOMAIN|GETSET_ALL)) {
		acx_ie_generic_t dom;

		acx_interrogate(priv, &dom, ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
		priv->reg_dom_id = dom.m.gp.bytes[0];
		/* FIXME: should also set chanmask somehow */
		acxlog(L_INIT, "Got regulatory domain 0x%02X\n", priv->reg_dom_id);
		CLEAR_BIT(priv->get_mask, GETSET_REG_DOMAIN);
	}

	if (priv->set_mask & (GETSET_STATION_ID|GETSET_ALL)) {
		u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
		u8 *paddr;
		unsigned int u;

		paddr = &stationID[4];
		for (u = 0; u < ETH_ALEN; u++) {
			/* copy the MAC address we obtained when we noticed
			 * that the ethernet iface's MAC changed 
			 * to the card (reversed in
			 * the card!) */
			paddr[u] = priv->dev_addr[ETH_ALEN - 1 - u];
		}
		acx_configure(priv, &stationID, ACX1xx_IE_DOT11_STATION_ID);
		CLEAR_BIT(priv->set_mask, GETSET_STATION_ID);
	}

	if (priv->set_mask & (SET_TEMPLATES|GETSET_ALL)) {
		switch(priv->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_3_AP:
			/* FIXME: why only for AP? STA need probe req templates... */
			acxlog(L_INIT, "Updating packet templates\n");
			if (OK != acx_set_beacon_template(priv))
				acxlog(L_STD, "acx_set_beacon_template FAILED\n");
			if (OK != acx_set_tim_template(priv))
				acxlog(L_STD, "acx_set_tim_template FAILED\n");
			/* BTW acx111 firmware would not send probe responses
			** if probe request does not have all basic rates flagged
			** by 0x80! Thus firmware does not conform to 802.11,
			** it should ignore 0x80 bit in ratevector from STA.
			** We can 'fix' it by not using this template and
			** sending probe responses by hand. TODO --vda */
			if (OK != acx_set_probe_response_template(priv))
				acxlog(L_STD, "acx_set_probe_response_template FAILED\n");
		}
		CLEAR_BIT(priv->set_mask, SET_TEMPLATES);
	}
	if (priv->set_mask & (SET_STA_LIST|GETSET_ALL)) {
		/* TODO insert a sweet if here */
		acx_sta_list_init(priv);
		CLEAR_BIT(priv->set_mask, SET_STA_LIST);
	}
	if (priv->set_mask & (SET_RATE_FALLBACK|GETSET_ALL)) {
		u8 rate[4 + ACX1xx_IE_RATE_FALLBACK_LEN];

		/* configure to not do fallbacks when not in auto rate mode */
		rate[4] = (priv->rate_auto) ? /* priv->txrate_fallback_retries */ 1 : 0;
		acxlog(L_INIT, "Updating Tx fallback to %u retries\n", rate[4]);
		acx_configure(priv, &rate, ACX1xx_IE_RATE_FALLBACK);
		CLEAR_BIT(priv->set_mask, SET_RATE_FALLBACK);
	}
	if (priv->set_mask & (GETSET_TXPOWER|GETSET_ALL)) {
		acxlog(L_INIT, "Updating transmit power: %u dBm\n",
					priv->tx_level_dbm);
		if (priv->chip_type == CHIPTYPE_ACX111) {
			acx111_set_tx_level(priv, priv->tx_level_dbm);
		} else if (priv->chip_type == CHIPTYPE_ACX100) {
			acx100_set_tx_level(priv, priv->tx_level_dbm);
		}
		CLEAR_BIT(priv->set_mask, GETSET_TXPOWER);
	}

	if (priv->set_mask & (GETSET_SENSITIVITY|GETSET_ALL)) {
		acxlog(L_INIT, "Updating sensitivity value: %u\n",
					priv->sensitivity);
		switch (priv->radio_type) {
		case RADIO_RFMD_11:
		case RADIO_MAXIM_0D:
		case RADIO_RALINK_15:
			acx_write_phy_reg(priv, 0x30, priv->sensitivity);
			break;
		case RADIO_RADIA_16:
		case RADIO_UNKNOWN_17:
			sens_radio_16_17(priv);
			break;
		default:
			acxlog(L_STD, "Don't know how to modify sensitivity "
				"for radio type 0x%02x, please try to add that!\n",
				priv->radio_type);
		}
		CLEAR_BIT(priv->set_mask, GETSET_SENSITIVITY);
	}

	if (priv->set_mask & (GETSET_ANTENNA|GETSET_ALL)) {
		/* antenna */
		u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

		memset(antenna, 0, sizeof(antenna));
		antenna[4] = priv->antenna;
		acxlog(L_INIT, "Updating antenna value: 0x%02X\n",
					priv->antenna);
		acx_configure(priv, &antenna, ACX1xx_IE_DOT11_CURRENT_ANTENNA);
		CLEAR_BIT(priv->set_mask, GETSET_ANTENNA);
	}

	if (priv->set_mask & (GETSET_ED_THRESH|GETSET_ALL)) {
		/* ed_threshold */
		acxlog(L_INIT, "Updating Energy Detect (ED) threshold: %u\n",
					priv->ed_threshold);
		if (CHIPTYPE_ACX100 == priv->chip_type) {
			u8 ed_threshold[4 + ACX1xx_IE_DOT11_ED_THRESHOLD_LEN];

			memset(ed_threshold, 0, sizeof(ed_threshold));
			ed_threshold[4] = priv->ed_threshold;
			acx_configure(priv, &ed_threshold, ACX1xx_IE_DOT11_ED_THRESHOLD);
		}
		else
			acxlog(L_INIT, "ACX111 doesn't support ED!\n");
		CLEAR_BIT(priv->set_mask, GETSET_ED_THRESH);
	}

	if (priv->set_mask & (GETSET_CCA|GETSET_ALL)) {
		/* CCA value */
		acxlog(L_INIT, "Updating Channel Clear Assessment (CCA) value: 0x%02X\n", priv->cca);
		if (CHIPTYPE_ACX100 == priv->chip_type)	{
			u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];

			memset(cca, 0, sizeof(cca));
			cca[4] = priv->cca;
			acx_configure(priv, &cca, ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
		}
		else
			acxlog(L_INIT, "ACX111 doesn't support CCA!\n");
		CLEAR_BIT(priv->set_mask, GETSET_CCA);
	}

	if (priv->set_mask & (GETSET_LED_POWER|GETSET_ALL)) {
		/* Enable Tx */
		acxlog(L_INIT, "Updating power LED status: %u\n", priv->led_power);
		acx_power_led(priv, priv->led_power);
		CLEAR_BIT(priv->set_mask, GETSET_LED_POWER);
	}

/* this seems to cause Tx lockup after some random time (Tx error 0x20),
 * so let's disable it for now until further investigation */
#if POWER_SAVE_80211
	if (priv->set_mask & (GETSET_POWER_80211|GETSET_ALL)) {
		acx100_ie_powermgmt_t pm;

		/* change 802.11 power save mode settings */
		acxlog(L_INIT, "Updating 802.11 power save mode settings: wakeup_cfg 0x%02x, listen interval %u, options 0x%02x, hangover period %u, enhanced_ps_transition_time %d\n", priv->ps_wakeup_cfg, priv->ps_listen_interval, priv->ps_options, priv->ps_hangover_period, priv->ps_enhanced_transition_time);
		acx_interrogate(priv, &pm, ACX100_IE_POWER_MGMT);
		acxlog(L_INIT, "Previous PS mode settings: wakeup_cfg 0x%02x, listen interval %u, options 0x%02x, hangover period %u, enhanced_ps_transition_time %d\n", pm.wakeup_cfg, pm.listen_interval, pm.options, pm.hangover_period, pm.enhanced_ps_transition_time);
		pm.wakeup_cfg = priv->ps_wakeup_cfg;
		pm.listen_interval = priv->ps_listen_interval;
		pm.options = priv->ps_options;
		pm.hangover_period = priv->ps_hangover_period;
		pm.enhanced_ps_transition_time = cpu_to_le16(priv->ps_enhanced_transition_time);
		acx_configure(priv, &pm, ACX100_IE_POWER_MGMT);
		acx_interrogate(priv, &pm, ACX100_IE_POWER_MGMT);
		acxlog(L_INIT, "wakeup_cfg: 0x%02x\n", pm.wakeup_cfg);
		acx_schedule(HZ / 25);
		acx_interrogate(priv, &pm, ACX100_IE_POWER_MGMT);
		acxlog(L_INIT, "power save mode change %s\n", (pm.wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
		/* FIXME: maybe verify via PS_CFG_PENDING bit here
		 * that power save mode change was successful. */
		/* FIXME: we shouldn't trigger a scan immediately after
		 * fiddling with power save mode (since the firmware is sending
		 * a NULL frame then). Does this need locking?? */
		CLEAR_BIT(priv->set_mask, GETSET_POWER_80211);
	}
#endif

	if (priv->set_mask & (GETSET_CHANNEL|GETSET_ALL)) {
		/* channel */
		acxlog(L_INIT, "Updating channel: %u\n", priv->channel);
		switch (priv->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:
			start_scan = 1;
		}
		/* This will actually tune RX/TX to the channel */
		SET_BIT(priv->set_mask, GETSET_RX|GETSET_TX);
		CLEAR_BIT(priv->set_mask, GETSET_CHANNEL);
	}
	
	if (priv->set_mask & (GETSET_TX|GETSET_ALL)) {
		/* set Tx */
		acxlog(L_INIT, "Updating: %s Tx\n", priv->tx_disabled ? "disable" : "enable");
		if (priv->tx_disabled)
			acx_issue_cmd(priv, ACX1xx_CMD_DISABLE_TX, NULL, 0x0 /* FIXME: this used to be 0x1, but since we don't transfer a parameter... */, ACX_CMD_TIMEOUT_DEFAULT);
		else
			acx_issue_cmd(priv, ACX1xx_CMD_ENABLE_TX, &(priv->channel), 0x1, ACX_CMD_TIMEOUT_DEFAULT);
		CLEAR_BIT(priv->set_mask, GETSET_TX);
	}

	if (priv->set_mask & (GETSET_RX|GETSET_ALL)) {
		/* Enable Rx */
		acxlog(L_INIT, "Updating: enable Rx on channel: %u\n", priv->channel);
		acx_issue_cmd(priv, ACX1xx_CMD_ENABLE_RX, &(priv->channel), 0x1, ACX_CMD_TIMEOUT_DEFAULT); 
		CLEAR_BIT(priv->set_mask, GETSET_RX);
	}

	if (priv->set_mask & (GETSET_RETRY|GETSET_ALL)) {
		u8 short_retry[4 + ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN];
		u8 long_retry[4 + ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN];

		acxlog(L_INIT, "Updating short retry limit: %u, long retry limit: %u\n",
					priv->short_retry, priv->long_retry);
		short_retry[0x4] = priv->short_retry;
		long_retry[0x4] = priv->long_retry;
		acx_configure(priv, &short_retry, ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT);
		acx_configure(priv, &long_retry, ACX1xx_IE_DOT11_LONG_RETRY_LIMIT);
		CLEAR_BIT(priv->set_mask, GETSET_RETRY);
	}

	if (priv->set_mask & (SET_MSDU_LIFETIME|GETSET_ALL)) {
		u8 xmt_msdu_lifetime[4 + ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN];

		acxlog(L_INIT, "Updating Tx MSDU lifetime: %u\n",
					priv->msdu_lifetime);
		*(u32 *)&xmt_msdu_lifetime[4] = cpu_to_le32((u32)priv->msdu_lifetime);
		acx_configure(priv, &xmt_msdu_lifetime, ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME);
		CLEAR_BIT(priv->set_mask, SET_MSDU_LIFETIME);
	}

	if (priv->set_mask & (GETSET_REG_DOMAIN|GETSET_ALL)) {
		/* reg_domain */
		acx_ie_generic_t dom;
		unsigned int u;

		acxlog(L_INIT, "Updating regulatory domain: 0x%02X\n",
					priv->reg_dom_id);
		for (u = 0; u < sizeof(reg_domain_ids); u++)
			if (reg_domain_ids[u] == priv->reg_dom_id)
				break;

		if (sizeof(reg_domain_ids) == u) {
			acxlog(L_STD, "Invalid or unsupported regulatory domain 0x%02X specified, falling back to FCC (USA)! Please report if this sounds fishy!\n", priv->reg_dom_id);
			u = 0;
			priv->reg_dom_id = reg_domain_ids[u];
		}

		priv->reg_dom_chanmask = reg_domain_channel_masks[u];
		dom.m.gp.bytes[0] = priv->reg_dom_id;
		acx_configure(priv, &dom, ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
		if (0 == (priv->reg_dom_chanmask & (1 << (priv->channel - 1) ) ))
		{ /* hmm, need to adjust our channel setting to reside within our
		domain */
			for (u = 1; u <= 14; u++)
				if (priv->reg_dom_chanmask & (1 << (u - 1)) ) {
					acxlog(L_STD, "adjusting selected channel from %u to %u due to new regulatory domain.\n", priv->channel, u);
					priv->channel = u;
					break;
				}
		}
		CLEAR_BIT(priv->set_mask, GETSET_REG_DOMAIN);
	}

	if (priv->set_mask & (GETSET_MODE|GETSET_ALL)) {
		switch (priv->mode) {
		case ACX_MODE_3_AP:
			acx_sta_list_init(priv);
			priv->aid = 0;
			priv->ap_client = NULL;
			acx111_feature_off(priv, 0, FEATURE2_NO_TXCRYPT|FEATURE2_SNIFFER);
			MAC_COPY(priv->bssid, priv->dev_addr);
			/* start sending beacons */
			acx_cmd_join_bssid(priv, priv->bssid);
			/* this basically says "we're connected" */
			acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);
			break;
		case ACX_MODE_MONITOR:
			acx111_feature_on(priv, 0, FEATURE2_NO_TXCRYPT|FEATURE2_SNIFFER);
			/* TODO: what exactly do we want here? */
			/* priv->netdev->type = ARPHRD_ETHER; */
			/* priv->netdev->type = ARPHRD_IEEE80211; */
			priv->netdev->type = ARPHRD_IEEE80211_PRISM;
			/* this stops beacons (invalid macmode...) */
			acx_cmd_join_bssid(priv, priv->bssid);
			/* this basically says "we're connected" */
			acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);
			SET_BIT(priv->set_mask, SET_RXCONFIG|SET_WEP_OPTIONS);
			break;
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:
			acx111_feature_off(priv, 0, FEATURE2_NO_TXCRYPT|FEATURE2_SNIFFER);
			priv->aid = 0;
			priv->ap_client = NULL;
			/* we want to start looking for peer or AP */
			start_scan = 1;
			break;
		case ACX_MODE_OFF:
			/* TODO: disable RX/TX, stop any scanning activity etc: */
			/* priv->tx_disabled = 1; */
			/* SET_BIT(priv->set_mask, GETSET_RX|GETSET_TX); */

			/* This stops beacons (invalid macmode...) */
			acx_cmd_join_bssid(priv, priv->bssid);
			acx_set_status(priv, ACX_STATUS_0_STOPPED);
			break;
		}
		CLEAR_BIT(priv->set_mask, GETSET_MODE);
	}

	if (priv->set_mask & (SET_RXCONFIG|GETSET_ALL)) {
		acx_initialize_rx_config(priv);
		CLEAR_BIT(priv->set_mask, SET_RXCONFIG);
	}

	if (priv->set_mask & (GETSET_RESCAN|GETSET_ALL)) {
		switch (priv->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:
			start_scan = 1;
			break;
		}
		CLEAR_BIT(priv->set_mask, GETSET_RESCAN);
	}

	if (priv->set_mask & (GETSET_WEP|GETSET_ALL)) {
		/* encode */

		ie_dot11WEPDefaultKeyID_t dkey;
#if DEBUG_WEP
		struct {
			u16 type ACX_PACKED;
			u16 len ACX_PACKED;
			u8  val ACX_PACKED;
		} keyindic;
#endif
		acxlog(L_INIT, "Updating WEP key settings\n");

		if (priv->chip_type == CHIPTYPE_ACX111) {
			acx111_set_wepkey(priv);
		} else {
			acx100_set_wepkey(priv);
		}

		dkey.KeyID = priv->wep_current_index;
		acxlog(L_INIT, "Setting WEP key %u as default.\n", dkey.KeyID);
		acx_configure(priv, &dkey, ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);
#if DEBUG_WEP
		keyindic.val = 3;
		acx_configure(priv, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif
		start_scan = 1;
		CLEAR_BIT(priv->set_mask, GETSET_WEP);
	}

	if (priv->set_mask & (SET_WEP_OPTIONS|GETSET_ALL)) {
		acx100_ie_wep_options_t options;

		if (priv->chip_type == CHIPTYPE_ACX111) {
			acxlog(L_DEBUG, "Setting WEP Options for ACX111 not supported!\n");
		} else {
			acxlog(L_INIT, "Setting WEP Options\n");
				
			options.NumKeys = cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10); /* let's choose maximum setting: 4 default keys, plus 10 other keys */
			options.WEPOption = 0; /* don't decrypt default key only, don't override decryption */
			if (priv->mode == ACX_MODE_MONITOR)
				options.WEPOption = 2; /* don't decrypt default key only, override decryption mechanism */

			acx_configure(priv, &options, ACX100_IE_WEP_OPTIONS);
		}
		CLEAR_BIT(priv->set_mask, SET_WEP_OPTIONS);
	}

	/* Rescan was requested */
	if (start_scan) {
		switch (priv->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:
			acx_sta_list_init(priv);
			/* TODO: check for races with
			** acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_STOP_SCAN) */
			/* stop any previous scan
			** (add a comment why is this needed...is it needed?)
			** A: this is needed since some tools do continuous
			** scanning and our driver then tries to start a new
			** scan while the old one is still running (which will
			** give an error).
			** Maybe simply figure out how to know when we're still
			** scanning and in this case don't start anew?
			**
			** FIXME: also do it cleanly: send "stop scan" cmd, check
			** ACX private IE until it says "scan is complete", etc. */
			acx_issue_cmd(priv, ACX1xx_CMD_STOP_SCAN, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
			acx_cmd_start_scan(priv);
			acx_set_status(priv, ACX_STATUS_1_SCANNING);
		}
	}

	/* debug, rate, and nick don't need any handling */
	/* what about sniffing mode?? */

	acxlog(L_INIT, "get_mask 0x%08x, set_mask 0x%08x - after update\n",
			priv->get_mask, priv->set_mask);

end:
#ifdef BROKEN_LOCKING
	acx_unlock(priv, &flags);
#endif
	FN_EXIT0();
}

void acx_initialize_rx_config(wlandevice_t *priv)
{
	struct {
		u16	id ACX_PACKED;
		u16	len ACX_PACKED;
		u16	rx_cfg1 ACX_PACKED;
		u16	rx_cfg2 ACX_PACKED;
	} cfg;

	switch (priv->mode) {
	case ACX_MODE_OFF:
		priv->rx_config_1 = (u16) (0
			/*| RX_CFG1_RCV_PROMISCUOUS */
			/*| RX_CFG1_INCLUDE_FCS */
			/*| RX_CFG1_INCLUDE_PHY_HDR */);
		priv->rx_config_2 = (u16) (0
			/*| RX_CFG2_RCV_ASSOC_REQ	*/
			/*| RX_CFG2_RCV_AUTH_FRAMES	*/
			/*| RX_CFG2_RCV_BEACON_FRAMES	*/
			/*| RX_CFG2_RCV_CONTENTION_FREE	*/
			/*| RX_CFG2_RCV_CTRL_FRAMES	*/
			/*| RX_CFG2_RCV_DATA_FRAMES	*/
			/*| RX_CFG2_RCV_BROKEN_FRAMES	*/
			/*| RX_CFG2_RCV_MGMT_FRAMES	*/
			/*| RX_CFG2_RCV_PROBE_REQ	*/
			/*| RX_CFG2_RCV_PROBE_RESP	*/
			/*| RX_CFG2_RCV_ACK_FRAMES	*/
			/*| RX_CFG2_RCV_OTHER		*/);
		break;
	case ACX_MODE_MONITOR:
		priv->rx_config_1 = (u16) (0
			| RX_CFG1_RCV_PROMISCUOUS
			/* | RX_CFG1_INCLUDE_FCS */
			/* | RX_CFG1_INCLUDE_PHY_HDR */);
		priv->rx_config_2 = (u16) (0
			| RX_CFG2_RCV_ASSOC_REQ
			| RX_CFG2_RCV_AUTH_FRAMES
			| RX_CFG2_RCV_BEACON_FRAMES
			| RX_CFG2_RCV_CONTENTION_FREE
			| RX_CFG2_RCV_CTRL_FRAMES
			| RX_CFG2_RCV_DATA_FRAMES
			| RX_CFG2_RCV_BROKEN_FRAMES
			| RX_CFG2_RCV_MGMT_FRAMES
			| RX_CFG2_RCV_PROBE_REQ
			| RX_CFG2_RCV_PROBE_RESP
			| RX_CFG2_RCV_ACK_FRAMES
			| RX_CFG2_RCV_OTHER);
		break;
	default:
		priv->rx_config_1 = (u16) (0
			/*| RX_CFG1_FILTER_SSID */
			/*| RX_CFG1_FILTER_BSSID */
			| RX_CFG1_FILTER_MAC
			/*| RX_CFG1_RCV_PROMISCUOUS */
			/*| RX_CFG1_INCLUDE_PHY_HDR */);
		priv->rx_config_2 = (u16) (0
			| RX_CFG2_RCV_ASSOC_REQ
			| RX_CFG2_RCV_AUTH_FRAMES
			| RX_CFG2_RCV_BEACON_FRAMES
			| RX_CFG2_RCV_CONTENTION_FREE
			| RX_CFG2_RCV_CTRL_FRAMES
			| RX_CFG2_RCV_DATA_FRAMES
			| RX_CFG2_RCV_MGMT_FRAMES
			| RX_CFG2_RCV_PROBE_REQ
			| RX_CFG2_RCV_PROBE_RESP
			| RX_CFG2_RCV_OTHER);
		break;
	}
#if DEBUG_WEP
	if (CHIPTYPE_ACX100 == priv->chip_type)
		/* only ACX100 supports that */
#endif
		priv->rx_config_1 |= RX_CFG1_INCLUDE_RXBUF_HDR;

	acxlog(L_INIT, "setting RXconfig to %04x:%04x\n", priv->rx_config_1, priv->rx_config_2);
	cfg.rx_cfg1 = cpu_to_le16(priv->rx_config_1);
	cfg.rx_cfg2 = cpu_to_le16(priv->rx_config_2);
	acx_configure(priv, &cfg, ACX1xx_IE_RXCONFIG);
}

/*----------------------------------------------------------------
* acx_schedule_after_interrupt_task
*
* Schedule the call of the after_interrupt method after leaving
* the interrupt context.
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/
void acx_schedule_after_interrupt_task(wlandevice_t *priv, unsigned int set_flag)
{
	SET_BIT(priv->after_interrupt_jobs, set_flag);
	SCHEDULE_WORK(&priv->after_interrupt_task);
}

/*----------------------------------------------------------------
* acx_after_interrupt_task
* 
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/
static void acx_after_interrupt_task(void *data)
{
	netdevice_t *dev = (netdevice_t *) data;
	wlandevice_t *priv;

	FN_ENTER;

	if (unlikely(in_interrupt())) {
		acxlog(L_IRQ, "Don't call this in IRQ context!\n");
		return;
	}
			
	priv = (struct wlandevice *) dev->priv;

	if (priv->irq_status & HOST_INT_SCAN_COMPLETE) {
		if (priv->status == ACX_STATUS_1_SCANNING) {
			acx_complete_dot11_scan(priv);
		}
		CLEAR_BIT(priv->irq_status, HOST_INT_SCAN_COMPLETE);
	}

	if (priv->after_interrupt_jobs == 0)
		goto end; /* no jobs to do */
	
#if TX_CLEANUP_IN_SOFTIRQ
	if (priv->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_TX_CLEANUP) {
		acx_clean_tx_desc(priv);
		CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_TX_CLEANUP);
	}
#endif
	if (priv->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_RADIO_RECALIB) {
		/* this helps with ACX100 at least;
		 * hopefully ACX111 also does a
		 * recalibration here */

		/* better wait a bit between recalibrations to
		 * prevent overheating due to torturing the card
		 * into working too long despite high temperature
		 * (just a safety measure) */
		if (priv->time_last_recalib_success && time_before(jiffies, priv->time_last_recalib_success + RECALIB_PAUSE * 60 * HZ)) {
			static unsigned int silence_msg = 0;

#define TO_STRING(x)	#x
#define GET_STRING(x)	TO_STRING(x)

			if (++silence_msg <= 5)
				acxlog(L_STD, "less than " GET_STRING(RECALIB_PAUSE) " minutes since last radio recalibration (maybe card too hot?): not recalibrating!\n");
			CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		} else {
			static unsigned int recalib_failure_count = 0;
			int res = NOT_OK;

			/* note that commands sometimes fail (card busy), so only clear flag if we were fully successful */
			res = acx_recalib_radio(priv);
			if (res == OK) {
				acxlog(L_STD, "successfully recalibrated radio\n");
				CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
				priv->time_last_recalib_success = jiffies;
				recalib_failure_count = 0;
			} else {
				/* failed: resubmit, but only limited
				 * amount of times within some time range
				 * to prevent endless loop */
				static unsigned long time_last_recalib_attempt = 0;

				/* we failed */
				priv->time_last_recalib_success = 0;
				
				/* if some time passed between last
				 * attempts, then reset failure retry counter
				 * to be able to do next recalib attempt */
				if (time_after(jiffies, time_last_recalib_attempt + HZ))
					recalib_failure_count = 0;

				if (++recalib_failure_count <= 5) {
					time_last_recalib_attempt = jiffies;
					acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
				}
			}
		}
	}
	if (priv->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_UPDATE_CARD_CFG) {
		acx_update_card_settings(priv, 0, 0, 0);
		CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_UPDATE_CARD_CFG);
	}
	if (priv->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_STOP_SCAN) {
		acxlog(L_IRQ, "Send a stop scan cmd...\n");
		acx_issue_cmd(priv, ACX1xx_CMD_STOP_SCAN, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);

		CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_STOP_SCAN);
	}
	if (priv->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_ASSOCIATE) {
		/* FIXME: the AID is supposed to be set after assoc to
		 * an AP only, not Ad-Hoc. AFAIK this code part here
		 * will be invoked in non-Ad-Hoc only, so it should be ok */
		acx_ie_generic_t pdr;
		pdr.m.asid.vala = cpu_to_le16(priv->aid);
		acx_configure(priv, &pdr, ACX1xx_IE_ASSOC_ID);
		acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);

		acxlog(L_BINSTD | L_ASSOC, "ASSOCIATED!\n");

		CLEAR_BIT(priv->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_ASSOCIATE);
	}

end:
	FN_EXIT0();
}

void acx_init_task_scheduler(wlandevice_t *priv)
{
	/* configure task scheduler */
	INIT_WORK(&priv->after_interrupt_task, acx_after_interrupt_task, priv->netdev);
}

void acx_flush_task_scheduler(void)
{
	FLUSH_SCHEDULED_WORK();
}


/*----------------------------------------------------------------
* acx_cmd_join_bssid
* STATUS: FINISHED, UNVERIFIED.
* Common code for both acx100 and acx111.
*----------------------------------------------------------------*/

/* NB: does NOT match RATE100_nn but matches ACX[111]_SCAN_RATE_n */
static const u8
bitpos2genframe_txrate[] = {
	10, /*  1 Mbit/s */
	20, /*  2 Mbit/s */
	55, /*  5.5 Mbit/s */
	0x0B, /*  6 Mbit/s */
	0x0F, /*  9 Mbit/s */
	110, /* 11 Mbit/s */
	0x0A, /* 12 Mbit/s */
	0x0E, /* 18 Mbit/s */
	220, /* 22 Mbit/s */
	0x09, /* 24 Mbit/s */
	0x0D, /* 36 Mbit/s */
	0x08, /* 48 Mbit/s */
	0x0C, /* 54 Mbit/s */
};

/* Looks scary, eh?
** Actually, each one compiled into one AND and one SHIFT,
** 31 bytes in x86 asm (more if uints are replaced by u16/u8) */
static unsigned int
rate111to5bits(unsigned int rate)
{
	return (rate & 0x7)
	| ( (rate & RATE111_11) / (RATE111_11/JOINBSS_RATES_11) )
	| ( (rate & RATE111_22) / (RATE111_22/JOINBSS_RATES_22) )
	;
}

extern void bug_joinbss_must_be_0x30_bytes_in_length(void);

/* Note that we use defpeer here, not ap_peer. Latter is valid only after join */
/* NB: content of beacons/probe responses depend on template.
** It is not initialized here! (maybe it should be) */
void
acx_cmd_join_bssid(wlandevice_t *priv, const u8 *bssid)
{
	unsigned int i, n;
	acx_joinbss_t tmp;
	u8 dtim_interval;
	
	if (sizeof(acx_joinbss_t) != 0x30)
		bug_joinbss_must_be_0x30_bytes_in_length();
	
	FN_ENTER;

	dtim_interval =
		(ACX_MODE_0_ADHOC == priv->mode) ?
			1 : priv->dtim_interval;

	memset(&tmp, 0, sizeof(tmp));

	for (i = 0; i < ETH_ALEN; i++) {
		tmp.bssid[i] = bssid[ETH_ALEN-1 - i];
	}

	tmp.beacon_interval = cpu_to_le16(priv->beacon_interval);

	/* basic rate set. Control frame responses (such as ACK or CTS frames)
	** are sent with one of these rates */
	if ( CHIPTYPE_ACX111 == priv->chip_type ) {
		/* It was experimentally determined that rates_basic
		** can take 11g rates as well, not only rates
		** defined with JOINBSS_RATES_BASIC111_nnn.
		** Just use RATE111_nnn constants... */
		tmp.u.acx111.dtim_interval = dtim_interval;
		tmp.u.acx111.rates_basic = cpu_to_le16(priv->rate_basic);
		acxlog(L_ASSOC, "%s rates_basic %04x, rates_supported %04x\n",
			__func__, priv->rate_basic, priv->rate_oper);
	} else {
		tmp.u.acx100.dtim_interval = dtim_interval;
		tmp.u.acx100.rates_basic = rate111to5bits(priv->rate_basic);
		tmp.u.acx100.rates_supported = rate111to5bits(priv->rate_oper);
		acxlog(L_ASSOC, "%s rates_basic %04x->%02x, "
			"rates_supported %04x->%02x\n",
			__func__,
			priv->rate_basic, tmp.u.acx100.rates_basic,
			priv->rate_oper, tmp.u.acx100.rates_supported);
	}

	/* Setting up how Beacon, Probe Response, RTS, and PS-Poll frames
	** will be sent (rate/modulation/preamble) */
	n = 0;
	{
		u16 t = priv->rate_basic;
		/* calculate number of highest bit set in t */
		while(t>1) { t>>=1; n++; }
	}
	if (n >= sizeof(bitpos2genframe_txrate)) {
		printk(KERN_ERR "join_bssid: driver BUG! n=%u. please report\n", n);
		n = 0;
	}
	/* look up what value the highest basic rate actually is */
	tmp.genfrm_txrate = bitpos2genframe_txrate[n];
	tmp.genfrm_mod_pre = 0; /* FIXME: was = priv->capab_short (which is always 0); */
	/* we can use short pre *if* all peers can understand it */
	/* FIXME #2: we need to correctly set PBCC/OFDM bits here too */

	tmp.macmode = priv->mode;
	tmp.channel = priv->channel;
	tmp.essid_len = priv->essid_len;
	/* NOTE: the code memcpy'd essid_len + 1 before, which is WRONG! */
	memcpy(tmp.essid, priv->essid, tmp.essid_len);
	acx_issue_cmd(priv, ACX1xx_CMD_JOIN, &tmp, tmp.essid_len + 0x11, ACX_CMD_TIMEOUT_DEFAULT);
/* What's this?? */
#if 0
	for (i = 0; i < ETH_ALEN; i++) {
		priv->bssid[ETH_ALEN - 1 - i] = tmp.bssid[i];
	}
#endif

	acxlog(L_ASSOC | L_BINDEBUG,
		"<%s> BSS_Type = %u\n", __func__, tmp.macmode);
	acxlog(L_ASSOC | L_BINDEBUG,
		"<%s> JoinBSSID MAC:"MACSTR"\n", __func__, MAC(priv->bssid));

	acx_update_capabilities(priv);
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_set_defaults
* Called from acx_init_mac
* STATUS: good
*----------------------------------------------------------------*/
static int acx_set_defaults(wlandevice_t *priv)
{
	int result = 0;

	FN_ENTER;

	/* query some settings from the card.
	 * NOTE: for some settings, e.g. CCA and ED (ACX100!), an initial
	 * query is REQUIRED, otherwise the card won't work correctly!! */
	priv->get_mask = GETSET_ANTENNA|GETSET_SENSITIVITY|GETSET_STATION_ID|GETSET_REG_DOMAIN;
	/* Only ACX100 supports ED and CCA */
	if (CHIPTYPE_ACX100 == priv->chip_type)
		priv->get_mask |= GETSET_CCA|GETSET_ED_THRESH;
	acx_update_card_settings(priv, 1, 0, 0);

	/* set our global interrupt mask */
	if (priv->chip_type == CHIPTYPE_ACX111) {
		priv->irq_mask = (u16)
				 ~( HOST_INT_FCS_THRESHOLD
				  | HOST_INT_SCAN_COMPLETE
				  | HOST_INT_INFO
				  | HOST_INT_CMD_COMPLETE
				  | HOST_INT_IV_ICV_FAILURE
				  | HOST_INT_DTIM
				  | HOST_INT_RX_COMPLETE
				  | HOST_INT_TX_COMPLETE ); /* 0x98e5 */
		priv->irq_mask_off = (u16)~( HOST_INT_CMD_COMPLETE ); /* 0xfdff */
	} else {
		/* priv->irq_mask = 0xdbb5; not longer used anymore! */
		priv->irq_mask = (u16)
			         ~( HOST_INT_SCAN_COMPLETE
				  | HOST_INT_INFO
				  | HOST_INT_CMD_COMPLETE
				  | HOST_INT_TIMER
				  | HOST_INT_RX_COMPLETE
				  | HOST_INT_TX_COMPLETE ); /* 0xd9b5 */
		priv->irq_mask_off = (u16)~( HOST_INT_UNKNOWN ); /* 0x7fff */
	}

	priv->led_power = 1; /* LED is active on startup */
	priv->brange_max_quality = 60; /* LED blink max quality is 60 */
	priv->brange_time_last_state_change = jiffies;

	/* copy the MAC address we just got from the card
	 * into our MAC address used during current 802.11 session */
	MAC_COPY(priv->dev_addr, priv->netdev->dev_addr);
	sprintf(priv->essid, "STA%02X%02X%02X",
		priv->dev_addr[3], priv->dev_addr[4], priv->dev_addr[5]);
	priv->essid_len = 9; /* make sure to adapt if changed above! */
	priv->essid_active = 1;

	/* we have a nick field to waste, so why not abuse it
	 * to announce the driver version? ;-) */
	strncpy(priv->nick, "acx100 ", IW_ESSID_MAX_SIZE);
	strncat(priv->nick, WLAN_RELEASE_SUB, IW_ESSID_MAX_SIZE);

	if ( priv->chip_type == CHIPTYPE_ACX111 ) { 
		/* Hope this is correct, only tested with domain 0x30 */
		acx_read_eeprom_offset(priv, 0x16F, &priv->reg_dom_id);
	} else if ( priv->chip_type == CHIPTYPE_ACX100 ) {
		if (priv->eeprom_version < 5) {
			acx_read_eeprom_offset(priv, 0x16F, &priv->reg_dom_id);
		} else {
			acx_read_eeprom_offset(priv, 0x171, &priv->reg_dom_id);
		}
	}

	priv->channel = 1;
	priv->scan_count = 1; /* 0xffff would be better, but then we won't get a "scan complete" interrupt, so our current infrastructure will fail */
	priv->scan_mode = ACX_SCAN_OPT_PASSIVE;
	priv->scan_duration = 100;
	priv->scan_probe_delay = 200;
	priv->scan_rate = ACX_SCAN_RATE_2;

	priv->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
	priv->preamble_mode = 2; /* auto */
	priv->listen_interval = 100;
	priv->beacon_interval = DEFAULT_BEACON_INTERVAL;
	priv->mode = ACX_MODE_OFF;
	priv->dtim_interval = DEFAULT_DTIM_INTERVAL;

	priv->msdu_lifetime = DEFAULT_MSDU_LIFETIME;
	SET_BIT(priv->set_mask, SET_MSDU_LIFETIME);

	priv->rts_threshold = DEFAULT_RTS_THRESHOLD;

	/* use standard default values for retry limits */
	priv->short_retry = 7; /* max. retries for (short) non-RTS packets */
	priv->long_retry = 4; /* max. retries for long (RTS) packets */
	SET_BIT(priv->set_mask, GETSET_RETRY);

	priv->fallback_threshold = 3;
	priv->stepup_threshold = 10;
	priv->rate_bcast = RATE111_2;
	priv->rate_basic = RATE111_1 | RATE111_2;
	priv->rate_auto = 0;
	if (priv->chip_type == CHIPTYPE_ACX111) { 
		priv->rate_oper = RATE111_ALL;
	} else {
		priv->rate_oper = RATE111_ACX100_COMPAT;
	}

	/* configure card to do rate fallback when in auto rate mode. */
	SET_BIT(priv->set_mask, SET_RATE_FALLBACK);

	/* Supported Rates element - the rates here are given in units of
	 * 500 kbit/s, plus 0x80 added. See 802.11-1999.pdf item 7.3.2.2 */
	acx_update_dot11_ratevector(priv);

	priv->capab_short = 0;
	priv->capab_pbcc = 1;
	priv->capab_agility = 0;

	priv->promiscuous = 0;
	priv->mc_count = 0;

	acx_initialize_rx_config(priv);
	SET_BIT(priv->set_mask, SET_RXCONFIG);

	/* set some more defaults */
	if ( priv->chip_type == CHIPTYPE_ACX111 ) { 
		priv->tx_level_dbm = 15; /* 30mW (15dBm) is default, at least in my acx111 card */
	} else {
		priv->tx_level_dbm = 18; /* don't use max. level, since it might be dangerous (e.g. WRT54G people experience excessive Tx power damage!) */
	}
	priv->tx_level_auto = 1;
	SET_BIT(priv->set_mask, GETSET_TXPOWER);

	if ( priv->chip_type == CHIPTYPE_ACX111 ) {
		priv->sensitivity = 1; /* start with sensitivity level 1 out of 3 */
	}

	/* better re-init the antenna value we got above */
	SET_BIT(priv->set_mask, GETSET_ANTENNA);

	priv->ps_wakeup_cfg = 0;
	priv->ps_listen_interval = 0;
	priv->ps_options = 0;
	priv->ps_hangover_period = 0;
	priv->ps_enhanced_transition_time = 0;
#if POWER_SAVE_80211
	SET_BIT(priv->set_mask, GETSET_POWER_80211);
#endif

	MAC_BCAST(priv->ap);

	result = OK;

	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_init_mac
* Arguments:
* init = 1/0: called from acx_probe_pci / acx_resume
* STATUS: FINISHED.
*----------------------------------------------------------------*/
int acx_init_mac(netdevice_t *dev, u16 init)
{
	int result = NOT_OK;
	acx_ie_memmap_t pkt;
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	FN_ENTER;

	acxlog(L_DEBUG,"sizeof(memmap)=%d bytes\n",sizeof(pkt));

	acxlog(L_BINDEBUG,          "******************************************\n");
	acxlog(L_BINDEBUG | L_INIT, "************* acx_init_mac_1 *************\n");
	acxlog(L_BINDEBUG,          "******************************************\n");
#if (WLAN_HOSTIF!=WLAN_USB)
	priv->memblocksize = 256; /* 256 is default */
#else
	priv->memblocksize = 128;
#endif

	acx_init_mboxes(priv);
#if (WLAN_HOSTIF!=WLAN_USB)	
	/* try to load radio for both ACX100 and ACX111, since both
	 * chips have at least some firmware versions making use of an
	 * external radio module */
	acx_upload_radio(priv);
#endif

	if(priv->chip_type == CHIPTYPE_ACX111) {
		/* for ACX111, the order is different from ACX100
		   1. init packet templates
		   2. create station context and create dma regions
		   3. init wep default keys 
		*/
		if (OK != acx111_init_packet_templates(priv)) 
			goto fail;

		if (OK != acx111_create_dma_regions(priv)) {
			acxlog(L_STD, "acx111_create_dma_regions FAILED\n");
			goto fail;
		}
#if DEBUG_WEP
		/* don't decrypt WEP in firmware */
		if (OK != acx111_feature_on(priv, 0, FEATURE2_SNIFFER))
			goto fail;
#endif
	} else {
		if (OK != acx100_init_wep(priv)) 
			goto fail;
		acxlog(L_DEBUG, "between init_wep and init_packet_templates\n");
		if (OK != acx100_init_packet_templates(priv,&pkt)) 
			goto fail;

		if (OK != acx100_create_dma_regions(priv)) {
			acxlog(L_STD, "acx100_create_dma_regions FAILED\n");
			goto fail;
		}
	}

	if (1 == init)
		if (OK != acx_set_defaults(priv)) {
			acxlog(L_STD, "acx_set_defaults FAILED\n");
			goto fail;
		}


	MAC_COPY(dev->dev_addr, priv->dev_addr);

	/* FIXME: this code shouldn't be necessary, it should be
	 * done via SET_TEMPLATES instead... */
	/* Ok to test removal, please do --vda */
	if (ACX_MODE_2_STA != priv->mode) {
		if (OK != acx_set_beacon_template(priv)) {
		    acxlog(L_STD, "acx_set_beacon_template FAILED\n");
		}
		if (OK != acx_set_probe_response_template(priv)) {
		    acxlog(L_STD, "acx_set_probe_response_template FAILED\n");
		    goto fail;
		}
	}
	result = OK;

fail:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_start
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* AcxStart()
 * STATUS: should be ok.
 */
void acx_start(wlandevice_t *priv)
{
	unsigned long flags;
	unsigned int dont_lock_up = 0;

	FN_ENTER;

	if (spin_is_locked(&priv->lock)) {
		printk(KERN_EMERG "Preventing lock-up!");
		dont_lock_up = 1;
	}

	if (0 == dont_lock_up)
		if (acx_lock(priv, &flags)) {
			acxlog(L_STD, "ERROR: lock FAILED\n");
			FN_EXIT0();
			return;
		}

	/* 
	 * Ok, now we do everything that can possibly be done with ioctl 
	 * calls to make sure that when it was called before the card 
	 * was up we get the changes asked for
	 */

	SET_BIT(priv->set_mask, SET_TEMPLATES|SET_STA_LIST|GETSET_WEP|GETSET_TXPOWER|GETSET_ANTENNA|GETSET_ED_THRESH|GETSET_CCA|GETSET_REG_DOMAIN|GETSET_MODE|GETSET_CHANNEL|GETSET_TX|GETSET_RX);
	acxlog(L_INIT, "initial settings update on iface activation.\n");
	acx_update_card_settings(priv, 1, 0, 0);

	if (0 == dont_lock_up)
		acx_unlock(priv, &flags);
	FN_EXIT0();
}

/*------------------------------------------------------------------------------
 * acx_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 * Arguments:
 *	@priv: per-device struct containing the management timer
 *	@timeout: timeout in us
 *
 * Returns: -
 *
 * Side effects:
 *
 * Call context:
 *
 * STATUS: FINISHED, but struct undefined.
 *
 * Comment:
 *
 *----------------------------------------------------------------------------*/
void acx_set_timer(wlandevice_t *priv, u32 timeout)
{
	FN_ENTER;

	acxlog(L_BINDEBUG | L_IRQ, "<%s> Elapse = %u\n", __func__, timeout);
	if (0 == (priv->dev_state_mask & ACX_STATE_IFACE_UP)) {
		acxlog(L_STD, "attempt to set the timer when the card interface is not up!\n");
		FN_EXIT0();
		return;
	}

	/* newer firmware versions abandoned timer configuration
	 * FIXME: any other versions between 1.8.3 (working) and
	 * 1.9.3.e (removed)? */
#if (WLAN_HOSTIF!=WLAN_USB)
	if (priv->firmware_numver < 0x0109030e
	 && priv->chip_type != CHIPTYPE_ACX111) {
		u32 tmp[5];

		/* first two 16-bit words reserved for type and length */
		tmp[1] = cpu_to_le32(timeout);
		tmp[4] = 0;
		acx_configure(priv, &tmp, ACX100_IE_ACX_TIMER);
	} else
#endif
	{
		/* first check if the timer was already initialized, THEN modify it */
		if (priv->mgmt_timer.function) {
			mod_timer(&(priv->mgmt_timer), jiffies + (timeout * HZ / 1000000));
		}
	}
	FN_EXIT0();
}

/* AcxUpdateCapabilities()
 * STATUS: FINISHED. Original name was
 * AcxUpdateCapabilies (spelling error!).
 */
void acx_update_capabilities(wlandevice_t *priv)
{
	u16 old_cap = priv->capabilities;
	u16 cap = 0;

	switch(priv->mode) {
	case ACX_MODE_3_AP:
		SET_BIT(cap, WF_MGMT_CAP_ESS); break;
	case ACX_MODE_0_ADHOC:
		SET_BIT(cap, WF_MGMT_CAP_IBSS); break;
	/* other types of stations do not emit beacons */
	}

	if (priv->wep_restricted) {
		SET_BIT(cap, WF_MGMT_CAP_PRIVACY);
	}
	if (priv->capab_short) {
		SET_BIT(cap, WF_MGMT_CAP_SHORT);
	}
	if (priv->capab_pbcc) {
		SET_BIT(cap, WF_MGMT_CAP_PBCC);
	}
	if (priv->capab_agility) {
		SET_BIT(cap, WF_MGMT_CAP_AGILITY);
	}
	priv->capabilities = cap;
	acxlog(L_DEBUG, "caps updated from 0x%04x to 0x%04x\n", old_cap, cap);
}

/*----------------------------------------------------------------
* acx_read_eeprom_offset
*
* Function called to read an octet in the EEPROM.
*
* This function is used by acx_probe_pci to check if the
* connected card is a legal one or not.
*
* Arguments:
*	priv		ptr to wlandevice structure
*	addr		address to read in the EEPROM
*	charbuf		ptr to a char. This is where the read octet
*			will be stored
*
* Returns:
*	zero (0)	- failed
*	one (1)		- success
*
* Side effects:
*
*
* Call context:
*
* STATUS: FINISHED.
* 	  NOT ADAPTED FOR ACX111 !!
*
* Comment:
----------------------------------------------------------------*/
u16 acx_read_eeprom_offset(wlandevice_t *priv,
					u32 addr, u8 *charbuf)
{
	u16 result = NOT_OK;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned int count = 0;

	FN_ENTER;

	acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
	acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr);
	acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 2);

	while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (++count > 0xffff) {
			acxlog(L_BINSTD, "%s: timeout waiting for read EEPROM cmd\n", __func__);
			goto fail;
		}
	}

	*charbuf = acx_read_reg8(priv, IO_ACX_EEPROM_DATA);
	acxlog(L_DEBUG, "EEPROM read 0x%04x --> 0x%02x\n", addr, *charbuf); 
	result = OK;

fail:
	FN_EXIT1(result);
#endif
	return result;
}

/* acx_read_eeprom_area
 * STATUS: OK.
 */
u16 acx_read_eeprom_area(wlandevice_t *priv)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	u32 offs;
	u8 tmp[0x3b];

	for (offs = 0x8c; offs < 0xb9; offs++) {
		acx_read_eeprom_offset(priv, offs, &tmp[offs - 0x8c]);
	}
#endif
	return OK;
}

u16 acx_write_eeprom_offset(wlandevice_t *priv, u32 addr, u32 len, const u8 *charbuf)
{
#if (WLAN_HOSTIF==WLAN_USB)
	u16 result = OK;
#else
	u16 result = NOT_OK;

	u16 gpio_orig;
	unsigned int i;
	u8 *data_verify = NULL;
	unsigned int count = 0;
	
	acxlog(0xffff, "WARNING: I would write to EEPROM now. Since I really DON'T want to unless you know what you're doing (THIS HAS NEVER BEEN TESTED!), I will abort that now.\n");
	return 0;
	
	FN_ENTER;

	/* first we need to enable the OE (EEPROM Output Enable) GPIO line
	 * to be able to write to the EEPROM */
	gpio_orig = acx_read_reg16(priv, IO_ACX_GPIO_OE);
	acx_write_reg16(priv, IO_ACX_GPIO_OE, gpio_orig & ~1);
	
	/* ok, now start writing the data out */
	for (i = 0; i < len; i++) {
		acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
		acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr + i);
		acx_write_reg32(priv, IO_ACX_EEPROM_DATA, *(charbuf + i));
		acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 1);

		while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
			/* scheduling away instead of CPU burning loop
			 * doesn't seem to work here at all:
			 * awful delay, sometimes also failure.
			 * Doesn't matter anyway (only small delay). */
			if (++count > 0xffff) {
				acxlog(L_BINSTD, "%s: WARNING, DANGER!!!! Timeout waiting for write EEPROM cmd\n", __func__);
				goto fail;
			}
		}
	}

	/* disable EEPROM writing */
	acx_write_reg16(priv, IO_ACX_GPIO_OE, gpio_orig);

	/* now start a verification run */
	data_verify = kmalloc(len, GFP_KERNEL);
	if (!data_verify) {
		goto fail;
	}

	for (i = 0; i < len; i++) {

		acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
		acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr + i);
		acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 2);

		while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
			/* scheduling away instead of CPU burning loop
			 * doesn't seem to work here at all:
			 * awful delay, sometimes also failure.
			 * Doesn't matter anyway (only small delay). */
			if (++count > 0xffff) {
				acxlog(L_BINSTD, "%s: timeout waiting for read EEPROM cmd\n", __func__);
				goto fail;
			}
		}

		data_verify[i] = acx_read_reg16(priv, IO_ACX_EEPROM_DATA);
	}

	if (0 == memcmp(charbuf, data_verify, len))
		result = OK; /* read data matches, success */
	
fail:
	kfree(data_verify);
	
	FN_EXIT1(result);
#endif
	return result;
}

u16 acx_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf)
{
	u16 result = NOT_OK;
#if (WLAN_HOSTIF!=WLAN_USB)
	unsigned int count = 0;

	FN_ENTER;

#ifdef BROKEN_KILLS_TRAFFIC
	acx_write_reg32(priv, IO_ACX_ENABLE, 0x0); /* disable Rx/Tx */
#endif

	acx_write_reg32(priv, IO_ACX_PHY_ADDR, reg);
	acx_write_reg32(priv, IO_ACX_PHY_CTL, 2);

	while (acx_read_reg32(priv, IO_ACX_PHY_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (++count > 0xffff) {
			acxlog(L_BINSTD, "%s: timeout waiting for read phy cmd\n", __func__);
			*charbuf = 0;
#ifdef BROKEN_KILLS_TRAFFIC
			acx_write_reg32(priv, IO_ACX_ENABLE, 0x3); /* reenable Rx/Tx */
#endif
			goto fail;
		}
	}

	acxlog(L_DEBUG, "count was %u\n", count);
	*charbuf = acx_read_reg8(priv, IO_ACX_PHY_DATA);
	
#ifdef BROKEN_KILLS_TRAFFIC
	acx_write_reg32(priv, IO_ACX_ENABLE, 0x3); /* reenable Rx/Tx */
#endif
#else
	mem_read_write_t mem;

	mem.addr = cpu_to_le16(reg);
	mem.type = cpu_to_le16(0x82);
	mem.len = cpu_to_le32(4);
	acx_issue_cmd(priv, ACX1xx_CMD_MEM_READ, &mem, 8, ACX_CMD_TIMEOUT_DEFAULT);
	*charbuf = mem.data;
#endif
	acxlog(L_DEBUG, "radio PHY read 0x%02x from 0x%04x\n", *charbuf, reg); 
	result = OK;
	goto fail; /* silence compiler warning */
fail:
	FN_EXIT1(result);
	return result;
}

u16 acx_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	FN_ENTER;

	acx_write_reg32(priv, IO_ACX_PHY_ADDR, reg);
	/* FIXME: we don't use 32bit access here since mprusko said that
	 * it results in distorted sensitivity on his card (huh!?!?
	 * doesn't happen with my setup...)
	 * Maybe we actually need a delay similar to the one in the read
	 * function, due to some radio chipsets being too slow...
	 * FIXME: which radio is in the problematic card? My working one
	 * is 0x11 */
	acx_write_reg16(priv, IO_ACX_PHY_DATA, value);
	acx_write_reg16(priv, IO_ACX_PHY_DATA + 2, 0);
	acx_write_reg32(priv, IO_ACX_PHY_CTL, 1);
#else
	mem_read_write_t mem;

	FN_ENTER;

	mem.addr = cpu_to_le16(reg);
	mem.type = cpu_to_le16(0x82);
	mem.len = cpu_to_le32(4);
	mem.data = value;
	acx_issue_cmd(priv, ACX1xx_CMD_MEM_WRITE, &mem, sizeof(mem), ACX_CMD_TIMEOUT_DEFAULT);
#endif
	acxlog(L_DEBUG, "radio PHY write 0x%02x to 0x%04x\n", value, reg); 
	FN_EXIT1(OK);
	return OK;
}


/* FIXME: check whether this function is indeed acx111 only,
 * rename ALL relevant definitions to indicate actual card scope! */
void acx111_read_configoption(wlandevice_t *priv)
{
	acx_configoption_t	co;
	acx_configoption_t	co2;
	unsigned int	i;
	const u8	*pEle;
	
	FN_ENTER;
	
	if (OK != acx_interrogate(priv, &co, ACX111_IE_CONFIG_OPTIONS) ) {
		acxlog(L_STD, "Reading ConfigOption FAILED\n");
		return;
	};

	memcpy(&co2, &co, sizeof(co_fixed_t));
	
	pEle = (u8 *)&co;
	pEle += (u8 *)sizeof(co_fixed_t) - (u8 *)4;
	
	co2.antennas.type = pEle[0];
	co2.antennas.len = pEle[1];
	acxlog(L_DEBUG, "AntennaID : %02X  Len: %02X, Data: ", co2.antennas.type, co2.antennas.len);
	for (i=0;i<pEle[1];i++) {
		co2.antennas.list[i] = pEle[i+2];
		acxlog(L_DEBUG, " %02X", pEle[i+2]);
	}
	acxlog(L_DEBUG, "\n");

	pEle += pEle[1] + 2;	
	co2.power_levels.type = pEle[0];
	co2.power_levels.len = pEle[1];
	acxlog(L_DEBUG, "PowerLevelID : %02X  Len: %02X, Data: ", co2.power_levels.type, co2.power_levels.len);
	for (i=0;i<pEle[1]*2;i++) {
		co2.power_levels.list[i] = pEle[i+2];
		acxlog(L_DEBUG, " %02X", pEle[i+2]);
	}
	acxlog(L_DEBUG, "\n");

	pEle += pEle[1]*2 + 2;	
	co2.data_rates.type = pEle[0];
	co2.data_rates.len = pEle[1];
	acxlog(L_DEBUG, "DataRatesID : %02X  Len: %02X, Data: ", co2.data_rates.type, co2.data_rates.len);
	for (i=0;i<pEle[1];i++) {
		co2.data_rates.list[i] = pEle[i+2];
		acxlog(L_DEBUG, " %02X", pEle[i+2]);
	}
	acxlog(L_DEBUG, "\n");
	
	pEle += pEle[1] + 2;
	co2.domains.type = pEle[0];
	co2.domains.len = pEle[1];
	acxlog(L_DEBUG, "DomainID : %02X  Len: %02X, Data: ", co2.domains.type, co2.domains.len);
	for (i=0;i<pEle[1];i++) {
		co2.domains.list[i] = pEle[i+2];
		acxlog(L_DEBUG, " %02X", pEle[i+2]);
	}
	acxlog(L_DEBUG, "\n");
	
	pEle += pEle[1] + 2;
	co2.product_id.type = pEle[0];
	co2.product_id.len = pEle[1];
	for (i=0;i<pEle[1];i++) {
		co2.product_id.list[i] = pEle[i+2];
	}
	acxlog(L_DEBUG, "ProductID : %02X  Len: %02X, Data: %.*s\n", co2.product_id.type, co2.product_id.len, co2.product_id.len, (char *)co2.product_id.list);

	pEle += pEle[1] + 2;	
	co2.manufacturer.type = pEle[0];
	co2.manufacturer.len = pEle[1];
	for (i=0;i<pEle[1];i++) {
		co2.manufacturer.list[i] = pEle[i+2];
	}
	acxlog(L_DEBUG, "ManufacturerID : %02X  Len: %02X, Data: %.*s\n", co2.manufacturer.type, co2.manufacturer.len, co2.manufacturer.len, (char *)co2.manufacturer.list);

/*
	acxlog(L_DEBUG, "EEPROM part : \n");
	for (i=0; i<58; i++) {
	    acxlog(L_DEBUG, "%02X =======>  0x%02x \n", i, (u8 *)co.configoption_fixed.NVSv[i-2]);
	}
*/	
	
	FN_EXIT1(OK);

}

