/**
 * Minimal libinput implementation for AMS.
 *
 * Reads from /dev/input/event* (our evdev layer) and presents
 * events through the libinput API that wlroots expects.
 */

#include "libinput.h"
#include <stdint.h>
#include <stddef.h>

extern uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2,
                            uint64_t p3, uint64_t p4, uint64_t p5);
extern void* malloc(size_t size);
extern void  free(void* ptr);
extern void* memset(void* dest, int ch, size_t n);

#define SYS_OPEN  2
#define SYS_READ  0
#define SYS_CLOSE 3
#define SYS_POLL  7

#define EV_SYN  0x00
#define EV_KEY  0x01
#define EV_REL  0x02
#define REL_X   0x00
#define REL_Y   0x01
#define BTN_LEFT  0x110
#define BTN_RIGHT 0x111

struct input_event_raw {
    uint64_t time_sec;
    uint64_t time_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

#define MAX_DEVICES 4
#define EVENT_QUEUE_SIZE 64

struct libinput_device {
    int fd;
    int dev_index; /* 0=kbd, 1=mouse */
    char name[64];
    int has_keyboard;
    int has_pointer;
    int in_use;
};

struct libinput_event {
    enum libinput_event_type type;
    struct libinput_device* device;
    uint32_t key;
    int32_t  key_state;
    double dx, dy;
    uint32_t button;
    int32_t  button_state;
    uint32_t time_ms;
};

struct libinput {
    const struct libinput_interface* iface;
    void* user_data;
    int ref_count;
    struct libinput_device devices[MAX_DEVICES];
    int device_count;
    struct libinput_event queue[EVENT_QUEUE_SIZE];
    int q_head, q_tail, q_count;
    int dummy_fd;
};

static struct libinput_event* alloc_event(struct libinput* li) {
    if (li->q_count >= EVENT_QUEUE_SIZE) return (struct libinput_event*)0;
    struct libinput_event* ev = &li->queue[li->q_tail];
    li->q_tail = (li->q_tail + 1) % EVENT_QUEUE_SIZE;
    li->q_count++;
    memset(ev, 0, sizeof(struct libinput_event));
    return ev;
}

struct libinput* libinput_path_create_context(
    const struct libinput_interface* interface, void* user_data)
{
    struct libinput* li = (struct libinput*)malloc(sizeof(struct libinput));
    if (!li) return (struct libinput*)0;
    memset(li, 0, sizeof(struct libinput));
    li->iface = interface;
    li->user_data = user_data;
    li->ref_count = 1;
    li->dummy_fd = -1;
    return li;
}

struct libinput_device* libinput_path_add_device(
    struct libinput* li, const char* path)
{
    if (!li || li->device_count >= MAX_DEVICES) return (struct libinput_device*)0;

    int fd = -1;
    if (li->iface && li->iface->open_restricted) {
        fd = li->iface->open_restricted(path, 0, li->user_data);
    } else {
        fd = (int)ams_syscall(SYS_OPEN, (uint64_t)path, 0, 0, 0, 0);
    }
    if (fd < 0) return (struct libinput_device*)0;

    struct libinput_device* dev = &li->devices[li->device_count];
    memset(dev, 0, sizeof(struct libinput_device));
    dev->fd = fd;
    dev->in_use = 1;

    /* determine device type from path */
    const char* p = path;
    while (*p) p++;
    if (p > path && *(p-1) == '0') {
        dev->dev_index = 0;
        dev->has_keyboard = 1;
        const char* n = "AMS Keyboard";
        for (int i = 0; n[i] && i < 63; i++) dev->name[i] = n[i];
    } else {
        dev->dev_index = 1;
        dev->has_pointer = 1;
        const char* n = "AMS Mouse";
        for (int i = 0; n[i] && i < 63; i++) dev->name[i] = n[i];
    }

    if (li->dummy_fd < 0) li->dummy_fd = fd;

    /* emit DEVICE_ADDED event */
    struct libinput_event* ev = alloc_event(li);
    if (ev) {
        ev->type = LIBINPUT_EVENT_DEVICE_ADDED;
        ev->device = dev;
    }

    li->device_count++;
    return dev;
}

void libinput_path_remove_device(struct libinput_device* device) {
    if (device) device->in_use = 0;
}

struct libinput* libinput_ref(struct libinput* li) {
    if (li) li->ref_count++;
    return li;
}

struct libinput* libinput_unref(struct libinput* li) {
    if (!li) return (struct libinput*)0;
    li->ref_count--;
    if (li->ref_count <= 0) {
        for (int i = 0; i < li->device_count; i++) {
            if (li->devices[i].in_use && li->devices[i].fd >= 0) {
                if (li->iface && li->iface->close_restricted)
                    li->iface->close_restricted(li->devices[i].fd, li->user_data);
                else
                    ams_syscall(SYS_CLOSE, (uint64_t)li->devices[i].fd, 0, 0, 0, 0);
            }
        }
        free(li);
        return (struct libinput*)0;
    }
    return li;
}

int libinput_get_fd(struct libinput* li) {
    return li ? li->dummy_fd : -1;
}

int libinput_dispatch(struct libinput* li) {
    if (!li) return -1;

    for (int d = 0; d < li->device_count; d++) {
        struct libinput_device* dev = &li->devices[d];
        if (!dev->in_use || dev->fd < 0) continue;

        struct input_event_raw raw;
        int pending_dx = 0, pending_dy = 0;
        int has_motion = 0;

        while (1) {
            int n = (int)ams_syscall(SYS_READ, (uint64_t)dev->fd,
                (uint64_t)&raw, sizeof(raw), 0, 0);
            if (n < (int)sizeof(raw)) break;

            uint32_t time_ms = (uint32_t)(raw.time_sec * 1000 + raw.time_usec / 1000);

            if (raw.type == EV_KEY && dev->has_keyboard) {
                struct libinput_event* ev = alloc_event(li);
                if (ev) {
                    ev->type = LIBINPUT_EVENT_KEYBOARD_KEY;
                    ev->device = dev;
                    ev->key = raw.code;
                    ev->key_state = raw.value;
                    ev->time_ms = time_ms;
                }
            }
            else if (raw.type == EV_KEY && dev->has_pointer) {
                struct libinput_event* ev = alloc_event(li);
                if (ev) {
                    ev->type = LIBINPUT_EVENT_POINTER_BUTTON;
                    ev->device = dev;
                    ev->button = raw.code;
                    ev->button_state = raw.value;
                    ev->time_ms = time_ms;
                }
            }
            else if (raw.type == EV_REL) {
                if (raw.code == REL_X) { pending_dx += raw.value; has_motion = 1; }
                if (raw.code == REL_Y) { pending_dy += raw.value; has_motion = 1; }
            }
            else if (raw.type == EV_SYN && has_motion) {
                struct libinput_event* ev = alloc_event(li);
                if (ev) {
                    ev->type = LIBINPUT_EVENT_POINTER_MOTION;
                    ev->device = dev;
                    ev->dx = (double)pending_dx;
                    ev->dy = (double)pending_dy;
                    ev->time_ms = time_ms;
                }
                pending_dx = 0; pending_dy = 0; has_motion = 0;
            }
        }
    }
    return 0;
}

struct libinput_event* libinput_get_event(struct libinput* li) {
    if (!li || li->q_count == 0) return (struct libinput_event*)0;
    struct libinput_event* ev = &li->queue[li->q_head];
    li->q_head = (li->q_head + 1) % EVENT_QUEUE_SIZE;
    li->q_count--;
    return ev;
}

/* Event accessors */
enum libinput_event_type libinput_event_get_type(struct libinput_event* event) {
    return event ? event->type : LIBINPUT_EVENT_NONE;
}

struct libinput_device* libinput_event_get_device(struct libinput_event* event) {
    return event ? event->device : (struct libinput_device*)0;
}

void libinput_event_destroy(struct libinput_event* event) {
    (void)event; /* events are in a ring buffer, no alloc needed */
}

/* Device accessors */
const char* libinput_device_get_name(struct libinput_device* device) {
    return device ? device->name : "unknown";
}

int libinput_device_has_capability(struct libinput_device* device,
    enum libinput_device_capability cap) {
    if (!device) return 0;
    if (cap == LIBINPUT_DEVICE_CAP_KEYBOARD) return device->has_keyboard;
    if (cap == LIBINPUT_DEVICE_CAP_POINTER) return device->has_pointer;
    return 0;
}

unsigned int libinput_device_get_id_vendor(struct libinput_device* device) {
    (void)device; return 0xAA5;
}

unsigned int libinput_device_get_id_product(struct libinput_device* device) {
    return device ? (unsigned int)(device->dev_index + 1) : 0;
}

/* Keyboard event accessors */
struct libinput_event_keyboard* libinput_event_get_keyboard_event(
    struct libinput_event* event) {
    return (struct libinput_event_keyboard*)event;
}

uint32_t libinput_event_keyboard_get_key(struct libinput_event_keyboard* event) {
    return ((struct libinput_event*)event)->key;
}

enum libinput_key_state libinput_event_keyboard_get_key_state(
    struct libinput_event_keyboard* event) {
    return (enum libinput_key_state)((struct libinput_event*)event)->key_state;
}

uint32_t libinput_event_keyboard_get_time(struct libinput_event_keyboard* event) {
    return ((struct libinput_event*)event)->time_ms;
}

/* Pointer event accessors */
struct libinput_event_pointer* libinput_event_get_pointer_event(
    struct libinput_event* event) {
    return (struct libinput_event_pointer*)event;
}

double libinput_event_pointer_get_dx(struct libinput_event_pointer* event) {
    return ((struct libinput_event*)event)->dx;
}

double libinput_event_pointer_get_dy(struct libinput_event_pointer* event) {
    return ((struct libinput_event*)event)->dy;
}

uint32_t libinput_event_pointer_get_button(struct libinput_event_pointer* event) {
    return ((struct libinput_event*)event)->button;
}

enum libinput_button_state libinput_event_pointer_get_button_state(
    struct libinput_event_pointer* event) {
    return (enum libinput_button_state)((struct libinput_event*)event)->button_state;
}

uint32_t libinput_event_pointer_get_time(struct libinput_event_pointer* event) {
    return ((struct libinput_event*)event)->time_ms;
}
