
/* include this above <linux> headers, as it modifies pr_*() macro
 * defaults via pr_fmt() if its defined beforehand.
 */

/* The pr_fmt() default is KBUILD_MODNAME, set in <linux> headers,
 * which is "acx-mac80211:" here.  Change it to "acx" and add function
 * name for more debug context.  Drop the function name later to match
 * the hard-coded prefix.  If acxmem and acxpci prefixes are desired,
 * then set pr_fmt() in mem.c and pci.c before including this header.
 */
#ifndef pr_fmt
#define pr_fmt(fmt)	"acx.%s: " fmt, __FUNCTION__
#endif

/* enable pr_debug by default, if included before <linux> headers.
 * We're not using pr_debug() yet, but we need this header for
 * pr_fmt() anyway, so set here for when we do use pr_debug()
 */
#define DEBUG		
