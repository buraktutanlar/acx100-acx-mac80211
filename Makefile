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
	BUILD_KIND="invoked from in-tree make"
	obj-$(CONFIG_ACX_MAC80211) += acx-mac80211.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_PCI) += pci.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_USB) += usb.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_MEM) += mem.o
	acx-mac80211-objs := $(acx-mac80211-obj-y) $(acx-mac80211-obj-m) 
	acx-mac80211-objs += common.o merge.o debugfs.o cmd.o ie.o init.o utils.o cardsetting.o tx.o rx.o

else
# Otherwise we were called directly from the command line: the kernel build
# system must be explicitly invoked.
	BUILD_KIND="standalone build"
	EXTRA_KCONFIG?= \
		CONFIG_ACX_MAC80211=m \
		CONFIG_ACX_MAC80211_PCI=y \
		CONFIG_ACX_MAC80211_USB=m \
		CONFIG_ACX_MAC80211_MEM=m

	EXTRA_CFLAGS:= \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))

	PWD := $(shell pwd)

	# Get the current git HEAD short version. In case something
	# goes wrong here, ACX_GIT_VERSION will be empty. This will
	# then be handled in the source files.
	ACX_GIT_VERSION ?= $(shell git describe --tag)

	KVERSION ?= $(shell uname -r)
	KERNELDIR ?= /lib/modules/$(KVERSION)/build

	# Uncomment following two lines to configure a build against a
	# compat-wireless tree
	#COMPAT_WIRELESS ?= "/path/to/compat-wireless"
	#LINUX_KARCH ?= $(shell make -pn -C $(KERNELDIR) asm-generic |grep SRCARCH |head -1 |awk '{print $$3}')

all:
	echo "make is $(BUILD_KIND)"
ifndef COMPAT_WIRELESS
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(EXTRA_KCONFIG) EXTRA_CFLAGS="$(EXTRA_CFLAGS) -DACX_GIT_VERSION=\\\"$(ACX_GIT_VERSION)\\\"" modules
else
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(EXTRA_KCONFIG) EXTRA_CFLAGS="$(EXTRA_CFLAGS) -DACX_GIT_VERSION=\\\"$(ACX_GIT_VERSION)\\\"" \
		LINUXINCLUDE="-I$(COMPAT_WIRELESS)/include -I$(KERNELDIR)/include \
			-Iarch/$(LINUX_KARCH)/include \
			-include generated/autoconf.h \
			-include linux/compat.h" \
		modules
endif

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

help:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) help


.PHONY: modules modules_install clean help

endif
