
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 40)
#include <linux/ratelimit.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)

#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif

/* to be submitted to LKML */
#ifndef BUILD_BUG_DECL
#define BUILD_BUG_DECL(name, condition)					\
       static __initdata struct {					\
               int BUILD_BUG_DECL_ ##name[1 - 2*!!(condition)];		\
       } BUILD_BUG_DECL_ ##name[0] __attribute__((unused))
#endif


#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/utsrelease.h>
#else
#include <generated/utsrelease.h>
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
/*
 * v2.6.34 added ieee80211_vif, and obsoleted ieee80211_if_init_conf.
 * To declutter acx_op_(add|remove)_interface(..), define macros to
 * hide this struct change, naming them close to new one.  This is
 * surely not a solution for every conceivable _vif situation (the new
 * struct surely holds a different set of fields), but is sufficient
 * here.
 */
#  define ieee80211_VIF ieee80211_if_init_conf
#  define VIF_vif(vif)  vif->vif
#  define VIF_addr(vif) vif->mac_addr
#else
#  define ieee80211_VIF ieee80211_vif
#  define VIF_vif(vif)  vif
#  define VIF_addr(vif) vif->addr
#endif

/* hide some of the version dependent function signature changes in
 * struct ieee80211_ops; starting with acx_op_tx().
 */
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
#  define OP_TX_RET_TYPE int
#  define OP_TX_RET_OK   NETDEV_TX_OK
#else
#  define OP_TX_RET_TYPE void
#  define OP_TX_RET_OK   /* void */
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
/* map new name to old */
#  define irq_set_irq_type set_irq_type
#endif
