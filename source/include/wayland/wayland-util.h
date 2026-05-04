#ifndef WAYLAND_UTIL_H
#define WAYLAND_UTIL_H

/*
 * Minimal subset of wayland-util.h for freestanding AMS builds.
 * Full upstream header lives in external/wayland-stack/wayland (see tools/wayland_stage.sh).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if (__has_attribute(visibility) || (defined(__GNUC__) && __GNUC__ >= 4))
#define WL_PRIVATE __attribute__((visibility("hidden")))
#else
#define WL_PRIVATE
#endif

struct wl_message {
    const char* name;
    const char* signature;
    const struct wl_interface** types;
};

struct wl_interface {
    const char* name;
    int version;
    int method_count;
    const struct wl_message* methods;
    int event_count;
    const struct wl_message* events;
};

#endif
