# Architecture Overview

[PL](../14-architecture-overview.md) | [EN](./14-architecture-overview.md)

AMS-OS architecture domains:

- boot bring-up,
- memory management,
- execution/task model,
- syscall ABI,
- VFS/storage,
- graphics/session runtime.

## Wayland/wlroots port status

The old in-tree Wayland compositor has been removed as the long-term display
server implementation. AMS now treats wlroots as the compositor source target
and stages the required upstream stack with:

- `make wayland_stage` for Wayland, wayland-protocols, wlroots, libinput,
  pixman and cairo,
- `make mesa_stage` for libdrm and Mesa's EGL/GBM software-first profile,
- `make wayland_port_preflight` for the kernel/libc capability matrix.

The current kernel already has early AF_UNIX sockets, `sendmsg`/`recvmsg`
with `SCM_RIGHTS`, `memfd_create`, `mmap`, `poll`, `ppoll`, `epoll` and
`eventfd` entry points. Userspace now also exposes a `shm_open` compatibility
wrapper backed by `memfd_create`. The preflight target intentionally keeps
DRM/KMS, GEM/TTM, a production mlibc port and libffi marked as missing until
those subsystems are real enough for Mesa and wlroots.
