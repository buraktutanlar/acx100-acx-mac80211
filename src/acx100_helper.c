/* src/acx100_helper.c - helper functions
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

#include <linux/config.h>
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pci.h>

#include <linux/pm.h>

#include <asm/pci.h>
#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <version.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>

UINT8 DTIM_count;

extern char *firmware_dir; /* declared in acx100.c, to keep together with other MODULE_PARMs */

/* acx100_schedule()
 * Make sure to schedule away sometimes, in order to not hog the CPU too much.
 */
void acx100_schedule(UINT32 timeout)
{
	FN_ENTER;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(timeout);
	FN_EXIT(0, 0);
}

/*------------------------------------------------------------------------------
 * acx100_read_proc
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
int acx100_read_proc(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *wlandev = (wlandevice_t *)data;
	/* fill buf */
	int length = acx100_proc_output(buf, wlandev);

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
	FN_EXIT(1, length);
	return length;
}

int acx100_read_proc_diag(char *buf, char **start, off_t offset, int count,
		     int *eof, void *data)
{
	wlandevice_t *wlandev = (wlandevice_t *)data;
	/* fill buf */
	int length = acx100_proc_diag_output(buf, wlandev);

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
	FN_EXIT(1, length);
	return length;
}

/*------------------------------------------------------------------------------
 * acx100_proc_output
 * Generate content for our /proc entry
 *
 * Arguments:
 *	buf is a pointer to write output to
 *	hw is the usual pointer to our private struct wlandevice
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
int acx100_proc_output(char *buf, wlandevice_t *wlandev)
{
	char *p = buf;
	int i;

	FN_ENTER;
	p += sprintf(p, "acx100 driver version:\t\t%s\n", WLAN_RELEASE_SUB);
	p += sprintf(p, "Wireless extension version:\t%d\n", WIRELESS_EXT);
	p += sprintf(p, "chip name:\t\t\t%s\n", wlandev->chip_name);
	p += sprintf(p, "radio type:\t\t\t0x%02x\n", wlandev->radio_type);
	/* TODO: add radio type string from acx100_display_hardware_details */
	p += sprintf(p, "form factor:\t\t\t0x%02x\n", wlandev->form_factor);
	/* TODO: add form factor string from acx100_display_hardware_details */
	p += sprintf(p, "EEPROM version:\t\t\t0x%04x\n", wlandev->eeprom_version);
	p += sprintf(p, "firmware version:\t\t%s (0x%08lx)\n",
		     wlandev->firmware_version, wlandev->firmware_id);
	p += sprintf(p, "BSS table has %d entries:\n", wlandev->bss_table_count);
	for (i = 0; i < wlandev->bss_table_count; i++) {
		struct bss_info *bss = &wlandev->bss_table[i];
		p += sprintf(p, " BSS %d  BSSID %02x:%02x:%02x:%02x:%02x:%02x  ESSID %s  channel %d  WEP %s  Cap 0x%x  SIR %ld  SNR %ld\n", 
			     i, bss->bssid[0], bss->bssid[1],
			     bss->bssid[2], bss->bssid[3], bss->bssid[4],
			     bss->bssid[5], bss->essid, bss->channel,
			     bss->wep ? "yes" : "no", bss->caps,
			     bss->sir, bss->snr);
	}
	p += sprintf(p, "status:\t\t\t%d (%s)\n", wlandev->status, acx100_get_status_name(wlandev->status));
	/* TODO: add more interesting stuff (essid, ...) here */
	FN_EXIT(1, p - buf);
	return p - buf;
}

int acx100_proc_diag_output(char *buf, wlandevice_t *wlandev)
{
	char *p = buf;
	int i;
        TIWLAN_DC *pDc = &wlandev->dc;
        struct rxhostdescriptor *pDesc;
	txdesc_t *pTxDesc;
        UINT8 *a;
	fw_stats_t *fw_stats;

	FN_ENTER;

	p += sprintf(p, "*** Rx buf ***\n");
	for (i = 0; i < pDc->rx_pool_count; i++)
	{
		pDesc = &pDc->pRxHostDescQPool[i];
		if ((pDesc->Ctl & DESC_CTL_FREE) && (pDesc->val0x14 < 0))
			p += sprintf(p, "%02d FULL\n", i);
		else
			p += sprintf(p, "%02d empty\n", i);
	}
	p += sprintf(p, "\n");
	p += sprintf(p, "*** Tx buf ***\n");
	for (i = 0; i < pDc->tx_pool_count; i++)
	{
		pTxDesc = &pDc->pTxDescQPool[i];
		if ((pTxDesc->Ctl & DESC_CTL_DONE) == DESC_CTL_DONE)
			p += sprintf(p, "%02d DONE\n", i);
		else
			p += sprintf(p, "%02d empty\n", i);
	}
	p += sprintf(p, "\n");
	p += sprintf(p, "*** network status ***\n");
	p += sprintf(p, "ifup: %d\n", wlandev->ifup);
	p += sprintf(p, "status: %d (%s), mode: %d, macmode: %d, channel: %d, reg_dom_id: 0x%02x, reg_dom_chanmask: 0x%04x, bitrateval: %d, bitrate_auto: %d, bss_table_count: %d\n",
		wlandev->status, acx100_get_status_name(wlandev->status),
		wlandev->mode, wlandev->macmode, wlandev->channel,
		wlandev->reg_dom_id, wlandev->reg_dom_chanmask,
		wlandev->bitrateval, wlandev->bitrate_auto,
		wlandev->bss_table_count);
	p += sprintf(p, "ESSID: \"%s\", essid_active: %d, essid_len: %d, essid_for_assoc: \"%s\", nick: \"%s\"\n",
		wlandev->essid, wlandev->essid_active, wlandev->essid_len,
		wlandev->essid_for_assoc, wlandev->nick);
	p += sprintf(p, "monitor: %d, monitor_setting: %d\n",
		wlandev->monitor, wlandev->monitor_setting);
	a = wlandev->dev_addr;
	p += sprintf(p, "dev_addr:  %02x:%02x:%02x:%02x:%02x:%02x\n",
		a[0], a[1], a[2], a[3], a[4], a[5]);
	a = wlandev->address;
	p += sprintf(p, "address:   %02x:%02x:%02x:%02x:%02x:%02x\n",
		a[0], a[1], a[2], a[3], a[4], a[5]);
	a = wlandev->bssid;
	p += sprintf(p, "bssid:     %02x:%02x:%02x:%02x:%02x:%02x\n",
		a[0], a[1], a[2], a[3], a[4], a[5]);
	a = wlandev->ap;
	p += sprintf(p, "ap_filter: %02x:%02x:%02x:%02x:%02x:%02x\n",
		a[0], a[1], a[2], a[3], a[4], a[5]);

        if ((fw_stats = kmalloc(sizeof(fw_stats_t), GFP_KERNEL)) == NULL) {
                return 0;
        }
	p += sprintf(p, "\n");
	acx100_interrogate(wlandev, fw_stats, ACX100_RID_FIRMWARE_STATISTICS);
	p += sprintf(p, "*** Firmware ***\n");
	p += sprintf(p, "tx_desc_overfl %d, rx_OutOfMem %d, rx_hdr_overfl %d, rx_hdr_use_next %d\n",
		fw_stats->tx_desc_of, fw_stats->rx_oom, fw_stats->rx_hdr_of, fw_stats->rx_hdr_use_next);
	p += sprintf(p, "rx_dropped_frame %d, rx_frame_ptr_err %d, rx_xfr_hint_trig %d, rx_dma_req %d\n",
		fw_stats->rx_dropped_frame, fw_stats->rx_frame_ptr_err, fw_stats->rx_xfr_hint_trig, fw_stats->rx_dma_req);
	p += sprintf(p, "rx_dma_err %d, tx_dma_req %d, tx_dma_err %d, cmd_cplt %d, fiq %d\n",
		fw_stats->rx_dma_err, fw_stats->tx_dma_req, fw_stats->tx_dma_err, fw_stats->cmd_cplt, fw_stats->fiq);
	p += sprintf(p, "rx_hdrs %d, rx_cmplt %d, rx_mem_overfl %d, rx_rdys %d, irqs %d\n",
		fw_stats->rx_hdrs, fw_stats->rx_cmplt, fw_stats->rx_mem_of, fw_stats->rx_rdys, fw_stats->irqs);
	p += sprintf(p, "acx_trans_procs %d, decrypt_done %d, dma_0_done %d, dma_1_done %d\n",
		fw_stats->acx_trans_procs, fw_stats->decrypt_done, fw_stats->dma_0_done, fw_stats->dma_1_done);
	p += sprintf(p, "tx_exch_complet %d, commands %d, acx_rx_procs %d\n",
		fw_stats->tx_exch_complet, fw_stats->commands, fw_stats->acx_rx_procs);
	p += sprintf(p, "hw_pm_mode_changes %d, host_acks %d, pci_pm %d, acm_wakeups %d\n",
		fw_stats->hw_pm_mode_changes, fw_stats->host_acks, fw_stats->pci_pm, fw_stats->acm_wakeups);
	p += sprintf(p, "wep_key_count %d, wep_default_key_count %d, dot11_def_key_mib %d\n",
		fw_stats->wep_key_count, fw_stats->wep_default_key_count, fw_stats->dot11_def_key_mib);
	p += sprintf(p, "wep_key_not_found %d, wep_decrypt_fail %d\n",
		fw_stats->wep_key_not_found, fw_stats->wep_decrypt_fail);

        kfree(fw_stats);

	FN_EXIT(1, p - buf);
	return p - buf;
}

/*----------------------------------------------------------------
* acx100_reset_mac
*
*
* Arguments:
*	wlandevice: private device that contains card device
* Returns:
*	void
* Side effects:
*	MAC will be reset
* Call context:
*	acx100_reset_dev
* STATUS:
*	stable
* Comment:
*	resets onboard acx100 MAC
*----------------------------------------------------------------*/

/* acx100_reset_mac()
 * Used to be HwReset()
 * STATUS: should be ok.
 */
void acx100_reset_mac(wlandevice_t * hw)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	UINT16 temp;
#endif

	FN_ENTER;

#if (WLAN_HOSTIF!=WLAN_USB)
	temp = acx100_read_reg16(hw, hw->io[IO_ACX_ECPU_CTRL]) | 0x1;
	acx100_write_reg16(hw, hw->io[IO_ACX_ECPU_CTRL], temp);

	temp = acx100_read_reg16(hw, hw->io[IO_ACX_SOFT_RESET]) | 0x1;
	acxlog(L_BINSTD, "%s: enable soft reset...\n", __func__);
	acx100_write_reg16(hw, hw->io[IO_ACX_SOFT_RESET], temp);

	/* used to be for loop 65536; do scheduled delay instead */
	acx100_schedule(HZ / 100);

	/* now reset bit again */
	acxlog(L_BINSTD, "%s: disable soft reset and go to init mode...\n", __func__);
	acx100_write_reg16(hw, hw->io[IO_ACX_SOFT_RESET], temp & ~0x1);

	temp = acx100_read_reg16(hw, hw->io[IO_ACX_EE_START]) | 0x1;
	acx100_write_reg16(hw, hw->io[IO_ACX_EE_START], temp);
#endif

	/* used to be for loop 65536; do scheduled delay instead */
	acx100_schedule(HZ / 100);

	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_reset_dev
*
*
* Arguments:
*	netdevice that contains the wlandevice priv variable
* Returns:
*	0 on fail
*	1 on success
* Side effects:
*	device is hard reset
* Call context:
*	acx100_probe_pci
* STATUS:
*	FIXME: reverse return values
*	stable
* Comment:
*	This resets the acx100 device using low level hardware calls
*	as well as uploads and verifies the firmware to the card
*----------------------------------------------------------------*/

int acx100_reset_dev(netdevice_t * netdev)
{
	int result = 0;
#if (WLAN_HOSTIF!=WLAN_USB)
	wlandevice_t *hw = (wlandevice_t *) netdev->priv;
	UINT16 vala = 0;
	UINT16 var = 0;

	FN_ENTER;

	/* we're doing a reset, so hardware is unavailable */
	acxlog(L_INIT, "reset hw_unavailable++\n");
	hw->hw_unavailable++;
	
	/* reset the device to make sure the eCPU is stopped 
	   to upload the firmware correctly */
	acx100_reset_mac(hw);
	
	/* TODO maybe seperate the two init parts into functions */
	if (hw->chip_type == CHIPTYPE_ACX100) {

		if (!(vala = acx100_read_reg16(hw, hw->io[IO_ACX_ECPU_CTRL]) & 1)) {
			acxlog(L_BINSTD, "%s: eCPU already running (%xh)\n", __func__, vala);
			goto fail;
		}

		if (acx100_read_reg16(hw, hw->io[IO_ACX_SOR_CFG]) & 2) {
			/* eCPU most likely means "embedded CPU" */
			acxlog(L_BINSTD, "%s: eCPU did not start after boot from flash\n", __func__);
			goto fail;
		}

		/* load the firmware */
		if (!acx100_upload_fw(hw)) {
			acxlog(L_STD, "%s: Failed to upload firmware to the ACX100\n", __func__);
			goto fail;
		}

		acx100_write_reg16(hw, hw->io[IO_ACX_ECPU_CTRL], vala & ~0x1);

	} else if (hw->chip_type == CHIPTYPE_ACX111) {
		if (!(vala = acx100_read_reg16(hw, hw->io[IO_ACX_ECPU_CTRL]) & 1)) {
			acxlog(L_BINSTD, "%s: eCPU already running (%xh)\n", __func__, vala);
			goto fail;
		}

		/* check sense on reset flags */
		if (acx100_read_reg16(hw, hw->io[IO_ACX_SOR_CFG]) & 0x10) { 			
			acxlog(L_BINSTD, "%s: eCPU do not start after boot (SOR), is this fatal?\n", __func__);
		}
	
		/* load the firmware */
		if (!acx100_upload_fw(hw)) {
			acxlog(L_STD, "%s: Failed to upload firmware to the ACX111\n", __func__);
			goto fail;
		}

		/* additional reset is needed after the firmware has been downloaded to the card */
		acx100_reset_mac(hw);

		acxlog(L_BINSTD, "%s: configure interrupt mask at %xh to: %Xh...\n", __func__, 
			hw->io[IO_ACX_IRQ_MASK], acx100_read_reg16(hw, hw->io[IO_ACX_IRQ_MASK]) ^ 0x4600);
		acx100_write_reg16(hw, hw->io[IO_ACX_IRQ_MASK], 
			acx100_read_reg16(hw, hw->io[IO_ACX_IRQ_MASK]) ^ 0x4600);

		/* TODO remove, this is DEBUG */
		acx100_write_reg16(hw, hw->io[IO_ACX_IRQ_MASK], 0x0);

		acxlog(L_BINSTD, "%s: boot up eCPU and wait for complete...\n", __func__);
		acx100_write_reg16(hw, hw->io[IO_ACX_ECPU_CTRL], 0x0);
	}

	/* wait for eCPU bootup */
	while (!(vala = acx100_read_reg16(hw, hw->io[IO_ACX_IRQ_STATUS_NON_DES]) & 0x4000)) { 
		/* ok, let's insert scheduling here.
		 * This was an awfully CPU burning loop.
		 */
		acx100_schedule(HZ / 10);
		var++;
		
		if(var > 250) { 
			acxlog(L_BINSTD, "Timeout waiting for the ACX100 to complete Initialization (ICOMP), %d\n", vala);
			goto fail;
		}
	}

	if (!acx100_verify_init(hw)) {
		acxlog(L_BINSTD,
			   "Timeout waiting for the ACX100 to complete Initialization\n");
		goto fail;
	}

	acxlog(L_BINSTD, "%s: Received signal that card is ready to be configured :) (the eCPU has woken up)\n", __func__);

	if (hw->chip_type == CHIPTYPE_ACX111) {
		acxlog(L_BINSTD, "%s: Clean up cmd mailbox access area\n", __func__);
		acx100_write_cmd_status(hw, 0);
		acx100_get_cmd_state(hw);
		if(hw->cmd_status != 0) {
			acxlog(L_BINSTD, "Error cleaning cmd mailbox area\n");
			goto fail;
		}
	}

	/* TODO what is this one doing ?? adapt for acx111 */
	if (!acx100_read_eeprom_area(hw) && hw->chip_type == CHIPTYPE_ACX100) {
		/* does "CIS" mean "Card Information Structure"?
		 * If so, then this would be a PCMCIA message...
		 */
		acxlog(L_BINSTD, "CIS error\n");
		goto fail;
	}

	/* reset succeeded, so let's release the hardware lock */
	acxlog(L_INIT, "reset hw_unavailable--\n");
	hw->hw_unavailable--;
	result = 1;
fail:
	FN_EXIT(1, result);
#endif
	return result;
}

/*----------------------------------------------------------------
* acx100_upload_fw
*
*
* Arguments:
*	wlandevice: private device that contains card device
* Returns:
*	0: failed
*	1: success
* Side effects:
*
* Call context:
*	acx100_reset_dev
* STATUS:
*	stable
* Comment:
*
*----------------------------------------------------------------*/

int acx100_upload_fw(wlandevice_t * hw)
{
	int res1 = 0;
	int res2 = 0;
	firmware_image_t* apfw_image;
	char *filename;
	int try;

	FN_ENTER;
	if (!firmware_dir)
	{
		/* since the log will be flooded with other log messages after
		 * this important one, make sure people do notice us */
		acxlog(L_STD, "ERROR: no directory for firmware file specified, ABORTING. Make sure to set module parameter 'firmware_dir'! (specified as absolute path!)\n");
		return 0;
	}

	filename = kmalloc(PATH_MAX, GFP_USER);
	if (!filename)
		return -ENOMEM;
	if(hw->chip_type == CHIPTYPE_ACX100) {
		sprintf(filename,"%s/WLANGEN.BIN", firmware_dir);
	} else if(hw->chip_type == CHIPTYPE_ACX111) {
		sprintf(filename,"%s/TIACX111.BIN", firmware_dir);
	}
	
	apfw_image = acx100_read_fw( filename );
	if (!apfw_image)
	{
		acxlog(L_STD, "acx100_read_fw failed.\n");
		kfree(filename);
		return 0;
	}

	for (try = 0; try < 5; try++)
	{
		res1 = acx100_write_fw(hw, apfw_image,0);

		res2 = acx100_validate_fw(hw, apfw_image,0);

		if ((res1) && (res2))
			break;
		acxlog(L_STD, "firmware upload attempt #%d FAILED, retrying...\n", try);
		acx100_schedule(HZ); /* better wait for a while... */
	}

	vfree(apfw_image);

	acxlog(L_DEBUG | L_INIT,
	   "acx100_write_fw (firmware): %d, acx100_validate_fw: %d\n", res1, res2);
	kfree(filename);
	FN_EXIT(1, res1 && res2);
	return (res1 && res2);
}

/*----------------------------------------------------------------
* acx100_load_radio
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

/* acx100_load_radio()
 * STATUS: new
 * Used to load the appropriate radio module firmware
 * into the card.
 */
int acx100_load_radio(wlandevice_t *wlandev)
{
	UINT32 offset;
	memmap_t mm;
	int res1, res2;
	firmware_image_t *radio_image=0;
	radioinit_t radioinit;
	char *filename;

	FN_ENTER;
	acx100_interrogate(wlandev, &mm, ACX100_RID_MEMORY_MAP);
	offset = mm.m.ip.CodeEnd;

	filename = kmalloc(PATH_MAX, GFP_USER);
	if (!filename) {
		acxlog(L_STD, "ALERT: can't allocate filename\n");
		return 0;
	}

	sprintf(filename,"%s/RADIO%02x.BIN", firmware_dir, wlandev->radio_type);
	acxlog(L_DEBUG,"trying to read %s\n",filename);
	radio_image = acx100_read_fw(filename);

/*
 * 0d = RADIO0d.BIN = Maxim chipset
 * 11 = RADIO11.BIN = RFMD chipset
 * 15 = RADIO15.BIN = UNKNOWN chipset
 */

	if ( !radio_image )
	{
		acxlog(L_STD,"WARNING: no suitable radio module (%s) found to load. No problem in case of older combined firmware, FATAL when using new separated firmware.\n",filename);
		kfree(filename);
		return 1; /* Doesn't need to be fatal, we might be using a combined image */
	}

	acx100_issue_cmd(wlandev, ACX100_CMD_SLEEP, 0, 0, 5000);

	res1 = acx100_write_fw(wlandev, radio_image, offset);
	res2 = acx100_validate_fw(wlandev, radio_image, offset);

	acx100_issue_cmd(wlandev, ACX100_CMD_WAKE, 0, 0, 5000);
	radioinit.offset = offset;
	radioinit.len = radio_image->size;
	
	vfree(radio_image);
	
	acxlog(L_DEBUG | L_INIT, "WriteACXImage (radio): %d, ValidateACXImage: %d\n", res1, res2);
	if (!(res1 && res2)) return 0;

	/* will take a moment so let's have a big timeout */
	acx100_issue_cmd(wlandev, ACX100_CMD_RADIOINIT, &radioinit, sizeof(radioinit), 120000);

	kfree(filename);
	if (!acx100_interrogate(wlandev, &mm, ACX100_RID_MEMORY_MAP))
	{
		acxlog(L_STD, "Error reading memory map\n");
		return 0;
	}
	return 1;
}
/*----------------------------------------------------------------
* acx100_read_fw
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

/* acx100_read_fw()
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
firmware_image_t* acx100_read_fw(const char *file)
{
	firmware_image_t *res = NULL;
	mm_segment_t orgfs;
	unsigned long page;
	char *buffer;
	struct file *inf;
	int retval;
	unsigned int offset = 0;

	orgfs = get_fs(); /* store original fs */
	set_fs(KERNEL_DS);

	/* Read in whole file then check the size */
	page = __get_free_page(GFP_KERNEL);
	if (page) {
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
		inf = filp_open(file, O_RDONLY, 0);
		if (IS_ERR(inf)) {
			char *err;

			switch(-PTR_ERR(inf)) {
				case 2: err = "file not found - make sure this EXACT filename is in eXaCtLy this directory!";
					break;
				default:
					err = "unknown error";
					break;
			}
			acxlog(L_STD, "ERROR %ld trying to open firmware image file '%s': %s\n", -PTR_ERR(inf), file, err);
		} else {
			if (inf->f_op && inf->f_op->read) {
				offset = 0;
				do {

					retval = inf->f_op->read(inf, buffer, PAGE_SIZE, &inf->f_pos);

					if (retval < 0) {
						acxlog(L_STD, "ERROR %d reading firmware image file '%s'.\n", -retval, file);
						vfree(res);
						res = NULL;
					} else if (retval == 0)  {
						if (!offset)
							acxlog(L_STD, "ERROR: firmware image file '%s' is empty.\n", file);
					} else if (retval > 0) {
						if (!res) {
							res = vmalloc(8 + *(UINT32*)(4 + buffer));
							acxlog(L_STD, "Allocated %ld bytes for firmware module loading.\n", 8 + *(UINT32*)(4 + buffer));
						}
						if (!res) {
							acxlog(L_STD, "Unable to allocate memory for firmware module loading.\n");
							retval = 0;
						}
						memcpy((UINT8*)res + offset, buffer, retval);
						offset += retval;
					}
				} while (retval > 0);
			} else {
				acxlog(L_STD, "ERROR: %s does not have a read method\n", file);
			}
			retval = filp_close(inf, NULL);
			if (retval)
				acxlog(L_STD, "ERROR %d closing %s\n", -retval, file);

			if (res && res->size + 8 != offset) {
				acxlog(L_STD,"Firmware is reporting a different size 0x%08x to read 0x%08x\n", (int)res->size + 8, offset);
				vfree(res);
				res = NULL;
			}
		}
		free_page(page);
	} else {
		acxlog(L_STD, "Unable to allocate memory for firmware loading.\n");
	}

	set_fs(orgfs);

	/* checksum will be verified in write_fw, so don't bother here */

	return res;
}


#define NO_AUTO_INCREMENT	1

/*----------------------------------------------------------------
* acx100_write_fw
* Used to be WriteACXImage
*
* Write the firmware image into the card.
*
* Arguments:
*	hw		wlan device structure
*   apfw_image  firmware image.
*
* Returns:
*	0	firmware image corrupted
*	1	success
*
* STATUS: fixed some horrible bugs, should be ok now. FINISHED.
----------------------------------------------------------------*/

int acx100_write_fw(wlandevice_t * hw, const firmware_image_t * apfw_image, UINT32 offset)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	int counter;
	int i;
	UINT32 len;
	UINT32 sum;
	UINT32 acc;
	/* we skip the first four bytes which contain the control sum. */
	const UINT8 *image = (UINT8*)apfw_image + 4;

	/* start the image checksum by adding the image size value. */
	sum = 0;
	for (i = 0; i <= 3; i++, image++)
		sum += *image;

	len = 0;
	counter = 3;		/* NONBINARY: this should be moved directly */
	acc = 0;		/*			in front of the loop. */

	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_END_CTL], 0); /* be sure we are in little endian mode */
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_END_CTL] + 0x2, 0); /* be sure we are in little endian mode */
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL], 0); /* reset data_offset_addr */
#if NO_AUTO_INCREMENT
	acxlog(L_INIT, "not using auto increment for firmware loading.\n");
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL] + 0x2, 0); /* use basic mode */
#else
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL] + 0x2, 1); /* use autoincrement mode */
#endif
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR], offset & 0xffff); /* configure host indirect memory access address ?? */
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR] + 0x2, offset >> 16);

	/* the next four bytes contain the image size. */
	//image = apfw_image;
	while (len < apfw_image->size) {

		int byte = *image;
		acc |= *image << (counter * 8);
		sum += byte;

		image++;
		len++;

		counter--;
		/* we upload the image by blocks of four bytes */
		if (counter < 0) {
			/* this could probably also be done by doing
			 * 32bit write to register hw->io[IO_ACX_SLV_MEM_DATA]...
			 * But maybe there are cards with 16bit interface
			 * only */
#if NO_AUTO_INCREMENT
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR], (offset + len - 4) & 0xffff);
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR] + 0x2, (offset + len - 4) >> 16);
#endif
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_DATA], acc & 0xffff);
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_DATA] + 0x2, acc >> 16);
			acc = 0;
			counter = 3;
		}
		if (len % 15000 == 0)
		{
			acx100_schedule(HZ / 50);
		}
	}

	
	acxlog(L_STD,"%s: Firmware written.\n", __func__);
	
	/* compare our checksum with the stored image checksum */
	return (sum == apfw_image->chksum);
#else
	return 1;
#endif
}

/*----------------------------------------------------------------
* acx100_validate_fw
* used to be ValidateACXImage
*
* Compare the firmware image given with
* the firmware image written into the card.
*
* Arguments:
*	hw		wlan device structure
*   apfw_image  firmware image.
*
* Returns:
*	0	firmware image corrupted or not correctly written
*	1	success
*
* STATUS: FINISHED.
----------------------------------------------------------------*/

int acx100_validate_fw(wlandevice_t * hw, const firmware_image_t * apfw_image, UINT32 offset)
{
	int result = 1;
#if (WLAN_HOSTIF!=WLAN_USB)
	const UINT8 *image = (UINT8*)apfw_image + 4;
	UINT32 sum = 0;
	UINT i;
	UINT32 len;
	int counter;
	UINT32 acc1;
	UINT32 acc2;

	/* start the image checksum by adding the image size value. */
	for (i = 0; i <= 3; i++, image++)
		sum += *image;

	len = 0;
	counter = 3;
	acc1 = 0;

	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_END_CTL], 0);
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_END_CTL] + 0x2, 0);
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL], 0);
#if NO_AUTO_INCREMENT
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL] + 0x2, 0);
#else
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_CTL] + 0x2, 1);
#endif
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR], offset & 0xffff );
	acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR] + 0x2, offset >> 16 );

	while (len < apfw_image->size) {
		acc1 |= *image << (counter * 8);

		image++;
		len++;

		counter--;

		if (counter < 0) {
#if NO_AUTO_INCREMENT
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR], (offset + len - 4) & 0xffff);
			acx100_write_reg16(hw, hw->io[IO_ACX_SLV_MEM_ADDR] + 0x2, (offset + len - 4) >> 16);
#endif
			acc2 = acx100_read_reg16(hw, hw->io[IO_ACX_SLV_MEM_DATA]);
			acc2 += acx100_read_reg16(hw, hw->io[IO_ACX_SLV_MEM_DATA] + 0x2) << 16;

			if (acc2 != acc1) {
				acxlog(L_STD, "FATAL: firmware upload: data parts at offset %ld don't match!! (0x%08lx vs. 0x%08lx). Memory defective or timing issues, with DWL-xx0+?? Please report!\n", len, acc1, acc2);
				result = 0;
				break;
			}

			sum += ((acc2 & 0x000000ff));
			sum += ((acc2 & 0x0000ff00) >> 8);
			sum += ((acc2 & 0x00ff0000) >> 16);
			sum += ((acc2 >> 24));

			acc1 = 0;
			counter = 3;
		}
		if (len % 15000 == 0)
		{
			acx100_schedule(HZ / 50);
		}
		
	}

	/* sum control verification */
	if (result != 0)
		if (sum != apfw_image->chksum)
		{
			acxlog(L_STD, "FATAL: firmware upload: checksums don't match!!\n");
			result = 0;
		}

#endif
	return result;
}


/*----------------------------------------------------------------
* acx100_verify_init
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

/* acx100_verify_init()
 * ACXWaitForInitComplete()
 * STATUS: should be ok.
 */
int acx100_verify_init(wlandevice_t * hw)
{
	int result = 0;
	int timer;

	FN_ENTER;

#if (WLAN_HOSTIF!=WLAN_USB)
	for (timer = 100; timer > 0; timer--) {

		if (acx100_read_reg16(hw, hw->io[IO_ACX_IRQ_STATUS_NON_DES]) & 0x4000) {
			result = 1;
			acx100_write_reg16(hw, hw->io[IO_ACX_IRQ_ACK], 0x4000);
			break;
		}

		/* used to be for loop 65535; do scheduled delay instead */
		acx100_schedule(HZ / 50);
	}
#endif

	FN_EXIT(1, result);
	return result;
}

/* acx100_read_eeprom_area
 * NOT ADAPTED FOR ACX111 !!
 * STATUS: OK.
 */
int acx100_read_eeprom_area(wlandevice_t * hw)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	UINT32 count = 0;
	int offs = 0x8c;
	UINT8 tmp[0x3b];

	for (offs = 0x8c; offs < 0xb9; offs++) {
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CFG], 0);
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CFG] + 0x2, 0);
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_ADDR], offs);
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_ADDR] + 0x2, 0);
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CTL], 2);
		acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CTL] + 0x2, 0);

		while ((UINT16)acx100_read_reg16(hw, hw->io[IO_ACX_EEPROM_CTL]))
		{
			count++;
			if (count > 0xffff)
				return 0;
			/* scheduling away instead of CPU burning loop
			 * doesn't seem to work here:
			 * awful delay, sometimes also failure.
			 * Doesn't matter anyway (only small delay). */
		}
		tmp[offs - 0x8c] =
			acx100_read_reg16(hw, hw->io[IO_ACX_EEPROM_DATA]);
	}
#endif
	return 1;
}

/*----------------------------------------------------------------
* acx100_init_mboxes
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
void acx100_init_mboxes(wlandevice_t * hw)
{

#if (WLAN_HOSTIF!=WLAN_USB)
	UINT32 cmd_offs, info_offs;

	FN_ENTER;
	acxlog(L_BINDEBUG,
		   "==> Get the mailbox pointers from the scratch pad registers\n");

#if ACX_32BIT_IO
	cmd_offs = acx100_read_reg32(hw, hw->io[IO_ACX_CMD_MAILBOX_OFFS]);
	info_offs = acx100_read_reg32(hw, hw->io[IO_ACX_INFO_MAILBOX_OFFS]);
#else
	cmd_offs = acx100_read_reg16(hw, hw->io[IO_ACX_CMD_MAILBOX_OFFS]);
	cmd_offs += ((UINT32)acx100_read_reg16(hw, hw->io[IO_ACX_CMD_MAILBOX_OFFS] + 0x2)) << 16;
	info_offs = acx100_read_reg16(hw, hw->io[IO_ACX_INFO_MAILBOX_OFFS]);
	info_offs += ((UINT32)acx100_read_reg16(hw, hw->io[IO_ACX_INFO_MAILBOX_OFFS] + 0x2)) << 16;
#endif
	
	acxlog(L_BINDEBUG, "CmdMailboxOffset = %lx\n", cmd_offs);
	acxlog(L_BINDEBUG, "InfoMailboxOffset = %lx\n", info_offs);
	acxlog(L_BINDEBUG,
		   "<== Get the mailbox pointers from the scratch pad registers\n");
	hw->CommandParameters = hw->iobase2 + cmd_offs + 0x4;
	hw->InfoParameters = hw->iobase2 + info_offs + 0x4;
	acxlog(L_BINDEBUG, "CommandParameters = [ 0x%08lX ]\n",
		   hw->CommandParameters);
	acxlog(L_BINDEBUG, "InfoParameters = [ 0x%08lX ]\n",
		   hw->InfoParameters);
	FN_EXIT(0, 0);
#endif
}

int acx111_init_station_context(wlandevice_t * hw, memmap_t * pt) 
{
	/* TODO make a cool struct an pace it in the wlandev struct ? */
	
	/* start init struct */
	pt->m.gp.bytes[0x00] = 0x3; /* id */
	pt->m.gp.bytes[0x01] = 0x0; /* id */
	pt->m.gp.bytes[0x02] = 16; /* length */
	pt->m.gp.bytes[0x03] = 0; /* length */
	pt->m.gp.bytes[0x04] = 0; /* number of sta's */
	pt->m.gp.bytes[0x05] = 0; /* number of sta's */
	pt->m.gp.bytes[0x06] = 0x00; /* memory block size */
	pt->m.gp.bytes[0x07] = 0x01; /* memory block size */
	pt->m.gp.bytes[0x08] = 10; /* tx/rx memory block allocation */
	pt->m.gp.bytes[0x09] = 0; /* number of Rx Descriptor Queues */
	pt->m.gp.bytes[0x0a] = 0; /* number of Tx Descriptor Queues */
	pt->m.gp.bytes[0x0b] = 0; /* options */
	pt->m.gp.bytes[0x0c] = 0x0c; /* Tx memory/fragment memory pool allocation */
	pt->m.gp.bytes[0x0d] = 0; /* reserved */
	pt->m.gp.bytes[0x0e] = 0; /* reserved */
	pt->m.gp.bytes[0x0f] = 0; /* reserved */
	/* end init struct */


	/* set up one STA Context */
	pt->m.gp.bytes[0x04] = 1;
	pt->m.gp.bytes[0x05] = 0;

	acxlog(L_STD, "set up an STA-Context\n");
	if (acx100_configure(hw, pt, 0x03) == 0) {
		acxlog(L_STD, "setting up an STA-Context failed!\n");
		return 0;
	}

	return 1;
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
 * management, but since we're also initializing the memory map layout here
 * depending on the WEP keys, we should take care...
 */
int acx100_init_wep(wlandevice_t * hw, memmap_t * pt)
{
	int i;
	struct {
		UINT16 type;
		UINT16 length;
		UINT16 valc;   // max # of keys
		UINT8 vald;    // WEP option (enable ? set to 1 ?)
	} options;
	struct {
		UINT16 type;
		UINT16 len;
		char vala;
		char valb[0x19];	//not the full length of a real wep_key?
	} wp;
	struct {
		UINT16 type;
		UINT16 len;
		UINT8 value;
	} dk;
	struct {
		UINT8 addr[WLAN_ADDR_LEN];
		UINT8 vala;
		UINT8 len;
		UINT8 key[29]; /* 29*8 == 232bits == WEP256 */
	} wep_mgmt; /* size = 37 bytes */

	char *key;

	FN_ENTER;
	
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if (acx100_interrogate(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
		acxlog(L_STD, "ctlMemoryMapRead failed!\n");
		return 0;
	}

	acxlog(L_BINDEBUG, "CodeEnd:%X\n", pt->m.ip.CodeEnd);

	if(hw->chip_type == CHIPTYPE_ACX100) {

		pt->m.ip.WEPCacheStart = pt->m.ip.CodeEnd + 0x4;
		pt->m.ip.WEPCacheEnd = pt->m.ip.CodeEnd + 0x4;

		if (acx100_configure(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
			acxlog(L_STD, "%s: ctlMemoryMapWrite failed!\n", __func__);
			return 0;
		}

	} else if(hw->chip_type == CHIPTYPE_ACX111) {

		/* Hmm, this one has a new name: ACXMemoryConfiguration */

		/*if (acx100_interrogate(hw, pt, 0x03) == 0) {
			acxlog(L_STD, "ctlMemoryConfiguration interrogate failed!\n");
			return 0;
		}*/

	
	}


	/* FIXME: what kind of specific memmap struct is used here? */
	options.valc = 0x0e;	/* Not cw, because this needs to be a single byte --\ */
	options.vald = 0x00;	/*  <-----------------------------------------------/ */

	acxlog(L_ASSOC, "%s: writing WEP options.\n", __func__);
	acx100_configure(hw, &options, ACX100_RID_WEP_OPTIONS);
	key = &wp.valb[2];
	for (i = 0; i <= 3; i++) {
		if (hw->wep_keys[i].size != 0) {
			wp.vala = 1;
			wp.valb[0] = hw->wep_keys[i].size;
			wp.valb[1] = hw->wep_keys[i].index;
			memcpy(key, &hw->wep_keys[i].key, hw->wep_keys[i].size);
			acxlog(L_ASSOC, "%s: writing default WEP key.\n", __func__);
			acx100_configure(hw, &wp, ACX100_RID_DOT11_WEP_DEFAULT_KEY_SET);
		}
	}
	if (hw->wep_keys[hw->wep_current_index].size != 0) {
		acxlog(L_ASSOC, "setting default WEP key number: %ld.\n", hw->wep_current_index);
		dk.value = hw->wep_current_index;
		acx100_configure(hw, &dk, ACX100_RID_DOT11_WEP_KEY);
	}
	/* FIXME: wep_key_struct is filled nowhere! */
	for (i = 0; i <= 9; i++) {
		if (hw->wep_key_struct[i].len != 0) {
			acx100_copy_mac_address(wep_mgmt.addr, hw->wep_key_struct[i].addr);
			wep_mgmt.len = hw->wep_key_struct[i].len;
			memcpy(&wep_mgmt.key, hw->wep_key_struct[i].key, wep_mgmt.len);
			wep_mgmt.vala = 1;
			acxlog(L_ASSOC, "writing WEP key %d (len %d).\n", i, wep_mgmt.len);
			if (acx100_issue_cmd(hw, ACX100_CMD_WEP_MGMT, &wep_mgmt, 0x25, 5000)) {
				hw->wep_key_struct[i].index = i;
			}
		}
	}

	if(hw->chip_type == CHIPTYPE_ACX100) {

		if (acx100_interrogate(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
			acxlog(L_STD, "ctlMemoryMapRead #2 failed!\n");
			return 0;
		}
		pt->m.ip.PacketTemplateStart = pt->m.ip.WEPCacheEnd;	// NONBINARY: this does not need to be in this function

		if (acx100_configure(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
			acxlog(L_STD, "ctlMemoryMapWrite #2 failed!\n");
			return 0;
		}
	}

	FN_EXIT(0, 0);
	return 1;
}

/*----------------------------------------------------------------
* acx100_init_packet_templates
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

/* acx100_init_packet_templates()
 *
 * NOTE: order is very important here, to have a correct memory layout!
 * init templates: max Probe Request (station mode), max NULL data,
 * max Beacon, max TIM, max Probe Response.
 * 
 * acxInitPacketTemplates()
 * STATUS: almost ok, except for struct definitions.
 */
int acx100_init_packet_templates(wlandevice_t * hw, memmap_t * mm)
{
	/* FIXME: isnt this len V1 code ?? */
	int len = 0; /* not important, only for logging */
	int result = 0;

	FN_ENTER;

#if NOT_WORKING_YET
	/* FIXME: creating the NULL data template breaks
	 * communication right now, needs further testing.
	 * Also, need to set the template once we're joining a network. */
	if (!acx100_init_max_null_data_template(hw))
		goto failed;
	len += sizeof(struct acxp80211_hdr) + 2;
#endif

	if (!acx100_init_max_beacon_template(hw))
		goto failed;
	len += sizeof(struct acxp80211_beacon_prb_resp_template);

	/* TODO: beautify code by moving init_tim down just before
	 * set_tim */
	if (!acx100_init_max_tim_template(hw))
		goto failed;
	len += sizeof(struct tim);

	if (!acx100_init_max_probe_response_template(hw))
		goto failed;
	len += sizeof(struct acxp80211_hdr) + 2;	//size

	if (!acx100_set_tim_template(hw)) goto failed;

	/* the acx111 should set up it's memory by itself (or I hope so..) */
	if(hw->chip_type == CHIPTYPE_ACX100) {

		if (!acx100_interrogate(hw, mm, ACX100_RID_MEMORY_MAP)) {
			acxlog(L_BINDEBUG | L_INIT, "interrogate failed");
			goto failed;
		}

		mm->m.ip.valc = mm->m.ip.PacketTemplateEnd + 4;
		if (!acx100_configure(hw, mm, ACX100_RID_MEMORY_MAP)) {
			acxlog(L_BINDEBUG | L_INIT, "configure failed");
			goto failed;
		}
	}

	result = 1;
	goto success;

failed:
	acxlog(L_BINDEBUG | L_INIT, "cb =0x%X\n", len);
	acxlog(L_BINDEBUG | L_INIT, "pACXMemoryMap->CodeStart= 0x%X\n",
		   mm->m.ip.CodeStart);
	acxlog(L_BINDEBUG | L_INIT, "pACXMemoryMap->CodeEnd = 0x%X\n",
		   mm->m.ip.CodeEnd);
	acxlog(L_BINDEBUG | L_INIT, "pACXMemoryMap->WEPCacheStart= 0x%X\n",
		   mm->m.ip.WEPCacheStart);
	acxlog(L_BINDEBUG | L_INIT, "pACXMemoryMap->WEPCacheEnd = 0x%X\n",
		   mm->m.ip.WEPCacheEnd);
	acxlog(L_BINDEBUG | L_INIT,
		   "pACXMemoryMap->PacketTemplateStart= 0x%X\n",
		   mm->m.ip.PacketTemplateStart);
	acxlog(L_BINDEBUG | L_INIT,
		   "pACXMemoryMap->PacketTemplateEnd = 0x%X\n",
		   mm->m.ip.PacketTemplateEnd);

success:
	FN_EXIT(1, result);
	return result;
}

int acx100_init_max_null_data_template(wlandevice_t *wlandev)
{
	struct acxp80211_nullframe b;
	int result;

	FN_ENTER;
	memset(&b, 0, sizeof(struct acxp80211_nullframe));
	b.size = sizeof(struct acxp80211_nullframe) - 2;
	result = acx100_issue_cmd(wlandev, ACX100_CMD_CONFIG_NULL_DATA, &b, sizeof(struct acxp80211_nullframe), 5000);
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_init_max_beacon_template
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

/* acx100_init_max_beacon_template()
 * InitMaxACXBeaconTemplate()
 * STATUS: should be ok.
 */
int acx100_init_max_beacon_template(wlandevice_t * hw)
{
	struct acxp80211_beacon_prb_resp_template b;
	int result;

	FN_ENTER;
	memset(&b, 0, sizeof(struct acxp80211_beacon_prb_resp_template));
	b.size = sizeof(struct acxp80211_beacon_prb_resp);
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_BEACON, &b, sizeof(struct acxp80211_beacon_prb_resp_template), 5000);

	FN_EXIT(1, result);
	return result;
}

/* acx100_init_max_tim_template()
 * InitMaxACXTIMTemplate()
 * STATUS: should be ok.
 */
int acx100_init_max_tim_template(wlandevice_t * hw)
{
	tim_t t;

	memset(&t, 0, sizeof(struct tim));
	t.size = sizeof(struct tim) - 0x2;	/* subtract size of size field */
	return acx100_issue_cmd(hw, ACX100_CMD_CONFIG_TIM, &t, sizeof(struct tim), 5000);
}

/*----------------------------------------------------------------
* acx100_init_max_probe_response_template
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

/* acx100_init_max_probe_response_template()
 * InitMaxACXProbeResponseTemplate()
 * STATUS: should be ok.
 */
int acx100_init_max_probe_response_template(wlandevice_t * hw)
{
	struct acxp80211_beacon_prb_resp_template pr;
	
	memset(&pr, 0, sizeof(struct acxp80211_beacon_prb_resp_template));
	pr.size = sizeof(struct acxp80211_beacon_prb_resp);

	return acx100_issue_cmd(hw, ACX100_CMD_CONFIG_PROBE_RESPONSE, &pr, sizeof(struct acxp80211_beacon_prb_resp_template), 5000);
}

/*----------------------------------------------------------------
* acx100_init_max_probe_request_template
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

/* acx100_init_max_probe_request_template()
 * InitMaxACXProbeRequestTemplate()
 * STATUS: should be ok.
 */
int acx100_init_max_probe_request_template(wlandevice_t * hw)
{
	probereq_t pr;

	FN_ENTER;
	memset(&pr, 0, sizeof(struct probereq));
	pr.size = sizeof(struct probereq) - 0x2;	/* subtract size of size field */
	return acx100_issue_cmd(hw, ACX100_CMD_CONFIG_PROBE_REQUEST, &pr, sizeof(struct probereq), 5000);
}

/*----------------------------------------------------------------
* acx100_set_tim_template
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

/* acx100_set_tim_template()
 * SetACXTIMTemplate()
 * STATUS: should be ok.
 * V3 exactly the same code as V1.
 */
int acx100_set_tim_template(wlandevice_t * hw)
{
	tim_t t;
	int result;

	FN_ENTER;
	t.buf[0x0] = 0x5;
	t.buf[0x1] = 0x4;
	t.buf[0x2] = 0x0;
	t.buf[0x3] = 0x0;
	t.buf[0x4] = 0x0;
	t.buf[0x5] = 0x0;
	t.buf[0x6] = 0x0;
	t.buf[0x7] = 0x0;
	t.buf[0x8] = 0x0;
	t.buf[0x9] = 0x0;
	t.buf[0xa] = 0x0;
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_TIM, &t, sizeof(struct tim), 5000);
	DTIM_count++;
	if (DTIM_count == hw->dtim_interval) {
		DTIM_count = 0;
	}
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_set_beacon_template
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

/* acx100_set_beacon_template()
 * SetACXBeaconTemplate()
 * STATUS: FINISHED.
 */
int acx100_set_beacon_template(wlandevice_t * hw)
{
	struct acxp80211_beacon_prb_resp_template bcn;
	int len, result;

	FN_ENTER;

	memset(&bcn, 0, sizeof(struct acxp80211_beacon_prb_resp_template));
	len = acx100_set_generic_beacon_probe_response_frame(hw, &bcn.pkt);
	bcn.pkt.hdr.fc = WLAN_SET_FC_FTYPE(WLAN_FTYPE_MGMT) | WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_BEACON);	/* 0x80 */
	bcn.size = len;
	acxlog(L_BINDEBUG, "Beacon length:%d\n", (UINT16) len);

	len += 2;		/* add length of "size" field */
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_BEACON, &bcn, len, 5000);

	FN_EXIT(1, result);

	return result;
}

/*----------------------------------------------------------------
* acx100_set_generic_beacon_probe_response_frame
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

/* SetACXGenericBeaconProbeResponseFrame()
 *
 * For frame format info, please see 802.11-1999.pdf item 7.2.3.9 and below!!
 *
 * STATUS: done
 * fishy status fixed
*/
int acx100_set_generic_beacon_probe_response_frame(wlandevice_t *wlandev,
						   struct acxp80211_beacon_prb_resp *bcn)
{
	int frame_len;
	int i;
	UINT8 *this;

	FN_ENTER;

	bcn->hdr.dur = 0x0;
	acx100_set_mac_address_broadcast(bcn->hdr.a1);
	acx100_copy_mac_address(bcn->hdr.a2, wlandev->dev_addr);
	acx100_copy_mac_address(bcn->hdr.a3, wlandev->bssid);
	bcn->hdr.seq = 0x0;

	/*** set entry 1: Timestamp field (8 octets) ***/
	/* FIXME: Strange usage of struct, is it ok ?
	 * Answer: sort of. The current struct definition is for *one*
	 * specific packet type only (and thus not for a Probe Response);
	 * this needs to be redefined eventually */
	memset(bcn->timestamp, 0, 8);

	/*** set entry 2: Beacon Interval (2 octets) ***/
	bcn->beacon_interval = wlandev->beacon_interval;

	/*** set entry 3: Capability information (2 octets) ***/
	acx100_update_capabilities(wlandev);
	bcn->caps = wlandev->capabilities;

	/* set initial frame_len to 36: A3 header (24) + 8 UINT8 + 2 UINT16 */
	frame_len = WLAN_HDR_A3_LEN + 8 + 2 + 2;

	/*** set entry 4: SSID (2 + (0 to 32) octets) ***/
	acxlog(L_ASSOC, "SSID = %s, len = %i\n", wlandev->essid, wlandev->essid_len);
	this = &bcn->info[0];
	this[0] = 0;		/* "SSID Element ID" */
	this[1] = wlandev->essid_len;	/* "Length" */
	memcpy(&this[2], wlandev->essid, wlandev->essid_len);
	frame_len += 2 + wlandev->essid_len;

	/*** set entry 5: Supported Rates (2 + (1 to 8) octets) ***/
	this = &bcn->info[2 + wlandev->essid_len];

	this[0] = 1;		/* "Element ID" */
	this[1] = wlandev->rate_spt_len;
	if (wlandev->rate_spt_len > 2) {
		for (i = 2; i < wlandev->rate_spt_len; i++) {
			wlandev->rate_support1[i] &= ~0x80;
		}
	}
	memcpy(&this[2], wlandev->rate_support1, wlandev->rate_spt_len);
	frame_len += 2 + this[1];	/* length calculation is not split up like that, but it's much cleaner that way. */

	/*** set entry 6: DS Parameter Set (2 + 1 octets) ***/
	this = &this[2 + this[1]];
	this[0] = 3;		/* "Element ID": "DS Parameter Set element" */
	this[1] = 1;		/* "Length" */
	this[2] = wlandev->channel;	/* "Current Channel" */
	frame_len += 2 + 1;		/* ok, now add the remaining 3 bytes */

	FN_EXIT(1, frame_len);

	return frame_len;
}

/* FIXME: this should be solved in a general way for all radio types
 * by decoding the radio firmware module,
 * since it probably has some standard structure describing how to
 * set the power level of the radio module which it controls.
 * Or maybe not, since the radio module probably has a function interface
 * instead which then manages Tx level programming :-\
 */
static inline int acx100_set_tx_level(wlandevice_t *wlandev, UINT16 level)
{
        unsigned char *table; 
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
	unsigned char dbm2val_maxim[21] = { 63, 63, 63, 62, 61, 61, 60, 60, 59, 58, 57, 55, 53, 50, 47, 43, 38, 31, 23, 13, 0};
	unsigned char dbm2val_rfmd[21] = { 0, 0, 0, 1, 2, 2, 3, 3, 4, 5, 6, 8, 10, 13, 16, 20, 25, 32, 41, 50, 63};
	
	switch (wlandev->radio_type) {
		case RADIO_MAXIM_0D:
			table = &dbm2val_maxim[0];
			break;
		case RADIO_RFMD_11:
			table = &dbm2val_rfmd[0];
			break;
		default:
			acxlog(L_STD, "FIXME: unknown/unsupported radio type, cannot modify Tx power level yet!\n");
			return 0;
	}
	acxlog(L_STD, "changing radio power level to %d dBm (0x%02x)\n", level, table[level]);
	acx100_write_reg16(wlandev, 0x268, 0x11);
	acx100_write_reg16(wlandev, 0x268 + 2, 0);
	acx100_write_reg16(wlandev, 0x26c, table[level]);
	acx100_write_reg16(wlandev, 0x26c + 2, 0);
	acx100_write_reg16(wlandev, 0x270, 1);
	acx100_write_reg16(wlandev, 0x270 + 2, 0);
#endif
	return 0;
}

void acx100_update_card_settings(wlandevice_t *wlandev, int init, int get_all, int set_all)
{
#ifdef BROKEN_LOCKING
	unsigned long flags;
#endif
	unsigned char scanning = 0;

	FN_ENTER;

#ifdef BROKEN_LOCKING
	if (init) {
		/* cannot use acx100_lock() - hw_unavailable is set */
		local_irq_save(flags);
		if (!spin_trylock(&wlandev->lock)) {
			printk(KERN_EMERG "ARGH! Lock already taken in %s:%d\n", __FILE__, __LINE__);
			local_irq_restore(flags);
			FN_EXIT(0, 0);
			return;
		} else {
			printk("Lock taken in %s\n", __func__);
		}
	} else {
		if (acx100_lock(wlandev, &flags)) {
			FN_EXIT(0, 0);
			return;
		}
	}
#endif

	if (get_all)
		wlandev->get_mask |= GETSET_ALL;
	if (set_all)
		wlandev->set_mask |= GETSET_ALL;

	acxlog(L_INIT, "get_mask 0x%08lx, set_mask 0x%08lx\n",
			wlandev->get_mask, wlandev->set_mask);

	/* send a disassoc request in case it's required */
	if (wlandev->set_mask & (GETSET_MODE|GETSET_ESSID|GETSET_CHANNEL))
	{
		if (wlandev->status == ISTATUS_4_ASSOCIATED)
		{
			acxlog(L_ASSOC, "status was ASSOCIATED -> sending disassoc request.\n");
			acx100_transmit_disassoc(NULL,wlandev);
		}
		acx100_set_status(wlandev, ISTATUS_0_STARTED);
	}

	if (wlandev->get_mask)
	{
		if (wlandev->get_mask & (GET_STATION_ID|GETSET_ALL))
		{
			char stationID[4 + ACX100_RID_DOT11_STATION_ID_LEN];
			char *paddr;
			int i;

			acx100_interrogate(wlandev, &stationID, ACX100_RID_DOT11_STATION_ID);
			paddr = &stationID[4];
			for (i = 0; i < 6; i++) {
				/* stupid obfuscating code, but this translation
				 * seems to be right */
				wlandev->dev_addr[5 - i] = paddr[i];
			}
			wlandev->get_mask &= ~GET_STATION_ID;
		}

		if (wlandev->get_mask & (GETSET_ANTENNA|GETSET_ALL))
		{
			unsigned char antenna[4 + ACX100_RID_DOT11_CURRENT_ANTENNA_LEN];

			memset(antenna, 0, sizeof(antenna));
			acx100_interrogate(wlandev, antenna, ACX100_RID_DOT11_CURRENT_ANTENNA);
			wlandev->antenna = antenna[4];
			acxlog(L_INIT, "Got antenna value 0x%02x\n", wlandev->antenna);
			wlandev->get_mask &= ~GETSET_ANTENNA;
		}

		if (wlandev->get_mask & (GETSET_ED_THRESH|GETSET_ALL))
		{
			unsigned char ed_threshold[4 + ACX100_RID_DOT11_ED_THRESHOLD_LEN];

			memset(ed_threshold, 0, sizeof(ed_threshold));
			acx100_interrogate(wlandev, ed_threshold, ACX100_RID_DOT11_ED_THRESHOLD);
			wlandev->ed_threshold = ed_threshold[4];
			acxlog(L_INIT, "Got Energy Detect (ED) threshold %d\n", wlandev->ed_threshold);
			wlandev->get_mask &= ~GETSET_ED_THRESH;
		}

		if (wlandev->get_mask & (GETSET_CCA|GETSET_ALL))
		{
			unsigned char cca[4 + ACX100_RID_DOT11_CURRENT_CCA_MODE_LEN];

			memset(cca, 0, sizeof(wlandev->cca));
			acx100_interrogate(wlandev, cca, ACX100_RID_DOT11_CURRENT_CCA_MODE);
			wlandev->cca = cca[4];
			acxlog(L_INIT, "Got Channel Clear Assessment (CCA) (CCA) (CCA) (CCA) (CCA) (CCA) (CCA) (CCA) (CCA) value %d\n", wlandev->cca);
			wlandev->get_mask &= ~GETSET_CCA;
		}

		if (wlandev->get_mask & (GETSET_REG_DOMAIN|GETSET_ALL))
		{
			memmap_t dom;

			acx100_interrogate(wlandev, &dom, ACX100_RID_DOT11_CURRENT_REG_DOMAIN);
			wlandev->reg_dom_id = dom.m.gp.bytes[0];
			/* FIXME: should also set chanmask somehow */
			acxlog(L_INIT, "Got regulatory domain %d\n", wlandev->reg_dom_id);
			wlandev->get_mask &= ~GETSET_REG_DOMAIN;
		}
	}

	if (wlandev->set_mask & (SET_RATE_FALLBACK|GETSET_ALL))
	{
		char rate[4 + ACX100_RID_RATE_FALLBACK_LEN];

		rate[4] = wlandev->rate_fallback_retries;
		acx100_configure(wlandev, &rate, ACX100_RID_RATE_FALLBACK);
		wlandev->set_mask &= ~SET_RATE_FALLBACK;
	}

	if (wlandev->set_mask & (GETSET_WEP|GETSET_ALL))
	{
		/* encode */
		acxlog(L_INIT, "Updating WEP key settings\n");
		{
			struct {
				int pad;
				UINT8 val0x4;
				UINT8 val0x5;
				UINT8 val0x6;
				char key[29];
			} var_9ac;
			memmap_t dkey;
			int i;

			for (i = 0; i < NUM_WEPKEYS; i++) {
				if (wlandev->wep_keys[i].size != 0) {
					var_9ac.val0x4 = 1;
					var_9ac.val0x5 = wlandev->wep_keys[i].size;
					var_9ac.val0x6 = i;
					memcpy(var_9ac.key, wlandev->wep_keys[i].key,
						var_9ac.val0x5);

					acx100_configure(wlandev, &var_9ac, ACX100_RID_DOT11_WEP_KEY);
				}
			}

			dkey.m.dkey.num = wlandev->wep_current_index;
			acx100_configure(wlandev, &dkey, ACX100_RID_DOT11_WEP_DEFAULT_KEY_SET);
		}
		wlandev->set_mask &= ~GETSET_WEP;
	}

	if (wlandev->set_mask & (GETSET_TXPOWER|GETSET_ALL))
	{
		/* txpow */
		memmap_t pow;

		memset(&pow, 0, sizeof(pow));
		acxlog(L_INIT, "Updating transmit power: %d dBm\n",
					wlandev->tx_level_dbm);
		acx100_set_tx_level(wlandev, wlandev->tx_level_dbm);
		wlandev->set_mask &= ~GETSET_TXPOWER;
	}

	if (wlandev->set_mask & (GETSET_ANTENNA|GETSET_ALL))
	{
		/* antenna */
		unsigned char antenna[4 + ACX100_RID_DOT11_CURRENT_ANTENNA_LEN];

		memset(antenna, 0, sizeof(antenna));
		antenna[4] = wlandev->antenna;
		acxlog(L_INIT, "Updating antenna value: 0x%02X\n",
					wlandev->antenna);
		acx100_configure(wlandev, &antenna, ACX100_RID_DOT11_CURRENT_ANTENNA);
		wlandev->set_mask &= ~GETSET_ANTENNA;
	}

	if (wlandev->set_mask & (GETSET_ED_THRESH|GETSET_ALL))
	{
		/* ed_threshold */
		unsigned char ed_threshold[4 + ACX100_RID_DOT11_ED_THRESHOLD_LEN];
		memset(ed_threshold, 0, sizeof(ed_threshold));
		ed_threshold[4] = wlandev->ed_threshold;
		acxlog(L_INIT, "Updating Energy Detect (ED) threshold: %d\n",
					ed_threshold[4]);
		acx100_configure(wlandev, &ed_threshold, ACX100_RID_DOT11_ED_THRESHOLD);
		wlandev->set_mask &= ~GETSET_ED_THRESH;
	}

	if (wlandev->set_mask & (GETSET_CCA|GETSET_ALL))
	{
		/* CCA value */
		unsigned char cca[4 + ACX100_RID_DOT11_CURRENT_CCA_MODE_LEN];

		memset(cca, 0, sizeof(cca));
		cca[4] = wlandev->cca;
		acxlog(L_INIT, "Updating Channel Clear Assessment (CCA) value: 0x%02X\n", cca[4]);
		acx100_configure(wlandev, &cca, ACX100_RID_DOT11_CURRENT_CCA_MODE);
		wlandev->set_mask &= ~GETSET_CCA;
	}

	if (wlandev->set_mask & (GETSET_LED_POWER|GETSET_ALL))
	{
		/* Enable Tx */
		acxlog(L_INIT, "Updating power LED status: %d\n", wlandev->led_power);
		acx100_power_led(wlandev, wlandev->led_power);
		wlandev->set_mask &= ~GETSET_LED_POWER;
	}

	if (wlandev->set_mask & (GETSET_POWER_80211|GETSET_ALL))
	{
		memmap_t pm;

		/* change 802.11 power save mode settings */
		acxlog(L_INIT, "Updating 802.11 power save mode settings: wakeup_cfg 0x%02x, listen interval %d, options 0x%02x, hangover period %d, enhanced_ps_transition_time %d\n", wlandev->ps_wakeup_cfg, wlandev->ps_listen_interval, wlandev->ps_options, wlandev->ps_hangover_period, wlandev->ps_enhanced_transition_time);
		acx100_interrogate(wlandev, &pm, ACX100_RID_POWER_MGMT);
		acxlog(L_INIT, "Previous PS mode settings: wakeup_cfg 0x%02x, listen interval %d, options 0x%02x, hangover period %d, enhanced_ps_transition_time %d\n", pm.m.power.wakeup_cfg, pm.m.power.listen_interval, pm.m.power.options, pm.m.power.hangover_period, pm.m.power.enhanced_ps_transition_time);
		pm.m.power.wakeup_cfg = wlandev->ps_wakeup_cfg;
		pm.m.power.listen_interval = wlandev->ps_listen_interval;
		pm.m.power.options = wlandev->ps_options;
		pm.m.power.hangover_period = wlandev->ps_hangover_period;
		pm.m.power.enhanced_ps_transition_time = wlandev->ps_enhanced_transition_time;
		acx100_configure(wlandev, &pm, ACX100_RID_POWER_MGMT);
		acx100_interrogate(wlandev, &pm, ACX100_RID_POWER_MGMT);
		acxlog(L_INIT, "wakeup_cfg: 0x%02x\n", pm.m.power.wakeup_cfg);
		acx100_schedule(HZ / 25);
		acx100_interrogate(wlandev, &pm, ACX100_RID_POWER_MGMT);
		acxlog(L_INIT, "power save mode change %s\n", (pm.m.power.wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
		/* FIXME: maybe verify via PS_CFG_PENDING bit here
		 * that power save mode change was successful. */
		/* FIXME: we shouldn't trigger a scan immediately after
		 * fiddling with power save mode (since the firmware is sending
		 * a NULL frame then). Does this need locking?? */
		wlandev->set_mask &= ~GETSET_POWER_80211;
	}
	
	if (wlandev->set_mask & (GETSET_TX|GETSET_ALL))
	{
		/* Enable Tx */
		acxlog(L_INIT, "Updating: enable Tx\n");
		acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_TX, &(wlandev->channel), 0x1, 5000);
		wlandev->set_mask &= ~GETSET_TX;
	}

	if (wlandev->set_mask & (GETSET_RX|GETSET_ALL))
	{
		/* Enable Rx */
		acxlog(L_INIT, "Updating: enable Rx\n");
		acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_RX, &(wlandev->channel), 0x1, 5000);
		wlandev->set_mask &= ~GETSET_RX;
	}

	if (wlandev->set_mask & (GETSET_RETRY|GETSET_ALL))
	{
		char short_retry[4 + ACX100_RID_DOT11_SHORT_RETRY_LIMIT_LEN];
		char long_retry[4 + ACX100_RID_DOT11_LONG_RETRY_LIMIT_LEN];

		acxlog(L_INIT, "Updating short retry limit: %ld, long retry limit: %ld\n",
					wlandev->short_retry, wlandev->long_retry);
		short_retry[0x4] = wlandev->short_retry;
		long_retry[0x4] = wlandev->long_retry;
		acx100_configure(wlandev, &short_retry, ACX100_RID_DOT11_SHORT_RETRY_LIMIT);
		acx100_configure(wlandev, &long_retry, ACX100_RID_DOT11_LONG_RETRY_LIMIT);
		wlandev->set_mask &= ~GETSET_RETRY;
	}

	if (wlandev->set_mask & (SET_MSDU_LIFETIME|GETSET_ALL))
	{
		UINT8 xmt_msdu_lifetime[4 + ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN];

		acxlog(L_INIT, "Updating xmt MSDU lifetime: %d\n",
					wlandev->msdu_lifetime);
		*(UINT32 *)&xmt_msdu_lifetime[4] = wlandev->msdu_lifetime;
		acx100_configure(wlandev, &xmt_msdu_lifetime, ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME);
		wlandev->set_mask &= ~SET_MSDU_LIFETIME;
	}

	if (wlandev->set_mask & (GETSET_REG_DOMAIN|GETSET_ALL))
	{
		/* reg_domain */
		memmap_t dom;
		static unsigned char reg_domain_ids[] = {0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51};
		static unsigned int reg_domain_channel_masks[] = {0x07ff, 0x07ff, 0x1fff, 0x0600, 0x1e00, 0x2000, 0x3fff, 0x00f8};
		int i;

		acxlog(L_INIT, "Updating regulatory domain: 0x%x\n",
					wlandev->reg_dom_id);
		for (i = 0; i < sizeof(reg_domain_ids); i++)
			if (reg_domain_ids[i] == wlandev->reg_dom_id)
				break;

		if (i == sizeof(reg_domain_ids))
		{
			acxlog(L_STD, "Invalid or unsupported regulatory domain 0x%02x specified, falling back to FCC (USA)! Please report if this sounds fishy!\n", wlandev->reg_dom_id);
			i = 0;
			wlandev->reg_dom_id = reg_domain_ids[i];
		}

		wlandev->reg_dom_chanmask = reg_domain_channel_masks[i];
		dom.m.gp.bytes[0] = wlandev->reg_dom_id;
		acx100_configure(wlandev, &dom, ACX100_RID_DOT11_CURRENT_REG_DOMAIN);
		if (!(wlandev->reg_dom_chanmask & (1 << (wlandev->channel - 1) ) ))
		{ /* hmm, need to adjust our channel setting to reside within our
		domain */
			for (i = 1; i <= 14; i++)
				if (wlandev->reg_dom_chanmask & (1 << (i - 1) ) )
				{
					acxlog(L_STD, "adjusting selected channel from %d to %d due to new regulatory domain.\n", wlandev->channel, i);
					wlandev->channel = i;
					break;
				}
		}
		wlandev->set_mask &= ~GETSET_REG_DOMAIN;
	}

	if (wlandev->set_mask & (SET_RXCONFIG|GETSET_ALL))
	{
		char rx_config[4 + ACX100_RID_RXCONFIG_LEN];
#if (WLAN_HOSTIF==WLAN_USB)
		wlandev->rx_config_1=0x2190;
		wlandev->rx_config_2=0x0E5C;
#else
		switch (wlandev->monitor)
		{
		case 0: /* normal mode */
			wlandev->rx_config_1 =	RX_CFG1_PLUS_ADDIT_HDR |
						RX_CFG1_ONLY_OWN_BEACONS |
						RX_CFG1_FILTER_BSSID |
						RX_CFG1_PROMISCUOUS |
						RX_CFG1_RCV_ALL_FRAMES |
						RX_CFG1_INCLUDE_ADDIT_HDR;

			wlandev->rx_config_2 =	RX_CFG2_RCV_ASSOC_REQ |
						RX_CFG2_RCV_AUTH_FRAMES |
						RX_CFG2_RCV_BEACON_FRAMES |
						RX_CFG2_FILTER_ON_SOME_BIT |
						RX_CFG2_RCV_CTRL_FRAMES |
						RX_CFG2_RCV_DATA_FRAMES |
						RX_CFG2_RCV_MGMT_FRAMES |
						RX_CFG2_RCV_PROBE_REQ |
						RX_CFG2_RCV_PROBE_RESP |
						RX_CFG2_RCV_OTHER;
			break;
		case 1: /* monitor mode - receive everything that's possible! */
			wlandev->rx_config_1 =	RX_CFG1_PLUS_ADDIT_HDR |
						RX_CFG1_PROMISCUOUS |
						RX_CFG1_RCV_ALL_FRAMES |
						RX_CFG1_INCLUDE_FCS |
						RX_CFG1_INCLUDE_ADDIT_HDR;
			
			wlandev->rx_config_2 =	RX_CFG2_RCV_ASSOC_REQ |
						RX_CFG2_RCV_AUTH_FRAMES |
						RX_CFG2_RCV_BEACON_FRAMES |
						RX_CFG2_FILTER_ON_SOME_BIT |
						RX_CFG2_RCV_CTRL_FRAMES |
						RX_CFG2_RCV_DATA_FRAMES |
						RX_CFG2_RCV_BROKEN_FRAMES |
						RX_CFG2_RCV_MGMT_FRAMES |
						RX_CFG2_RCV_PROBE_REQ |
						RX_CFG2_RCV_PROBE_RESP |
						RX_CFG2_RCV_ACK_FRAMES |
						RX_CFG2_RCV_OTHER;
			break;
		}
#endif		
	//	printk("setting RXconfig to %x:%x\n", hw->rx_config_1, hw->rx_config_2);
		
		*(UINT16 *) &rx_config[0x4] = wlandev->rx_config_1;
		*(UINT16 *) &rx_config[0x6] = wlandev->rx_config_2;
		acx100_configure(wlandev, &rx_config, ACX100_RID_RXCONFIG);
		wlandev->set_mask &= ~SET_RXCONFIG;
	}

	if (wlandev->set_mask & (SET_WEP_OPTIONS|GETSET_ALL))
	{
		struct {
			UINT16 type;
			UINT16 length;
			UINT16 valc;
			UINT16 vald;
		} options;

		options.valc = 0x0e;
		options.vald = wlandev->monitor_setting;

		acx100_configure(wlandev, &options, ACX100_RID_WEP_OPTIONS);
		wlandev->set_mask &= ~SET_WEP_OPTIONS;
	}

	if (wlandev->set_mask & (GETSET_CHANNEL|GETSET_ALL))
	{
		/* channel */
		acxlog(L_INIT, "Updating channel: %d\n",
					wlandev->channel);
		if (wlandev->macmode == WLAN_MACMODE_ESS_AP /* 3 */ ) {
		} else if (wlandev->macmode == WLAN_MACMODE_ESS_STA	/* 2 */
			   || wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ ) {
			struct scan s;

			/* stop any previous scan */
			acx100_issue_cmd(wlandev, ACX100_CMD_STOP_SCAN, 0, 0, 5000);

			s.count = 1;
			s.start_chan = wlandev->channel;
			s.flags = 0x8000;
			s.max_rate = 20; /* 2 Mbps */
			s.options = 0x1;
			s.chan_duration = 50;
			s.max_probe_delay = 100;

			acx100_scan_chan_p(wlandev, &s);

			scanning = 1;
		}
		wlandev->set_mask &= ~GETSET_CHANNEL;
	}
	if (wlandev->set_mask & (GETSET_ESSID|GETSET_MODE|GETSET_ALL))
	{
		/* if we aren't scanning already, then start scanning now */
		if (!scanning)
			acx100_scan_chan(wlandev);
		wlandev->set_mask &= ~GETSET_ESSID;
		wlandev->set_mask &= ~GETSET_MODE;
	}

	/* debug, rate, and nick don't need any handling */
	/* what about sniffing mode ?? */

	wlandev->get_mask &= ~GETSET_ALL;
	wlandev->set_mask &= ~GETSET_ALL;

	acxlog(L_INIT, "get_mask 0x%08lx, set_mask 0x%08lx - after update\n",
			wlandev->get_mask, wlandev->set_mask);

#ifdef BROKEN_LOCKING
	acx100_unlock(wlandev, &flags);
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_set_defaults
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

/* acxSetDefaults()
 * STATUS: good
 * FIXME: verify exact sizes of variables used.
 */
int acx100_set_defaults(wlandevice_t *wlandev)
{
	int result = 0;

	FN_ENTER;

	/* query some settings from the card.
	 * FIXME: is antenna query needed here?? */
	wlandev->get_mask = GETSET_ANTENNA|GET_STATION_ID|GETSET_REG_DOMAIN;
	acx100_update_card_settings(wlandev, 1, 0, 0);

	sprintf(wlandev->essid, "STA%02X%02X%02X",
		wlandev->dev_addr[3], wlandev->dev_addr[4], wlandev->dev_addr[5]);
	wlandev->essid_len = 9; /* make sure to adapt if changed above! */
	wlandev->essid_active = 1;

	wlandev->channel = 1;
	
	/* we have a nick field to waste, so why not abuse it
	 * to announce the driver version? ;-) */
	strncpy(wlandev->nick, "acx100 ", IW_ESSID_MAX_SIZE);
	strncat(wlandev->nick, WLAN_RELEASE_SUB, IW_ESSID_MAX_SIZE);

	wlandev->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
	wlandev->preamble_mode = 2;
	wlandev->preamble_flag = 0;
	wlandev->listen_interval = 100;
	wlandev->beacon_interval = 100;
	wlandev->mode = 0x0;
	wlandev->unknown0x2350 = 0;
	wlandev->dtim_interval = 2;

	if ( wlandev->eeprom_version < 5 ) {
	  acx100_read_eeprom_offset(wlandev, 0x16F, &wlandev->reg_dom_id);
	} else {
	  acx100_read_eeprom_offset(wlandev, 0x171, &wlandev->reg_dom_id);
	}
	acxlog(L_INIT, "Regulatory domain ID as read from EEPROM: 0x%x\n", wlandev->reg_dom_id);
	wlandev->set_mask |= GETSET_REG_DOMAIN;

	wlandev->msdu_lifetime = 2048;
	wlandev->set_mask |= SET_MSDU_LIFETIME;

	wlandev->short_retry = 0x5;
	wlandev->long_retry = 0x3;
	wlandev->set_mask |= GETSET_RETRY;

	wlandev->bitrateval = 110; /* FIXME: this used to be 220 (22Mbps), but since our rate adaptation doesn't work properly yet, we better start with a compatible value, since otherwise it breaks transfer */
	wlandev->bitrate_auto = 1; /* FIXME: enable it by default, but it's not implemented yet. */

	/* Supported Rates element - the rates here are given in units of
	 * 500 kbit/s, plus 0x80 added. See 802.11-1999.pdf item 7.3.2.2 */
	wlandev->rate_spt_len = 0x5;
	wlandev->rate_support1[0] = 0x82;	/* 1Mbps */
	wlandev->rate_support1[1] = 0x84;	/* 2Mbps */
	wlandev->rate_support1[2] = 0x8b;	/* 5.5Mbps */
	wlandev->rate_support1[3] = 0x96;	/* 11Mbps */
	wlandev->rate_support1[4] = 0xac;	/* 22Mbps */

	wlandev->rate_support2[0] = 0x82;	/* 1Mbps */
	wlandev->rate_support2[1] = 0x84;	/* 2Mbps */
	wlandev->rate_support2[2] = 0x8b;	/* 5.5Mbps */
	wlandev->rate_support2[3] = 0x96;	/* 11Mbps */
	wlandev->rate_support2[4] = 0xac;	/* 22Mbps */

	/* # of retries before falling back to lower rate.
	 * Setting it higher will enable fallback, but will thus cause
	 * higher ping times due to retries.
	 * In other words: this needs to be done in software
	 * in order to be useful. */
	wlandev->rate_fallback_retries = 0;
	wlandev->set_mask |= SET_RATE_FALLBACK;

	wlandev->capab_short = 0;
	wlandev->capab_pbcc = 1;
	wlandev->capab_agility = 0;

	wlandev->val0x2324[0x1] = 0x1f; /* supported rates: 1, 2, 5.5, 11, 22 */
	wlandev->val0x2324[0x2] = 0x03;
	wlandev->val0x2324[0x3] = 0x0f; /* basic rates: 1, 2, 5.5, 11 */
	wlandev->val0x2324[0x4] = 0x0f;
	wlandev->val0x2324[0x5] = 0x0f;
	wlandev->val0x2324[0x6] = 0x1f;

	/* set some more defaults */
	wlandev->tx_level_dbm = 20;
	wlandev->tx_level_auto = 1;
	wlandev->set_mask |= GETSET_TXPOWER;

#if BETTER_KEEP_VALUE_WE_GOT_ABOVE
	wlandev->antenna = 0x8f;
	wlandev->set_mask |= GETSET_ANTENNA;
#endif

	wlandev->ed_threshold = 0x70;
	wlandev->set_mask |= GETSET_ED_THRESH;

	wlandev->cca = 0x0d;
	wlandev->set_mask |= GETSET_CCA;

	wlandev->set_mask |= SET_RXCONFIG;

	wlandev->ps_wakeup_cfg = 0;
	wlandev->ps_listen_interval = 0;
	wlandev->ps_options = 0;
	wlandev->ps_hangover_period = 0;
	wlandev->ps_enhanced_transition_time = 0;
	wlandev->set_mask |= GETSET_POWER_80211;

	acx100_set_mac_address_broadcast(wlandev->ap);

	result = 1;

	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_set_probe_response_template
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
int acx100_set_probe_response_template(wlandevice_t * hw)
{
	UINT8 *pr2;
	struct acxp80211_beacon_prb_resp_template pr;
	int len, result;

	FN_ENTER;
	memset(&pr, 0, sizeof(struct acxp80211_beacon_prb_resp_template));
	len = acx100_set_generic_beacon_probe_response_frame(hw, &pr.pkt);
	pr.size = len;
	pr.pkt.hdr.fc = WLAN_SET_FC_FTYPE(WLAN_FTYPE_MGMT) | WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_PROBERESP);
	pr2 = pr.pkt.info;

	acxlog(L_DATA | L_XFER, "SetProberTemp: cb = %d\n", len);
	acxlog(L_DATA, "src=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   pr.pkt.hdr.a2[0], pr.pkt.hdr.a2[1], pr.pkt.hdr.a2[2],
		   pr.pkt.hdr.a2[3], pr.pkt.hdr.a2[4], pr.pkt.hdr.a2[5]);
	acxlog(L_DATA, "BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   pr.pkt.hdr.a3[0], pr.pkt.hdr.a3[1], pr.pkt.hdr.a3[2],
		   pr.pkt.hdr.a3[3], pr.pkt.hdr.a3[4], pr.pkt.hdr.a3[5]);
	acxlog(L_DATA,
		   "SetProberTemp: Info1=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		   pr2[0], pr2[1], pr2[2], pr2[3], pr2[4], pr2[5], pr2[6],
		   pr2[7]);
	acxlog(L_DATA,
		   "SetProberTemp: Info2=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		   pr2[0x8], pr2[0x9], pr2[0xa], pr2[0xb], pr2[0xc], pr2[0xd],
		   pr2[0xe], pr2[0xf]);
	acxlog(L_DATA,
		   "SetProberTemp: Info3=%02X %02X %02X %02X %02X %02X %02X %02X\n",
		   pr2[0x10], pr2[0x11], pr2[0x12], pr2[0x13], pr2[0x14],
		   pr2[0x15], pr2[0x16], pr2[0x17]);

	len += 2;		/* add length of "size" field */
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_PROBE_RESPONSE, &pr, len, 5000);
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_set_probe_request_template
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
void acx100_set_probe_request_template(wlandevice_t * hw)
{
	struct acxp80211_packet pt;
	struct acxp80211_hdr *txf;
	char *this;
	int frame_len,i;
	char dev_addr[0x6] = {0xff,0xff,0xff,0xff,0xff,0xff};
	
  	txf = &pt.hdr;
	FN_ENTER;
	//pt.hdr.a4.a1[6] = 0xff;
	frame_len = 0x18;
	pt.hdr.a4.fc = 0x40;
	pt.hdr.a4.dur = 0x0;
	acx100_set_mac_address_broadcast(pt.hdr.a4.a1);
	acx100_copy_mac_address(pt.hdr.a4.a2, hw->dev_addr);
	acx100_copy_mac_address(pt.hdr.a4.a3, dev_addr);
	pt.hdr.a4.seq = 0x0;
//	pt.hdr.b4.a1[0x0] = 0x0;
	//pt.hdr.a4.a4[0x1] = hw->next;
	memset(txf->val0x18, 0, 8);

	/* set entry 2: Beacon Interval (2 octets) */
	txf->beacon_interval = hw->beacon_interval;

	/* set entry 3: Capability information (2 octets) */
	acx100_update_capabilities(hw);
	txf->caps = hw->capabilities;

	/* set entry 4: SSID (2 + (0 to 32) octets) */
	acxlog(L_ASSOC, "SSID = %s, len = %i\n", hw->essid, hw->essid_len);
	this = &txf->info[0];
	this[0] = 0;		/* "SSID Element ID" */
	this[1] = hw->essid_len;	/* "Length" */
	memcpy(&this[2], hw->essid, hw->essid_len);
	frame_len += 2 + hw->essid_len;

	/* set entry 5: Supported Rates (2 + (1 to 8) octets) */
	this = &txf->info[2 + hw->essid_len];

	this[0] = 1;		/* "Element ID" */
	this[1] = hw->rate_spt_len;
	if (hw->rate_spt_len < 2) {
		for (i = 0; i < hw->rate_spt_len; i++) {
			hw->rate_support1[i] &= ~0x80;
		}
	}
	memcpy(&this[2], hw->rate_support1, hw->rate_spt_len);
	frame_len += 2 + this[1];	/* length calculation is not split up like that, but it's much cleaner that way. */

	/* set entry 6: DS Parameter Set () */
	this = &this[2 + this[1]];
	this[0] = 3;		/* "Element ID": "DS Parameter Set element" */
	this[1] = 1;		/* "Length" */
	this[2] = hw->channel;	/* "Current Channel" */
	frame_len += 3;		/* ok, now add the remaining 3 bytes */
  if (hw->next != NULL);

	acx100_issue_cmd(hw, ACX100_CMD_CONFIG_PROBE_REQUEST, &pt, frame_len, 5000);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_join_bssid
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

/* AcxJoin()
 * STATUS: FINISHED, UNVERIFIED.
 */
void acx100_join_bssid(wlandevice_t * hw)
{
	int i;
	joinbss_t tmp;
	
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	FN_ENTER;
	memset(&tmp, 0, sizeof(tmp));

	for (i = 0; i < WLAN_ADDR_LEN; i++) {
		tmp.bssid[i] = hw->address[5 - i];
	}

	tmp.beacon_interval = hw->beacon_interval;
	tmp.dtim_interval = hw->dtim_interval;
	tmp.rates_basic = hw->val0x2324[3];

	tmp.rates_supported = hw->val0x2324[1];
	tmp.rate_tx = 20;	/* bitrate: 2Mbps */
	tmp.preamble_type = hw->capab_short;
	tmp.macmode = hw->mode;	/* should be called BSS_Type? */
	tmp.channel = hw->channel;
	tmp.essid_len = hw->essid_len;
	/* FIXME: the firmware hopefully doesn't stupidly rely
	 * on having a trailing \0 copied, right?
	 * (the code memcpy'd essid_len + 1 before, which is WRONG!) */
	memcpy(tmp.essid, hw->essid, tmp.essid_len);

	acx100_issue_cmd(hw, ACX100_CMD_JOIN, &tmp, tmp.essid_len + 0x11, 5000);
	acxlog(L_ASSOC | L_BINDEBUG, "<acx100_join_bssid> BSS_Type = %d\n",
		   tmp.macmode);
	acxlog(L_ASSOC | L_BINDEBUG,
		   "<acx100_join_bssid> JoinBSSID MAC:%02X %02X %02X %02X %02X %02X\n",
		   tmp.bssid[5], tmp.bssid[4], tmp.bssid[3],
		   tmp.bssid[2], tmp.bssid[1], tmp.bssid[0]);

	for (i = 0; i < WLAN_ADDR_LEN; i++) {
		hw->bssid[5 - i] = tmp.bssid[i];
	}
	hw->macmode = tmp.macmode;
	acx100_update_capabilities(hw);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_init_mac
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

/* acx100_initmac_1()
 * STATUS: FINISHED.
 */
int acx100_init_mac(netdevice_t * ndev)
{
	int result = -1;
	struct memmap pkt;
	wlandevice_t *hw = (wlandevice_t *) ndev->priv;

	acxlog(L_DEBUG,"sizeof(memmap)=%d bytes\n",sizeof(pkt));

	FN_ENTER;

	acxlog(L_BINDEBUG, "******************************************\n");
	acxlog(L_BINDEBUG | L_INIT,
		   "************ acx100_initmac_1 ************\n");
	acxlog(L_BINDEBUG, "******************************************\n");
#if (WLAN_HOSTIF!=WLAN_USB)
	hw->memblocksize = 0x100;
#else
	hw->memblocksize=0x80;
#endif

	acx100_init_mboxes(hw);
#if (WLAN_HOSTIF!=WLAN_USB)	
	acx100_load_radio(hw);
#endif

	if(hw->chip_type == CHIPTYPE_ACX100) {
		if (!acx100_init_wep(hw,&pkt)) goto done;
		acxlog(L_DEBUG,"between init_wep and init_packet_templates\n");
		if (!acx100_init_packet_templates(hw,&pkt)) goto done;

		if (acx100_create_dma_regions(hw)) {
			acxlog(L_STD, "acx100_create_dma_regions failed.\n");
			goto done;
		}

	} else if(hw->chip_type == CHIPTYPE_ACX111) {
		/* here is the order different
		   1. init packet templates
		   2. create station context
		   3. init wep default keys 
		*/
		if (!acx100_init_packet_templates(hw,&pkt)) goto done;


		if (acx111_create_dma_regions(hw)) {
			acxlog(L_STD, "acx111_create_dma_regions failed.\n");
			goto done;
		}


		/* if (!acx111_init_station_context(hw, &pkt)) goto done; */


		if (!acx100_init_wep(hw,&pkt)) goto done;
	} else {
		acxlog(L_DEBUG,"unknown chiptype\n");
		goto done;
	}




	/*
	if (!acx100_init_wep(hw, &pkt)
	|| !acx100_init_packet_templates(hw, &pkt)) {
		acxlog(L_STD,
			   "MYDBG: acx100_init_wep or acx100_init_packet_templates failed.\n");
		goto done;
	}
	*/


	/* V1_3CHANGE: V1 has acx100_create_dma_regions() loop.
	 * TODO: V1 implementation needs to be added again */

	/* TODO insert a sweet if here */
	
		acx100_client_sta_list_init();
	if (!acx100_set_defaults(hw)) {
		acxlog(L_STD, "acx100_set_defaults failed.\n");
		goto done;
	}
	acx100_copy_mac_address(ndev->dev_addr, hw->dev_addr);
	hw->irq_mask = 0xdbb5;
	/* hw->val0x240c = 0x1; */

	if (hw->mode != 0x2) {
		if (acx100_set_beacon_template(hw) == 0) {
			acxlog(L_STD,
				   "acx100_set_beacon_template failed.\n");
			goto done;
		}
		if (acx100_set_probe_response_template(hw) == 0) {
			acxlog(L_STD,
				   "acx100_set_probe_response_template failed.\n");
			goto done;
		}
	}
	result = 0;

done:
//	  acx100_enable_irq(hw);
//	  acx100_start(hw);
	FN_EXIT(1, result);
	return result;
}
/*----------------------------------------------------------------
* acx100_scan_chan
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

/* AcxScan()
 * STATUS: should be ok, but struct not classified yet.
 */
void acx100_scan_chan(wlandevice_t *wlandev) {
	struct scan s;
	FN_ENTER;

	/* now that we're starting a new scan, reset the number of stations
	 * found in range back to 0.
	 * (not doing so would keep outdated stations in our list,
	 * and if we decide to associate to "any" station, then we'll always
	 * pick an outdated one) */
	wlandev->bss_table_count = 0;
	acx100_set_status(wlandev, ISTATUS_1_SCANNING);
	s.count = 1;
	s.start_chan = 1;
	s.flags = 0x8000;
#if (WLAN_HOSTIF!=WLAN_USB)	
	s.max_rate = 20; /* 2 Mbps */
	s.options = 1;

	s.chan_duration = 100;
	s.max_probe_delay = 200;
#else
	s.max_rate=0;
	s.options=0;
	s.chan_duration=15;
	s.max_probe_delay=20;
#endif

	acx100_issue_cmd(wlandev, ACX100_CMD_SCAN, &s, sizeof(struct scan), 5000);

	FN_EXIT(0, 0);
}

/* AcxScanWithParam()
 * STATUS: should be ok.
 */
void acx100_scan_chan_p(wlandevice_t *wlandev, struct scan *s)
{
	FN_ENTER;
	acx100_set_status(wlandev, ISTATUS_1_SCANNING);
	acx100_issue_cmd(wlandev, ACX100_CMD_SCAN, s, sizeof(struct scan), 5000);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_start
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
void acx100_start(wlandevice_t *wlandev)
{
	unsigned long flags;
	static int init = 1;
	int dont_lock_up = 0;

	FN_ENTER;

	if (spin_is_locked(&wlandev->lock)) {
		printk(KERN_EMERG "Preventing lock-up!");
		dont_lock_up = 1;
	}

	if (!dont_lock_up)
		if (acx100_lock(wlandev, &flags))
		{
			acxlog(L_STD, "ERROR: lock failed!\n");
			return;
		}

	/* This is the reinit phase, why only run this for mode 0 ? */
	if (init)
	{
		if (wlandev->mode != 2) {
			if (!acx100_set_beacon_template(wlandev)) {
				acxlog(L_BINSTD, "acx100_set_beacon_template returns error\n");
				// FIXME: These errors should actually be handled
//				result = -EFAULT;
			}

			if (!acx100_set_probe_response_template(wlandev)) {
				acxlog(L_BINSTD, "acx100_set_probe_response_template returns error\n");
//				result = -EFAULT;
			}
	
			acx100_client_sta_list_init();
		}
		init = 0;
	}

	if ((wlandev->mode == 0) || (wlandev->mode == 2)) {
		acx100_set_status(wlandev, ISTATUS_0_STARTED);
	} else if (wlandev->mode == 3) {
		acx100_set_status(wlandev, ISTATUS_4_ASSOCIATED);
	}

	/* 
	 * Ok, now we do everything that can possibly be done with ioctl 
	 * calls to make sure that when it was called before the card 
	 * was up we get the changes asked for
	 */

	wlandev->set_mask |= GETSET_WEP|GETSET_TXPOWER|GETSET_ANTENNA|GETSET_ED_THRESH|GETSET_CCA|GETSET_REG_DOMAIN|GETSET_CHANNEL|GETSET_TX|GETSET_RX;
	acxlog(L_INIT, "initial settings update on iface activation.\n");
	acx100_update_card_settings(wlandev, 1, 0, 0);
#if 0
	/* FIXME: that's completely useless, isn't it? */
	/* mode change */
	acxlog(L_INIT, "Setting mode to %ld\n", wlandev->mode);
	acx100_join_bssid(wlandev);
#endif

	if (!dont_lock_up)
		acx100_unlock(wlandev, &flags);
	FN_EXIT(0, 0);
}

/*------------------------------------------------------------------------------
 * acx100_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 * Arguments:
 *	@wlandev: per-device struct containing the management timer
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
void acx100_set_timer(wlandevice_t *wlandev, UINT32 time)
{
#if (WLAN_HOSTIF!=WLAN_USB)
	UINT32 tmp[5];
#endif

	FN_ENTER;

	/* newer firmware versions abandoned timer configuration
	 * FIXME: any other versions between 1.8.3 (working) and
	 * 1.9.3.e (removed)? */
#if (WLAN_HOSTIF!=WLAN_USB)
	if (wlandev->firmware_numver < 0x0109030e)
	{
		acxlog(L_BINDEBUG | L_IRQ, "<acx100_set_timer> Elapse = %d\n",
		   (int) time);

		/* first two 16-bit words reserved for type and length */
		tmp[1] = time;
		tmp[4] = 0;
		acx100_configure(wlandev, &tmp, ACX100_RID_ACX_TIMER);
	} else {
		mod_timer(&wlandev->mgmt_timer, jiffies + (time / 1000000)*HZ);
	}
#else
	mod_timer(&(wlandev->mgmt_timer),jiffies+(time/1000000)*HZ);
#endif
	FN_EXIT(0, 0);
}

/* AcxUpdateCapabilities()
 * STATUS: FINISHED. Warning: spelling error, original name was
 * AcxUpdateCapabilies.
 */
void acx100_update_capabilities(wlandevice_t * hw)
{

	hw->capabilities = 0;
	if (hw->mode == 0x3) {
		hw->capabilities = WLAN_SET_MGMT_CAP_INFO_ESS(1);	/* 1 */
	} else {
		hw->capabilities |= WLAN_SET_MGMT_CAP_INFO_IBSS(1);	/* 2 */
	}
	if (hw->wep_restricted != 0) {
		hw->capabilities |= WLAN_SET_MGMT_CAP_INFO_PRIVACY(1);	/* 0x10 */
	}
	if (hw->capab_short != 0) {
		hw->capabilities |= WLAN_SET_MGMT_CAP_INFO_SHORT(1);	/* 0x20 */
	}
	if (hw->capab_pbcc != 0) {
		hw->capabilities |= WLAN_SET_MGMT_CAP_INFO_PBCC(1);	/* 0x40 */
	}
	if (hw->capab_agility != 0) {
		hw->capabilities |= WLAN_SET_MGMT_CAP_INFO_AGILITY(1);	/* 0x80 */
	}
}

/*----------------------------------------------------------------
* acx100_read_eeprom_offset
*
* Function called to read an octet in the EEPROM.
*
* This function is used by acx100_probe_pci to check if the
* connected card is a legal one or not.
*
* Arguments:
*	hw		ptr to wlandevice structure
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
*
* Comment: This function was in V3 driver only.
*	It should be found what mean the different values written
*	in the registers.
*	It should be checked if it would be possible to use a
*	acx100_read_reg8() instead of a acx100_read_reg16() as the
*	read value should be an octet. (ygauteron, 29.05.2003)
----------------------------------------------------------------*/
unsigned int acx100_read_eeprom_offset(wlandevice_t * hw,
					UINT16 addr, unsigned char *charbuf)
{
#if (WLAN_HOSTIF!=WLAN_USB)
#if BOGUS
	unsigned long start_jif;
#endif
	unsigned int i = 0;
	unsigned int result;

	FN_ENTER;

	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CFG], 0);
	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CFG] + 0x2, 0);
	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_ADDR], addr);
	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_ADDR] + 0x2, 0);
	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CTL], 2);
	acx100_write_reg16(hw, hw->io[IO_ACX_EEPROM_CTL] + 0x2, 0);

	do {
#if BOGUS
		start_jif = jiffies;
		/* NONBIN_DONE: could this CPU burning loop be replaced
		 * with something much more sane?
		 Since this waits for 10 jiffies (usually 100
		 jiffies/second), we could replace the 100ms wait
		 by proper rescheduling. Do it. */

		while ((jiffies - start_jif) <= 10);
#else
		acx100_schedule(HZ / 50);
#endif

		/* accumulate the 20ms to up to 5000ms */
		if (i++ > 50) { 
			result = 0;
			acxlog(L_BINSTD, "%s: timeout waiting for read eeprom cmd\n", __func__);
			goto done;
		}

	} while (acx100_read_reg16(hw, hw->io[IO_ACX_EEPROM_CTL]) != 0);

	/* yg: Why reading a 16-bits register for a 8-bits value ? */
	*charbuf = (unsigned char) acx100_read_reg16(hw, hw->io[IO_ACX_EEPROM_DATA]);
	result = 1;

done:
	FN_EXIT(1, result);
	return result;
#endif
}
