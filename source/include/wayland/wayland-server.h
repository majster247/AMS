#ifndef _AMS_WAYLAND_SERVER_H
#define _AMS_WAYLAND_SERVER_H

#include <stdint.h>

struct wl_display;
struct wl_event_loop;
struct wl_client;
struct wl_resource;
struct wl_listener;
struct wl_signal;
struct wl_list;

struct wl_list {
    struct wl_list* prev;
    struct wl_list* next;
};

struct wl_signal {
    struct wl_list listener_list;
};

struct wl_listener {
    struct wl_list link;
    void (*notify)(struct wl_listener* listener, void* data);
};

struct wl_display* wl_display_create(void);
void wl_display_destroy(struct wl_display* display);
struct wl_event_loop* wl_display_get_event_loop(struct wl_display* display);
int wl_display_add_socket_auto(struct wl_display* display);
void wl_display_run(struct wl_display* display);
void wl_display_terminate(struct wl_display* display);

void wl_signal_init(struct wl_signal* signal);
void wl_signal_add(struct wl_signal* signal, struct wl_listener* listener);

#endif
