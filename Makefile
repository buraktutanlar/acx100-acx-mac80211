# set to 0 if you don't want any debugging code to be compiled in
export ACX_DEBUG=1

# assume 32bit I/O width (16bit is also compatible with Compact Flash)
export ACX_IO_WIDTH=32

.PHONY: all extract_firmware fetch_firmware driver install uninstall clean

all: driver

config.mk: Configure
	@./Configure

extract_firmware: firmware
	make -C firmware extract_firmware

fetch_firmware:
	scripts/fetch_firmware

driver: config.mk
	make -C src

install:
	make -C src install

uninstall:
	make -C src uninstall

clean:
	make -C src clean
	make -C firmware clean
	rm -f config.mk

inject:
	./scripts/inject_kernel_tree .

distclean: 
#	@echo "WARNING this will remove all binaries, including the \
#		file that contains the firmware information. If you \
#		wish to continue press enter, else CTL-C"; \
#	pause; \
#	cd firmware; rm *.o -f; rm CVS -rf; cd ..; \
#	cd src; rm *.o -f; rm CVS -rf; cd ..; \
#	cd include; rm CVS -rf; cd ..; \ 
#	cd scripts; rm CVS -rf; cd ..;
