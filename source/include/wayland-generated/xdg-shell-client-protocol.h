/* Pre-generated xdg-shell client protocol header (vendored).
 * Regenerate with: bash source/tools/wayland_scan.sh
 * From xdg-shell.xml (stable, protocol version 6)
 */
#pragma once
#include "wayland-client-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_popup_interface;

/* ---- xdg_wm_base ---- */
struct xdg_wm_base;
struct xdg_surface;
struct xdg_positioner;
struct xdg_toplevel;
struct xdg_popup;

typedef struct xdg_wm_base_listener {
    void (*ping)(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial);
} xdg_wm_base_listener;

int  xdg_wm_base_add_listener(struct xdg_wm_base* xdg_wm_base, const xdg_wm_base_listener* l, void* data);
void xdg_wm_base_destroy(struct xdg_wm_base* xdg_wm_base);
void xdg_wm_base_pong(struct xdg_wm_base* xdg_wm_base, uint32_t serial);
struct xdg_positioner* xdg_wm_base_create_positioner(struct xdg_wm_base* xdg_wm_base);
struct xdg_surface*    xdg_wm_base_get_xdg_surface(struct xdg_wm_base* xdg_wm_base, struct wl_surface* surface);

/* ---- xdg_positioner ---- */
#define XDG_POSITIONER_GRAVITY_TOP_RIGHT 5
#define XDG_POSITIONER_ANCHOR_TOP_RIGHT  5
#define XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X 4
void xdg_positioner_destroy(struct xdg_positioner* xdg_positioner);
void xdg_positioner_set_size(struct xdg_positioner*, int32_t width, int32_t height);
void xdg_positioner_set_anchor_rect(struct xdg_positioner*, int32_t x, int32_t y, int32_t width, int32_t height);
void xdg_positioner_set_anchor(struct xdg_positioner*, uint32_t anchor);
void xdg_positioner_set_gravity(struct xdg_positioner*, uint32_t gravity);
void xdg_positioner_set_constraint_adjustment(struct xdg_positioner*, uint32_t constraint_adjustment);
void xdg_positioner_set_offset(struct xdg_positioner*, int32_t x, int32_t y);

/* ---- xdg_surface ---- */
typedef struct xdg_surface_listener {
    void (*configure)(void* data, struct xdg_surface* xdg_surface, uint32_t serial);
} xdg_surface_listener;

int  xdg_surface_add_listener(struct xdg_surface* s, const xdg_surface_listener* l, void* data);
void xdg_surface_destroy(struct xdg_surface* xdg_surface);
void xdg_surface_ack_configure(struct xdg_surface* xdg_surface, uint32_t serial);
void xdg_surface_set_window_geometry(struct xdg_surface*, int32_t x, int32_t y, int32_t width, int32_t height);
struct xdg_toplevel* xdg_surface_get_toplevel(struct xdg_surface* xdg_surface);
struct xdg_popup*    xdg_surface_get_popup(struct xdg_surface* xdg_surface, struct xdg_surface* parent,
                                            struct xdg_positioner* positioner);

/* ---- xdg_toplevel ---- */
#define XDG_TOPLEVEL_STATE_MAXIMIZED  1
#define XDG_TOPLEVEL_STATE_FULLSCREEN 2
#define XDG_TOPLEVEL_STATE_RESIZING   3
#define XDG_TOPLEVEL_STATE_ACTIVATED  4
#define XDG_TOPLEVEL_STATE_TILED_LEFT 5

typedef struct xdg_toplevel_listener {
    void (*configure)(void* data, struct xdg_toplevel* toplevel,
                      int32_t width, int32_t height, struct wl_array* states);
    void (*close)(void* data, struct xdg_toplevel* toplevel);
} xdg_toplevel_listener;

int  xdg_toplevel_add_listener(struct xdg_toplevel* t, const xdg_toplevel_listener* l, void* data);
void xdg_toplevel_destroy(struct xdg_toplevel* xdg_toplevel);
void xdg_toplevel_set_title(struct xdg_toplevel* xdg_toplevel, const char* title);
void xdg_toplevel_set_app_id(struct xdg_toplevel* xdg_toplevel, const char* app_id);
void xdg_toplevel_set_maximized(struct xdg_toplevel* xdg_toplevel);
void xdg_toplevel_unset_maximized(struct xdg_toplevel* xdg_toplevel);
void xdg_toplevel_set_fullscreen(struct xdg_toplevel* xdg_toplevel, struct wl_output* output);
void xdg_toplevel_unset_fullscreen(struct xdg_toplevel* xdg_toplevel);
void xdg_toplevel_set_minimized(struct xdg_toplevel* xdg_toplevel);
void xdg_toplevel_move(struct xdg_toplevel* xdg_toplevel, struct wl_seat* seat, uint32_t serial);
void xdg_toplevel_resize(struct xdg_toplevel* xdg_toplevel, struct wl_seat* seat, uint32_t serial, uint32_t edges);
void xdg_toplevel_set_min_size(struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height);
void xdg_toplevel_set_max_size(struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height);
void xdg_toplevel_set_parent(struct xdg_toplevel* xdg_toplevel, struct xdg_toplevel* parent);

/* ---- xdg_popup ---- */
typedef struct xdg_popup_listener {
    void (*configure)(void* data, struct xdg_popup* popup,
                      int32_t x, int32_t y, int32_t width, int32_t height);
    void (*popup_done)(void* data, struct xdg_popup* popup);
} xdg_popup_listener;

int  xdg_popup_add_listener(struct xdg_popup* p, const xdg_popup_listener* l, void* data);
void xdg_popup_destroy(struct xdg_popup* xdg_popup);
void xdg_popup_grab(struct xdg_popup* xdg_popup, struct wl_seat* seat, uint32_t serial);

#ifdef __cplusplus
}
#endif
