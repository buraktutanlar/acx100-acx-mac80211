# This makefile, which has been adapted from http://lwn.net/Kernel/LDD3/, CH02,
# allows to compile the driver either inside or outside the kernel tree
# without further modifications.
#
# * Inside the kernel tree: simply configure and compile the kernel in the usual
# way, making sure that the CONFIG_ACX_MAC80211 option and at least one of the
# CONFIG_ACX_MAC80211_PCI or CONFIG_ACX_MAC80211_USB options are set
#
# * Outside the kernel tree: either provide the configuration options mentioned
# above on the command line or define them within this makefile. You then simply
# have to invoke 'make' and an optional target.

# Remove the following two lines once the driver is in the kernel. Alternatively,
# they can also be removed if they are specified from the command-line, as in:
#
#   export CONFIG_ACX_MAC80211=m && export CONFIG_ACX_MAC80211_PCI=y && make
#
# If you are not actually running the kernel that you are building for, you can 
# supply a KERNELDIR= option on the command line or set the KERNELDIR 
# environment variable
CONFIG_ACX_MAC80211=m
CONFIG_ACX_MAC80211_PCI=y

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
# First pass, kernel Makefile reads module objects
ifneq ($(KERNELRELEASE),)
	obj-$(CONFIG_ACX_MAC80211) += acx-mac80211.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_PCI) += pci.o
	acx-mac80211-obj-$(CONFIG_ACX_MAC80211_USB) += usb.o
	acx-mac80211-objs := common.o log.o $(acx-mac80211-obj-y)

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
# Second pass, the actual build.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

help:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) help

endif
