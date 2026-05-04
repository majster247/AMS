#ifndef _AMS_WLR_CURSOR_H
#define _AMS_WLR_CURSOR_H

#include "wayland/wayland-server.h"

struct wlr_cursor;
struct wlr_output_layout;
struct wlr_input_device;
struct wlr_xcursor_manager;

struct wlr_cursor {
    double x, y;
    struct wl_signal motion;
    struct wl_signal motion_absolute;
    struct wl_signal button;
    struct wl_signal axis;
    struct wl_signal frame;
};

struct wlr_cursor* wlr_cursor_create(void);
void wlr_cursor_destroy(struct wlr_cursor* cursor);
void wlr_cursor_attach_output_layout(struct wlr_cursor* cursor, struct wlr_output_layout* layout);
void wlr_cursor_attach_input_device(struct wlr_cursor* cursor, struct wlr_input_device* dev);
void wlr_cursor_move(struct wlr_cursor* cursor, struct wlr_input_device* dev, double delta_x, double delta_y);
void wlr_cursor_warp_absolute(struct wlr_cursor* cursor, struct wlr_input_device* dev, double x, double y);
void wlr_cursor_set_xcursor(struct wlr_cursor* cursor, struct wlr_xcursor_manager* manager, const char* name);

struct wlr_xcursor_manager* wlr_xcursor_manager_create(const char* name, uint32_t size);
void wlr_xcursor_manager_destroy(struct wlr_xcursor_manager* manager);
void wlr_xcursor_manager_load(struct wlr_xcursor_manager* manager, float scale);

#endif
