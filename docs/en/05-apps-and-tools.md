# Applications and Tools

[PL](../05-aplikacje-i-narzedzia.md) | [EN](./05-apps-and-tools.md)

AMS-OS builds userspace tools and test applications such as:

- `tcc`, `gcc`, `bash`, `jobctl`, `apt`,
- `doom` and Wayland smoke/session components.

Artifacts are staged into `disk_run.img` under structured paths like `/tools/system`, `/tools/compiler`, `/programs/doom`, `/programs/wayland`.

The old in-tree toy Wayland compositor has been removed as the target display
server. Host-side staging scripts now fetch Wayland, wayland-protocols,
wlroots, libinput, pixman, cairo, libffi, libdrm, and Mesa so the graphics stack
can converge on a wlroots compositor with EGL/GBM instead of the legacy
framebuffer-only protocol shim. Run `make wayland_port_preflight` to inspect the
remaining kernel/libc gaps before attempting a full wlroots/Mesa port.
