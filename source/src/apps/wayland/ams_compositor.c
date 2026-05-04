/**
 * @file ams_compositor.c
 * @brief AMS-OS compositor glue around wlroots.
 *
 * The previous AMS-OS compositor was a hand-rolled Wayland byte-pump
 * (>2000 lines of opcodes and state machines). It has been removed.
 *
 * This file is the new entry point. It links against:
 *   - libwayland-server   (from external/wayland-stack/wayland)
 *   - wayland-protocols   (xdg-shell etc., generated via wayland-scanner)
 *   - wlroots             (rendering, output, seat, xdg-shell adapter)
 *   - pixman              (software rendering of damage)
 *   - cairo               (decorations / debug overlays)
 *   - libxkbcommon        (key event translation)
 *   - libinput            (input device discovery via /dev/input/event*)
 *   - libports            (AMS-OS shim for shm_open / poll / DRM helpers)
 *
 * Build flow:
 *   1. tools/wayland_stage.sh fetches all upstream sources.
 *   2. tools/wayland_build.sh meson-builds them into the AMS sysroot.
 *   3. The Makefile target `build/ams-compositor.elf` compiles this
 *      translation unit against the sysroot and produces a static
 *      AMS user-space ELF.
 *
 * Because the Wayland headers expect glibc-shaped declarations, this
 * file uses the upstream wlroots API; mlibc + libports satisfy the
 * underlying syscalls.
 */

#define WLR_USE_UNSTABLE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libports/libports.h"
#include "ams_syscall.h"

#if __has_include(<wayland-server-core.h>)
#  include <wayland-server-core.h>
#  include <wlr/backend.h>
#  include <wlr/render/allocator.h>
#  include <wlr/render/wlr_renderer.h>
#  include <wlr/types/wlr_compositor.h>
#  include <wlr/types/wlr_output.h>
#  include <wlr/types/wlr_output_layout.h>
#  include <wlr/types/wlr_scene.h>
#  include <wlr/types/wlr_seat.h>
#  include <wlr/types/wlr_xdg_shell.h>
#  include <wlr/util/log.h>
#  define AMS_HAVE_WLROOTS 1
#else
#  define AMS_HAVE_WLROOTS 0
#endif

static void puts_serial(const char* s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

#if AMS_HAVE_WLROOTS
struct ams_server {
    struct wl_display*               display;
    struct wlr_backend*              backend;
    struct wlr_renderer*             renderer;
    struct wlr_allocator*            allocator;
    struct wlr_scene*                scene;
    struct wlr_output_layout*        output_layout;
    struct wlr_compositor*           compositor;
    struct wlr_xdg_shell*            xdg_shell;
    struct wlr_seat*                 seat;
};

static void on_new_output(struct wl_listener* listener, void* data) {
    (void)listener;
    struct wlr_output* output = data;
    wlr_output_init_render(output, NULL, NULL);
    if (!wl_list_empty(&output->modes)) {
        struct wlr_output_mode* mode = wl_container_of(output->modes.prev, mode, link);
        wlr_output_set_mode(output, mode);
    }
    wlr_output_enable(output, true);
    wlr_output_commit(output);
    puts_serial("[ams-compositor] new output online");
}

static void on_new_xdg_toplevel(struct wl_listener* listener, void* data) {
    (void)listener; (void)data;
    puts_serial("[ams-compositor] new xdg toplevel surface");
}
#endif /* AMS_HAVE_WLROOTS */

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    puts_serial("ams-compositor: starting");

#if AMS_HAVE_WLROOTS
    wlr_log_init(WLR_DEBUG, NULL);
    struct ams_server srv = {0};
    srv.display = wl_display_create();
    if (!srv.display) {
        puts_serial("ams-compositor: wl_display_create failed");
        return 1;
    }
    srv.backend = wlr_backend_autocreate(srv.display, NULL);
    if (!srv.backend) {
        puts_serial("ams-compositor: wlr_backend_autocreate failed");
        return 2;
    }
    srv.renderer  = wlr_renderer_autocreate(srv.backend);
    srv.allocator = wlr_allocator_autocreate(srv.backend, srv.renderer);
    wlr_renderer_init_wl_display(srv.renderer, srv.display);

    srv.compositor    = wlr_compositor_create(srv.display, 5, srv.renderer);
    srv.output_layout = wlr_output_layout_create(srv.display);
    srv.scene         = wlr_scene_create();
    wlr_scene_attach_output_layout(srv.scene, srv.output_layout);

    srv.xdg_shell = wlr_xdg_shell_create(srv.display, 3);
    srv.seat      = wlr_seat_create(srv.display, "seat0");

    static struct wl_listener new_output_listener   = { .notify = on_new_output };
    static struct wl_listener new_toplevel_listener = { .notify = on_new_xdg_toplevel };
    wl_signal_add(&srv.backend->events.new_output,         &new_output_listener);
    wl_signal_add(&srv.xdg_shell->events.new_toplevel,     &new_toplevel_listener);

    const char* socket = wl_display_add_socket_auto(srv.display);
    if (!socket) {
        puts_serial("ams-compositor: wl_display_add_socket_auto failed");
        return 3;
    }
    setenv("WAYLAND_DISPLAY", socket, 1);
    puts_serial("ams-compositor: socket bound");

    if (!wlr_backend_start(srv.backend)) {
        puts_serial("ams-compositor: wlr_backend_start failed");
        return 4;
    }
    wl_display_run(srv.display);
    wl_display_destroy(srv.display);
    return 0;
#else
    puts_serial("ams-compositor: wlroots headers not available; "
                "run 'make wayland_build' to populate the sysroot.");
    return 0;
#endif
}
