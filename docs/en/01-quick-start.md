# Quick Start

[PL](../01-szybki-start.md) | [EN](./01-quick-start.md)

## 1. Clone and enter build directory

```bash
git clone <repository-url>
cd AMS-1/source
```

## 2. Host requirements

- Linux or WSL2 recommended,
- `make`, `bash`, `tar`, `nasm`,
- cross-toolchain `x86_64-elf-*`,
- `qemu-system-x86_64`, `qemu-img`,
- `grub-mkrescue`, `mkfs.ext2`, `mount`, `umount`.

## 3. Build all artifacts

```bash
make all
```

## 4. Run

```bash
make run
make run_nograph
make run_virgl
```
