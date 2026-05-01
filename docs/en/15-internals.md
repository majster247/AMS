# Internals

[PL](../15-internals.md) | [EN](./15-internals.md)

Core internal invariants:

- `CR3` and task context consistency,
- ABI-aligned userspace stack,
- valid `iretq` transition frame,
- consistent ELF segment mapping and zeroing.
