ifndef ACX_DEBUG
  ACX_DEBUG=1
  $(warning ACX_DEBUG was not defined, assuming 1)
endif # ACX_DEBUG

PWD=$(shell pwd)
KERNELRELEASE=$(shell uname -r)
KERNELDIR = /lib/modules/$(KERNELRELEASE)
MODULE_PATH = net
INSTALL = /usr/bin/install -c
DEPMOD = $(shell which depmod)

# FIXME: should be changed to -DACX_32BIT_IO=$(ACX_32BIT_IO),
# also same handling as for ACX_DEBUG above
export CPPFLAGS += -DACX_DEBUG=$(ACX_DEBUG) -DACX_32BIT_IO

include $(KERNELDIR)/build/.config

.PHONY: depmod all clean install uninstall

# File naming style:
# Driver modules should be named acx_HARDWARE.(k)o
# Other files should be named acx_HARDWARE_FUNCTION.o
# acx100 specific files should be named acx100_XXX
# Renaming can happen gradually over time.
pci_objs = acx100.o acx100_conv.o acx100_helper.o acx100_helper2.o acx100_ioctl.o acx80211frm.o idma.o ihw.o ioregister.o

usb_objs = acx_usb_80211frm.o acx_usb_conv.o acx_usb_helper.o acx_usb_helper2.o acx_usb_idma.o acx_usb_main.o acx_usb_ihw.o acx_usb_ioctl.o acx_usb_main.o 

ifeq ($(PATCHLEVEL),) # PATCHLEVEL only set if acting as "kbuild 2.6 makefile"

include $(ACX100_SRC)../config.mk

USBCFLAGS =

ifdef CONFIG_MODVERSIONS
CPPFLAGS += -DMODVERSIONS -include $(KERNEL_BUILD)/include/linux/modversions.h
USBCFLAGS += -DMODVERSIONS -include $(KERNEL_BUILD)/include/linux/modversions.h
endif

# 2.6 in-tree (kbuild)
ifdef KERN26
#MODULES = acx_pci.ko acx_usb.ko
MODULES = acx_pci.ko # sorry, no USB 2.6 compile yet
CLEAN_EXTRA = *.mod.c

modules:
ifeq ($(KERNEL_BUILD),$(KERNEL_OUTPUT))
	make -C $(KERNEL_BUILD) SUBDIRS=$(PWD) MODVERDIR=$(PWD) \
		modules
else # ifeq KERNEL_BUILD...
	mkdir -p $(PWD)/tmp
	cp $(KERNEL_OUTPUT)/.config $(PWD)/tmp
	make -C $(KERNEL_BUILD) SUBDIRS=$(PWD) O=$(PWD)/tmp \
		MODVERDIR=$(PWD) modules
endif # ifeq KERNEL_BUILD...

all: modules
	@echo
	@echo "*** WARNING: acx100_pci.ko changed to acx_pci.ko. Don't forget to update any scripts! ***"

install: all
else # KERN26
MODULES = "acx_pci.o acx_usb.o"

# 2.4 out-of-tree
include $(ACX100_SRC)../config.mk

CPPFLAGS += -D__KERNEL__ -DMODULE -DWLAN_HOSTIF=WLAN_PCI -I$(KERNEL_BUILD)/include -I../include
LDFLAGS = -m elf_i386 -r
CC = gcc
USBCFLAGS += -D__KERNEL__ -I$(KERNEL_BUILD)/include  -Wall -Wstrict-prototypes  -Wno-trigraphs  -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586 -DMODULE -I../include/ -c -DWLAN_HOSTIF=WLAN_USB -DDEBUG -DACX_DEBUG
CFLAGS += -Wall -Wstrict-prototypes -Wno-trigraphs -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586
#CPPFLAGS += -D__KERNEL__ -DMODULE -DMODVERSIONS -DWLAN_HOSTIF=WLAN_PCI -I$(KERNEL_BUILD)/include -I../include -include $(KERNEL_BUILD)/include/linux/modversions.h

includes = $(wildcard ../include/*.h)

all: acx_pci.o acx_usb.o
	@echo
	@echo "*** WARNING: acx100_pci.o changed to acx_pci.o. Don't forget to update any scripts! ***"

acx_pci.o: $(pci_objs)
	$(LD) $(LDFLAGS) -o $@ $^

acx_usb.o: $(usb_objs)
	ld $(LDFLAGS) -o $@ $^

acx_usb_80211frm.o: acx80211frm.c
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_conv.o: acx100_conv.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_helper.o: acx100_helper.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_helper2.o: acx100_helper2.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_ioctl.o: acx100_ioctl.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_idma.o: idma.c
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_ihw.o: ihw_usb.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx_usb_main.o: acx100_usb.c
	$(CC) $(USBCFLAGS) -c -o $@ $<

$(pci_objs): $(includes)

$(usb_objs): $(includes)

endif # KERN26

clean:
	rm -f $(pci_objs) $(usb_objs) ${MODULES} ${CLEAN_EXTRA}

distclean: clean
	rm -f *~ ../doc/*~ ../include/*~ ../scripts/*~ *.rej

install:
	@echo "*** Copying driver modules ${MODULES} to ${KERNELDIR}/${MODULE_PATH}... ***"
	test -d ${KERNELDIR}/${MODULE_PATH} || mkdir ${KERNELDIR}/${MODULE_PATH}
	for mod in ${MODULES}; do \
	  ${INSTALL} $$mod ${KERNELDIR}/${MODULE_PATH}; \
	done
	${DEPMOD} -ae

uninstall:
	for mod in ${MODULES}; do \
	  ${RM} ${KERNELDIR}/${MODULE_PATH}/$$mod; \
	done
	${DEPMOD} -ae

# 2.6 in-tree (kbuild)
else # ifeq PATCHLEVEL

EXTRA_CFLAGS += -I$(src)/../include -DWLAN_HOSTIF=WLAN_PCI
#EXTRA_CFLAGS += -Idrivers/net/wireless/acx100 -DWLAN_HOSTIF=WLAN_PCI

obj-m += acx_pci.o
#obj-m += acx_usb.o

acx_pci-objs = $(pci_objs)
#acx_usb-objs := $(usb_objs)

endif # ifeq PATCHLEVEL
