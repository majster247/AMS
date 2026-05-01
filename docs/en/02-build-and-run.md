# Build and Run

[PL](../02-budowanie-i-uruchamianie.md) | [EN](./02-build-and-run.md)

`source/Makefile` is the single source of truth for AMS-OS build and runtime automation.

## Main targets

- `make all` - builds kernel, userspace binaries, ISO and EXT2 disk image.
- `make run` - default graphical QEMU run.
- `make run_nograph` - serial/text diagnostic run.
- `make run_virgl` - OpenGL path validation.
- `make clean` - cleanup artifacts.

## Main outputs

- `kernel.elf`
- `ams_run.iso`
- `disk_run.img`
- `build/*.elf`
