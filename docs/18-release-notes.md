# Release Notes

[PL](./18-release-notes.md) | [EN](./en/18-release-notes.md)

## vNext (unreleased)

### Documentation

- Migrated `docs/` from legacy frontend assets to Markdown-first GitHub Pages layout.
- Added enterprise-style documentation IA:
  - Getting Started
  - Architecture Overview
  - Internals
  - Operations
  - Contributing
  - Release Notes
- Expanded technical deep dives:
  - boot sequence step-by-step
  - memory model step-by-step
  - VFS/EXT2/ELF path
  - interrupts and syscall path

### Platform

- Added GitHub Actions workflow for automatic Pages deployment from `docs/`.

## Release policy

For each future release, include:

- behavior changes,
- ABI/syscall compatibility notes,
- migration notes for contributors and test environments.
