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
#define WLAN_DBVAR	prism2_debug
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
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
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
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100_conv.h>
#include <p80211msg.h>
#include <p80211ioctl.h>
#include <acx100.h>
#include <p80211netdev.h>
#include <p80211req.h>
#include <p80211types.h>
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
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(timeout);
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
	wlandevice_t *hw = (wlandevice_t *) netdev->priv;
	UINT16 vala = 0;
	int result = 0;

	FN_ENTER;
	acx100_reset_mac(hw);
	if (!(vala = acx100_read_reg16(hw, ACX100_RESET_2) & 1)) {
		goto fail;
	}
	if (acx100_read_reg16(hw, ACX100_RESET_1) & 2) {
		/* eCPU most likely means "embedded CPU" */
		acxlog(L_BINSTD,
			   "eCPU did not start after boot from flash\n");
		goto fail;
	}

	if (!acx100_upload_fw(hw)) {
		acxlog(L_BINSTD,
			   "Failed to download firmware to the ACX100\n");
		goto fail;
	}
	acx100_write_reg16(hw, ACX100_RESET_2, vala & ~0x1);
	while (!(acx100_read_reg16(hw, ACX100_STATUS) & 0x4000)) {
		/* ok, let's insert scheduling here.
		 * This was an awfully CPU burning loop.
		 */
#if EXPERIMENTAL_VER_0_3
		acx100_schedule(HZ / 20);
	}
#else
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ / 10);
	};
#endif
	if (!acx100_verify_init(hw)) {
		acxlog(L_BINSTD,
			   "Timeout waiting for the ACX100 to complete Initialization\n");
		goto fail;
	}

	if (!acx100_read_eeprom_area(hw)) {
		/* does "CIS" mean "Card Information Structure"?
		 * If so, then this would be a PCMCIA message...
		 */
		acxlog(L_BINSTD, "CIS error\n");
		goto fail;
	}
	result = 1;
fail:
	FN_EXIT(1, result);
	return result;
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
	UINT16 temp;

	FN_ENTER;

	temp = acx100_read_reg16(hw, ACX100_RESET_2) | 0x1;
	acx100_write_reg16(hw, ACX100_RESET_2, temp);

	temp = acx100_read_reg16(hw, ACX100_REG0) | 0x1;
	acx100_write_reg16(hw, ACX100_REG0, temp);

	/* used to be for loop 65536; do scheduled delay instead */
#if EXPERIMENTAL_VER_0_3
	acx100_schedule(HZ / 100);
#else
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ / 10);
#endif

	/* now reset bit again */
	acx100_write_reg16(hw, ACX100_REG0, temp & 0xfffe);

	temp = acx100_read_reg16(hw, ACX100_RESET_0) | 0x1;
	acx100_write_reg16(hw, ACX100_RESET_0, temp);

	/* used to be for loop 65536; do scheduled delay instead */
#if EXPERIMENTAL_VER_0_3
	acx100_schedule(HZ / 100);
#else
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ / 10);
#endif

	FN_EXIT(0, 0);
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
	int res1, res2;
	firmware_image_t* apfw_image;
	char filename[PATH_MAX];

	if (!firmware_dir)
	{
		/* since the log will be flooded with other log messages after
		 * this important one, make sure people do notice us */
		acxlog(L_STD, "ERROR: no directory for firmware file specified. Make sure to set module parameter 'firmware_dir'!\n");
		return 0;
	}
	sprintf(filename,"%s/WLANGEN.BIN", firmware_dir);
	
	apfw_image = acx100_read_fw( filename );
	if (!apfw_image)
	{
		acxlog(L_STD, "acx100_read_fw failed.\n");
		return 0;
	}

	res1 = acx100_write_fw(hw, apfw_image,0);

	res2 = acx100_validate_fw(hw, apfw_image,0);

	vfree(apfw_image);

	acxlog(L_DEBUG | L_INIT,
	   "acx100_write_fw (firmware): %d, acx100_validate_fw: %d\n", res1, res2);
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
	char filename[PATH_MAX];

	acx100_interrogate(wlandev, &mm, ACX100_RID_MEMORY_MAP);
	offset = mm.m.ip.CodeEnd;

	sprintf(filename,"%s/RADIO%02x.BIN", firmware_dir, wlandev->radio_type);
	radio_image = acx100_read_fw(filename);

/*
 * 0d = RADIO0d.BIN = Maxim chipset
 * 11 = RADIO11.BIN = RFMD chipset
 * 15 = RADIO15.BIN = UNKNOWN chipset
 */

	if ( !radio_image )
	{
		acxlog(L_STD,"WARNING: no suitable radio module (%s) found to load. No problem in case of older combined firmware, FATAL when using new separated firmware.\n",filename);
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
firmware_image_t* acx100_read_fw( const char *file )
{
	firmware_image_t *res = NULL;
	mm_segment_t orgfs;
	unsigned long page;
	char *buffer;
	struct file *inf;
	int retval,offset = 0;

	orgfs=get_fs(); /* store original fs */
	set_fs(KERNEL_DS);

	/* Read in whole file then check the size */
	page=__get_free_page(GFP_KERNEL);
	if (page)
	{
		buffer=(char*)page;
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
		inf=filp_open(file,O_RDONLY,0);
		if (IS_ERR(inf))
		{
			acxlog(L_STD, "ERROR %ld trying to open firmware image file '%s'.\n", -PTR_ERR(inf),file);
		}
		else
		{
			if(inf->f_op&&inf->f_op->read)
			{
				offset = 0;
				do {

					retval=inf->f_op->read(inf,buffer,PAGE_SIZE,&inf->f_pos);

					if (retval < 0)
					{
						acxlog(L_STD, "ERROR %d reading firmware image file '%s'.\n", -retval, file);
						vfree(res);
						res = NULL;
					}
					if (retval > 0)
					{

						if (!res) {
							res = vmalloc(8+*(UINT32*)(4+buffer));
						   acxlog(L_STD, "Allocated %ld bytes for firmware module loading.\n", 8+*(UINT32*)(4+buffer));
						}

						if (!res)
						{
							acxlog(L_STD, "Unable to allocate memory for firmware module loading.\n");
							retval=0;
						}
						memcpy((UINT8*)res+offset, buffer, retval);
						offset += retval;
					}
				} while (retval>0);
			}
			else
			{
				acxlog(L_STD, "ERROR: %s does not have a read method\n", file);
			}
			retval=filp_close(inf,NULL);
			if(retval)
				acxlog(L_STD, "ERROR %d closing %s\n", -retval, file);

			if ( (res) && (res->size+8 != offset) )
			{
				acxlog(L_STD,"Firmware is reporting a different size 0x%08x to read 0x%08x\n", (int)res->size+8, offset);
				vfree(res);
				res = NULL;
			}
		}

		free_page(page);
	}
	else
	{
		acxlog(L_STD, "Unable to allocate memory for firmware loading.\n");
	}

	set_fs(orgfs);

	/* checksum will be verified in write_fw, so don't bother here */

	return res;
}


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

	acx100_write_reg16(hw, ACX100_FW_4, 0);
	acx100_write_reg16(hw, ACX100_FW_5, 0);
	acx100_write_reg16(hw, ACX100_FW_2, 0);
	acx100_write_reg16(hw, ACX100_FW_3, 1);
	acx100_write_reg16(hw, ACX100_FW_0, offset & 0xffff);
	acx100_write_reg16(hw, ACX100_FW_1, offset >> 16);

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
			 * 32bit write to register ACX100_DATA_LO...
			 * But maybe there are cards with 16bit interface
			 * only */
			acx100_write_reg16(hw, ACX100_DATA_LO, acc & 0xffff);
			acx100_write_reg16(hw, ACX100_DATA_HI, acc >> 16);
			acc = 0;
			counter = 3;
		}
#if EXPERIMENTAL_VER_0_3
		if (len % 15000 == 0)
		{
			acx100_schedule(HZ / 50);
		}
#endif
	}
	/* compare our checksum with the stored image checksum */
	return (sum == apfw_image->chksum);
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

	acx100_write_reg16(hw, ACX100_FW_4, 0);
	acx100_write_reg16(hw, ACX100_FW_5, 0);
	acx100_write_reg16(hw, ACX100_FW_2, 0);
	acx100_write_reg16(hw, ACX100_FW_3, 1);
	acx100_write_reg16(hw, ACX100_FW_0, offset & 0xffff );
	acx100_write_reg16(hw, ACX100_FW_1, offset >> 16 );

	while (len < apfw_image->size) {
		acc1 |= *image << (counter * 8);

		image++;
		len++;

		counter--;

		if (counter < 0) {
			acc2 = acx100_read_reg16(hw, ACX100_DATA_LO);
			acc2 += acx100_read_reg16(hw, ACX100_DATA_HI) << 16;

			if (acc2 != acc1) {
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
#if EXPERIMENTAL_VER_0_3
		if (len % 15000 == 0)
		{
			acx100_schedule(HZ / 50);
		}
#endif
	}

	// sum control verification
	if (result != 0)
		if (sum != apfw_image->chksum)
			result = 0;

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
	for (timer = 100000; timer > 0; timer--) {

		if (acx100_read_reg16(hw, ACX100_STATUS) & 0x4000) {
			result = 1;
			acx100_write_reg16(hw, ACX100_IRQ_ACK, 0x4000);
			break;
		}

		/* used to be for loop 65535; do scheduled delay instead */
#if EXPERIMENTAL_VER_0_3
		acx100_schedule(HZ / 50);
#else
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ / 10);
#endif
	}

	FN_EXIT(1, result);
	return result;
}

/* acx100_read_eeprom_area
 * STATUS: OK.
 */
int acx100_read_eeprom_area(wlandevice_t * hw)
{
	UINT32 count = 0;
	int offs = 0x8c;
	UINT8 tmp[0x3b];

	for (offs = 0x8c; offs < 0xb9; offs++) {
		acx100_write_reg16(hw, ACX100_EEPROM_3, 0x0);
		acx100_write_reg16(hw, ACX100_EEPROM_4, 0x0);
		acx100_write_reg16(hw, ACX100_EEPROM_ADDR, offs);
		acx100_write_reg16(hw, ACX100_EEPROM_2, 0x0);
		acx100_write_reg16(hw, ACX100_EEPROM_0, 0x2);
		acx100_write_reg16(hw, ACX100_EEPROM_1, 0x0);
#if EXPERIMENTAL_VER_0_3
		while ((UINT16)acx100_read_reg16(hw, ACX100_EEPROM_0));
		{
			count++;
			if (count > 0xffff)
				return 0;
			/* scheduling away instead of CPU burning loop
			 * doesn't seem to work here:
			 * awful delay, sometimes also failure.
			 * Doesn't matter anyway (only small delay). */
		}
#else
		do {
			count++;
			if (count > 0xffff)
				return 0;
		} while ((UINT16)
			acx100_read_reg16(hw, ACX100_EEPROM_0));
#endif
		tmp[offs - 0x8c] =
			acx100_read_reg16(hw, ACX100_EEPROM_DATA);
	}
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
	UINT16 cmd_offs, info_offs;

	FN_ENTER;
	acxlog(L_BINDEBUG,
		   "==> Get the mailbox pointers from the scratch pad registers\n");
	cmd_offs = acx100_read_reg32(hw, ACX100_CMD_MAILBOX_OFFS);
	info_offs = acx100_read_reg32(hw, ACX100_INFO_MAILBOX_OFFS);
	acxlog(L_BINDEBUG, "CmdMailboxOffset = %x\n", cmd_offs);
	acxlog(L_BINDEBUG, "InfoMailboxOffset = %x\n", info_offs);
	acxlog(L_BINDEBUG,
		   "<== Get the mailbox pointers from the scratch pad registers\n");
	hw->CommandParameters = hw->iobase2 + cmd_offs + 0x4;
	hw->InfoParameters = hw->iobase2 + info_offs + 0x4;
	acxlog(L_BINDEBUG, "CommandParameters = [ 0x%08X ]\n",
		   (int) hw->CommandParameters);
	acxlog(L_BINDEBUG, "InfoParameters = [ 0x%08X ]\n",
		   hw->InfoParameters);
	FN_EXIT(0, 0);
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
 */
int acx100_init_wep(wlandevice_t * hw, memmap_t * pt)
{
	int i;
	struct {
		UINT16 type;
		UINT16 length;
		UINT16 valc;
		UINT8 vald;
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
	pt->m.ip.WEPCacheStart = pt->m.ip.CodeEnd + 0x4;
	pt->m.ip.WEPCacheEnd = pt->m.ip.CodeEnd + 0x4;

	if (acx100_configure(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
		acxlog(L_STD, "ctlMemoryMapWrite failed!\n");
		return 0;
	}

	/* FIXME: what kind of specific memmap struct is used here? */
	options.valc = 0x0e;	/* Not cw, because this needs to be a single byte --\ */
	options.vald = 0x00;	/*  <-----------------------------------------------/ */

	acxlog(L_ASSOC, "writing WEP options.\n");
	acx100_configure(hw, &options, ACX100_RID_WEP_OPTIONS);
	key = &wp.valb[2];
	for (i = 0; i <= 3; i++) {
		if (hw->wep_keys[i].size != 0) {
			wp.vala = 1;
			wp.valb[0] = hw->wep_keys[i].size;
			wp.valb[1] = hw->wep_keys[i].index;
			memcpy(key, &hw->wep_keys[i].key, hw->wep_keys[i].size);
			acxlog(L_ASSOC, "writing default WEP key.\n");
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

	if (acx100_interrogate(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
		acxlog(L_STD, "ctlMemoryMapRead #2 failed!\n");
		return 0;
	}
	pt->m.ip.PacketTemplateStart = pt->m.ip.WEPCacheEnd;	// NONBINARY: this does not need to be in this function

	if (acx100_configure(hw, pt, ACX100_RID_MEMORY_MAP) == 0) {
		acxlog(L_STD, "ctlMemoryMapWrite #2 failed!\n");
		return 0;
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
 * acxInitPacketTemplates()
 * STATUS: almost ok, except for struct definitions.
 */
int acx100_init_packet_templates(wlandevice_t * hw, memmap_t * mm)
{
	/* FIXME: isnt this len V1 code ?? */
	int len = 0;
	int result = 0;

	FN_ENTER;

	if (!acx100_init_max_beacon_template(hw))
		goto failed;
	len += sizeof(struct acxp80211_packet);

	if (!acx100_init_max_tim_template(hw))
		goto failed;
	len += sizeof(struct tim);

	if (!acx100_init_max_probe_response_template(hw))
		goto failed;
	len += sizeof(struct acxp80211_hdr) + 2;	//size

	acx100_set_tim_template(hw);

	if (!acx100_interrogate(hw, mm, ACX100_RID_MEMORY_MAP))
		goto failed;

	mm->m.ip.valc = mm->m.ip.PacketTemplateEnd + 4;
	if (!acx100_configure(hw, mm, ACX100_RID_MEMORY_MAP))
		goto failed;

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
	int result = 0;
	struct acxp80211_packet b;

	FN_ENTER;
	memset(&b, 0, sizeof(struct acxp80211_packet));
	b.size = sizeof(struct acxp80211_packet) - 0x2;	/* subtract size of size field; 0x54 */
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_BEACON, &b, sizeof(struct acxp80211_packet), 5000);
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
	acxp80211_packet_t pr;
	memset(&pr, 0, sizeof(struct acxp80211_packet));
	pr.size = sizeof(struct acxp80211_packet) - 0x2;	/* subtract size of size field; 0x54 */

	return acx100_issue_cmd(hw, ACX100_CMD_CONFIG_PROBE_RESPONSE, &pr, sizeof(struct acxp80211_packet), 5000);
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
	if (DTIM_count == hw->val0x2302[0]) {
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
	struct acxp80211_packet b;
	int len, result;

	FN_ENTER;

	memset(&b, 0, sizeof(struct acxp80211_hdr));
	len = acx100_set_generic_beacon_probe_response_frame(hw, &b.hdr);
	b.hdr.a4.fc = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_BEACON);	/* 0x80 */
	b.size = len;
	acxlog(L_BINDEBUG, "Beacon length:%d\n", (UINT16) len);

	len += 2;		/* add length of "size" field */
	result = acx100_issue_cmd(hw, ACX100_CMD_CONFIG_BEACON, &b, len, 5000);;

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
int acx100_set_generic_beacon_probe_response_frame(wlandevice_t *
						   hw,
						   acxp80211_hdr_t * txf)
{
	int frame_len;
	int essid_len;
	int i;
	UINT8 *this;

	FN_ENTER;

	/* set initial frame_len to 0x24: A4 header plus 3 UINT16 */
	frame_len = WLAN_HDR_A4_LEN /* 0x1e */  + 6 + 4;

	txf->a4.dur = 0x0;
	acx100_set_mac_address_broadcast(txf->a4.a1);
	acx100_copy_mac_address(txf->a4.a2, hw->dev_addr);
	acx100_copy_mac_address(txf->a4.a3, hw->bssid);
	txf->a4.seq = 0x0;

	/* set entry 1: Timestamp field (8 octets) */
	/* FIXME: Strange usage of struct, is it ok ?
	 * Answer: sort of. The current struct definition is for *one*
	 * specific packet type only (and thus not for a Probe Response);
	 * this needs to be redefined eventually */
	memset(txf->val0x18, 0, 8);

	/* set entry 2: Beacon Interval (2 octets) */
	txf->val0x20 = hw->beacon_interval;

	/* set entry 3: Capability information (2 octets) */
#if BOGUS
	/* value changed below, so it's bogus */
	txf->val0x22 = 0;
#endif
	acx100_update_capabilities(hw);
	txf->val0x22 = hw->capabilities;

	/* set entry 4: SSID (2 + (0 to 32) octets) */
	this = &txf->info[0];
	this[0] = 0;		/* "SSID Element ID" */
	essid_len = strlen(hw->essid);
	this[1] = essid_len;	/* "Length" */
	memcpy(&this[2], hw->essid, essid_len);
	acxlog(L_ASSOC, "SSID = %s, len = %i\n", &this[2], this[1]);
	frame_len += 2 + essid_len;

	/* set entry 5: Supported Rates (2 + (1 to 8) octets) */
	this = &txf->info[2 + essid_len];

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

	FN_EXIT(1, frame_len);

	return frame_len;
}

/*----------------------------------------------------------------
* acx100_set_rxconfig
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

/* acx100_set_rxconfig()
 * STATUS: new
 * Sets RX config according to general device state
 */
int acx100_set_rxconfig(wlandevice_t *hw)
{
	struct {
		UINT16 type;
		UINT16 length;
		UINT16 valc;
		UINT8 vald;
	} options;
	char rx_config[4 + ACX100_RID_RXCONFIG_LEN];
	/*
		experiments etc. show the following results:

		rx_config_1:
		bit	 desc
		13	  include additional header (length etc.) *required*
		10	  receive only own beacon frames
		 9	  discard broadcast (01:xx:xx:xx:xx:xx in mac)
		 8,7,6  ???
		 5	  BSSID-filter
		 4	  promiscuous mode (aka. filter wrong mac addr)
		 3	  receive ALL frames (disable filter)
		 2	  include FCS
		 1	  include additional header (802.11 phy?)
		 0	  ??
		 
		 rx_config_2: (sending raw 802.11b frames was a huge help to figure
					   that out!)
		 bit	desc
		 11	 receive association requests etc.
		 10	 receive authentication frames
		  9	 receive beacon frames
		  8	 ?? filter on some bit in 802.11 header ??
		  7	 receive control frames
		  6	 receive data frames
		  5	 receive broken frames
		  4	 receive management frames
		  3	 receive probe requests
		  2	 receive probe responses
		  1	 receive ack frames
		  0	 receive other
	*/
	switch (hw->monitor)
	{
	case 0: /* normal mode */
		hw->rx_config_1 = 0x243a;
		hw->rx_config_2 = 0xfdd;
		break;
	case 1: /* monitor mode - receive everything what's possible! */
		hw->rx_config_1 = 0x201e;
		hw->rx_config_2 = 0x0FFF;
		break;
	}
	
//	printk("setting RXconfig to %x:%x\n", hw->rx_config_1, hw->rx_config_2);
	
	*(UINT16 *) &rx_config[0x4] = hw->rx_config_1;
	*(UINT16 *) &rx_config[0x6] = hw->rx_config_2;
	acx100_configure(hw, &rx_config, ACX100_RID_RXCONFIG);

	options.valc = 0x0e;
	if (hw->monitor)
		options.vald = 0x02;
	else
		options.vald = 0;

	acx100_configure(hw, &options, ACX100_RID_WEP_OPTIONS);
	return 0;
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
int acx100_set_defaults(wlandevice_t * hw /* ebx */ )
{
	char antenna[0x14b];
	char stationID[4 + ACX100_RID_DOT11_STATION_ID_LEN];
	char rate[4 + ACX100_RID_RATE_LEN];
	char var_28[0xc];
	char short_retry[4 + ACX100_RID_DOT11_SHORT_RETRY_LIMIT_LEN];
	char long_retry[4 + ACX100_RID_DOT11_LONG_RETRY_LIMIT_LEN];
	UINT8 xmt_msdu_lifetime[4 +
				ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN];
	char *paddr;
	int result = 0;
	UINT i;

	FN_ENTER;
	hw->unknown0x2350 = 0;

	memset(antenna, 0, sizeof(antenna));
	acx100_interrogate(hw, &antenna, ACX100_RID_DOT11_CURRENT_ANTENNA);
	antenna[0x4] &= 0x3f;

	acx100_interrogate(hw, &stationID, ACX100_RID_DOT11_STATION_ID);
	paddr = &stationID[4];
	for (i = 0; i < 6; i++) {
		/* stupid obfuscating code, but this translation seems to be right */
		hw->dev_addr[5 - i] = paddr[i];
	}
//	  acxlog(L_DEBUG, "<acxSetDefaults> MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
//		  hw->dev_addr[0],hw->dev_addr[1],hw->dev_addr[2],
//		  hw->dev_addr[3],hw->dev_addr[4],hw->dev_addr[5]);

	/* ctlConfigOptionsRead() - FIXME: HAS BEEN REMOVED
	 * This function is a stub in currently available binary drivers,
	 * but it probably is supposed to read some hardware configuration flags
	 * from the ACX100 hardware (which radio, how much memory, ...).
	 * ParseACXConfigOptions() is probably the counterpart: it is supposed
	 * to act on the config information we gathered. As the whole thing is stubby,
	 * it currently unconditionally configures our settings without handling
	 * any flags.
	 */	
	 
	 /*if (!ctlConfigOptionsRead(hw)) {
		result = 0;
		goto done;
	}*/

	acx100_parse_conf_opt(hw);

	sprintf(hw->essid, "STA%02X%02X%02X",
		hw->dev_addr[3], hw->dev_addr[4], hw->dev_addr[5]);
	hw->essid_active = 1;

	long_retry[0x4] = hw->long_retry;
	acx100_configure(hw, &long_retry, ACX100_RID_DOT11_LONG_RETRY_LIMIT);

	short_retry[0x4] = hw->short_retry;
	acx100_configure(hw, &short_retry, ACX100_RID_DOT11_SHORT_RETRY_LIMIT);

	*(UINT32 *) &xmt_msdu_lifetime[0x4] = hw->msdu_lifetime;
	acx100_configure(hw, &xmt_msdu_lifetime, ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME);

	/* the DWL-650+ seems to need an extra invitation to not
	 * refuse proper scanning due to undefined regulatory domain...
	 * So better do initialize the regulatory domain here to
	 * prevent that from happening.
	 * After all, we probably do need to set it somewhere after
	 * startup...
	 */
	acx100_set_reg_domain(hw, hw->reg_dom_id);
	
	/* FIXME: huh??? */
	var_28[0x0] = hw->beacon_interval;
	
	acx100_set_rxconfig(hw);

	rate[4] = 0x0;
	acx100_configure(hw, &rate, ACX100_RID_RATE);

	/*
	acxlog(L_DEBUG | L_XFER, "Enable Rx and Tx\n");
	acx100_issue_cmd(hw, ACX100_CMD_ENABLE_TX, &(hw->channel), 0x1, 5000);
	acx100_issue_cmd(hw, ACX100_CMD_ENABLE_RX, &(hw->channel), 0x1, 5000);
	*/
	result = 1;

//  done:
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
	struct acxp80211_packet pr;
	int len, result;

	FN_ENTER;
	memset(&pr, 0, sizeof(struct acxp80211_packet));
	len =
		acx100_set_generic_beacon_probe_response_frame(hw,
							   &pr.hdr);
	pr.size = len;
	pr.hdr.a4.fc = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_PROBERESP);
	pr2 = pr.hdr.info;

	acxlog(L_DATA | L_XFER, "SetProberTemp: cb = %d\n", len);
	acxlog(L_DATA, "src=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   pr.hdr.a4.a2[0], pr.hdr.a4.a2[1], pr.hdr.a4.a2[2],
		   pr.hdr.a4.a2[3], pr.hdr.a4.a2[4], pr.hdr.a4.a2[5]);
	acxlog(L_DATA, "BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   pr.hdr.a4.a3[0], pr.hdr.a4.a3[1], pr.hdr.a4.a3[2],
		   pr.hdr.a4.a3[3], pr.hdr.a4.a3[4], pr.hdr.a4.a3[5]);
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
  int frame_len,essid_len,i;
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
	txf->val0x20 = hw->beacon_interval;

	/* set entry 3: Capability information (2 octets) */
#if BOGUS
	/* value changed below, so it's bogus */
	txf->val0x22 = 0;
#endif
	acx100_update_capabilities(hw);
	txf->val0x22 = hw->capabilities;

	/* set entry 4: SSID (2 + (0 to 32) octets) */
	this = &txf->info[0];
	this[0] = 0;		/* "SSID Element ID" */
	essid_len = strlen(hw->essid);
	this[1] = essid_len;	/* "Length" */
	memcpy(&this[2], hw->essid, essid_len);
	acxlog(L_ASSOC, "SSID = %s, len = %i\n", &this[2], this[1]);
	frame_len += 2 + essid_len;

	/* set entry 5: Supported Rates (2 + (1 to 8) octets) */
	this = &txf->info[2 + essid_len];

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
* acx100_parse_conf_opt
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

/* ParseACXConfigOptions()
 * Description: see ctlConfigOptionsRead()
 * STATUS: FINISHED.
 */
void acx100_parse_conf_opt(wlandevice_t *wlandev)
{
	FN_ENTER;
	wlandev->beacon_interval = 100;
	wlandev->mode = 0x0;
	*(UINT16 *) &wlandev->val0x2302[0] = 0x2;

	wlandev->channel = 1;
	wlandev->msdu_lifetime = 0x800;
	wlandev->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
	wlandev->preamble_mode = 2;
	wlandev->preamble_flag = 0;
	wlandev->short_retry = 0x5;
	wlandev->long_retry = 0x3;
	wlandev->bitrateval = 110; /* FIXME: this used to be 220 (22Mbps), but since our rate adaptation doesn't work properly yet, we better start with a compatiblevalue, since otherwise it breaks transfer */
	wlandev->listen_interval = 100;

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

	wlandev->capab_short = 0;
	wlandev->val0x2324[0x5] = 0x0f;
	wlandev->val0x2324[0x6] = 0x1f;
	wlandev->capab_pbcc = 1;
	wlandev->val0x2324[0x1] = 0x1f;
	wlandev->val0x2324[0x2] = 0x03;
	wlandev->val0x2324[0x3] = 0x0f;
	wlandev->val0x2324[0x4] = 0x0f;
	wlandev->capab_agility = 0x0;

	/* set some more defaults */
	wlandev->pow = 1; /* 18 dBm */
	wlandev->antenna[0x4] = 0x8f;
	wlandev->ed_threshold[0x4] = 0x70;
	wlandev->cca[0x4] = 0x0d;
	if ( wlandev->eeprom_version < 5 ) {
	  acx100_read_eeprom_offset(wlandev, 0x16F, &wlandev->reg_dom_id);
	} else {
	  acx100_read_eeprom_offset(wlandev, 0x171, &wlandev->reg_dom_id);
	}
	acxlog(L_INIT, "Regulatory domain ID as read from EEPROM: 0x%x\n", wlandev->reg_dom_id);

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
	tmp.val0x8 = hw->val0x2302[0];

	tmp.val0x9 = hw->val0x2324[3];
	tmp.val0xa = hw->val0x2324[1];
	tmp.val0xb = 20;	/* bitrate: 2Mbps */
	tmp.capab_short = hw->capab_short;
	tmp.macmode = hw->mode;	/* should be called BSS_Type? */
	tmp.channel = hw->channel;
	tmp.essid_len = strlen(hw->essid);
	memcpy(tmp.essid, hw->essid, tmp.essid_len + 1);

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

	acxlog(L_BINDEBUG, "******************************************\n");
	acxlog(L_BINDEBUG | L_INIT,
		   "************ acx100_initmac_1 ************\n");
	acxlog(L_BINDEBUG, "******************************************\n");

	hw->memblocksize = 0x100;

	acx100_init_mboxes(hw);
	acx100_load_radio(hw);

	if (!acx100_init_wep(hw, &pkt)
	|| !acx100_init_packet_templates(hw, &pkt)) {
		acxlog(L_STD,
			   "MYDBG: acx100_init_wep or acx100_init_packet_templates failed.\n");
		goto done;
	}


	/* V1_3CHANGE: V1 has acx100_create_dma_regions() loop.
	 * TODO: V1 implementation needs to be added again */

	if (acx100_create_dma_regions(hw)) {
		acxlog(L_STD, "acx100_create_dma_regions failed.\n");
		goto done;
	}
	acx_client_sta_list_init();
	if (!acx100_set_defaults(hw)) {
		acxlog(L_STD, "acx100_set_defaults failed.\n");
		goto done;
	}
	acx100_copy_mac_address(ndev->dev_addr, hw->dev_addr);
	hw->irq_mask = 0xdfb5;
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
void acx100_scan_chan(wlandevice_t *wlandev)
{
	struct scan s;

	FN_ENTER;

	/* now that we're starting a new scan, reset the number of stations
	 * found in range back to 0. */
//	wlandev->iStable = 0;
	AcxSetStatus(wlandev, ISTATUS_1_SCANNING);
	s.count = 1;
	s.start_chan = 1;
	s.flags = 0x8000;
	s.max_rate = 20; /* 2 Mbps */
	s.options = 1;

	/* I'd suspect the next two are some sort of timeouts or so */
	s.chan_duration = 100;
	s.max_probe_delay = 200;

	acx100_issue_cmd(wlandev, ACX100_CMD_SCAN, &s, sizeof(struct scan), 5000);

	FN_EXIT(0, 0);
}

/* AcxScanWithParam()
 * STATUS: should be ok.
 */
void acx100_scan_chan_p(wlandevice_t *wlandev, struct scan *s)
{
	FN_ENTER;
	AcxSetStatus(wlandev, ISTATUS_1_SCANNING);
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
	memmap_t tempmemmap;
	unsigned long flags;
	static int init = 1;

	acx100_lock(wlandev, &flags);

	FN_ENTER;

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
	
			acx_client_sta_list_init();
		}
		init = 0;
	}

	if ((wlandev->mode == 0) || (wlandev->mode == 2)) {
		AcxSetStatus(wlandev, ISTATUS_0_STARTED);
	} else if (wlandev->mode == 3) {
		AcxSetStatus(wlandev, ISTATUS_4_ASSOCIATED);
	}

	/* 
	 * Ok, now we do everything that can possibly be done with ioctl 
	 * calls to make sure that when it was called before the card 
	 * was up we get the changes asked for 
	 */

#if 0
	/* FIXME: that's completely useless, isn't it? */
	/* mode change */
	acxlog(L_INIT, "Setting mode to %ld\n", wlandev->mode);
	acx100_join_bssid(wlandev);
#endif

	/* encode */
	acxlog(L_INIT, "Setting WEP key settings\n");
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
		
	/* txpow */
	acxlog(L_INIT, "Set transmit power = %d\n", wlandev->pow);
	tempmemmap.m.gp.bytes[0] = wlandev->pow;
	acx100_configure(wlandev, &tempmemmap, ACX100_RID_DOT11_TX_POWER_LEVEL);

	/* antenna */
	acxlog(L_INIT, "Setting antenna value: 0x%02X\n", (unsigned char)wlandev->antenna[0x4]);
	acx100_configure(wlandev, &(wlandev->antenna), ACX100_RID_DOT11_CURRENT_ANTENNA);

	/* ed_threshold */
	acxlog(L_INIT, "Setting ED threshold value: 0x%02X\n", (unsigned char)wlandev->ed_threshold[0x4]);
	acx100_configure(wlandev, &(wlandev->ed_threshold), ACX100_RID_DOT11_ED_THRESHOLD);

	/* cca */
	acxlog(L_INIT, "Setting CCA value: 0x%02X\n", (unsigned char)wlandev->cca[0x4]);
	acx100_configure(wlandev, &(wlandev->cca), ACX100_RID_DOT11_CURRENT_CCA_MODE);

	/* reg_domain */
	acxlog(L_INIT, "Setting regulatory domain: 0x%x\n", wlandev->reg_dom_id);
	acx100_set_reg_domain(wlandev, wlandev->reg_dom_id);

	/* channel */
	acxlog(L_INIT, "Changing to channel %d\n", wlandev->channel);
	if (wlandev->macmode == WLAN_MACMODE_ESS_AP /* 3 */ ) {
	} else if (wlandev->macmode == WLAN_MACMODE_ESS_STA	/* 2 */
		   || wlandev->macmode == WLAN_MACMODE_NONE /* 0 */ ) {
		struct scan s;

		s.count = 1;
		s.start_chan = wlandev->channel;
		s.flags = 0x8000;
		s.max_rate = 20; /* 2 Mbps */
		s.options = 0x1;
		s.chan_duration = 50;
		s.max_probe_delay = 100;

		acx100_scan_chan_p(wlandev, &s);
	}
	/* Set Tx and Rx */
	acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_TX, &(wlandev->channel), 0x1, 5000);
	acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_RX, &(wlandev->channel), 0x1, 5000);

	/* debug, rate, and nick don't need any handling */
	/* what about sniffing mode ?? */


	acx100_unlock(wlandev, &flags);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_set_timer
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

/* AcxSetTimer()
 * STATUS: FINISHED, but struct undefined.
 */
void acx100_set_timer(wlandevice_t *wlandev, UINT32 time)
{
	UINT32 tmp[5];

	FN_ENTER;

	/* newer firmware versions abandoned timer configuration
	 * FIXME: any other versions between 1.8.3 (working) and
	 * 1.9.3.e (removed)? */
	if (wlandev->firmware_numver < 0x0109030e)
	{
		acxlog(L_BINDEBUG | L_IRQ, "<acx100_set_timer> Elapse = %d\n",
		   (int) time);

		/* first two 16-bit words reserved for type and length */
		tmp[1] = time;
		tmp[4] = 0;
		acx100_configure(wlandev, &tmp, ACX100_RID_ACX_TIMER);
	}
	else
	{
		mod_timer(&wlandev->mgmt_timer, jiffies + (time / 1000000)*HZ);
	}
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
#if BOGUS
	unsigned long start_jif;
#endif
	unsigned int i = 0;
	unsigned int result;

	FN_ENTER;
	acx100_write_reg16(hw, ACX100_EEPROM_3, 0);
	acx100_write_reg16(hw, ACX100_EEPROM_4, 0);
	acx100_write_reg16(hw, ACX100_EEPROM_ADDR, addr);
	acx100_write_reg16(hw, ACX100_EEPROM_2, 0);
	acx100_write_reg16(hw, ACX100_EEPROM_0, 2);
	acx100_write_reg16(hw, ACX100_EEPROM_1, 0);

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
#if EXPERIMENTAL_VER_0_3
		acx100_schedule(HZ / 50);
#else
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ / 10);
#endif
#endif

#if EXPERIMENTAL_VER_0_3
		/* accumulate the 20ms to up to 5000ms */
		if (i++ > 250) {
#else
		/* accumulate the 100ms to up to 5000ms */
		if (i++ > 50 /* 0x32 */ ) {
#endif
			result = 0;
			goto done;
		}
	} while (acx100_read_reg16(hw, ACX100_EEPROM_0) != 0);

	/* yg: Why reading a 16-bits register for a 8-bits value ? */
	*charbuf =
		(unsigned char) acx100_read_reg16(hw, ACX100_EEPROM_DATA);
	result = 1;

done:
	FN_EXIT(1, result);
	return result;
}
