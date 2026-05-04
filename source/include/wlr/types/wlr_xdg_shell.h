#ifndef _AMS_WLR_XDG_SHELL_H
#define _AMS_WLR_XDG_SHELL_H

#include "wayland/wayland-server.h"
#include "wlr/types/wlr_compositor.h"

struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;

enum wlr_xdg_surface_role {
    WLR_XDG_SURFACE_ROLE_NONE,
    WLR_XDG_SURFACE_ROLE_TOPLEVEL,
    WLR_XDG_SURFACE_ROLE_POPUP,
};

struct wlr_xdg_toplevel {
    struct wlr_xdg_surface* base;
    char* title;
    char* app_id;
    struct wl_signal request_move;
    struct wl_signal request_resize;
    struct wl_signal request_minimize;
    struct wl_signal request_fullscreen;
};

struct wlr_xdg_surface {
    struct wlr_surface* surface;
    enum wlr_xdg_surface_role role;
    struct wlr_xdg_toplevel* toplevel;
    struct wl_signal map;
    struct wl_signal unmap;
    struct wl_signal destroy;
    int mapped;
};

struct wlr_xdg_shell* wlr_xdg_shell_create(struct wl_display* display, uint32_t version);

void wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel* toplevel, uint32_t width, uint32_t height);
void wlr_xdg_toplevel_set_activated(struct wlr_xdg_toplevel* toplevel, int activated);
uint32_t wlr_xdg_surface_schedule_configure(struct wlr_xdg_surface* surface);

#endif
