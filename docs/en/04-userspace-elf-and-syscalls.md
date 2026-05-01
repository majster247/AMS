# Userspace, ELF and Syscalls

[PL](../04-uzytkownik-i-syscalls.md) | [EN](./04-userspace-elf-and-syscalls.md)

`sys_exec` is the central process-launch path:

1. finds executable in VFS,
2. parses ELF headers,
3. maps `PT_LOAD` segments into a fresh userspace address space,
4. builds user stack (`argc`, `argv`, `envp`, `auxv`),
5. switches context and enters ring 3.

Ring transition is performed by `iretq` with validated userspace selectors and stack frame.
