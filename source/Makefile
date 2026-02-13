# --- Ścieżki i Narzędzia ---
export PATH := $(HOME)/opt/cross/bin:$(PATH)

CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld

# --- Flagi Jądra (Ring 0) - PANCERNE (Bez SSE, Bez RedZone) ---
CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -fno-stack-protector \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -DKERNEL_MODE

ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

# --- Flagi Aplikacji (Ring 3) ---
# TCC i Hello mogą używać SSE, ale muszą mieć biblioteki.
USER_CFLAGS = -m64 -ffreestanding -O2 -Wall -Iinclude -fno-exceptions -fno-rtti -fno-stack-protector
USER_LDFLAGS = -n -T src/gui/apps/user.ld

# =============================================================================
# PLIKI
# =============================================================================

# 1. Wszystkie pliki .cpp
SRCS_ALL_CPP := $(shell find src -name '*.cpp')
SRCS_USER_LIB := $(shell find src/lib -name '*.cpp')

# 2. Pliki JĄDRA
#    Wyrzucamy wszystko co jest w src/lib oraz hello_user.
#    Jądro używa WŁASNYCH wersji z utils.cpp i heap.cpp
SRCS_KERNEL_CPP := $(filter-out src/gui/apps/hello_user.cpp $(SRCS_USER_LIB), $(SRCS_ALL_CPP))

# 3. ASM Jądra
SRCS_KERNEL_ASM := $(shell find src -name '*.s' ! -path "src/lib/*")

# 4. Obiekty Jądra
KERNEL_OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS_KERNEL_CPP))
KERNEL_OBJS += $(patsubst src/%.s, build/%.o, $(SRCS_KERNEL_ASM))

# 5. Obiekty Biblioteki Użytkownika (do linkowania z TCC i Hello)
#    Te pliki będą kompilowane flagami USER_CFLAGS (albo bezpiecznymi, zależy od reguły)
LIBC_OBJS := build/lib/crt0.o \
             build/lib/syscall.o \
             build/lib/ams_syscall.o \
             build/lib/stdlib.o \
             build/lib/string.o \
             build/lib/ctype.o \
             build/lib/stdio.o \
             build/lib/stubs.o \
             build/lib/setjmp.o

# =============================================================================
# CELE
# =============================================================================
all: kernel.elf build/hello.elf build/tcc.elf ams.iso disk.img

# --- KOMPILACJA JĄDRA ---

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# LINKOWANIE JĄDRA
# UWAGA: Usunąłem $(LIBC_OBJS) stąd! Jądro jest samowystarczalne.
kernel.elf: $(KERNEL_OBJS)
	@echo "[LINK] Budowanie Jądra..."
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

# --- KOMPILACJA BIBLIOTEKI USERA ---

# Biblioteki kompilujemy BEZPIECZNIE (CFLAGS), żeby uniknąć syfu z SSE w dziwnych miejscach.
# Ale jeśli TCC potrzebuje floatów, to w stdlib.cpp masz #ifdef __SSE__.
# Tutaj użyjemy USER_CFLAGS, żeby TCC miało floaty, ale Jądro i tak korzysta z utils.cpp.

build/lib/%.o: src/lib/%.cpp
	@mkdir -p build/lib
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/lib/%.o: src/lib/%.s
	@mkdir -p build/lib
	$(AS) $(ASFLAGS) $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p build/lib
	x86_64-elf-gcc $(USER_CFLAGS) -c $< -o $@

# --- KOMPILACJA USER APPS ---

# Hello User
build/apps/hello_user.o: src/gui/apps/hello_user.cpp
	@mkdir -p build/apps
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/hello.elf: build/apps/hello_user.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie hello.elf..."
	$(LD) $(USER_LDFLAGS) build/apps/hello_user.o $(LIBC_OBJS) -o build/hello.elf

# TCC
build/apps/tcc.o: src/apps/tcc/tcc.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -w -DONE_SOURCE=1 -DTCC_TARGET_X86_64 -DHAVE_SYS_MMAP=1\
	-nostdinc -Iinclude -isystem include \
	-Isrc/apps/tcc \
	-c src/apps/tcc/tcc.c -o build/apps/tcc.o

build/tcc.elf: build/apps/tcc.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie TCC..."
	$(LD) $(USER_LDFLAGS) build/apps/tcc.o $(LIBC_OBJS) -o build/tcc.elf

# --- OBRAZY ---
disk.img: build/tcc.elf
	@echo "[DISK] Budowanie srodowiska..."
	qemu-img create -f raw disk.img 500M
	mkfs.ext2 -b 4096 disk.img
	mkdir -p ./temp_mount
	sudo mount -o loop disk.img ./temp_mount
	
	# Kopiujemy wszystko bezpośrednio do roota, bo nasz EXT2 nie widzi podfolderów!
	sudo cp build/tcc.elf ./temp_mount/tcc
	sudo cp src/apps/tcc/kupa.c ./temp_mount/kupa.c
	sudo cp tinycc-mirror-repository/include/tccdefs.h ./temp_mount/tccdefs.h
	sudo cp libtcc1.a ./temp_mount/libtcc1.a
	
	sudo sync
	sudo umount -d ./temp_mount

ams.iso: kernel.elf
	@echo "[ISO] Tworzenie obrazu bootowalnego..."
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/
	echo "AMSx64 v1.2.5 ring 3 version" > hello2.txt
	tar -cf initrd.tar hello2.txt bible.txt
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "AMSx64 v1.2.5 ring 3 version" {\n multiboot2 /boot/kernel.elf\n module2 /boot/initrd.tar\n boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o ams.iso iso

run: all
	qemu-system-x86_64 -enable-kvm -m 4G -cdrom ams.iso -serial stdio \
		-drive id=disk,file=disk.img,if=none,format=raw,readonly=off \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=disk,bus=ahci.0 \
		-vga vmware \
		-display sdl,gl=on \
		-no-reboot -no-shutdown -d int,cpu_reset -D qemu.log

dump: kernel.elf
	@echo "[DEBUG] Zrzucanie asemblera..."
	$(HOME)/opt/cross/bin/x86_64-elf-objdump -d -S -M intel kernel.elf > kernel.asm
	@echo "Gotowe! Otwórz plik kernel.asm i szukaj adresu: 10abd7"

clean:
	rm -rf build iso ams.iso kernel.elf initrd.tar hello2.txt disk.img qemu.log