.phony: libs

CIRCLEHOME = ./circle

OBJS	= main.o kernel.o ppu.o

LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

FOR_QEMU = --qemu

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)

$(CIRCLEHOME)/Config.mk:
	cd $(CIRCLEHOME) && ./configure $(FOR_QEMU) -r 3 -p aarch64-elf- --c++17 -f

$(LIBS): $(CIRCLEHOME)/Config.mk
	cd $(CIRCLEHOME) && ./makeall

libs: $(LIBS)
