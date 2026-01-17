CC = x86_64-elf-g++
AS = nasm
LD = x86_64-elf-ld

CFLAGS = -m64 -ffreestanding -O2 -Wall -Wextra -Iinclude \
         -fno-exceptions -fno-rtti -mno-red-zone -mno-mmx -mno-sse -mno-sse2
ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld

# Dodajemy interrupts.s do listy źródeł (załóżmy, że jest w src/tasking/)
SRCS = src/kernel.cpp src/arch/x86_64/idt.cpp src/drivers/keyboard.cpp \
       src/drivers/vga.cpp src/drivers/serial.cpp src/memory/multiboot_parser.cpp \
	   src/memory/pmm.cpp src/utils.cpp src/memory/vmm.cpp src/memory/heap.cpp \
       src/tasking/scheduler.cpp src/fs/vfs.cpp

# Automatyczne generowanie listy plików .o dla .cpp i .s
OBJS = build/start.o \
       build/tasking/interrupts.o \
       $(patsubst src/%.cpp, build/%.o, $(SRCS))

all: ams.iso

# Reguła dla plików C++
build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Poprawiona reguła dla plików asemblera (obsługuje start.s i interrupts.s)
build/%.o: src/%.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

# Specyficzna reguła dla start.o jeśli jest bezpośrednio w src/
build/start.o: src/start.s
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

ams.iso: kernel.elf
	mkdir -p iso/boot/grub
	cp kernel.elf iso/boot/
	echo "AMSx64 suko jebać CIA wielbić Terrego Davisa i TempleOS" > hello.txt
	tar -cf initrd.tar hello.txt
	cp initrd.tar iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "AMSx64 v04.5" {\n multiboot2 /boot/kernel.elf\n module2 /boot/initrd.tar\n boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue -o ams.iso iso

run: all
	qemu-system-x86_64 -cdrom ams.iso -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf build iso ams.iso kernel.elf initrd.tar hello.txt