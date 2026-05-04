#ifndef WAYLAND_SERVER_H
#define WAYLAND_SERVER_H

#include <stdint.h>
#include "wayland-server-core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Generated server headers expect these typedefs from full wayland-server-core.h */
typedef void (*wl_global_bind_func_t)(struct wl_client* client, void* data, uint32_t version, uint32_t id);

#ifdef __cplusplus
}
#endif

#include "wayland-server-protocol.h"

#endif
