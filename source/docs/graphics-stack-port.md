# Graphics stack port status

This repository previously shipped a custom AMS-specific compositor binary.
That implementation has been removed from the build and boot path.

## Foundation implemented in-tree

- AF_UNIX sockets with bind/listen/accept/connect
- sendmsg/recvmsg with SCM_RIGHTS
- poll/ppoll
- epoll_create1/epoll_ctl/epoll_wait
- eventfd2
- memfd_create
- file-backed mmap with shared physical page backing for VFS nodes and memfd
- unlink
- socketpair
- public headers for socket, unix sockets, poll, epoll, eventfd, memfd, shm

## New staging scripts

- `tools/graphics_stack_stage.sh`
- `tools/wayland_stage.sh`
- `tools/mesa_stage.sh`
- `tools/wlroots_port.sh`

They fetch and optionally host-build:

- Wayland
- wayland-protocols
- libffi
- pixman
- cairo
- libinput
- libdrm
- Mesa (software-first profile with EGL/GBM enabled when dependencies exist)
- wlroots

## Current limitations

The kernel still exposes a framebuffer-oriented display path and does not yet
provide a full Linux DRM subsystem. In particular:

- KMS/DRM device nodes are not implemented
- GEM/TTM are not feature-complete Linux ports
- GBM/EGL/Mesa/wlroots runtime integration is therefore staged/prepared, not
  yet runnable as a native desktop stack on AMS-OS

## Planned direction

The next kernel-side milestones should be:

1. introduce explicit DRM device abstractions
2. add KMS objects (CRTC/plane/connector/mode)
3. replace framebuffer blit-only presentation with a DRM presentation path
4. implement GEM-style buffer allocation/export
5. add enough uAPI compatibility to move wlroots and Mesa from staged to live
