# Debugging and Tests

[PL](../06-debugowanie.md) | [EN](./06-debugging.md)

Primary diagnostics:

- serial logs from kernel runtime,
- `qemu.log`,
- assembly dump via `make dump`.

Recommended triage path:

1. reproduce in `make run_nograph`,
2. isolate subsystem (memory/ELF/syscall/scheduler/GUI),
3. validate with targeted rerun.
