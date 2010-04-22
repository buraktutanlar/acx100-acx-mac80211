# Makefile platform
# 
# Copyright (C) 2010
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
	obj-$(CONFIG_HX4700_ACX)        += hx4700_acx.o
	
else
# Otherwise we were called directly from the command line: the kernel build
# system must be explicitly invoked.
	EXTRA_KCONFIG?= \
		CONFIG_HX4700_ACX=m 

	EXTRA_CFLAGS:= \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
		$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
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
