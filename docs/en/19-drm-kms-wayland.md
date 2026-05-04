# DRM/KMS, GEM/TTM, and the wlroots Compositor

## Overview

AMS-OS now includes a proper GPU/display stack built on industry-standard
kernel interfaces and a wlroots-based Wayland compositor.

```
┌──────────────────────────────────────────────────────────┐
│          Wayland Clients (EGL/SHM applications)          │
└────────────────────┬─────────────────────────────────────┘
                     │  Wayland protocol (Unix domain socket)
┌────────────────────▼─────────────────────────────────────┐
│               ams-compositor (wlroots)                   │
│  • xdg-shell  • layer-shell  • seat / libinput           │
│  • pixman (SHM compositing)  • cairo (chrome/UI)         │
└────────────────────┬─────────────────────────────────────┘
                     │  DRM/KMS ioctls + GBM buffer alloc
┌────────────────────▼─────────────────────────────────────┐
│          Mesa3D  (EGL + GBM + swrast/llvmpipe)           │
│  libEGL  libGBM  libGL  libGLESv2  libOSMesa             │
└────────────────────┬─────────────────────────────────────┘
                     │  open("/dev/dri/card0") + ioctl()
┌────────────────────▼─────────────────────────────────────┐
│          AMS-OS Kernel: DRM / GEM / TTM / KMS            │
│  src/drivers/drm/gem_ttm.cpp                             │
│  • GEM dumb buffers  • KMS CRTC/connector/mode           │
│  • DRM_IOCTL_MODE_CREATE_DUMB / MAP_DUMB / PAGE_FLIP     │
│  • PRIME dma-buf export/import                           │
└────────────────────┬─────────────────────────────────────┘
                     │  multiboot2 linear framebuffer
┌────────────────────▼─────────────────────────────────────┐
│          Hardware / QEMU VGA framebuffer                 │
└──────────────────────────────────────────────────────────┘
```

## Kernel Subsystems

### GEM (Graphics Execution Manager)

File: `source/src/drivers/drm/gem_ttm.cpp`  
Header: `source/include/drm/gem_ttm.h`

GEM buffer objects (BOs) hold GPU-accessible memory.  Each BO is backed by
contiguous physical pages allocated from the PMM.  The subsystem is
initialised by `gem_ttm_init()` which is called from `init_syscall_table()`.

Key operations:
| Function | Description |
|----------|-------------|
| `gem_create(size, placement)` | Allocate a new BO |
| `gem_mmap_bo(bo)` | Map a BO into the current user task |
| `gem_put(bo)` / `gem_get(bo)` | Reference counting |
| `gem_lookup(handle)` | Look up a BO by handle |

TTM placement flags (`TTM_PL_FLAG_*`) control whether a BO lives in system
RAM or the VRAM region.

### KMS (Kernel Mode Setting)

AMS-OS exposes a single CRTC (id=1) and a single connector (id=1) that
map to the multiboot2 linear framebuffer.  `kms_page_flip()` blits a GEM
BO through the kernel backbuffer and calls `graphics_flip()` to push the
image to the physical LFB.

### DRM Device

User space opens `/dev/dri/card0` via `sys_open()`.  The resulting fd has
kind `FD_KIND_DRM`; all `ioctl()` calls on it are routed to `drm_ioctl()`.

Supported ioctls:

| Ioctl | Description |
|-------|-------------|
| `DRM_IOCTL_GET_CAP` | Report capabilities (dumb buffers = yes) |
| `DRM_IOCTL_MODE_GETRESOURCES` | Enumerate CRTCs / connectors |
| `DRM_IOCTL_MODE_GETCRTC` | Get CRTC state + current mode |
| `DRM_IOCTL_MODE_SETCRTC` | Configure scanout |
| `DRM_IOCTL_MODE_CREATE_DUMB` | Allocate a dumb GEM buffer |
| `DRM_IOCTL_MODE_MAP_DUMB` | Get mmap offset for a dumb buffer |
| `DRM_IOCTL_MODE_DESTROY_DUMB` | Free a dumb buffer |
| `DRM_IOCTL_MODE_PAGE_FLIP` | Flip to a new scanout buffer |
| `DRM_IOCTL_PRIME_HANDLE_TO_FD` | Export BO as dma-buf |
| `DRM_IOCTL_PRIME_FD_TO_HANDLE` | Import dma-buf as BO handle |

## Shared Memory (POSIX)

`shm_open(name, oflag, mode)` and `shm_unlink(name)` are implemented as
AMS custom syscalls 462 and 463.  Internally they use `memfd_create` with
a kernel-side name registry.

`mmap()` now supports `MAP_SHARED` on memfd-backed fds: the `tar_data`
buffer of the memfd vfs_node is mapped into the user task's address space
by translating kernel virtual addresses to physical frames via
`vmm_get_phys()`.

## libffi

A minimal x86-64 libffi is included at `source/src/lib/libffi/`.

- `ffi.c` – type descriptors, `ffi_call()`, closure allocation
- `ffi_asm.s` – NASM thunk that marshals integer/FP register arguments
  and calls the target function under the System V AMD64 ABI

libwayland and wlroots use libffi for their internal event dispatch.

## Building the Ports

### 1. Mesa3D (EGL + GBM + software renderer)

```bash
cd source
make mesa_stage
```

This clones `mesa/drm` and `mesa3d/mesa`, then builds with Meson:
```
platforms=wayland  gallium-drivers=swrast,zink  egl=enabled  gbm=enabled
```
Outputs are installed to `external/mesa-stack/prefix/`.

### 2. wlroots + libinput + pixman + cairo

```bash
make wlroots_stage
```

Builds in order:
1. wayland (libwayland-server, wayland-scanner)
2. wayland-protocols
3. xkbcommon
4. pixman
5. libinput
6. cairo
7. wlroots (backends: drm, libinput; renderers: gles2, pixman; allocators: gbm)

Then runs `wayland-scanner` to generate C bindings for:
- `xdg-shell`
- `xdg-output-unstable-v1`
- `wlr-layer-shell-unstable-v1`
- `wlr-output-management-unstable-v1`

### 3. Full compositor build

After both stages:
```bash
make build/ams_compositor.elf
```

The Makefile detects `external/wlroots-stack/prefix/include/wlr/backend.h`
and automatically adds `-DHAVE_WLROOTS` plus the required include/library paths.

Without the wlroots stage the compositor compiles in **bootstrap mode**:
a self-contained binary that uses the AMS DRM syscalls directly and
listens on the Wayland socket.

## Custom Syscalls Added

| Number | Name | Description |
|--------|------|-------------|
| 460 | `SYS_AMS_DRM_OPEN` | Open `/dev/dri/card0` |
| 461 | `SYS_AMS_DRM_IOCTL` | DRM ioctl pass-through |
| 462 | `SYS_AMS_SHM_OPEN` | POSIX `shm_open` |
| 463 | `SYS_AMS_SHM_UNLINK` | POSIX `shm_unlink` |
| 464 | `SYS_AMS_GBM_ALLOC` | GBM BO alloc stub |
