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
#ifndef _ACX_H_
#define _ACX_H_

#define NONESSENTIAL_FEATURES 1	// acx_show_card_eeprom_id()
/* #define UNUSED 0		// lots of errs if defd
 * #define CONFIG_PM 0		// in include/generated/autoconf.h
 */

/*
   DECL_OR_STUB(X, proto, stubret) macro: expands as either a
   prototype declaring a function defined elsewhere, or a static
   inline function stub.

   It reads like if (X) ? proto : stubret;
   if X is true, expand as declaration matching the fn-defn elsewhere
   if X is false, expand as a stub function, returning stubret.

   stubret: body of function used in stub expansion.
	For void use { }, for int use { return 0; }, etc

   proto: full function prototype, excluding "static inline" which is
	added in stub expansion.  Ex: int foo(void), void bar(int).

   X: condition.  if X is true, expansion is a declaration.
	if X is NOTHING, expands as an static inline stub.
	Note that defined(X) expands as 0 or 1, so cant be used directly.

   How It Works:
   4 macros; 3 xDECL_OR_STUB, and __unshift_A
   2nd assembles args: "proto;", "static inline proto subret"
   3rd selects one of them.
   1st is key: it concats symbol to "__unshift_".
	If symbol is true, result is __unshift_<true>, it expands as empty,
	If symbol NOTHING, result is __unshift_, which expands to add
	another arg, shifting others, thus 4th macro selects decl,
	not inline defn

   Usage: avoid direct use of defined(X) conditions, instead do:
	#if (complex CPP expression)
	# define X
	#else
	// dont even mention X, best just drop the #else
	#endif

   Based upon:
   commit 69349c2dc01c489eccaa4c472542c08e370c6d7e
   Author: Paul Gortmaker <paul.gortmaker@windriver.com>
   Date:   Thu Apr 12 19:46:32 2012 -0400
*/

#define __unshift_1	/* if cat expands to this, no shift */
#define __unshift_ 0,
/* if cat expands to this, unshift arg (ala perl) */

#define DECL_OR_STUB(_X_, proto, stubret) \
	_DECL_OR_STUB(_X_, proto, stubret) \

#define _DECL_OR_STUB(_X_, proto, stubret) \
	__DECL_OR_STUB(__unshift_##_X_, proto, stubret)

#define __DECL_OR_STUB(arg1_or_empty, proto, stubret)	\
	___DECL_OR_STUB(arg1_or_empty proto; , static inline proto stubret)

#define ___DECL_OR_STUB(__ignored, val, ...) val



#include "acx_config.h"
#include "acx_struct_hw.h"
#include "acx_struct_dev.h"
#include "acx_func.h"
#include "acx_compat.h"

#endif /* _ACX_H_ */
