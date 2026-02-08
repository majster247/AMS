export PATH := $(HOME)/opt/cross/bin:$(PATH)

CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld

CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -mno-red-zone -mno-mmx -mno-sse -mno-sse2
ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

# Flagi dla aplikacji Ring 3 (muszą być statyczne i bez bibliotek standardowych)
USER_CFLAGS = -m64 -ffreestanding -O2 -Wall -Iinclude -fno-exceptions -fno-rtti
USER_LDFLAGS = -n -T src/drivers/gui/apps/user.ld

SRCS = src/kernel.cpp src/arch/x86_64/idt.cpp src/drivers/keyboard.cpp \
       src/drivers/vga.cpp src/drivers/serial.cpp src/memory/multiboot_parser.cpp \
       src/memory/pmm.cpp src/utils.cpp src/memory/vmm.cpp src/memory/heap.cpp \
       src/tasking/scheduler.cpp src/fs/vfs.cpp src/drivers/pci.cpp \
       src/drivers/ahci.cpp src/fs/ext2.cpp src/drivers/graphics.cpp src/graphics_utils.cpp \
       src/drivers/font_data.cpp src/drivers/wallpaper_data.cpp \
       src/drivers/mouse.cpp src/drivers/gui/gui_core.cpp src/drivers/gui/apps/notepad.cpp \
       src/drivers/gui/apps/filemgr.cpp src/drivers/gui/apps/extras.cpp src/drivers/gui/apps/terminal.cpp \
       src/arch/x86_64/gdt.cpp src/arch/x86_64/syscall.cpp src/drivers/timer.cpp
	   
OBJS = build/start.o \
       build/tasking/interrupts.o \
	   build/arch/x86_64/user_jump.o \
       $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: build_user ams.iso disk.img

# 0. Kompilacja programu użytkownika (Ring 3)
build_user:
	@mkdir -p build/apps
	$(CC) $(USER_CFLAGS) -c src/drivers/gui/apps/hello_user.cpp -o build/apps/hello_user.o
	$(LD) $(USER_LDFLAGS) build/apps/hello_user.o -o build/hello.elf

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

# 1. Tworzenie obrazu dysku (HDD) - EXT2
disk.img: build_user
	qemu-img create -f raw disk.img 500M
	mkfs.ext2 -b 4096 disk.img
	mkdir -p ./temp_mount
	sudo mount -o loop disk.img ./temp_mount
	
	# Kopiowanie binarki Ring 3 na dysk SATA
	sudo cp build/hello.elf ./temp_mount/
	
	# Tworzenie plików tekstowych
	sudo sh -c 'echo "AMS x64 suczko lecimy nie śpimy" > ./temp_mount/hello.txt'
	sudo dd if=/dev/zero of=./temp_mount/big_file.txt bs=1 count=5000
	
	# Kopiowanie wideo
	@if [ -f movie.vid ]; then \
	    echo "Kopiowanie movie.vid na obraz dysku..."; \
	    sudo cp movie.vid ./temp_mount/; \
	fi

	sudo sync
	sudo umount -l ./temp_mount || true
	rmdir ./temp_mount

# 2. Tworzenie ISO (Bootloader + Kernel + Initrd)
ams.iso: kernel.elf
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/
	echo "AMSx64 v1.2.5 ring 3 version" > hello2.txt
	tar -cf initrd.tar hello2.txt bible.txt
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "AMSx64 v1.2.5 ring 3 version" {\n multiboot2 /boot/kernel.elf\n module2 /boot/initrd.tar\n boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o ams.iso iso

# 3. Uruchomienie
run: all
	qemu-system-x86_64 -enable-kvm -m 4G -cdrom ams.iso -serial stdio \
	    -drive id=disk,file=disk.img,if=none,format=raw,readonly=off \
	    -device ich9-ahci,id=ahci \
	    -device ide-hd,drive=disk,bus=ahci.0 \
	    -vga vmware \
	    -display sdl,gl=on \
	    -no-reboot -no-shutdown -d int,cpu_reset -D qemu.log

clean:
	rm -rf build iso ams.iso kernel.elf initrd.tar hello2.txt disk.img qemu.log