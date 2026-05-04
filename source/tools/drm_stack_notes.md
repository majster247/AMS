# AMS DRM/KMS migration notes

This repository currently boots with a software backbuffer and custom framebuffer syscalls.
The userspace migration path is staged to move compositing to wlroots with a DRM backend:

- Renderer path: Mesa EGL + GBM
- Display path: KMS/DRM (`libdrm`)
- Input path: `libinput`
- Buffer transport: Unix domain sockets + shared memory
- Protocol codegen: `wayland-scanner`

Kernel-side gaps still to be implemented for a complete native DRM stack:

1. DRM device model and `/dev/dri/card*` nodes.
2. KMS ioctls for connector/CRTC/plane mode setting.
3. GEM object lifetime and mmap exposure for userspace buffers.
4. TTM memory manager integration for eviction/placement policy.
5. Explicit sync primitives for compositing and scanout handoff.

These notes are used by the stage scripts as an implementation contract.
