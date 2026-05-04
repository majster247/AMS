# System Architecture

[PL](../03-architektura-systemu.md) | [EN](./03-system-architecture.md)

AMS-OS uses strict kernel/userspace separation on x86_64:

- ring 0: kernel subsystems, drivers, memory, scheduler,
- ring 3: ELF userspace processes,
- controlled transitions through syscall entry and `iretq`.

Core subsystems include memory (`PMM`, `VMM`), filesystem (`VFS`, `EXT2`), tasking/scheduler, and graphics/session runtime.

## Graphics / Display Stack

AMS-OS implements a modern Linux-compatible graphics stack:

- **DRM/KMS** — Kernel Mode Setting with GEM (Graphics Execution Manager) and TTM (Translation Table Maps) for buffer management. Virtual KMS output maps to the Multiboot2 linear framebuffer.
- **Unix domain sockets** — `AF_UNIX SOCK_STREAM` with `SCM_RIGHTS` fd-passing for Wayland IPC.
- **Shared memory** — `shm_open`/`mmap` and `memfd_create`/`mmap` for client buffer sharing.
- **Poll/Epoll** — Event-driven I/O multiplexing (`poll`, `ppoll`, `epoll_create1`, `epoll_ctl`, `epoll_wait`).
- **Wayland compositor** — wlroots-style compositor using DRM/KMS backend, pixman software rendering, and the Wayland wire protocol with xdg-shell.
- **Mesa3D (staged)** — EGL + GBM via Mesa software rasterizer (`swrast`), ported from upstream source.
- **libinput** — Input handling via AMS kernel input syscalls mapped to libinput events.
- **pixman/cairo** — Software rendering for 2D composition and UI drawing.
