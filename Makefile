.phony: libs run

CIRCLEHOME = ./circle

OBJS	= main.o kernel.o ppu.o

LIBS	= $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

FOR_QEMU = --qemu

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)

$(CIRCLEHOME)/Config.mk:
	cd $(CIRCLEHOME) && ./configure $(FOR_QEMU) -r 3 -p aarch64-elf- --c++17 -f

$(LIBS): $(CIRCLEHOME)/Config.mk
	cd $(CIRCLEHOME) && ./makeall
	cd $(CIRCLEHOME)/addon/SDCard && make

libs: $(LIBS)

# https://www.reddit.com/r/osdev/comments/165wdl3/how_to_create_sd_card_img_and_add_files_to_it_in/
roms.img: roms/*.* roms_img.sh
	./roms_img.sh

clean:
	cd $(CIRCLEHOME) && ./makeall clean
	rm -f *.img *.elf *.map *.lst $(CIRCLEHOME)/Config.mk

# this is specific to my setup and probably won't work for anyone else
# qemu-raspi is the special build of qemu for circle USB support
run: $(LIBS) kernel8.img roms.img
	sudo /usr/local/cross-aarch64/bin/qemu-rpi \
		-M raspi3 -kernel kernel8.img -serial null -serial stdio -device usb-host,vendorid=0x045e,productid=0x0b12 \
		-drive file=roms.img,format=raw,if=sd,id=sd0
