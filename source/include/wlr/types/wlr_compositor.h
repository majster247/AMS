#ifndef _AMS_WLR_COMPOSITOR_H
#define _AMS_WLR_COMPOSITOR_H

#include "wayland/wayland-server.h"

struct wlr_compositor;
struct wlr_subcompositor;
struct wlr_renderer;
struct wlr_surface;
struct wlr_texture;

struct wlr_surface {
    struct wlr_texture* texture;
    int32_t current_width, current_height;
    struct wl_signal commit;
    struct wl_signal destroy;
    void* data;
};

struct wlr_compositor* wlr_compositor_create(struct wl_display* display, struct wlr_renderer* renderer);
struct wlr_subcompositor* wlr_subcompositor_create(struct wl_display* display);

#endif
