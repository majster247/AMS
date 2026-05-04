# Linux Compatibility Layer — Implementation Plan

> **Branch:** `cursor/linux-compat-layer-07ae`  
> **Goal:** Close every gap in the compatibility table; enable wlroots (swaywm/wlroots) to be compiled and linked against AMS, with a custom AMS backend that drives the existing framebuffer through a KMS/DRM abstraction, exposes input via evdev, and uses pixman for compositing.

---

## Gap table — before this work

| Requirement | Was in AMS | Status after this work |
|---|---|---|
| wlroots compositor | No | AMS backend provided |
| Wayland (full, libwayland) | Partial | Full wire + wayland-scanner headers |
| wayland-scanner | No | Pre-generated headers + script |
| libinput | No | Stub library over evdev |
| pixman | No | Minimal port (pixel ops) |
| cairo | No | Thin stub over pixman (optional) |
| Unix domain sockets | Yes | Unchanged |
| shm_open + mmap | Partial | Full: /dev/shm + MAP_SHARED file mmap |
| poll / epoll | Yes | Unchanged |
| libffi | No | Minimal x86_64 freestanding port |
| mlibc (as libc) | No | mlibc-compatible syscall surface |
| GEM / TTM | No | Dumb-buffer DRM (no GPU, software only) |
| KMS (kernel mode setting) | No | Virtual KMS over multiboot framebuffer |
| Mesa EGL + GBM | No | Stub EGL/GBM backed by pixman + DRM |

---

## Architecture after this work

```
┌────────────────────────────────────────────────────────┐
│                     Wayland clients                     │
│  (use libwayland-client or raw wire + generated hdrs)  │
└────────────────┬───────────────────────────────────────┘
                 │ Unix socket  /run/user/0/wayland-0
┌────────────────▼───────────────────────────────────────┐
│              wlroots compositor (AMS backend)           │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │  DRM/KMS │ │  evdev/  │ │  pixman  │ │  libffi  │  │
│  │  backend │ │ libinput │ │ renderer │ │ dispatch │  │
│  └─────┬────┘ └────┬─────┘ └────┬─────┘ └──────────┘  │
└────────┼───────────┼────────────┼───────────────────────┘
         │ ioctl     │ read()     │ memcpy
┌────────▼───────────▼────────────▼───────────────────────┐
│               AMS Kernel additions                        │
│  /dev/dri/card0  /dev/input/event{0,1}  /dev/shm/        │
│  DRM dumb-buf    evdev structs           shm_open shim    │
│  sys_mmap(fd)    mlibc syscall surface   futex/clone stubs│
└──────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────┐
│       Multiboot2 linear framebuffer  +  PS/2 keyboard/    │
│       mouse  (unchanged hardware layer)                   │
└──────────────────────────────────────────────────────────┘
```

---

## Work items — detailed

### 1. Kernel: virtual DRM/KMS device (`/dev/dri/card0`)

**File:** `source/src/drivers/drm/drm_virt.cpp`  
**Header:** `source/include/drm_virt.h`

Implements a character-device-like VFS node mounted at `/dev/dri/card0`.
`ioctl` on this FD handles a minimum DRM subset:

| ioctl | Constant | Behaviour |
|---|---|---|
| `DRM_IOCTL_VERSION` | 0xC0406400 | Returns "ams-drm 0.1" |
| `DRM_IOCTL_GET_CAP` | 0xC01064 0C | DRM_CAP_DUMB_BUFFER=1, others 0 |
| `DRM_IOCTL_MODE_GETRESOURCES` | 0xC04064A0 | 1 CRTC, 1 encoder, 1 connector |
| `DRM_IOCTL_MODE_GETCONNECTOR` | 0xC05064A7 | mode 1920×1080 (or actual fb) |
| `DRM_IOCTL_MODE_GETCRTC` | 0xC06864A1 | current mode |
| `DRM_IOCTL_MODE_CREATE_DUMB` | 0xC02064B2 | alloc page-backed buffer, return handle |
| `DRM_IOCTL_MODE_MAP_DUMB` | 0xC01064B3 | return offset for mmap |
| `DRM_IOCTL_MODE_ADDFB` | 0xC06464AE | associate dumb buf with framebuffer |
| `DRM_IOCTL_MODE_SETCRTC` | 0xC06864A2 | schedule scanout → `graphics_flip()` |
| `DRM_IOCTL_MODE_PAGE_FLIP` | 0xC01864B0 | flip → `graphics_flip()` |
| `DRM_IOCTL_MODE_DESTROY_DUMB` | 0xC00464B4 | free buffer |

`sys_mmap` gains a new code path: when `fd` points to a DRM dumb-buffer offset, it maps the same physical pages that were allocated during `CREATE_DUMB` into the calling process.

### 2. Kernel: evdev virtual input devices

**Files:** `source/src/drivers/input/evdev.cpp`, `source/include/evdev.h`  
**Devices:** `/dev/input/event0` (keyboard), `/dev/input/event1` (mouse/pointer)

Each device is a VFS node. `read()` on it returns `struct input_event` records (Linux ABI: `timeval` + `type` + `code` + `value`, 24 bytes each).

The existing PS/2 keyboard IRQ handler (`keyboard.cpp`) and mouse handler (`mouse.cpp`) are extended to push events into per-device ring-buffers. A `poll`/`epoll` notifier wakes blocked readers.

Keyboard: `EV_KEY` events with Linux `KEY_*` codes.  
Mouse: `EV_REL REL_X/Y` + `EV_KEY BTN_LEFT/RIGHT/MIDDLE` + `EV_SYN`.

### 3. Kernel: fix `sys_mmap` for file-backed shared mappings

**File:** `source/src/arch/x86_64/syscall.cpp` (`sys_mmap`)

Current implementation ignores `fd` and `flags`. Extended to:

- If `flags & MAP_SHARED` and `fd >= 0`: look up the VFS node, map its `tar_data` pages (for memfd) or DRM dumb-buffer pages at a fresh virtual address, return same physical addresses to all callers sharing the same node → enables Wayland SHM pools across compositor + client.
- `MAP_ANONYMOUS`: unchanged bump-pointer allocation.
- `sys_munmap` demaps pages from the page table and returns physical frames to PMM (no more stub).

### 4. Kernel: `shm_open` / `shm_unlink` via `/dev/shm`

**File:** `source/src/arch/x86_64/syscall.cpp` (extended `sys_open`)

Create a VFS directory node for `/dev/shm/`. When `sys_open` receives a path beginning with `/dev/shm/`, it creates a new memfd-backed node (or opens an existing one). `shm_unlink` calls `sys_unlink` on that node.

Userspace wrapper in `source/src/lib/shm.c`:
```c
int shm_open(const char* name, int oflag, mode_t mode);
int shm_unlink(const char* name);
```

### 5. Userspace: wayland-scanner protocol headers

**Directory:** `source/include/wayland-generated/`  
**Script:** `source/tools/wayland_scan.sh`

Pre-generate (or vendor) C headers from the key XML protocols:
- `wayland.xml` → `wayland-client-protocol.h`, `wayland-server-protocol.h`
- `xdg-shell.xml` → `xdg-shell-client-protocol.h`, `xdg-shell-server-protocol.h`
- `linux-dmabuf-unstable-v1.xml` → `linux-dmabuf-unstable-v1-protocol.h`
- `wl-drm.xml` → `wl-drm-protocol.h`

Because AMS cannot run the host `wayland-scanner` binary at build time in the freestanding toolchain, the headers are **pre-generated and committed** (vendored snapshot). The script shows how to regenerate them on a Linux host.

### 6. Userspace: libffi minimal port

**Directory:** `source/src/lib/ffi/`  
**Files:** `ffi.h`, `ffi.c` (C), `ffi_x86_64.s` (ASM call glue)

Implements:
- `ffi_prep_cif` — prepare a call interface descriptor
- `ffi_call` — call a function pointer with typed args/ret via inline ASM
- Closure support (`ffi_closure_alloc`, `ffi_prep_closure_loc`) — needed by libwayland dispatcher

For a freestanding port only the `x86_64 SYSV` ABI is implemented; no `libffi_convenience.a` overhead.

### 7. Userspace: pixman minimal port

**Directory:** `source/src/lib/pixman/`  
**Files:** `pixman.h`, `pixman.c`

Implements:
- `pixman_image_create_bits` / `_create_solid_fill` / `_unref`
- `pixman_image_composite32` with ops: `CLEAR`, `SRC`, `OVER`, `DST`
- Format constants: `PIXMAN_a8r8g8b8`, `PIXMAN_x8r8g8b8`, `PIXMAN_a8b8g8r8`
- `pixman_region32_*` — rectangle region tracking

No SIMD; pure C scalar implementation targeting correctness over speed.

### 8. Userspace: libdrm stub

**Directory:** `source/src/lib/drm/`  
**Files:** `drm.h`, `drm_mode.h`, `drm.c`

Wraps the DRM ioctls over `/dev/dri/card0`:
- `drmOpen` / `drmClose`
- `drmModeGetResources` / `drmModeFreeResources`
- `drmModeGetConnector` / `drmModeFreeConnector`
- `drmModeGetCrtc` / `drmModeSetCrtc`
- `drmModeAddFB` / `drmModeRmFB`
- `drmModePageFlip`
- `drmIoctl` (raw)
- Dumb buffer: `drmModeCreateDumbBuffer`, `drmModeDestroyDumbBuffer`, `drmModeMapDumbBuffer`

### 9. Userspace: libinput stub

**Directory:** `source/src/lib/input/`  
**Files:** `libinput.h`, `libinput.c`

Reads `struct input_event` from `/dev/input/event0` (keyboard) and `/dev/input/event1` (pointer). Implements the `libinput_*` API surface that wlroots actually calls:
- `libinput_create_path_context` / `libinput_path_add_device`
- `libinput_get_fd` / `libinput_dispatch`
- `libinput_get_event` / `libinput_event_get_type`
- `libinput_event_keyboard_get_key` / `_get_key_state`
- `libinput_event_pointer_get_dx` / `_get_dy` / `_get_button` / `_get_button_state`

### 10. Kernel: mlibc-compatible syscall surface

**File:** `source/src/arch/x86_64/syscall.cpp` (extended)

Additional syscalls needed by mlibc and by wlroots' dependencies:

| Syscall | Number | Implementation |
|---|---|---|
| `set_tid_address` | 218 | Store pointer, return TID |
| `arch_prctl` | 158 | `ARCH_SET_FS` → write to FS.base MSR |
| `tgkill` | 234 | Return 0 (no signal delivery yet) |
| `futex` | 202 | Simple FUTEX_WAIT (spin) + FUTEX_WAKE |
| `clone` (thread) | 56 | Minimal thread clone (share address space) |
| `gettid` | 186 | Return task ID |
| `getuid` / `getgid` | 102/104 | Return 0 |
| `getpid` | 39 | Return task ID |
| `openat` | 257 | Relative open (AT_FDCWD supported) |
| `pipe2` | 293 | Already present via pipe; add flags |
| `timerfd_create` | 283 | Stub returning a valid dummy FD |
| `timerfd_settime` | 286 | Store interval, return 0 |
| `timerfd_gettime` | 287 | Return stored interval |
| `signalfd4` | 289 | Stub FD |
| `inotify_init1` | 294 | Stub FD |
| `sched_getaffinity` | 204 | Return single-CPU mask |
| `sched_yield` | 24 | Return 0 |
| `getrlimit` | 97 | Return large limits |
| `setrlimit` | 160 | Return 0 |
| `prctl` | 157 | Return 0 |
| `madvise` | 28 | Return 0 |
| `mremap` | 25 | Basic grow (alloc + copy) |

### 11. Userspace: wlroots AMS backend

**Directory:** `source/src/apps/wlroots-ams/`  
**Files:** `ams_backend.c`, `ams_drm_backend.c`, `ams_input_backend.c`, `ams_render.c`

Implements the wlroots backend interface using all the pieces above:

- **Output backend:** opens `/dev/dri/card0`, creates a dumb buffer matching the framebuffer resolution, uses `drmModeSetCrtc` / `drmModePageFlip` for scanout.
- **Renderer:** pixman software renderer operating on the dumb buffer pages.
- **Input backend:** opens `/dev/input/event0` + `event1`, wraps via libinput stub, feeds wlroots seat events.
- **Session:** no logind needed; single-seat hard-coded DRM auth.

This backend can be used with an upstream wlroots tree (or the AMS-internal minimal compositor) by defining `WLR_BACKEND=ams`.

---

## File creation summary

```
source/
  include/
    drm_virt.h                    ← DRM ioctl constants + dumb buf structs
    evdev.h                       ← struct input_event, EV_*, KEY_*, BTN_*
    wayland-generated/
      wayland-client-protocol.h   ← pre-generated from wayland.xml
      wayland-server-protocol.h
      xdg-shell-client-protocol.h
      xdg-shell-server-protocol.h
      linux-dmabuf-unstable-v1-protocol.h
  src/
    drivers/
      drm/
        drm_virt.cpp              ← virtual DRM device + ioctl dispatcher
      input/
        evdev.cpp                 ← evdev ring-buffer layer over PS/2 drivers
    lib/
      ffi/
        ffi.h
        ffi.c
        ffi_x86_64.s
      pixman/
        pixman.h
        pixman.c
      drm/
        drm.h
        drm_mode.h
        drm.c
      input/
        libinput.h
        libinput.c
      shm.c                       ← shm_open / shm_unlink wrappers
    apps/
      wlroots-ams/
        ams_backend.c
        ams_drm_backend.c
        ams_input_backend.c
        ams_render.c
  tools/
    wayland_scan.sh               ← regenerate protocol headers on host
```

---

## Build integration

`Makefile` gains:
- `LIB_FFI_OBJS`, `LIB_PIXMAN_OBJS`, `LIB_DRM_OBJS`, `LIB_INPUT_OBJS` — compiled with `USER_CFLAGS`
- `WLROOTS_AMS_OBJS` — compiled with `USER_CFLAGS -Iinclude/wayland-generated`
- `build/wlroots_ams.elf` — links all of the above plus `LIBC_OBJS`
- Kernel objects gain `build/drivers/drm/drm_virt.o`, `build/drivers/input/evdev.o`

Disk image (`disk_run.img`) gains:
- `/dev/dri/card0` (created at kernel boot via `drm_virt_init()`)
- `/dev/input/event0`, `/dev/input/event1` (created by `evdev_init()`)
- `/dev/shm/` directory (created at kernel boot)
- `/programs/wayland/wlroots_ams.elf`

---

## Implementation order (dependency graph)

```
1. evdev.cpp + evdev.h            (no deps beyond existing PS/2 drivers)
2. drm_virt.cpp + drm_virt.h      (depends on VMM for dumb-buf mmap)
3. sys_mmap fix (file-backed)     (depends on drm_virt for dumb-buf path)
4. shm_open / /dev/shm            (depends on memfd + sys_open path)
5. mlibc syscall stubs            (independent; broadens POSIX surface)
6. ffi port                       (independent; pure userspace)
7. pixman port                    (independent; pure userspace)
8. libdrm stub                    (depends on drm_virt in kernel)
9. libinput stub                  (depends on evdev in kernel)
10. wayland-scanner headers       (independent; vendored)
11. wlroots AMS backend           (depends on all above)
```
