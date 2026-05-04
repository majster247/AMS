#ifndef _AMS_WLR_SEAT_H
#define _AMS_WLR_SEAT_H

#include "wayland/wayland-server.h"
#include "wlr/types/wlr_compositor.h"

struct wlr_seat;
struct wlr_seat_pointer_state;
struct wlr_seat_keyboard_state;

struct wlr_seat {
    struct wl_signal request_set_cursor;
    struct wl_signal request_set_selection;
    struct wlr_seat_pointer_state* pointer_state;
    struct wlr_seat_keyboard_state* keyboard_state;
};

struct wlr_seat* wlr_seat_create(struct wl_display* display, const char* name);
void wlr_seat_destroy(struct wlr_seat* seat);

void wlr_seat_set_capabilities(struct wlr_seat* seat, uint32_t capabilities);
void wlr_seat_pointer_notify_enter(struct wlr_seat* seat, struct wlr_surface* surface, double sx, double sy);
void wlr_seat_pointer_notify_motion(struct wlr_seat* seat, uint32_t time_msec, double sx, double sy);
void wlr_seat_pointer_notify_button(struct wlr_seat* seat, uint32_t time_msec, uint32_t button, uint32_t state);
void wlr_seat_pointer_notify_frame(struct wlr_seat* seat);
void wlr_seat_keyboard_notify_enter(struct wlr_seat* seat, struct wlr_surface* surface, uint32_t* keycodes, size_t num_keycodes, void* modifiers);
void wlr_seat_keyboard_notify_key(struct wlr_seat* seat, uint32_t time_msec, uint32_t key, uint32_t state);

#define WL_SEAT_CAPABILITY_POINTER  1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_SEAT_CAPABILITY_TOUCH    4

#endif
