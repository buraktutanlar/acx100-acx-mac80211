# This makefile, adapted from http://lwn.net/Kernel/LDD3/, CH02 allows 
# compiling driver either inside or outside the kernel tree without further 
# modifications.
#
# * Inside the kernel tree: simply configure and compile the kernel in the usual
# way, making sure that the CONFIG_ACX_MAC80211 option and at least one of the
# CONFIG_ACX_MAC80211_PCI or CONFIG_ACX_MAC80211_USB options are set in the
# .config file.
#
# * Outside the kernel tree: modify EXTRA_KCONFIG to define any required
# configuration options. Then execute 'make' to build the module ('make V=1' for
# verbose output). Execute 'make modules_install' as root in order to install
# the module.

################################################################################

# Here you can manually define configuration options when doing a build outside
# the kernel tree. Note that you can either modify this file or define the
# EXTRA_KCONFIG variable on the command line, i.e.:
#
#   export EXTRA_KCONFIG="CONFIG_ACX_MAC80211=m CONFIG_ACX_MAC80211_USB=y" && make
#
# If you use this method (CLI), do not forget to either clear or explicitly 
# define EXTRA_KCONFIG for subsequent builds!

EXTRA_KCONFIG?= \
	CONFIG_ACX_MAC80211=m \
	CONFIG_ACX_MAC80211_PCI=y

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))

################################################################################

ifneq ($(KERNELRELEASE),)
# If KERNELRELEASE is defined, we've been invoked from the kernel build system
# and can use its language (this variable can only be set only from the top
# level Makefile of the source tree). We simply have to declare the modules
# and leave the rest to the kernel build system.
	obj-$(CONFIG_ACX_MAC80211) += acx-mac80211.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_PCI) += pci.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_USB) += usb.o
	acx-mac80211-objs := common.o $(acx-mac80211-obj-y)
else
# Otherwise we were called directly from the command line: the the kernel build
# system must be explicitly invoked.
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(EXTRA_KCONFIG) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

distclean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) distclean

help:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) help

.PHONY: modules modules_install distclean

endif
