
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
