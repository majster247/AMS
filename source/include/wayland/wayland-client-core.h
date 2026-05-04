#ifndef AMS_WAYLAND_CLIENT_CORE_H
#define AMS_WAYLAND_CLIENT_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_display;
struct wl_proxy;
struct wl_event_queue;

struct wl_message {
    const char *name;
    const char *signature;
    const struct wl_interface **types;
};
struct wl_interface {
    const char *name;
    int         version;
    int         method_count;
    const struct wl_message *methods;
    int         event_count;
    const struct wl_message *events;
};

struct wl_display *wl_display_connect(const char *name);
void               wl_display_disconnect(struct wl_display *display);
int                wl_display_dispatch(struct wl_display *display);
int                wl_display_roundtrip(struct wl_display *display);
int                wl_display_get_fd(struct wl_display *display);
int                wl_display_flush(struct wl_display *display);

struct wl_proxy   *wl_proxy_marshal_constructor(struct wl_proxy *proxy,
                                                uint32_t opcode,
                                                const struct wl_interface *iface, ...);
void               wl_proxy_destroy(struct wl_proxy *proxy);

#ifdef __cplusplus
}
#endif

#endif
