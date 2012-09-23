#ifndef _ACX_DEBUGFS_H_
#define _ACX_DEBUGFS_H_

/* debugfs.c API used by common.c */
#if defined CONFIG_DEBUG_FS && !defined ACX_NO_DEBUG_FILES
int acx_debugfs_add_adev(struct acx_device *adev);
void acx_debugfs_remove_adev(struct acx_device *adev);
int __init acx_debugfs_init(void);
void __exit acx_debugfs_exit(void);
#else
static int acx_debugfs_add_adev(struct acx_device *adev) { return 0; }
static void acx_debugfs_remove_adev(struct acx_device *adev) { }
static int __init acx_debugfs_init(void)  { return 0; }
static void __exit acx_debugfs_exit(void) { }
#endif /* defined CONFIG_DEBUG_FS */


#endif
