#ifndef _AMS_WAYLAND_CLIENT_H
#define _AMS_WAYLAND_CLIENT_H

#include <stdint.h>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_shm;
struct wl_shm_pool;
struct wl_buffer;
struct wl_callback;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_output;

struct wl_display* wl_display_connect(const char* name);
void wl_display_disconnect(struct wl_display* display);
int wl_display_dispatch(struct wl_display* display);
int wl_display_roundtrip(struct wl_display* display);
int wl_display_get_fd(struct wl_display* display);
int wl_display_flush(struct wl_display* display);

#endif
