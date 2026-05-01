# Boot Sequence Step by Step

[PL](../08-sekwencja-bootowania.md) | [EN](./08-boot-sequence-step-by-step.md)

High-level boot flow:

1. firmware -> GRUB (`ams_run.iso`),
2. GRUB loads `kernel.elf` and `initrd.tar`,
3. kernel initializes PMM and VMM,
4. GDT/IDT/drivers/syscalls/VFS are initialized,
5. scheduler + graphics/session runtime start,
6. userspace launch enters ring 3 through `iretq`.
