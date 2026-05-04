# Graphics stack migration roadmap (AMS)

This repository currently ships a tiny custom Wayland protocol handler and does
not expose a complete Linux userspace ABI required by production compositors.

The requested end-state is:

1. Kernel graphics stack with DRM/KMS primitives (and GEM/TTM memory model).
2. Userspace compatibility sufficient for mlibc + libffi + wlroots.
3. Mesa3D (EGL + GBM path) built from source.
4. wlroots compositor binary launched by `/ams-wl-compositor`.

## What is implemented now

- `source/tools/graphics_stack_stage.sh` stages the upstream source trees for:
  - wayland + wayland-protocols
  - libdrm
  - mesa
  - libffi
  - mlibc
  - weston (reference)
  - libinput
  - pixman
  - cairo
  - wlroots
- The compositor app in `src/apps/wayland/ams_wl_compositor.c` was replaced by
  a launcher for a staged wlroots compositor binary.
- POSIX-facing headers/wrappers were added for:
  - shm (`shm_open`, `shm_unlink`)
  - poll/ppoll
  - epoll
  - unix sockets + sendmsg/recvmsg ABI structs

## Remaining kernel work (required for true wlroots runtime)

1. **DRM subsystem**
   - `/dev/dri/card*` device model
   - ioctl dispatch table for KMS + dumb buffers + mode setting
2. **GEM/TTM memory management**
   - handle table, refcounting, VM mapping
   - synchronization + fencing semantics
3. **Input stack for libinput**
   - evdev-compatible `/dev/input/event*`
   - ioctl coverage for capabilities, keymaps, and absolute axes
4. **Shared memory + VM semantics**
   - proper `shm_open` namespace + unlink lifecycle
   - `mmap/munmap/mprotect` backing and page accounting
5. **Loader/runtime**
   - dynamic linking coverage (`dlopen/dlsym`) and ELF relocations required by
     wlroots + mesa shared objects.

Until those kernel components are done, source staging/build can proceed, but a
full runtime desktop stack inside AMS remains "in progress".
