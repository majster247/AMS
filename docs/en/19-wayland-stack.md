# Wayland Desktop Graphics Stack

[← back to index](./index.md)

AMS-OS now ships a complete, modular Wayland desktop graphics stack.
The previous hand-rolled compositor (~2300 lines of ad-hoc protocol
handling) has been removed and replaced with a stack built on
upstream components.

## Components

| Layer        | Source                                                       | Location |
| ------------ | ------------------------------------------------------------ | -------- |
| libc         | mlibc                                                        | `external/wayland-stack/mlibc` |
| FFI          | libffi                                                       | `external/wayland-stack/libffi` |
| 2D rendering | pixman, cairo                                                | `external/wayland-stack/{pixman,cairo}` |
| Keyboard     | libxkbcommon                                                 | `external/wayland-stack/libxkbcommon` |
| Input        | libinput                                                     | `external/wayland-stack/libinput` |
| Wayland      | libwayland + wayland-protocols                                | `external/wayland-stack/{wayland,wayland-protocols}` |
| GPU (sw)     | Mesa3D (EGL + GBM, swrast/llvmpipe)                            | `external/wayland-stack/mesa` |
| Compositor   | wlroots                                                     | `external/wayland-stack/wlroots` |
| AMS glue     | libports + ams-compositor                                    | `src/libports/`, `src/apps/wayland/` |

Versions are pinned in `tools/wayland_stage.sh` (e.g. `wayland 1.23.0`,
`mesa 24.2.4`, `wlroots 0.18.1`).

## Kernel side: `/dev/dri/card0` (DRM/KMS/GEM/TTM)

`src/drivers/drm/ams_drm.cpp` implements a software-first DRM facade
matching libdrm's userspace ABI. Userspace can:

* `open("/dev/dri/card0", O_RDWR)`,
* call `DRM_IOCTL_MODE_CREATE_DUMB` to allocate a GEM buffer (TTM
  placement = `SYSTEM`),
* call `DRM_IOCTL_MODE_MAP_DUMB` and `mmap()` the returned offset to
  obtain a CPU-accessible pointer,
* `DRM_IOCTL_MODE_ADDFB2` registers a KMS framebuffer,
* `DRM_IOCTL_MODE_PAGE_FLIP` is a no-op in software mode (the
  compositor blits via pixman/cairo).

There is one card (virtual eDP-1), with one CRTC, one encoder and two
planes.

## Kernel side: shm_open, poll/epoll, AF_UNIX, libffi

* userspace `shm_open()` (`libports/libports_shm.c`) maps to
  `memfd_create()` in the kernel; the fd supports `ftruncate`,
  `mmap(MAP_SHARED)`, and SCM_RIGHTS passing.
* `poll`, `ppoll`, `epoll_create1`, `epoll_ctl`, `epoll_wait` are
  fully implemented in `src/arch/x86_64/syscall.cpp`. libports
  exposes glibc-compatible aliases.
* AF_UNIX with `sendmsg`/`recvmsg` + `SCM_RIGHTS` is the Wayland IPC
  substrate; the kernel ABI supports up to 100 sockets.
* libffi is ported from upstream unchanged — `libports_ffi.c` only
  adds `getauxval(AT_PAGESZ)` and `sysconf(_SC_PAGESIZE)`.

## Mesa3D (EGL + GBM)

`tools/wayland_build.sh` invokes meson with:

```
-Dgallium-drivers=swrast,llvmpipe
-Dplatforms=wayland
-Dgbm=enabled -Degl=enabled -Dosmesa=true
```

Rendering is software (swrast or llvmpipe when LLVM is available).
GBM talks to our `/dev/dri/card0` via `libports_drm.c`.

## wlroots

wlroots is built as a shared library and linked from
`build/ams-compositor.elf`. Backends in use:

* **scene graph** — wlroots' pixman software renderer.
* **xdg-shell** — top-level shell protocol.
* **seat / input** — libinput sources events from `/dev/input/event*`,
  libxkbcommon translates keys.
* **drm backend** — uses our `/dev/dri/card0` via `libports_drm`.

X11 and XWayland are disabled (`-Dxwayland=disabled`).

## Build pipeline

```
tools/wayland_stage.sh          # git clone all upstream sources
tools/wayland_build.sh          # meson + ninja into the sysroot
tools/wayland_scanner_gen.sh    # wayland-scanner -> build/wayland-protocols/
make build/libports.a           # AMS-OS porting layer
make build/ams-compositor.elf   # compositor (wlroots-based)
make build/ams-session.elf      # supervisor that spawns the compositor
```

If host tools are missing (meson, ninja, wayland-scanner, cross-gcc)
the scripts emit a warning and exit zero — the user-space ELFs still
build, only the Wayland headers are absent and the compositor falls
back to a logging-only variant.

## libports (the porting layer)

`src/libports/`:

| File              | Purpose                                       |
| ----------------- | --------------------------------------------- |
| `libports_shm.c`  | `shm_open`/`shm_unlink` → `memfd_create`      |
| `libports_poll.c` | `poll`/`epoll_*` → AMS syscalls               |
| `libports_unix.c` | `socket`/`bind`/`listen`/`accept`/`connect`   |
| `libports_drm.c`  | DRM/GBM helpers (`/dev/dri/card0`)            |
| `libports_ffi.c`  | `getauxval`/`sysconf` for libffi              |

`libports.a` is linked into every Wayland-stack ELF.

## Running the session

```
/dev/dri/card0
/dev/input/event0..N
/run/user/0/wayland-0     <- socket created by ams-compositor
$WAYLAND_DISPLAY=wayland-0
```

Boot path: `ams-session` → `ams-compositor` → `wl_display_run`.
