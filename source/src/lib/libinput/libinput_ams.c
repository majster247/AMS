/*
 * AMS libinput shim.
 *
 * Polls AMS pseudo-devices via SYS_AMS_GET_MOUSE_EVENT / SYS_AMS_GET_KEY
 * and queues libinput_event records for the consumer (wlroots backend).
 *
 * The shim does NOT use real evdev/udev; it pretends two devices are
 * always present:
 *   /dev/ams/keyboard0
 *   /dev/ams/mouse0
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <libinput/libinput.h>
#include "ams_syscall.h"

#define LI_QUEUE_CAP 256

struct libinput_event {
    enum libinput_event_type type;
    int                       active;
    union {
        struct {
            double   dx, dy;
            uint32_t button;
            uint32_t state;
        } pointer;
        struct {
            uint32_t key;
            uint32_t state;
        } keyboard;
    } u;
};

struct libinput {
    const struct libinput_interface *iface;
    void   *user_data;
    int     fd;
    libinput_event ring[LI_QUEUE_CAP];
    int     head, tail;
    uint32_t last_x, last_y;
    uint8_t  last_buttons;
};

struct libinput_device {
    libinput *ctx;
    char      path[64];
};

static int li_push(libinput *li, libinput_event ev) {
    int n = (li->tail + 1) % LI_QUEUE_CAP;
    if (n == li->head) return -1;
    ev.active = 1;
    li->ring[li->tail] = ev;
    li->tail = n;
    return 0;
}

static libinput_event *li_pop(libinput *li) {
    if (li->head == li->tail) return NULL;
    libinput_event *ev = &li->ring[li->head];
    li->head = (li->head + 1) % LI_QUEUE_CAP;
    return ev;
}

libinput *libinput_path_create_context(const struct libinput_interface *iface, void *user_data) {
    libinput *li = (libinput*)calloc(1, sizeof(*li));
    if (!li) return NULL;
    li->iface = iface;
    li->user_data = user_data;
    li->fd = -1;
    return li;
}

void libinput_unref(libinput *li) { free(li); }

int libinput_get_fd(libinput *li) { return li ? li->fd : -1; }

int libinput_dispatch(libinput *li) {
    if (!li) return -1;
    int produced = 0;

    uint64_t mev = ams_syscall(SYS_AMS_GET_MOUSE_EVENT, 0, 0, 0, 0, 0);
    if (mev) {
        uint32_t x = (uint32_t)(mev & 0xFFFFu);
        uint32_t y = (uint32_t)((mev >> 16) & 0xFFFFu);
        uint8_t  b = (uint8_t)((mev >> 32) & 0xFFu);
        if (x != li->last_x || y != li->last_y) {
            libinput_event ev = {0};
            ev.type = LIBINPUT_EVENT_POINTER_MOTION;
            ev.u.pointer.dx = (double)((int32_t)x - (int32_t)li->last_x);
            ev.u.pointer.dy = (double)((int32_t)y - (int32_t)li->last_y);
            li_push(li, ev);
            produced++;
            li->last_x = x;
            li->last_y = y;
        }
        if (b != li->last_buttons) {
            libinput_event ev = {0};
            ev.type = LIBINPUT_EVENT_POINTER_BUTTON;
            ev.u.pointer.button = 0x110; /* BTN_LEFT */
            ev.u.pointer.state  = (b & 1u) ? LIBINPUT_BUTTON_STATE_PRESSED : LIBINPUT_BUTTON_STATE_RELEASED;
            li_push(li, ev);
            produced++;
            li->last_buttons = b;
        }
    }

    uint64_t kev = ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
    if (kev) {
        int32_t k = (int32_t)kev;
        uint32_t st = LIBINPUT_KEY_STATE_PRESSED;
        if (k < 0) { st = LIBINPUT_KEY_STATE_RELEASED; k = -k; }
        libinput_event ev = {0};
        ev.type = LIBINPUT_EVENT_KEYBOARD_KEY;
        ev.u.keyboard.key = (uint32_t)k;
        ev.u.keyboard.state = st;
        li_push(li, ev);
        produced++;
    }
    return produced;
}

libinput_event *libinput_get_event(libinput *li) { return li ? li_pop(li) : NULL; }

void libinput_event_destroy(libinput_event *ev) {
    if (!ev) return;
    ev->active = 0;
}

enum libinput_event_type libinput_event_get_type(libinput_event *ev) {
    return ev ? ev->type : LIBINPUT_EVENT_NONE;
}

libinput_event_pointer  *libinput_event_get_pointer_event(libinput_event *ev) {
    if (!ev) return NULL;
    if (ev->type != LIBINPUT_EVENT_POINTER_MOTION &&
        ev->type != LIBINPUT_EVENT_POINTER_BUTTON &&
        ev->type != LIBINPUT_EVENT_POINTER_AXIS) return NULL;
    return (libinput_event_pointer*)ev;
}
libinput_event_keyboard *libinput_event_get_keyboard_event(libinput_event *ev) {
    if (!ev || ev->type != LIBINPUT_EVENT_KEYBOARD_KEY) return NULL;
    return (libinput_event_keyboard*)ev;
}

double  libinput_event_pointer_get_dx(libinput_event_pointer *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? e->u.pointer.dx : 0.0;
}
double  libinput_event_pointer_get_dy(libinput_event_pointer *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? e->u.pointer.dy : 0.0;
}
uint32_t libinput_event_pointer_get_button(libinput_event_pointer *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? e->u.pointer.button : 0u;
}
enum libinput_button_state libinput_event_pointer_get_button_state(libinput_event_pointer *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? (enum libinput_button_state)e->u.pointer.state : LIBINPUT_BUTTON_STATE_RELEASED;
}
uint32_t libinput_event_keyboard_get_key(libinput_event_keyboard *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? e->u.keyboard.key : 0u;
}
enum libinput_key_state libinput_event_keyboard_get_key_state(libinput_event_keyboard *ev) {
    libinput_event *e = (libinput_event*)ev;
    return e ? (enum libinput_key_state)e->u.keyboard.state : LIBINPUT_KEY_STATE_RELEASED;
}

libinput_device *libinput_path_add_device(libinput *li, const char *path) {
    libinput_device *d = (libinput_device*)calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->ctx = li;
    if (path) {
        size_t i = 0;
        while (i + 1 < sizeof(d->path) && path[i]) { d->path[i] = path[i]; ++i; }
        d->path[i] = '\0';
    }
    return d;
}
