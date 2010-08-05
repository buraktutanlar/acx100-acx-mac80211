#                                                       -*-Makefile-*-
# Copyright (C) 2008
# The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# This makefile, allows compiling driver either inside or outside the kernel
# tree without further modifications.
#
# Please read the INSTALL file for further information.

ifneq ($(KERNELRELEASE),)
# If KERNELRELEASE is defined, we've been invoked from the kernel build system
# and can use its language (this variable can only be set only from the top
# level Makefile of the source tree). We simply have to declare the modules and
# leave the rest to the kernel build system.
	obj-$(CONFIG_ACX_MAC80211) += acx-mac80211.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_PCI) += pci.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_USB) += usb.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_MEM) += mem.o
	acx-mac80211-objs := common.o $(acx-mac80211-obj-y)
	
else
# Otherwise we were called directly from the command line: the kernel build
# system must be explicitly invoked.
	EXTRA_KCONFIG?= \
		CONFIG_ACX_MAC80211=m \
		CONFIG_ACX_MAC80211_PCI=y \
		CONFIG_ACX_MAC80211_USB=y \
		CONFIG_ACX_MAC80211_MEM=n

	EXTRA_CFLAGS:= \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))
	
	KVERSION ?= $(shell uname -r)
	KERNELDIR ?= /lib/modules/$(KVERSION)/build
	PWD := $(shell pwd)
	
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(EXTRA_KCONFIG) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

help:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) help

.PHONY: modules modules_install clean help

endif
