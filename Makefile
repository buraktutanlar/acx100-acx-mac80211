.PHONY: config driver all

all:	config driver

config:
	@./Configure

extract_firmware: firmware
	make -C firmware extract_firmware

driver:
	make -C src

clean:
	make -C src clean; \
	make -C firmware clean

distclean: 
#	@echo "WARNING this will remove all binaries, including the \
#		file that contains the firmware information. If you \
#		wish to continue press enter, else CTL-C"; \
#	pause; \
#	cd firmware; rm *.o -f; rm CVS -rf; cd ..; \
#	cd src; rm *.o -f; rm CVS -rf; cd ..; \
#	cd include; rm CVS -rf; cd ..; \ 
#	cd scripts; rm CVS -rf; cd ..;
