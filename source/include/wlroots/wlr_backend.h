/*
 * AMS wlroots ABI shim - just enough surface for tinywl-style apps.
 * The real implementations live in src/lib/wlroots/.
 */

#ifndef AMS_WLR_BACKEND_H
#define AMS_WLR_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <wayland/wayland-server-core.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wlr_backend;
struct wlr_output;
struct wlr_renderer;
struct wlr_compositor;
struct wlr_seat;
struct wlr_xdg_shell;

struct wlr_backend *wlr_backend_autocreate(struct wl_display *display, void *session);
int                  wlr_backend_start(struct wlr_backend *backend);
void                 wlr_backend_destroy(struct wlr_backend *backend);

struct wlr_renderer *wlr_renderer_autocreate(struct wlr_backend *backend);
int                  wlr_renderer_init_wl_display(struct wlr_renderer *r, struct wl_display *display);

struct wlr_compositor *wlr_compositor_create(struct wl_display *display, int version, struct wlr_renderer *r);
struct wlr_xdg_shell  *wlr_xdg_shell_create(struct wl_display *display, int version);
struct wlr_seat       *wlr_seat_create(struct wl_display *display, const char *name);

#ifdef __cplusplus
}
#endif

#endif
