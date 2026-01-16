GPPPARAMS = -m64 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -fno-builtin
ASPARAMS  = -f bin

BOOTLOADER = obj/loader.bin
KERNEL     = obj/kernel.o
DISKIMG    = AMS-OS.img

all: $(DISKIMG)

# Kompilacja kernel.cpp
obj/%.o: src/%.cpp
	mkdir -p $(@D)
	g++ $(GPPPARAMS) -c $< -o $@

# Kompilacja bootloader.s
obj/%.bin: src/%.s
	mkdir -p $(@D)
	nasm $(ASPARAMS) $< -o $@

# Tworzenie minimalnego obrazu dysku 2MB
$(DISKIMG): $(BOOTLOADER) $(KERNEL)
	# 2MB = 4096 * 512B sektory
	dd if=/dev/zero of=$(DISKIMG) bs=512 count=4096 status=none
	# Bootloader na sektor 0
	dd if=$(BOOTLOADER) of=$(DISKIMG) bs=512 count=1 conv=notrunc status=none
	# Kernel zaczyna od sektora 1
	dd if=$(KERNEL) of=$(DISKIMG) bs=512 seek=1 conv=notrunc status=none

run: $(DISKIMG)
	qemu-system-x86_64 -drive format=raw,file=$(DISKIMG)

.PHONY: clean
clean:
	rm -rf obj $(DISKIMG)
