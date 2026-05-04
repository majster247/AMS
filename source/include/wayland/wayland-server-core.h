#ifndef WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "wayland-util.h"
#include "wayland-version.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wl_client;
struct wl_resource;

typedef void (*wl_resource_destroy_func_t)(struct wl_resource* resource);

struct wl_resource {
    struct wl_resource* next;
    struct wl_client* client;
    void* data;
    uint32_t id;
    const struct wl_interface* interface;
};

void wl_resource_post_event(struct wl_resource* resource, uint32_t opcode, ...);

#ifdef __cplusplus
}
#endif

#endif
