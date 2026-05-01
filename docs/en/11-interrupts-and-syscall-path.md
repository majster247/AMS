# Interrupts and Syscall Path

[PL](../11-przerwania-i-syscall.md) | [EN](./11-interrupts-and-syscall-path.md)

Interrupt and syscall architecture relies on:

- GDT and privilege model setup,
- IDT entries and ISR paths,
- syscall entry from ring 3 to ring 0,
- safe return to userspace context.

Ring transitions must keep stack, selectors and frame integrity consistent to avoid critical faults.
