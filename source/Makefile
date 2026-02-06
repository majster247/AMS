export PATH := $(HOME)/opt/cross/bin:$(PATH)

CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld

CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -mno-red-zone -mno-mmx -mno-sse -mno-sse2
ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

SRCS = src/kernel.cpp src/arch/x86_64/idt.cpp src/drivers/keyboard.cpp \
       src/drivers/vga.cpp src/drivers/serial.cpp src/memory/multiboot_parser.cpp \
       src/memory/pmm.cpp src/utils.cpp src/memory/vmm.cpp src/memory/heap.cpp \
       src/tasking/scheduler.cpp src/fs/vfs.cpp src/shell.cpp src/drivers/pci.cpp \
       src/drivers/ahci.cpp src/fs/ext2.cpp src/drivers/graphics.cpp src/graphics_utils.cpp \
	   src/drivers/font_data.cpp src/drivers/wallpaper_data.cpp \
	   src/drivers/mouse.cpp src/drivers/gui/gui.cpp
OBJS = build/start.o \
       build/tasking/interrupts.o \
       $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: ams.iso disk.img

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

build/start.o: src/start.s
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Tworzenie obrazu dysku 64MB dla Ext2 oraz wypełnienie go przykładowym plikiem z napisem
disk.img:
	qemu-img create -f raw disk.img 64M
	mkfs.ext2 -b 1024 disk.img
	mkdir -p ./temp_mount
	sudo mount -o loop disk.img ./temp_mount
	sudo sh -c 'echo "AMS x64 suczko lecimy nie śpimy" > ./temp_mount/hello.txt'
	sudo sh -c 'printf "A%.0s" {1..5000} > ./temp_mount/big_file.txt'
	sudo chmod 666 ./temp_mount/hello.txt
	sudo sync
	sudo umount ./temp_mount
	rmdir ./temp_mount

ams.iso: kernel.elf
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/
	echo "AMSx64 suko jebać CIA wielbić Terrego Davisa i TempleOS" > hello.txt
	tar -cf initrd.tar hello.txt hello2.txt bible.txt
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "AMSx64 v04.5" {\n multiboot2 /boot/kernel.elf\n module2 /boot/initrd.tar\n boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o ams.iso iso

# POPRAWIONE URUCHAMIANIE Z AHCI
run: all
	qemu-system-x86_64 -enable-kvm -m 4G -cdrom ams.iso -serial stdio \
		-drive id=disk,file=disk.img,if=none,format=raw,readonly=off \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=disk,bus=ahci.0 \
		-vga vmware \
        -display sdl,gl=on \
		-no-reboot -no-shutdown -d int,cpu_reset -D qemu.log 

clean:
	rm -rf build iso ams.iso kernel.elf initrd.tar hello.txt disk.img qemu.log