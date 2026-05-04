# Architecture Overview

[PL](./14-architecture-overview.md) | [EN](./en/14-architecture-overview.md)

## System profile

AMS-OS is a 64-bit x86_64 operating system with kernel/user separation and ELF-based userspace execution.

## Core domains

- **Boot and platform bring-up**: multiboot handoff, early kernel init.
- **Memory management**: PMM + VMM + per-process address spaces.
- **Execution model**: scheduler, task state, ring transitions.
- **System interface**: syscall ABI and userspace runtime.
- **Storage and files**: VFS abstraction with EXT2 backend path.
- **Graphics and Display**: DRM/KMS + Wayland compositor (wlroots-based).
- **Input**: evdev + libinput integration.

## Display Stack Architecture

```
  ┌─────────────────────────────────────────────┐
  │              Wayland Clients                │
  │     (ams-wayland-shell, applications)       │
  └────────────────┬────────────────────────────┘
                   │ Wayland protocol (AF_UNIX)
  ┌────────────────▼────────────────────────────┐
  │         ams-wl-compositor (wlroots)         │
  │   ┌──────────┬──────────┬──────────┐        │
  │   │ pixman   │  cairo   │ Mesa EGL │        │
  │   │ renderer │  drawing │ GBM/GLES │        │
  │   └──────────┴──────────┴──────────┘        │
  │   ┌──────────────────┬─────────────┐        │
  │   │   DRM backend    │  libinput   │        │
  │   │ (KMS mode set)   │  backend    │        │
  │   └──────────────────┴─────────────┘        │
  └────────────────┬──────────┬─────────────────┘
                   │          │
  ┌────────────────▼──────────▼─────────────────┐
  │              AMS-OS Kernel                  │
  │   ┌──────────────────┬─────────────┐        │
  │   │   DRM/KMS        │   evdev     │        │
  │   │ ┌────────┐       │  input      │        │
  │   │ │  GEM   │       │  layer      │        │
  │   │ │ buffer │       │             │        │
  │   │ │ mgmt   │       │ /dev/input/ │        │
  │   │ ├────────┤       │  event0,1   │        │
  │   │ │  TTM   │       │             │        │
  │   │ │ place  │       └─────────────┘        │
  │   │ │ ment   │                              │
  │   │ └────────┘       ┌─────────────┐        │
  │   │ /dev/dri/card0   │ Framebuffer │        │
  │   └──────────────────┤  (LFB/VBE) │        │
  │                      └─────────────┘        │
  └─────────────────────────────────────────────┘
```

### DRM/KMS Subsystem (kernel)

- **GEM** (Graphics Execution Manager): Buffer object allocation with handle-based access, reference counting, and flink naming for cross-process sharing.
- **TTM** (Translation Table Manager): Tracks buffer placement (system RAM, VRAM, TT/GART).
- **KMS** (Kernel Mode Setting): CRTC, connector, and encoder management. Dumb buffer allocation for software compositors. Framebuffer objects with page-flip.
- **Device**: `/dev/dri/card0` with standard DRM ioctl interface.

### Wayland Stack

- **libwayland**: Protocol marshalling via wayland-scanner generated code.
- **wayland-protocols**: xdg-shell, linux-dmabuf, etc.
- **wayland-scanner**: Host-built tool for protocol code generation.

### Compositor (wlroots-based)

- Multi-client support via poll(2).
- DRM/KMS backend with fallback to AMS `SYS_AMS_FB_BLIT`.
- Alpha-blending compositing with per-surface positioning.
- wl_shm buffer sharing via memfd + mmap.
- Full xdg-shell support (surfaces, toplevels, popups).
- wl_seat with pointer and keyboard.
- wl_output with mode reporting.

### Rendering

- **pixman**: Software pixel operations for compositing.
- **cairo**: 2D vector graphics for shell/UI drawing.
- **Mesa3D**: EGL/GBM/GLESv2 for GPU-accelerated path (swrast fallback).

### Input

- **evdev**: Linux-compatible input events (`struct input_event`).
- **libevdev**: Event device library.
- **libinput**: Unified input handling from evdev devices.

## IPC Mechanisms

| Mechanism | Syscalls |
|-----------|----------|
| AF_UNIX stream sockets | socket, bind, listen, accept, connect |
| SCM_RIGHTS FD passing | sendmsg, recvmsg |
| POSIX shared memory | shm_open, shm_unlink, mmap |
| Anonymous shared memory | memfd_create, ftruncate, mmap |
| poll/ppoll I/O | poll, ppoll |
| epoll | epoll_create1, epoll_ctl, epoll_wait |
| eventfd | eventfd2 |
| Pipes | pipe2 |

## Security boundaries

- ring 0 performs privileged operations and address-space control,
- ring 3 processes execute in mapped user pages only,
- syscall entry is the controlled boundary crossing.

## Architectural priorities

1. Deterministic boot and execution behavior.
2. Explicit memory ownership and mapping.
3. Debuggability through serial-first tracing.
4. Standard Linux ABI compatibility (mlibc, DRM, evdev).
5. Incremental migration toward full Wayland desktop.

## See also

- `03-architektura-systemu.md`
- `04-uzytkownik-i-syscalls.md`
- `15-internals.md`
