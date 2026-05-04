/**
 * @file evdev.cpp
 * @brief AMS-OS evdev input device layer for libinput compatibility
 *
 * Translates AMS-OS keyboard/mouse events into Linux evdev format
 * for consumption by libinput and other input-handling libraries.
 */

#include "evdev.h"
#include "kernel.h"
#include <stdint.h>

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" char* k_strcpy(char* dest, const char* src);
extern "C" void write_serial_string(const char* str);
extern "C" void write_serial_dec(uint64_t val);

struct evdev_device g_evdev_devices[EVDEV_MAX_DEVICES];

static uint64_t evdev_timestamp_sec = 0;
static uint64_t evdev_timestamp_usec = 0;

static void update_timestamp() {
    evdev_timestamp_usec += 16667; /* ~60 Hz */
    while (evdev_timestamp_usec >= 1000000) {
        evdev_timestamp_usec -= 1000000;
        evdev_timestamp_sec++;
    }
}

void evdev_init(void) {
    k_memset(g_evdev_devices, 0, sizeof(g_evdev_devices));

    /* Device 0: keyboard */
    g_evdev_devices[0].in_use = 1;
    g_evdev_devices[0].dev_type = 0;
    g_evdev_devices[0].id.bustype = BUS_I8042;
    g_evdev_devices[0].id.vendor = 0x0001;
    g_evdev_devices[0].id.product = 0x0001;
    g_evdev_devices[0].id.version = 1;
    k_strcpy(g_evdev_devices[0].name, "AMS Virtual Keyboard");

    /* Device 1: mouse */
    g_evdev_devices[1].in_use = 1;
    g_evdev_devices[1].dev_type = 1;
    g_evdev_devices[1].id.bustype = BUS_USB;
    g_evdev_devices[1].id.vendor = 0x0001;
    g_evdev_devices[1].id.product = 0x0002;
    g_evdev_devices[1].id.version = 1;
    k_strcpy(g_evdev_devices[1].name, "AMS Virtual Mouse");

    write_serial_string("[EVDEV] Initialized: keyboard=event0, mouse=event1\n");
}

void evdev_push_event(uint32_t dev_idx, uint16_t type, uint16_t code, int32_t value) {
    if (dev_idx >= EVDEV_MAX_DEVICES) return;
    struct evdev_device* dev = &g_evdev_devices[dev_idx];
    if (!dev->in_use) return;

    uint32_t next = (dev->head + 1) % EVDEV_RING_SIZE;
    if (next == dev->tail) return; /* Ring full, drop event */

    struct input_event* ev = &dev->ring[dev->head];
    ev->time_sec = (int64_t)evdev_timestamp_sec;
    ev->time_usec = (int64_t)evdev_timestamp_usec;
    ev->type = type;
    ev->code = code;
    ev->value = value;

    dev->head = next;
}

void evdev_push_key(uint32_t scancode, int pressed) {
    update_timestamp();
    evdev_push_event(0, EV_KEY, (uint16_t)scancode, pressed ? 1 : 0);
    evdev_push_event(0, EV_SYN, SYN_REPORT, 0);
}

void evdev_push_mouse_rel(int32_t dx, int32_t dy) {
    update_timestamp();
    if (dx != 0) evdev_push_event(1, EV_REL, REL_X, dx);
    if (dy != 0) evdev_push_event(1, EV_REL, REL_Y, dy);
    evdev_push_event(1, EV_SYN, SYN_REPORT, 0);
}

void evdev_push_mouse_abs(int32_t x, int32_t y) {
    update_timestamp();
    evdev_push_event(1, EV_ABS, ABS_X, x);
    evdev_push_event(1, EV_ABS, ABS_Y, y);
    evdev_push_event(1, EV_SYN, SYN_REPORT, 0);
}

void evdev_push_mouse_button(uint16_t button, int pressed) {
    update_timestamp();
    evdev_push_event(1, EV_KEY, button, pressed ? 1 : 0);
    evdev_push_event(1, EV_SYN, SYN_REPORT, 0);
}

int evdev_read(uint32_t dev_idx, void* buf, uint32_t count) {
    if (dev_idx >= EVDEV_MAX_DEVICES) return -9; /* EBADF */
    struct evdev_device* dev = &g_evdev_devices[dev_idx];
    if (!dev->in_use) return -9;

    uint32_t ev_size = sizeof(struct input_event);
    uint32_t max_events = count / ev_size;
    uint32_t read_count = 0;
    uint8_t* out = (uint8_t*)buf;

    while (read_count < max_events && dev->tail != dev->head) {
        k_memcpy(out + read_count * ev_size, &dev->ring[dev->tail], ev_size);
        dev->tail = (dev->tail + 1) % EVDEV_RING_SIZE;
        read_count++;
    }

    return (int)(read_count * ev_size);
}

/* EVIOCGNAME, EVIOCGID, EVIOCGBIT, etc. */
#define EVIOCGVERSION  0x80044501
#define EVIOCGID       0x80084502
#define EVIOCGNAME_0   0x80FF4506
#define EVIOCGBIT_0    0x80404520
#define EVIOCGBIT_EV_KEY 0x80404521
#define EVIOCGBIT_EV_REL 0x80404522
#define EVIOCGBIT_EV_ABS 0x80404523

int evdev_ioctl(uint32_t dev_idx, uint64_t request, uint64_t arg) {
    if (dev_idx >= EVDEV_MAX_DEVICES) return -9;
    struct evdev_device* dev = &g_evdev_devices[dev_idx];
    if (!dev->in_use) return -9;

    switch (request & 0xFFFF00FF) {
        case 0x80044501: { /* EVIOCGVERSION */
            if (!arg) return -14;
            *(int32_t*)arg = 0x010001; /* EV_VERSION 1.0.1 */
            return 0;
        }
        case 0x80084502: { /* EVIOCGID */
            if (!arg) return -14;
            k_memcpy((void*)arg, &dev->id, sizeof(struct input_id));
            return 0;
        }
        default:
            /* EVIOCGNAME - return device name */
            if ((request & 0xFF00FFFF) == 0x80004506 ||
                (request & 0xFFFF0000) == 0x80FF0000) {
                if (!arg) return -14;
                int len = 0;
                while (dev->name[len]) len++;
                k_memcpy((void*)arg, dev->name, (uint32_t)(len + 1));
                return len;
            }
            break;
    }

    return -25; /* ENOTTY */
}
