# System Architecture

[PL](../03-architektura-systemu.md) | [EN](./03-system-architecture.md)

AMS-OS uses strict kernel/userspace separation on x86_64:

- ring 0: kernel subsystems, drivers, memory, scheduler,
- ring 3: ELF userspace processes,
- controlled transitions through syscall entry and `iretq`.

Core subsystems include memory (`PMM`, `VMM`), filesystem (`VFS`, `EXT2`), tasking/scheduler, and graphics/session runtime.
