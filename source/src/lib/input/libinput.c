/* AMS libinput stub — reads /dev/input/event{0,1} struct input_event records
 * and presents them as libinput events.
 */
#include "libinput.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Linux input_event (24 bytes) */
struct ams_input_event {
    int64_t  tv_sec;
    int64_t  tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

#define EV_SYN  0
#define EV_KEY  1
#define EV_REL  2
#define REL_X   0
#define REL_Y   1
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/* AMS syscall helpers */
static long ams_open(const char* path, int flags) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "0"(2), "D"(path), "S"(flags) : "rcx","r11","memory");
    return r;
}
static long ams_read(int fd, void* buf, long count) {
    long r;
    __asm__ volatile("syscall" : "=a"(r) : "0"(0), "D"((long)fd), "S"(buf), "d"(count) : "rcx","r11","memory");
    return r;
}

/* ---- internal structures ---- */

#define MAX_DEVICES 4
#define EVENT_QUEUE 256

struct libinput_device {
    int   fd;
    int   dev_idx; /* 0=kbd, 1=mouse */
    char  path[64];
    char  name[32];
    struct libinput* ctx;
};

struct libinput_event_keyboard {
    uint32_t key;
    libinput_key_state state;
    uint64_t time_usec;
    struct libinput_device* device;
};

struct libinput_event_pointer {
    double dx, dy;
    uint32_t button;
    libinput_button_state btn_state;
    uint64_t time_usec;
    struct libinput_device* device;
    int is_motion;
    int is_button;
};

struct libinput_event {
    libinput_event_type type;
    struct libinput_device* device;
    union {
        struct libinput_event_keyboard kbd;
        struct libinput_event_pointer  ptr;
    } u;
};

#define EVQUEUE_SIZE 256
struct libinput {
    struct libinput_interface iface;
    void* user_data;
    int refs;
    struct libinput_device* devices[MAX_DEVICES];
    int n_devices;
    /* event ring buffer */
    struct libinput_event* ring[EVQUEUE_SIZE];
    int head, tail;
    /* aggregation for REL events */
    double pending_dx, pending_dy;
    struct libinput_device* pending_dev;
    int pending_motion;
};

static void enqueue(struct libinput* li, struct libinput_event* ev) {
    int next = (li->head + 1) % EVQUEUE_SIZE;
    if (next == li->tail) { free(ev); return; } /* full — drop */
    li->ring[li->head] = ev;
    li->head = next;
}

struct libinput* libinput_path_create_context(const struct libinput_interface* iface, void* user_data) {
    struct libinput* li = (struct libinput*)calloc(1, sizeof(struct libinput));
    if (!li) return NULL;
    if (iface) li->iface = *iface;
    li->user_data = user_data;
    li->refs = 1;
    return li;
}

struct libinput* libinput_ref(struct libinput* li)   { if (li) li->refs++; return li; }
struct libinput* libinput_unref(struct libinput* li) {
    if (!li) return NULL;
    if (--li->refs <= 0) {
        for (int i = 0; i < li->n_devices; i++) {
            if (li->devices[i]->fd >= 0) {
                long r; __asm__ volatile("syscall":"=a"(r):"0"(3),"D"((long)li->devices[i]->fd):);
            }
            free(li->devices[i]);
        }
        while (li->tail != li->head) {
            free(li->ring[li->tail]);
            li->tail = (li->tail + 1) % EVQUEUE_SIZE;
        }
        free(li);
        return NULL;
    }
    return li;
}

int libinput_get_fd(struct libinput* li) {
    /* Return fd of first device (compositor can poll it) */
    if (!li || li->n_devices == 0) return -1;
    return li->devices[0]->fd;
}

struct libinput_device* libinput_path_add_device(struct libinput* li, const char* path) {
    if (!li || !path || li->n_devices >= MAX_DEVICES) return NULL;
    int fd = (int)ams_open(path, 0 /*O_RDONLY*/);
    if (fd < 0) return NULL;

    struct libinput_device* dev = (struct libinput_device*)calloc(1, sizeof(*dev));
    if (!dev) return dev;
    dev->fd = fd;
    dev->ctx = li;
    strncpy(dev->path, path, sizeof(dev->path) - 1);
    /* Guess device type from path */
    if (strstr(path, "event0")) { dev->dev_idx = 0; strncpy(dev->name, "AMS Keyboard", 31); }
    else                        { dev->dev_idx = 1; strncpy(dev->name, "AMS Pointer",  31); }

    li->devices[li->n_devices++] = dev;

    /* Emit DEVICE_ADDED event */
    struct libinput_event* ev = (struct libinput_event*)calloc(1, sizeof(*ev));
    if (ev) { ev->type = LIBINPUT_EVENT_DEVICE_ADDED; ev->device = dev; enqueue(li, ev); }
    return dev;
}

struct libinput_device* libinput_device_ref(struct libinput_device* d) { return d; }
struct libinput_device* libinput_device_unref(struct libinput_device* d) { return d; }
const char* libinput_device_get_name(struct libinput_device* d) { return d ? d->name : ""; }

int libinput_dispatch(struct libinput* li) {
    if (!li) return -1;
    struct ams_input_event raw[32];

    for (int di = 0; di < li->n_devices; di++) {
        struct libinput_device* dev = li->devices[di];
        long n = ams_read(dev->fd, raw, sizeof(raw));
        if (n <= 0) continue;
        int count = (int)n / (int)sizeof(struct ams_input_event);
        for (int i = 0; i < count; i++) {
            struct ams_input_event* re = &raw[i];
            uint64_t ts = (uint64_t)re->tv_sec * 1000000ULL + (uint64_t)re->tv_usec;

            if (re->type == EV_SYN) {
                /* Flush pending motion */
                if (li->pending_motion) {
                    struct libinput_event* ev = (struct libinput_event*)calloc(1, sizeof(*ev));
                    if (ev) {
                        ev->type = LIBINPUT_EVENT_POINTER_MOTION;
                        ev->device = li->pending_dev;
                        ev->u.ptr.dx = li->pending_dx;
                        ev->u.ptr.dy = li->pending_dy;
                        ev->u.ptr.is_motion = 1;
                        ev->u.ptr.time_usec = ts;
                        enqueue(li, ev);
                    }
                    li->pending_dx = li->pending_dy = 0;
                    li->pending_motion = 0;
                }
            } else if (re->type == EV_KEY) {
                if (re->code >= 0x110 && re->code <= 0x116) {
                    /* Mouse button */
                    struct libinput_event* ev = (struct libinput_event*)calloc(1, sizeof(*ev));
                    if (ev) {
                        ev->type = LIBINPUT_EVENT_POINTER_BUTTON;
                        ev->device = dev;
                        ev->u.ptr.button    = re->code;
                        ev->u.ptr.btn_state = re->value ? LIBINPUT_BUTTON_STATE_PRESSED
                                                        : LIBINPUT_BUTTON_STATE_RELEASED;
                        ev->u.ptr.is_button = 1;
                        ev->u.ptr.time_usec = ts;
                        enqueue(li, ev);
                    }
                } else {
                    /* Keyboard key */
                    struct libinput_event* ev = (struct libinput_event*)calloc(1, sizeof(*ev));
                    if (ev) {
                        ev->type = LIBINPUT_EVENT_KEYBOARD_KEY;
                        ev->device = dev;
                        ev->u.kbd.key       = re->code;
                        ev->u.kbd.state     = re->value ? LIBINPUT_KEY_STATE_PRESSED
                                                        : LIBINPUT_KEY_STATE_RELEASED;
                        ev->u.kbd.time_usec = ts;
                        enqueue(li, ev);
                    }
                }
            } else if (re->type == EV_REL) {
                if (re->code == REL_X) { li->pending_dx += (double)re->value; li->pending_motion = 1; li->pending_dev = dev; }
                if (re->code == REL_Y) { li->pending_dy += (double)re->value; li->pending_motion = 1; li->pending_dev = dev; }
            }
        }
    }
    return 0;
}

struct libinput_event* libinput_get_event(struct libinput* li) {
    if (!li || li->head == li->tail) return NULL;
    struct libinput_event* ev = li->ring[li->tail];
    li->tail = (li->tail + 1) % EVQUEUE_SIZE;
    return ev;
}

libinput_event_type libinput_event_get_type(struct libinput_event* ev) {
    return ev ? ev->type : LIBINPUT_EVENT_NONE;
}

struct libinput_device* libinput_event_get_device(struct libinput_event* ev) {
    return ev ? ev->device : NULL;
}

void libinput_event_destroy(struct libinput_event* ev) { free(ev); }

/* Keyboard */
struct libinput_event_keyboard* libinput_event_get_keyboard_event(struct libinput_event* ev) {
    return (ev && ev->type == LIBINPUT_EVENT_KEYBOARD_KEY) ? &ev->u.kbd : NULL;
}
uint32_t libinput_event_keyboard_get_key(struct libinput_event_keyboard* e) { return e ? e->key : 0; }
libinput_key_state libinput_event_keyboard_get_key_state(struct libinput_event_keyboard* e) {
    return e ? e->state : LIBINPUT_KEY_STATE_RELEASED;
}
uint64_t libinput_event_keyboard_get_time_usec(struct libinput_event_keyboard* e) { return e ? e->time_usec : 0; }

/* Pointer */
struct libinput_event_pointer* libinput_event_get_pointer_event(struct libinput_event* ev) {
    return (ev && (ev->type == LIBINPUT_EVENT_POINTER_MOTION ||
                   ev->type == LIBINPUT_EVENT_POINTER_BUTTON ||
                   ev->type == LIBINPUT_EVENT_POINTER_AXIS)) ? &ev->u.ptr : NULL;
}
double libinput_event_pointer_get_dx(struct libinput_event_pointer* e) { return e ? e->dx : 0; }
double libinput_event_pointer_get_dy(struct libinput_event_pointer* e) { return e ? e->dy : 0; }
uint32_t libinput_event_pointer_get_button(struct libinput_event_pointer* e) { return e ? e->button : 0; }
libinput_button_state libinput_event_pointer_get_button_state(struct libinput_event_pointer* e) {
    return e ? e->btn_state : LIBINPUT_BUTTON_STATE_RELEASED;
}
uint64_t libinput_event_pointer_get_time_usec(struct libinput_event_pointer* e) { return e ? e->time_usec : 0; }
