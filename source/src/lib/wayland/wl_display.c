/*
 * AMS libwayland-server-ams: display + client + resource scaffolding.
 *
 * The display owns: the AF_UNIX listener, the event loop, a list of
 * globals, and a list of clients. Wire-format encoding/decoding is
 * shared with src/apps/wayland/ams_wl_compositor.c (host-LE, header
 * `oid|size<<16|opcode`).
 *
 * Functional scope:
 *   - bind/listen on /run/user/0/wayland-0 or custom name,
 *   - wl_display_run() = busy loop dispatching the event loop,
 *   - wl_resource_create / wl_resource_post_event, no varargs marshaling
 *     yet (use wl_resource_post_event_array with packed buffer).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ams_syscall.h"
#include "linux_syscalls.h"
#include <wayland/wayland-server-core.h>

#define MAX_CLIENTS    8
#define MAX_RESOURCES  512
#define MAX_GLOBALS    32

struct wl_event_loop *wl_event_loop_create(void);
void                  wl_event_loop_destroy(struct wl_event_loop*);

struct wl_resource {
    uint32_t                    id;
    int                         in_use;
    struct wl_client           *client;
    const struct wl_interface  *iface;
    int                         version;
    const void                 *impl;
    void                       *data;
    void                      (*destroy_cb)(struct wl_resource*);
};

struct wl_client {
    int                  fd;
    int                  in_use;
    struct wl_display   *display;
    struct wl_resource   resources[MAX_RESOURCES];
    uint32_t             next_serial;
};

struct wl_global {
    int                          in_use;
    const struct wl_interface   *iface;
    int                          version;
    void                        *data;
    void                       (*bind)(struct wl_client*, void*, uint32_t, uint32_t);
};

struct wl_display {
    struct wl_event_loop *loop;
    int                   listen_fd;
    int                   running;
    uint32_t              serial;
    struct wl_client      clients[MAX_CLIENTS];
    struct wl_global      globals[MAX_GLOBALS];
};

void wl_list_init(struct wl_list *l)        { if (l) { l->prev = l->next = l; } }
void wl_list_insert(struct wl_list *l, struct wl_list *e) {
    if (!l || !e) return;
    e->prev = l; e->next = l->next;
    l->next->prev = e; l->next = e;
}
void wl_list_remove(struct wl_list *e) {
    if (!e) return;
    e->prev->next = e->next; e->next->prev = e->prev;
    e->prev = e->next = NULL;
}
int  wl_list_empty(const struct wl_list *l) { return l ? l->next == l : 1; }

struct wl_display *wl_display_create(void) {
    struct wl_display *d = (struct wl_display*)calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->loop = wl_event_loop_create();
    d->listen_fd = -1;
    d->running = 1;
    return d;
}

void wl_display_destroy(struct wl_display *d) {
    if (!d) return;
    if (d->listen_fd >= 0) ams_syscall(SYS_CLOSE, (uint64_t)d->listen_fd, 0, 0, 0, 0);
    wl_event_loop_destroy(d->loop);
    free(d);
}

struct wl_event_loop *wl_display_get_event_loop(struct wl_display *d) {
    return d ? d->loop : NULL;
}

uint32_t wl_display_next_serial(struct wl_display *d) { return d ? ++d->serial : 0; }
uint32_t wl_display_get_serial(struct wl_display *d)  { return d ? d->serial    : 0; }
void     wl_display_terminate(struct wl_display *d)   { if (d) d->running = 0; }

struct linux_sockaddr_un { uint16_t sun_family; char sun_path[108]; };

int wl_display_add_socket(struct wl_display *d, const char *name) {
    if (!d) return -1;
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = 1; /* AF_UNIX */
    const char *path = name ? name : "/run/user/0/wayland-0";
    for (int i = 0; path[i] && i < 107; ++i) addr.sun_path[i] = path[i];
    int fd = (int)ams_syscall(SYS_SOCKET, 1, 1, 0, 0, 0);
    if (fd < 0) return -1;
    if ((int)ams_syscall(SYS_BIND, fd, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
        return -1;
    }
    ams_syscall(SYS_LISTEN, fd, 8, 0, 0, 0);
    d->listen_fd = fd;
    return 0;
}

const char *wl_display_add_socket_auto(struct wl_display *d) {
    static const char *name = "/run/user/0/wayland-0";
    return wl_display_add_socket(d, name) == 0 ? name : NULL;
}

void wl_display_run(struct wl_display *d) {
    if (!d) return;
    while (d->running) {
        wl_event_loop_dispatch(d->loop, 50);
    }
}

void wl_display_flush_clients(struct wl_display *d) { (void)d; }

struct wl_global *wl_global_create(struct wl_display *d,
                                   const struct wl_interface *iface,
                                   int version, void *data,
                                   void (*bind)(struct wl_client*, void*, uint32_t, uint32_t)) {
    if (!d) return NULL;
    for (int i = 0; i < MAX_GLOBALS; ++i) {
        if (!d->globals[i].in_use) {
            d->globals[i].in_use = 1;
            d->globals[i].iface = iface;
            d->globals[i].version = version;
            d->globals[i].data = data;
            d->globals[i].bind = bind;
            return &d->globals[i];
        }
    }
    return NULL;
}

void wl_global_destroy(struct wl_global *g) { if (g) g->in_use = 0; }

struct wl_client *wl_client_create(struct wl_display *d, int fd) {
    if (!d) return NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!d->clients[i].in_use) {
            d->clients[i].in_use = 1;
            d->clients[i].fd = fd;
            d->clients[i].display = d;
            return &d->clients[i];
        }
    }
    return NULL;
}

void wl_client_destroy(struct wl_client *c) {
    if (!c) return;
    if (c->fd >= 0) ams_syscall(SYS_CLOSE, (uint64_t)c->fd, 0, 0, 0, 0);
    c->in_use = 0;
}

struct wl_resource *wl_resource_create(struct wl_client *c,
                                       const struct wl_interface *iface,
                                       int version, uint32_t id) {
    if (!c) return NULL;
    for (int i = 0; i < MAX_RESOURCES; ++i) {
        if (!c->resources[i].in_use) {
            c->resources[i].in_use = 1;
            c->resources[i].id = id;
            c->resources[i].iface = iface;
            c->resources[i].version = version;
            c->resources[i].client = c;
            return &c->resources[i];
        }
    }
    return NULL;
}

void wl_resource_destroy(struct wl_resource *r) {
    if (!r) return;
    if (r->destroy_cb) r->destroy_cb(r);
    r->in_use = 0;
}

uint32_t           wl_resource_get_id(struct wl_resource *r)     { return r ? r->id : 0u; }
struct wl_client  *wl_resource_get_client(struct wl_resource *r) { return r ? r->client : NULL; }

void wl_resource_set_implementation(struct wl_resource *r, const void *impl,
                                    void *data, void (*destroy)(struct wl_resource*)) {
    if (!r) return;
    r->impl = impl;
    r->data = data;
    r->destroy_cb = destroy;
}

static void wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

void wl_resource_post_event_array(struct wl_resource *r, uint32_t opcode, void *args) {
    if (!r || !r->client) return;
    /* args = packed wire payload (size + bytes), produced by codegen. */
    uint8_t *raw = (uint8_t*)args;
    if (!raw) return;
    uint32_t payload_len = ((uint32_t*)raw)[0];
    uint8_t  pkt[1024];
    if (payload_len > sizeof(pkt) - 8) return;
    wr_u32(pkt, r->id);
    wr_u32(pkt + 4, ((payload_len + 8) << 16) | (opcode & 0xFFFFu));
    memcpy(pkt + 8, raw + 4, payload_len);
    /* Use SENDMSG so it stays consistent with our compositor protocol. */
    struct linux_iovec { void *b; uint64_t l; } iov = { pkt, payload_len + 8 };
    struct linux_msghdr {
        void *n; uint32_t nl; uint32_t pad0; void *iov; uint64_t iovl;
        void *c; uint64_t cl; uint32_t f; uint32_t pad1;
    } msg = {0};
    msg.iov = &iov;
    msg.iovl = 1;
    ams_syscall(SYS_SENDMSG, (uint64_t)r->client->fd, (uint64_t)&msg, 0, 0, 0);
}

void wl_resource_post_event(struct wl_resource *r, uint32_t opcode, ...) {
    /* Variadic marshaling will be generated per-protocol by the scanner.
     * For now, callers should use wl_resource_post_event_array with a
     * pre-packed payload buffer. */
    (void)r; (void)opcode;
}
