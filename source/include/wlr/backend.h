#ifndef _AMS_WLR_BACKEND_H
#define _AMS_WLR_BACKEND_H

#include "wayland/wayland-server.h"

struct wlr_backend;
struct wlr_renderer;

struct wlr_backend* wlr_backend_autocreate(struct wl_display* display);
void wlr_backend_destroy(struct wlr_backend* backend);
int wlr_backend_start(struct wlr_backend* backend);
struct wlr_renderer* wlr_backend_get_renderer(struct wlr_backend* backend);

#endif
