#ifndef _AMS_WLR_ALLOCATOR_H
#define _AMS_WLR_ALLOCATOR_H

struct wlr_allocator;
struct wlr_backend;
struct wlr_renderer;

struct wlr_allocator* wlr_allocator_autocreate(struct wlr_backend* backend, struct wlr_renderer* renderer);
void wlr_allocator_destroy(struct wlr_allocator* alloc);

#endif
