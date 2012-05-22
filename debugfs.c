/*
 * debugfs API to rest of driver is prototyped in merge.h, including
 * the stubs.
 */
#if defined CONFIG_DEBUG_FS && !defined ACX_NO_DEBUG_FILES

#define pr_fmt(fmt) "acx.%s: " fmt, __func__

#include <linux/version.h>
#include <linux/wireless.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <net/mac80211.h>
#include "acx.h"
#include "merge.h" /* for acx_proc_(show|write)_funcs[]; */

/*
 * debugfs files are created under $DBGMNT/acx_mac80211/phyX by
 * acx_debugfs_add_dev(), where phyX is the vif of each wlanY ifupd on
 * the driver.  The acx_device *ptr is attached to the phyX directory.
 * acx_debugfs_add_dev() is called by acx_op_add_interface(); this
 * may be the wrong lifecyle, but is close, and works for now.
 * Each file gets a file_index, attached by debugfs_create_file() to
 * the inode's private data.
 *
 * A single open() handler uses the attached file_index to select the
 * right read callback, this avoids a linear scan of filenames to
 * match/strcmp against the callback.  The acx_device *ptr is
 * retrieved from the file's parent's private data, and passed to the
 * callback so it knows what vif to print data for.
 *
 * Similarly, a singe write handler retrieves the acx_device_t pointer
 * and file-index, and dispatches to the file handler for that index.
 */

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
		pr_info("opening filename=%s fmode=%o fidx=%d adev=%p\n",
			dbgfs_files[fidx], file->f_mode, (int)fidx, adev);
		break;
	default:
		pr_err("unknown file @ %d: %s\n", (int)fidx,
			file->f_path.dentry->d_name.name);
		return -ENOENT;
	}
	return single_open(file, acx_proc_show_funcs[fidx], adev);
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
		pr_info("opening filename=%s fmode=%o fidx=%d adev=%p\n",
			dbgfs_files[fidx], file->f_mode, (int)fidx, adev);
		break;
	default:
		pr_err("unknown file @ %d: %s\n", (int)fidx,
			file->f_path.dentry->d_name.name);
		return -ENOENT;
	}
	return (acx_proc_write_funcs[fidx])(file, buf, count, ppos);
}

static const struct file_operations acx_fops = {
	.read		= seq_read,
	.write		= acx_dbgfs_write,
	.open		= acx_dbgfs_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35)
	.llseek		= noop_llseek,
#endif
};

static struct dentry *acx_dbgfs_dir;

int acx_debugfs_add_adev(struct acx_device *adev)
{
	size_t i;
	int fmode;
	struct dentry *file;
	const char *devname = wiphy_name(adev->ieee->wiphy);
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

		fmode = (acx_proc_write_funcs[i])
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
	pr_info("%s %p\n", wiphy_name(adev->ieee->wiphy),
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
