# Memory Step by Step

[PL](../09-pamiec-krok-po-kroku.md) | [EN](./09-memory-step-by-step.md)

Memory model in AMS-OS:

- PMM manages physical frames,
- VMM maps kernel and userspace virtual memory,
- HHDM enables kernel access to physical pages.

During `sys_exec`, a new userspace address space is built, ELF segments are mapped/copied, stack is prepared, and `CR3` is switched to process context.
