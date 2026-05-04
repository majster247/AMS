# AMS Linux-stack shims and ports

Each subdirectory is a small, statically-linked library that gives AMS
ABI parity with a Linux desktop component. The high-level plan and
roadmap live in `../../docs/19-plan-pelnego-stosu-graficznego.md`.

| Directory       | Provides                                  | Build target                         |
|-----------------|-------------------------------------------|--------------------------------------|
| `pixman/`       | `<pixman/pixman.h>` subset                | `build/lib/libpixman-ams.a`          |
| `cairo/`        | `<cairo/cairo.h>` subset on pixman        | `build/lib/libcairo-ams.a`           |
| `libinput/`     | `<libinput/libinput.h>` subset            | `build/lib/libinput-ams.a`           |
| `libffi/`       | `<ffi/ffi.h>` subset (x86_64 SysV)        | `build/lib/libffi-ams.a`             |
| `wayland/`      | libwayland-server-ams + soft-EGL          | `build/lib/libwayland-server-ams.a`, `build/lib/libegl-soft-ams.a` |
| `wlroots/`      | wlr_backend / wlr_renderer / wlr_seat etc.| `build/lib/libwlroots-ams.a`         |
| `posix/`        | `shm_open` + `mlibc` sysdeps shim         | `build/lib/libposix-ams.a`           |

The kernel-side DRM/KMS/GEM skeleton lives in
`src/drivers/drm/{drm_core.cpp,kms/drm_kms.cpp,gem/drm_gem.cpp}` and is
linked into `kernel.elf` automatically.

The host-built `wayland-scanner` is in `tools/scanner/wayland-scanner.c`
and produces `build/wl_proto/<name>-{client,server}-protocol.h` plus the
matching `<name>-protocol.c` from any Wayland XML.

To build everything in this PR's scope:

```
make linux_stack
```

Run individual smoke tests on AMS via the disk image (see `Makefile`
`$(DISK_IMAGE)` rule) or in the on-system shell.
