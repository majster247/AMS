# ------------------------
# Toolchain
# ------------------------
ASM = nasm
CC  = x86_64-elf-g++
LD  = x86_64-elf-g++
GRUB_MKRESCUE = grub-mkrescue
QEMU = qemu-system-x86_64

# ------------------------
# Directories
# ------------------------
SRC   = src
BUILD = build
ISO   = iso
ISO_DIR = $(ISO)/boot/grub

# ------------------------
# Files
# ------------------------
START_OBJ   = $(BUILD)/start.o
KERNEL_OBJ  = $(BUILD)/kernel_cpp.o
KERNEL_ELF  = $(BUILD)/kernel.elf
GRUB_CFG    = $(ISO_DIR)/grub.cfg

# ------------------------
# Default target
# ------------------------
all: run

# ------------------------
# Create build and ISO directories
# ------------------------
$(BUILD):
	mkdir -p $@

$(ISO_DIR):
	mkdir -p $@

# ------------------------
# Compile kernel.cpp
# ------------------------
$(KERNEL_OBJ): $(SRC)/kernel.cpp | $(BUILD)
	$(CC) -m64 -ffreestanding -O2 -Wall -Wextra -c $< -o $@

# ------------------------
# Assemble start.s
# ------------------------
$(START_OBJ): $(SRC)/start.s | $(BUILD)
	$(ASM) -f elf64 $< -o $@

# ------------------------
# Link kernel + start.o into ELF
# ------------------------
$(KERNEL_ELF): $(START_OBJ) $(KERNEL_OBJ)
	$(LD) -m64 -ffreestanding -O2 -nostdlib -T linker.ld $(START_OBJ) $(KERNEL_OBJ) -o $@

# ------------------------
# Copy kernel ELF into ISO
# ------------------------
$(ISO)/boot/kernel.elf: $(KERNEL_ELF) | $(ISO_DIR)
	cp $< $@

# ------------------------
# Generate grub.cfg
# ------------------------
$(GRUB_CFG): | $(ISO_DIR)
	echo 'set timeout=0' > $@
	echo 'set default=0' >> $@
	echo 'menuentry "64-bit Kernel" {' >> $@
	echo '    multiboot2 /boot/kernel.elf' >> $@
	echo '    boot' >> $@
	echo '}' >> $@

# ------------------------
# Build ISO
# ------------------------
iso: $(ISO)/boot/kernel.elf $(GRUB_CFG)
	grub-mkrescue -o $(BUILD)/AMS.iso iso --modules="biosdisk part_msdos multiboot"

# ------------------------
# Run in QEMU
# ------------------------
run: iso
	$(QEMU) -cdrom $(BUILD)/AMS.iso -boot d -serial stdio

# ------------------------
# Clean build artifacts
# ------------------------
clean:
	rm -rf $(BUILD) $(ISO)

# ------------------------
# Rebuild and run
# ------------------------
rebuild: clean run
