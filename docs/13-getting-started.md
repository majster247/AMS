# Getting Started

[PL](./13-getting-started.md) | [EN](./en/13-getting-started.md)

## Purpose

This page is the official onboarding entrypoint for engineers working with AMS-OS.

## Prerequisites

- Linux or WSL2 host,
- cross-toolchain (`x86_64-elf-*`),
- QEMU and disk/image tooling (`qemu-system-x86_64`, `qemu-img`, `mkfs.ext2`, `grub-mkrescue`).

## First successful run

1. Build everything:
   - `make all`
2. Validate boot logs in text mode:
   - `make run_nograph`
3. Validate graphical path:
   - `make run`

## What "ready" means

An environment is considered ready when:

- kernel builds without errors,
- `ams_run.iso` and `disk_run.img` are produced,
- userspace binaries are present in `build/`,
- system reaches boot/runtime loop in QEMU.

## Recommended reading order

1. `01-szybki-start.md`
2. `02-budowanie-i-uruchamianie.md`
3. `14-architecture-overview.md`
4. `15-internals.md`
