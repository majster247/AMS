# Contributing

[PL](./17-contributing.md) | [EN](./en/17-contributing.md)

## Contribution standard

AMS-OS contributions should follow system-level quality practices:

- small, reviewable changes,
- explicit rationale for low-level changes,
- no hidden ABI changes,
- docs updated with code.

## Pull request checklist

Before opening a PR:

1. run `make all`,
2. validate `make run_nograph`,
3. update impacted docs pages,
4. describe behavior change and risk profile.

## Required PR sections

- **Problem**: what is broken or missing.
- **Approach**: why this implementation is chosen.
- **Risk**: what can regress (boot, memory, syscall, GUI, tooling).
- **Validation**: commands and observed result.

## Change categories that require extra care

- memory mapping and page-table logic,
- ring transition and syscall entry/return,
- ELF loading and userspace stack layout,
- build pipeline and disk/ISO staging.
