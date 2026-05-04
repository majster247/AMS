# AMS Linux Compatibility Layer — Implementation Plan

## Overview

This document describes the plan for adding missing Linux subsystems to AMS so that
**wlroots** (and eventually a full Sway-like compositor) can be built and run natively.
Each section corresponds to one row in the requirements matrix.

## Architecture Diagram

```
┌─────────────────── User Space ───────────────────┐
│  wlroots compositor  ←→  libwayland-server       │
│       ↓                       ↓                   │
│   libinput (AMS)      wayland-scanner generated   │
│       ↓                                           │
│   pixman (AMS)         libffi (stubs)             │
│       ↓                                           │
│   Mesa EGL/GBM (swrast)                           │
│       ↓                                           │
│   libc (mlibc-compat)                             │
└───────────┬───────────────────┬───────────────────┘
            │  syscall boundary │
┌───────────▼───────────────────▼───────────────────┐
│                AMS Kernel                         │
│  ┌──────────┐ ┌──────────┐ ┌───────────────────┐ │
│  │ DRM/KMS  │ │  evdev   │ │  AF_UNIX / epoll  │ │
│  │ (bridge  │ │ (bridge  │ │  memfd / mmap     │ │
│  │  to FB)  │ │  PS/2)   │ │  (existing)       │ │
│  └──────────┘ └──────────┘ └───────────────────┘ │
│  ┌──────────────────────────────────────────────┐ │
│  │         Framebuffer / PS/2 drivers           │ │
│  └──────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────┘
```

---

## 1. DRM/KMS Kernel Subsystem

**Status**: Not implemented  
**Why needed**: wlroots backend requires `ioctl()` on `/dev/dri/card0` for mode
setting and buffer management.

**Implementation approach** — bridge DRM ioctls to existing AMS framebuffer:

- New kernel file: `src/drivers/video/drm_ams.cpp`
- Opens as `/dev/dri/card0` (VFS virtual device node)
- Supports minimal DRM ioctl set:
  - `DRM_IOCTL_VERSION` — returns driver info
  - `DRM_IOCTL_GET_CAP` — advertise DUMB_BUFFER capability
  - `DRM_IOCTL_MODE_GETRESOURCES` — one CRTC, one connector, one encoder
  - `DRM_IOCTL_MODE_GETCONNECTOR` — connected, mode = current FB resolution
  - `DRM_IOCTL_MODE_GETCRTC` / `SET_CRTC` — current mode
  - `DRM_IOCTL_MODE_CREATE_DUMB` / `MAP_DUMB` / `DESTROY_DUMB` — allocate mmap-able buffers
  - `DRM_IOCTL_MODE_ADDFB` / `RMFB` — framebuffer objects
  - `DRM_IOCTL_MODE_PAGE_FLIP` — triggers blit to real framebuffer

---

## 2. evdev Input Layer

**Status**: Not implemented  
**Why needed**: libinput reads `/dev/input/event*` files using the evdev protocol.

**Implementation approach**:

- New kernel file: `src/drivers/input/evdev_ams.cpp`
- Exposes `/dev/input/event0` (keyboard) and `/dev/input/event1` (mouse)
- Translates PS/2 scancode queue into `struct input_event` (time, type, code, value)
- Supports `read()` to pull events and `poll()` / `epoll` readiness
- Supports ioctl: `EVIOCGNAME`, `EVIOCGID`, `EVIOCGBIT`, `EVIOCGABS`

---

## 3. POSIX shm_open / shm_unlink

**Status**: Partial (memfd_create exists, shm_open missing in libc)  
**Why needed**: Some Wayland clients and libraries use `shm_open()`.

**Implementation approach**:

- Add `shm_open()` and `shm_unlink()` to `src/lib/stubs.c`
- `shm_open()` → `memfd_create(name, 0)` (already supported in kernel)
- `shm_unlink()` → no-op (memfd are anonymous)
- Add header `include/sys/mman.h` with declarations

---

## 4. Pixman Library

**Status**: Not implemented  
**Why needed**: wlroots pixman renderer requires it for software compositing.

**Implementation approach** — minimal AMS-native pixman:

- New files under `src/lib/pixman/`
- Implements core types: `pixman_image_t`, `pixman_region32_t`, `pixman_box32_t`
- Core functions:
  - `pixman_image_create_bits()` — wraps a pixel buffer
  - `pixman_image_composite32()` — SRC/OVER compositing
  - `pixman_region32_*()` — rectangle-based region math
  - `pixman_image_set_clip_region32()`, `pixman_image_set_transform()`
- Software-only, CPU rendering (matching AMS's existing approach)

---

## 5. libffi

**Status**: Not implemented  
**Why needed**: libwayland uses libffi for closure dispatch in signal/listener system.

**Implementation approach** — static stubs:

- New file: `src/lib/libffi_stubs.c`
- Provides `ffi_prep_cif()`, `ffi_prep_closure_loc()`, `ffi_call()`
- Closure dispatch: pre-allocate trampolines using simple function pointers
- Sufficient for wayland's limited use (signal dispatch pattern)

---

## 6. wayland-scanner Integration

**Status**: Not implemented (apps use hand-coded wire messages)  
**Why needed**: wlroots builds with generated protocol headers from XML.

**Implementation approach**:

- New script: `tools/wayland_scanner_stage.sh`
- Downloads or builds `wayland-scanner` as a **host tool**
- Processes `*.xml` protocol files → generates C headers + marshalling code
- Generated files placed in `external/wayland-stack/generated/`
- Makefile target: `wayland_scanner_gen`

---

## 7. libinput Compatibility Layer

**Status**: Not implemented  
**Why needed**: wlroots libinput backend.

**Implementation approach** — thin AMS-native libinput:

- New files under `src/lib/libinput/`
- `libinput_ams.c`: reads from `/dev/input/event*` (our evdev layer)
- Provides: `libinput_path_create_context()`, `libinput_get_fd()`,
  `libinput_dispatch()`, `libinput_get_event()`
- Event types: `LIBINPUT_EVENT_POINTER_MOTION`, `LIBINPUT_EVENT_KEYBOARD_KEY`,
  `LIBINPUT_EVENT_POINTER_BUTTON`
- Maps directly to evdev events from our kernel driver

---

## 8. GBM (Generic Buffer Management)

**Status**: Not implemented  
**Why needed**: wlroots allocator uses GBM to create scanout buffers.

**Implementation approach**:

- New file: `src/lib/gbm_ams.c`
- `gbm_create_device()` — wraps our DRM fd
- `gbm_bo_create()` → `DRM_IOCTL_MODE_CREATE_DUMB`
- `gbm_bo_map()` → `DRM_IOCTL_MODE_MAP_DUMB` + `mmap`
- `gbm_surface_create()` — ring buffer of 2–3 dumb buffers
- Sufficient for swrast + wlroots pixman renderer path

---

## 9. Mesa EGL/GBM Software Renderer

**Status**: Stub only  
**Why needed**: Some wlroots paths require minimal EGL.

**Implementation approach**:

- New file: `src/lib/egl_stubs.c`
- Software EGL implementation over dumb buffers
- `eglGetDisplay()`, `eglInitialize()`, `eglCreateContext()`
- `eglCreateWindowSurface()` → returns a GBM-backed surface
- `eglSwapBuffers()` → page flip through DRM
- Enough for wlroots GLES2 renderer fallback; pixman renderer doesn't need EGL

---

## 10. mlibc-Compatible libc Enhancements

**Status**: Partial  
**Why needed**: wlroots and dependencies need more POSIX surface.

**Missing functions to add** (in `src/lib/` files):

- `src/lib/stubs.c`: `getenv()`, `setenv()`, `realpath()`, `clock_gettime()`,
  `strdup()`, `strerror()`, `strtol()`, `strtoul()`, `qsort()`, `bsearch()`
- `src/lib/stdlib.cpp`: proper `free()` (currently no-op), `realloc()` improvements
- `src/lib/string.cpp`: `memmove()`, `memcmp()`, `strncmp()`, `strrchr()`,
  `strncat()`, `strpbrk()`
- New `src/lib/math_stubs.c`: `log()`, `log2()`, `pow()`, `sqrt()`, `ceil()`,
  `floor()`, `round()`, `fmod()`
- New `src/lib/locale_stubs.c`: `setlocale()`, `localeconv()`
- New `src/lib/signal_stubs.c`: `signal()`, `sigaction()` (thin wrappers)
- New headers: `errno.h`, `fcntl.h`, `sys/types.h`, `sys/stat.h`, `sys/ioctl.h`,
  `dirent.h`, `dlfcn.h`, `pthread.h`

---

## 11. wlroots Staging & AMS Backend

**Status**: Not implemented  
**Why needed**: The final goal.

**Implementation approach**:

- New script: `tools/wlroots_stage.sh` — clones wlroots
- New file: `src/apps/wayland/wlroots_backend_ams.c`
  - Custom wlroots backend using AMS DRM/evdev
  - Implements `wlr_backend` interface
  - Output: one display at current resolution
  - Input: keyboard + mouse from evdev
- Build: cross-compile wlroots with meson, targeting x86_64-elf
  - Pixman renderer only (no GLES2 initially)
  - libinput backend → our libinput layer
  - DRM backend → our DRM layer

---

## Implementation Order

The dependencies form a DAG:

```
               ┌──── mlibc enhancements ────┐
               │                            │
     ┌─────────▼──────┐          ┌──────────▼─────────┐
     │  DRM/KMS kernel │          │  evdev kernel      │
     └─────────┬──────┘          └──────────┬─────────┘
               │                            │
     ┌─────────▼──────┐          ┌──────────▼─────────┐
     │  GBM library    │          │  libinput library  │
     └─────────┬──────┘          └──────────┬─────────┘
               │                            │
     ┌─────────▼──────┐          ┌──────────┘
     │  EGL stubs      │          │
     └─────────┬──────┘          │
               │    ┌────────────┤
               │    │            │
          ┌────▼────▼────┐  ┌───▼──────────┐
          │  pixman lib  │  │ libffi stubs │
          └────┬─────────┘  └───┬──────────┘
               │                │
          ┌────▼────────────────▼───┐
          │  wayland-scanner        │
          │  + shm_open             │
          └────┬────────────────────┘
               │
          ┌────▼────────────────────┐
          │  wlroots (AMS backend)  │
          └─────────────────────────┘
```

**Phase 1** (foundation): mlibc enhancements + shm_open + DRM/KMS + evdev  
**Phase 2** (libraries): GBM + pixman + libinput + libffi  
**Phase 3** (integration): wayland-scanner + EGL stubs  
**Phase 4** (wlroots): staging script + AMS backend + build integration
