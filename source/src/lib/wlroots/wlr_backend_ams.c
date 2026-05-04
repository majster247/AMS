/*
 * AMS wlroots backend - software-only shim.
 *
 * Provides the wlr_backend / wlr_renderer / wlr_compositor / wlr_seat /
 * wlr_xdg_shell symbols against AMS infrastructure:
 *   - rendering -> AMS pixman shim, blit via SYS_AMS_FB_BLIT,
 *   - input    -> AMS libinput shim,
 *   - protocol -> libwayland-server-ams.
 *
 * This is a starting point; subsequent PRs are expected to swap each
 * subsystem with a faithful port from external/wlroots-stage/wlroots.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wayland/wayland-server-core.h>
#include <pixman/pixman.h>
#include <wlroots/wlr_backend.h>
#include "ams_syscall.h"

struct wlr_backend {
    struct wl_display *display;
    int                started;
};
struct wlr_renderer {
    struct wlr_backend *backend;
    pixman_image_t     *frame;
    int                 width, height;
};
struct wlr_compositor { struct wl_display *display; int version; };
struct wlr_xdg_shell  { struct wl_display *display; int version; };
struct wlr_seat       { struct wl_display *display; char name[32]; };

struct wlr_backend *wlr_backend_autocreate(struct wl_display *display, void *session) {
    (void)session;
    struct wlr_backend *b = (struct wlr_backend*)calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->display = display;
    return b;
}

int wlr_backend_start(struct wlr_backend *b) {
    if (!b) return -1;
    b->started = 1;
    return 0;
}

void wlr_backend_destroy(struct wlr_backend *b) { free(b); }

struct wlr_renderer *wlr_renderer_autocreate(struct wlr_backend *b) {
    struct wlr_renderer *r = (struct wlr_renderer*)calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->backend = b;
    uint32_t w = 1280, h = 720;
    ams_syscall(452 /*SYS_AMS_GET_FB_INFO*/, (uint64_t)&w, (uint64_t)&h, 0, 0, 0);
    r->width = (int)w; r->height = (int)h;
    r->frame = pixman_image_create_bits(PIXMAN_a8r8g8b8, r->width, r->height, NULL, 0);
    return r;
}

int wlr_renderer_init_wl_display(struct wlr_renderer *r, struct wl_display *d) {
    (void)r; (void)d;
    return 0;
}

struct wlr_compositor *wlr_compositor_create(struct wl_display *d, int version, struct wlr_renderer *r) {
    (void)r;
    struct wlr_compositor *c = (struct wlr_compositor*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->display = d;
    c->version = version;
    return c;
}

struct wlr_xdg_shell *wlr_xdg_shell_create(struct wl_display *d, int version) {
    struct wlr_xdg_shell *s = (struct wlr_xdg_shell*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->display = d;
    s->version = version;
    return s;
}

struct wlr_seat *wlr_seat_create(struct wl_display *d, const char *name) {
    struct wlr_seat *s = (struct wlr_seat*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->display = d;
    if (name) {
        size_t i = 0;
        while (i + 1 < sizeof(s->name) && name[i]) { s->name[i] = name[i]; ++i; }
        s->name[i] = '\0';
    }
    return s;
}
