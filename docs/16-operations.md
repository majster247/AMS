# Operations

[PL](./16-operations.md) | [EN](./en/16-operations.md)

## Build operations

Standard operations:

- full build: `make all`
- clean rebuild: `make clean && make all`
- artifact inspection: check `kernel.elf`, `ams_run.iso`, `disk_run.img`, `build/*.elf`

## Runtime operations

- text diagnostics: `make run_nograph`
- default graphical run: `make run`
- GL path validation: `make run_virgl`

## Incident response

When a regression appears:

1. reproduce in `run_nograph`,
2. capture serial and QEMU logs,
3. isolate domain (memory, ELF, syscall, scheduler, GUI),
4. validate with targeted rerun.

## Documentation operations

- Keep docs updated in same PR as architecture/ABI changes.
- Treat docs drift as release risk.
- Rebuild mental model from `14-architecture-overview.md` and `15-internals.md` before deep fixes.
