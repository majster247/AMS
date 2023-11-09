#sudo apt-get install g++ binutils libc6-dev-i386 mtools
#sudo apt-get install qemu-system-x86_64 grub-legacy grub-mkrescue grub2 xorriso


GPPPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin -fno-rtti -fno-exceptions -fno-threadsafe-statics -fno-leading-underscore -Wno-write-strings
ASPARAMS = --32
LDPARAMS = -melf_i386

objects = obj/loader.o \
	  obj/gdt.o \
	  obj/memorymanagement.o \
	  obj/art.o \
	  obj/drivers/driver.o \
	  obj/hardwarecommunication/port.o \
	  obj/hardwarecommunication/interruptstubs.o \
	  obj/hardwarecommunication/interrupts.o \
	  obj/multitasking.o \
	  obj/drivers/amd_am79c973.o \
	  obj/hardwarecommunication/pci.o \
	  obj/drivers/keyboard.o \
	  obj/drivers/mouse.o \
	  obj/drivers/vga.o \
	  obj/drivers/ata.o \
	  obj/drivers/pit.o \
	  obj/drivers/speaker.o \
	  obj/gui/widget.o \
	  obj/gui/desktop.o \
	  obj/gui/window.o \
	  obj/net/etherframe.o \
	  obj/net/arp.o \
	  obj/net/ipv4.o \
	  obj/net/icmp.o \
	  obj/filesys/ofs.o \
	  obj/cli.o \
	  obj/script.o \
	  obj/mode/piano.o \
	  obj/mode/snake.o \
	  obj/mode/file_edit.o \
	  obj/mode/space.o \
	  obj/kernel.o
#obj/common/universalfunc.o \

obj/%.o: src/%.cc
	mkdir -p $(@D)
	g++ $(GPPPARAMS) -o $@ -c $<

obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o $@ $<

AMS-OS.bin: linker.ld $(objects)
	ld $(LDPARAMS) -T $< -o $@ $(objects)

install: AMS-OS.bin
	sudo cp $< /boot/AMS-OS.bin

AMS-OS.iso: AMS-OS.bin
	mkdir iso
	mkdir iso/boot
	mkdir iso/boot/grub
	cp AMS-OS.bin iso/boot/AMS-OS.bin
	echo 'set timeout=0' >> iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "AMS-OS v0.6.5 major pre-alpha update" {' >> iso/boot/grub/grub.cfg
	echo '	multiboot /boot/AMS-OS.bin' >> iso/boot/grub/grub.cfg
	echo '	boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue --output=AMS-OS.iso iso
	rm -rf iso
	
	qemu-img create -f qcow2 AMS-OS.img 1G
	dd if=AMS-OS.iso of=AMS-OS.img	

run: AMS-OS.iso
	echo "You can now install AMS to VMware fucking lazy nigger"


.PHONY: clean
clean:
	rm -rf obj AMS-OS.bin AMS-OS.iso

