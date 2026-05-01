# Architecture Overview

[PL](./14-architecture-overview.md) | [EN](./en/14-architecture-overview.md)

## System profile

AMS-OS is a 64-bit x86_64 operating system with kernel/user separation and ELF-based userspace execution.

## Core domains

- **Boot and platform bring-up**: multiboot handoff, early kernel init.
- **Memory management**: PMM + VMM + per-process address spaces.
- **Execution model**: scheduler, task state, ring transitions.
- **System interface**: syscall ABI and userspace runtime.
- **Storage and files**: VFS abstraction with EXT2 backend path.
- **Graphics and UX**: desktop path and Wayland session path.

## Security boundaries

- ring 0 performs privileged operations and address-space control,
- ring 3 processes execute in mapped user pages only,
- syscall entry is the controlled boundary crossing.

## Architectural priorities

1. Deterministic boot and execution behavior.
2. Explicit memory ownership and mapping.
3. Debuggability through serial-first tracing.
4. Incremental migration path toward richer userspace.

## See also

- `03-architektura-systemu.md`
- `04-uzytkownik-i-syscalls.md`
- `15-internals.md`
