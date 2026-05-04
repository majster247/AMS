#include "wayland-server-core.h"

#include <stdarg.h>

/* Stub until libwayland-server is ported; keeps generated headers linkable. */
void wl_resource_post_event(struct wl_resource* resource, uint32_t opcode, ...)
{
    (void)resource;
    (void)opcode;
}
