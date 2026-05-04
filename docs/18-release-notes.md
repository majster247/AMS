# Release Notes

[PL](./18-release-notes.md) | [EN](./en/18-release-notes.md)

## vNext (unreleased)

### Wayland desktop graphics stack — pełny przepis

- Usunięto poprzedni ręcznie pisany kompozytor (`ams_wl_compositor.c`,
  `ams_wayland_shell.c`, `wayland_smoke.c`, `wayland_smoke_client.c`,
  `wayland_egl_smoke.c`, `wayland_session.c`).
- Wprowadzono moduł jądra `src/drivers/drm/ams_drm.cpp` z fasadą
  DRM/KMS/GEM/TTM (`/dev/dri/card0`, dumb buffers, mmap, addfb2,
  ioctl, page-flip).
- Dodano warstwę portu `src/libports/` (shm_open, poll/epoll, AF_UNIX,
  DRM/GBM helpers, libffi `getauxval`/`sysconf`) wraz z headerem
  `include/libports/libports.h`. Buduje się jako `build/libports.a`.
- Dodano stage script `tools/wayland_stage.sh` przypinający tagi:
  wayland 1.23.0, wayland-protocols 1.36, libxkbcommon 1.7.0,
  libinput 1.26.1, pixman 0.44.0, cairo 1.18.2, libffi 3.4.6,
  mlibc master, mesa 24.2.4, wlroots 0.18.1.
- Dodano build orchestrator `tools/wayland_build.sh` (meson + ninja,
  cross-file `tools/meson-cross-ams.ini`, sysroot
  `external/wayland-stack/sysroot`).
- Dodano integrację `wayland-scanner`
  (`tools/wayland_scanner_gen.sh` → `build/wayland-protocols/`).
- Nowy kompozytor `src/apps/wayland/ams_compositor.c` linkujący się
  do `wlroots`, `libwayland-server`, `libxkbcommon`, `libinput`,
  `pixman`, `cairo`, `EGL`, `GBM`, `libffi`.
- Nowe binaria desktopowe: `ams-compositor`, `ams-session`,
  `ams-smoke-client`, `ams-egl-smoke`.
- Boot-time launch przepiętły z `/wayland-session` na `/ams-session`.

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
