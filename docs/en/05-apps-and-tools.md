# Applications and Tools

[PL](../05-aplikacje-i-narzedzia.md) | [EN](./05-apps-and-tools.md)

AMS-OS builds userspace tools and test applications such as:

- `tcc`, `gcc`, `bash`, `jobctl`, `apt`,
- `doom` and Wayland smoke/session components.

## Wayland/Wlroots graphics stack staging

The repository now treats the legacy in-tree compositor as removed and stages a
wlroots-oriented stack from source:

- `make graphics_stage` clones/stages Wayland + protocols, Mesa3D/libdrm (EGL + GBM profile),
  wlroots, libinput, pixman, cairo, mlibc and libffi.
- `make wayland_desktop_matrix` verifies expected AMS userspace artifacts, including
  `build/wlroots_compositor.elf`.
- `tools/wayland_stage.sh` runs `wayland-scanner` generation checks against upstream XML
  protocol files.

This keeps AMS aligned with modern compositor architecture (KMS/DRM + shared memory + event loop APIs)
while preserving the lightweight in-kernel test binaries.

Artifacts are staged into `disk_run.img` under structured paths like `/tools/system`, `/tools/compiler`, `/programs/doom`, `/programs/wayland`.
