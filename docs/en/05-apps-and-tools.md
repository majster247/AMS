# Applications and Tools

[PL](../05-aplikacje-i-narzedzia.md) | [EN](./05-apps-and-tools.md)

AMS-OS builds userspace tools and test applications:

## System Tools
- `tcc` - TinyCC C compiler
- `gcc` - GCC frontend wrapper
- `bash` - Basic shell
- `jobctl` - Job control utility
- `apt` - Package manager MVP (supports: wayland, mesa, drm, wlroots, pixman, cairo, libinput, libffi, mlibc, gcc)

## Games
- `doom` - Doomgeneric port (Freedoom WAD)

## Wayland Display Stack
- `ams-wl-compositor` - Wayland compositor (DRM/KMS backend with GEM/TTM, wlroots-based architecture)
- `wayland-session` - Session supervisor (restarts compositor on crash)
- `ams-wayland-shell` - Wayland shell client
- `wayland-smoke-client` - SHM buffer test client
- `wayland_smoke` - AF_UNIX + SCM_RIGHTS IPC test
- `wayland_egl_smoke` - DRM/GEM/EGL/GBM integration test

## Graphics Stack
- **DRM/KMS** - Kernel-integrated Direct Rendering Manager (GEM buffer objects, TTM placement, KMS mode setting)
- **Mesa3D** - EGL + GBM + software rasterizer (swrast)
- **pixman** - Software pixel manipulation
- **cairo** - 2D vector graphics library

## Input Stack
- **evdev** - Linux-compatible input event interface (/dev/input/event0, event1)
- **libinput** - Input device handling
- **libevdev** - evdev library

## Libraries
- **libffi** - Foreign function interface (for Wayland protocol dispatch)
- **mlibc** - Portable C library (managarm project, AMS sysdeps layer)
- **libwayland** - Wayland protocol library (wayland-scanner generated)

Artifacts are staged into `disk_run.img` under structured paths:
- `/tools/system` - system utilities
- `/tools/compiler` - compilers and runtime
- `/programs/doom` - Doom game
- `/programs/wayland` - Wayland stack
- `/programs/wayland/mesa` - Mesa3D artifacts
- `/programs/wayland/protocols` - Generated Wayland protocol sources
- `/dev/dri/card0` - DRM device node
- `/dev/input/event0` - Keyboard evdev device
- `/dev/input/event1` - Mouse evdev device
- `/dev/shm/` - POSIX shared memory directory
