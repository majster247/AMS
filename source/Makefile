# --- Ścieżki i Narzędzia ---
export PATH := $(HOME)/opt/cross/bin:$(PATH)

CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld

# --- Flagi Jądra (Ring 0) ---
CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -mno-red-zone -mno-mmx -mno-sse -mno-sse2

ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

# --- Flagi Aplikacji / Biblioteki (Ring 3) ---
USER_CFLAGS = -m64 -ffreestanding -O2 -Wall -Iinclude -fno-exceptions -fno-rtti -fno-stack-protector
USER_LDFLAGS = -n -T src/gui/apps/user.ld

# =============================================================================
# AUTOMATYCZNE WYKRYWANIE PLIKÓW
# =============================================================================

# 1. Wszystkie pliki .cpp w folderze src
SRCS_ALL_CPP := $(shell find src -name '*.cpp')

# 2. Znajdź pliki biblioteki użytkownika (żeby ich NIE wkompilować do jądra)
SRCS_USER_LIB := $(shell find src/lib -name '*.cpp')

# 3. Znajdź pliki TCC (żeby ich NIE wkompilować do jądra)
#    Zakładamy, że wrzuciłeś źródła TCC do src/apps/tcc/
SRCS_TCC := $(shell find src/apps/tcc -name '*.c')

# 4. Lista plików JĄDRA:
#    Bierzemy WSZYSTKO, ale wyrzucamy:
#    - hello_user.cpp
#    - bibliotekę LibC
#    - źródła TCC
SRCS_KERNEL_CPP := $(filter-out src/gui/apps/hello_user.cpp $(SRCS_USER_LIB), $(SRCS_ALL_CPP))

# 5. Pliki asemblera jądra (wykluczamy te z biblioteki, np. crt0.s, setjmp.s)
SRCS_KERNEL_ASM := $(shell find src -name '*.s' ! -path "src/lib/*")

# 6. Generowanie listy obiektów dla JĄDRA
KERNEL_OBJS := $(patsubst src/%.cpp, build/%.o, $(SRCS_KERNEL_CPP))
KERNEL_OBJS += $(patsubst src/%.s, build/%.o, $(SRCS_KERNEL_ASM))

# 7. Lista obiektów dla BIBLIOTEKI C (LibC)
LIBC_OBJS := build/lib/crt0.o \
             build/lib/ams_syscall.o \
             build/lib/stdlib.o \
             build/lib/string.o \
             build/lib/ctype.o \
             build/lib/stdio.o \
			 build/lib/stubs.o \
             build/lib/setjmp.o

# =============================================================================
# GŁÓWNE CELE
# =============================================================================
all: build/hello.elf build/tcc.elf ams.iso disk.img

# --- KOMPILACJA JĄDRA (Ring 0) ---

# Reguła dla C++ jądra
build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Reguła dla ASM jądra
build/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Linkowanie Jądra
kernel.elf: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

# --- KOMPILACJA UŻYTKOWNIKA (Ring 3) ---

# 1. Biblioteka C - Asembler (crt0.s, setjmp.s)
build/lib/%.o: src/lib/%.s
	@mkdir -p build/lib
	$(AS) $(ASFLAGS) $< -o $@

# 2. Biblioteka C - C++ (stdio, stdlib, string...)
build/lib/%.o: src/lib/%.cpp
	@mkdir -p build/lib
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p build/lib
	x86_64-elf-gcc $(USER_CFLAGS) -c $< -o $@

# 3. Aplikacja Testowa (hello_user)
build/apps/hello_user.o: src/gui/apps/hello_user.cpp
	@mkdir -p build/apps
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/hello.elf: build/apps/hello_user.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie hello.elf..."
	$(LD) $(USER_LDFLAGS) \
		$(LIBC_OBJS) \
		build/apps/hello_user.o \
		-o build/hello.elf

# 4. KOMPILATOR TCC (The Boss)
build/apps/tcc.o: src/apps/tcc/tcc.c
	@mkdir -p build/apps
	x86_64-elf-gcc $(USER_CFLAGS) -w -DONE_SOURCE=1 -DTCC_TARGET_X86_64 \
	-nostdinc -Iinclude -isystem include \
	-Isrc/apps/tcc \
	-c src/apps/tcc/tcc.c -o build/apps/tcc.o

build/tcc.elf: build/apps/tcc.o $(LIBC_OBJS) src/gui/apps/user.ld
	@echo "[LINK] Budowanie TCC..."
	$(LD) $(USER_LDFLAGS) \
		$(LIBC_OBJS) \
		build/apps/tcc.o \
		-o build/tcc.elf

# --- OBRAZY DYSKU I ISO ---

disk.img: build/hello.elf build/tcc.elf
	@echo "[DISK] Tworzenie obrazu dysku..."
	qemu-img create -f raw disk.img 500M
	mkfs.ext2 -b 4096 disk.img
	mkdir -p ./temp_mount

# 1. NAJPIERW MONTUJEMY
	sudo mount -o loop disk.img ./temp_mount

# 2. TERAZ TWORZYMY STRUKTURĘ KATALOGÓW NA ZAMONTOWANYM DYSKU
	sudo mkdir -p ./temp_mount/usr/include

# 3. TERAZ KOPIUJEMY PLIKI
	sudo cp build/hello.elf ./temp_mount/hello.elf
	sudo cp build/tcc.elf ./temp_mount/tcc
	sudo cp src/apps/tcc/kupa.c ./temp_mount/kupa.c
	sudo cp -r include/* ./temp_mount/usr/include/

# ... reszta (hello.txt, movie.vid) ...
	sudo sh -c 'echo "AMS x64 suczko lecimy nie śpimy" > ./temp_mount/hello.txt'	
	sleep 1
	sudo sync
	sudo umount -d ./temp_mount || (sleep 1 && sudo umount -f ./temp_mount)
	@if mountpoint -q ./temp_mount; then \
        echo "Błąd: temp_mount nadal zamontowany!"; \
    else \
        rmdir ./temp_mount; \
    fi

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

clean:
	rm -rf build iso ams.iso kernel.elf initrd.tar hello2.txt disk.img qemu.log