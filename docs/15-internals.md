# Internals

[PL](./15-internals.md) | [EN](./en/15-internals.md)

## Runtime flow map

AMS-OS internals can be understood in four deterministic phases:

1. **Bring-up**: PMM/VMM/GDT/IDT/drivers/scheduler bootstrap.
2. **System activation**: VFS/storage/graphics and runtime services.
3. **Userspace launch**: ELF load, stack setup, ring3 transition.
4. **Operational loop**: scheduler + interrupts + syscalls + GUI/session.

## Critical codepaths

- Kernel startup orchestration: `src/kernel/kernel.cpp`
- Ring transition trampoline: `src/arch/x86_64/user_jump.s`
- Syscall and privilege path: `src/arch/x86_64/syscall*`
- Memory management: `src/memory/*`
- Filesystem and loader: `src/fs/*`

## Invariant checklist

For internals stability, maintain these invariants:

- active address space (`CR3`) matches current task context,
- userspace stack remains ABI-aligned on process entry,
- `iretq` frame is fully valid (`SS`, `RSP`, `RFLAGS`, `CS`, `RIP`),
- mapped ELF segments are zeroed and copied consistently.

## Deep dives

- `08-sekwencja-bootowania.md`
- `09-pamiec-krok-po-kroku.md`
- `10-vfs-ext2-i-elf.md`
- `11-przerwania-i-syscall.md`
