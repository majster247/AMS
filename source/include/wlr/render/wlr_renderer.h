#ifndef _AMS_WLR_RENDERER_H
#define _AMS_WLR_RENDERER_H

#include <stdint.h>
#include "wayland/wayland-server.h"

struct wlr_renderer;
struct wlr_output;
struct wlr_texture;

struct wlr_renderer* wlr_renderer_autocreate(struct wlr_backend* backend);
void wlr_renderer_init_wl_display(struct wlr_renderer* r, struct wl_display* display);
void wlr_renderer_begin(struct wlr_renderer* r, struct wlr_output* output);
void wlr_renderer_end(struct wlr_renderer* r);
void wlr_renderer_clear(struct wlr_renderer* r, const float color[4]);

struct wlr_texture* wlr_texture_from_pixels(struct wlr_renderer* r,
    uint32_t fmt, uint32_t stride, uint32_t width, uint32_t height, const void* data);
void wlr_render_texture(struct wlr_renderer* r, struct wlr_texture* texture,
    const float matrix[9], int x, int y, float alpha);

#endif
