/**
 * evdev compatibility layer for AMS.
 *
 * Bridges PS/2 keyboard/mouse events into Linux evdev-style
 * struct input_event streams, readable via /dev/input/event0 (kbd)
 * and /dev/input/event1 (mouse).
 */

#include <stdint.h>

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void  write_serial_string(const char* s);
extern "C" void  write_serial_hex(uint64_t val);

extern "C" uint64_t sys_get_key();
extern "C" uint64_t sys_get_mouse_event();
extern uint32_t fb_width;
extern uint32_t fb_height;

/* matches Linux struct input_event (24 bytes) */
struct input_event {
    uint64_t time_sec;
    uint64_t time_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

/* constants */
#define EV_SYN  0x00
#define EV_KEY  0x01
#define EV_REL  0x02
#define EV_ABS  0x03

#define SYN_REPORT 0
#define REL_X      0x00
#define REL_Y      0x01
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111

static constexpr uint32_t EVDEV_RING_SIZE = 128;

struct evdev_device {
    input_event ring[EVDEV_RING_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    input_id id;
    char name[64];
    bool in_use;

    /* for mouse delta tracking */
    int32_t last_x;
    int32_t last_y;
    uint8_t last_buttons;
};

static evdev_device g_evdev_kbd;
static evdev_device g_evdev_mouse;
static uint64_t g_monotonic_secs = 0;
static uint64_t g_monotonic_usecs = 0;

static void tick_time() {
    g_monotonic_usecs += 1000;
    if (g_monotonic_usecs >= 1000000) {
        g_monotonic_usecs -= 1000000;
        g_monotonic_secs++;
    }
}

static void evdev_push(evdev_device* dev, uint16_t type, uint16_t code, int32_t value) {
    if (dev->count >= EVDEV_RING_SIZE) return;
    input_event* ev = &dev->ring[dev->tail];
    ev->time_sec  = g_monotonic_secs;
    ev->time_usec = g_monotonic_usecs;
    ev->type  = type;
    ev->code  = code;
    ev->value = value;
    dev->tail = (dev->tail + 1) % EVDEV_RING_SIZE;
    dev->count++;
}

static void evdev_push_syn(evdev_device* dev) {
    evdev_push(dev, EV_SYN, SYN_REPORT, 0);
}

extern "C" void evdev_ams_init() {
    k_memset(&g_evdev_kbd, 0, sizeof(g_evdev_kbd));
    k_memset(&g_evdev_mouse, 0, sizeof(g_evdev_mouse));

    g_evdev_kbd.in_use = true;
    g_evdev_kbd.id.bustype = 0x06; /* BUS_VIRTUAL */
    g_evdev_kbd.id.vendor  = 0xAA5;
    g_evdev_kbd.id.product = 0x01;
    g_evdev_kbd.id.version = 1;
    const char* kn = "AMS PS/2 Keyboard";
    for (int i = 0; kn[i] && i < 63; i++) g_evdev_kbd.name[i] = kn[i];

    g_evdev_mouse.in_use = true;
    g_evdev_mouse.id.bustype = 0x06;
    g_evdev_mouse.id.vendor  = 0xAA5;
    g_evdev_mouse.id.product = 0x02;
    g_evdev_mouse.id.version = 1;
    const char* mn = "AMS PS/2 Mouse";
    for (int i = 0; mn[i] && i < 63; i++) g_evdev_mouse.name[i] = mn[i];

    write_serial_string("[EVDEV] AMS evdev subsystem initialized\n");
}

/**
 * Poll the PS/2 drivers and convert to evdev events.
 * Called periodically from the compositor event loop or from
 * poll/epoll when those FDs are watched.
 */
extern "C" void evdev_ams_poll() {
    tick_time();

    /* keyboard */
    uint64_t kev = sys_get_key();
    if (kev) {
        int32_t k = (int32_t)kev;
        int32_t state = 1; /* pressed */
        if (k < 0) { state = 0; k = -k; }
        evdev_push(&g_evdev_kbd, EV_KEY, (uint16_t)k, state);
        evdev_push_syn(&g_evdev_kbd);
    }

    /* mouse */
    uint64_t mev = sys_get_mouse_event();
    if (mev) {
        int32_t mx = (int32_t)(mev & 0xFFFFu);
        int32_t my = (int32_t)((mev >> 16) & 0xFFFFu);
        uint8_t buttons = (uint8_t)((mev >> 32) & 0xFFu);

        int32_t dx = mx - g_evdev_mouse.last_x;
        int32_t dy = my - g_evdev_mouse.last_y;
        g_evdev_mouse.last_x = mx;
        g_evdev_mouse.last_y = my;

        if (dx || dy) {
            evdev_push(&g_evdev_mouse, EV_REL, REL_X, dx);
            evdev_push(&g_evdev_mouse, EV_REL, REL_Y, dy);
        }

        uint8_t old = g_evdev_mouse.last_buttons;
        if ((buttons & 1) != (old & 1))
            evdev_push(&g_evdev_mouse, EV_KEY, BTN_LEFT, (buttons & 1) ? 1 : 0);
        if ((buttons & 2) != (old & 2))
            evdev_push(&g_evdev_mouse, EV_KEY, BTN_RIGHT, (buttons & 2) ? 1 : 0);
        g_evdev_mouse.last_buttons = buttons;

        if (dx || dy || old != buttons)
            evdev_push_syn(&g_evdev_mouse);
    }
}

/**
 * Read evdev events for a device.
 * dev_idx: 0 = keyboard, 1 = mouse
 * Returns number of bytes read, or 0 if no events pending.
 */
extern "C" int evdev_ams_read(int dev_idx, void* buf, int max_bytes) {
    evdev_device* dev = (dev_idx == 0) ? &g_evdev_kbd : &g_evdev_mouse;
    if (!dev->in_use || dev->count == 0) return 0;

    int events_to_read = max_bytes / (int)sizeof(input_event);
    if (events_to_read <= 0) return 0;
    if ((uint32_t)events_to_read > dev->count) events_to_read = (int)dev->count;

    uint8_t* out = (uint8_t*)buf;
    int total = 0;
    for (int i = 0; i < events_to_read; i++) {
        k_memcpy(out + total, &dev->ring[dev->head], sizeof(input_event));
        dev->head = (dev->head + 1) % EVDEV_RING_SIZE;
        dev->count--;
        total += (int)sizeof(input_event);
    }
    return total;
}

/**
 * Check if the evdev device has pending events (for poll/epoll).
 */
extern "C" bool evdev_ams_has_events(int dev_idx) {
    evdev_device* dev = (dev_idx == 0) ? &g_evdev_kbd : &g_evdev_mouse;
    return dev->in_use && dev->count > 0;
}

/**
 * Handle evdev ioctls (EVIOCGNAME, EVIOCGID, EVIOCGBIT, etc.)
 */
extern "C" int64_t evdev_ams_ioctl(int dev_idx, uint64_t request, void* argp) {
    evdev_device* dev = (dev_idx == 0) ? &g_evdev_kbd : &g_evdev_mouse;
    if (!dev->in_use) return -19; /* ENODEV */

    uint32_t req32 = (uint32_t)(request & 0xFFFFFFFFu);

    /* EVIOCGVERSION */
    if (req32 == 0x80044501u) {
        *(int32_t*)argp = 0x010001; /* 1.0.1 */
        return 0;
    }

    /* EVIOCGID */
    if (req32 == 0x80084502u) {
        k_memcpy(argp, &dev->id, sizeof(input_id));
        return 0;
    }

    /* EVIOCGNAME(len) — upper bits have length */
    if ((req32 & 0xFF) == 0x06 && ((req32 >> 8) & 0xFF) == 0x45) {
        uint32_t len = (req32 >> 16) & 0xFFFFu;
        uint32_t nl = 0;
        while (dev->name[nl] && nl < 63) nl++;
        if (nl + 1 > len) nl = len - 1;
        k_memcpy(argp, dev->name, nl);
        ((char*)argp)[nl] = '\0';
        return (int64_t)(nl + 1);
    }

    /* EVIOCGBIT — return bitmask of supported event types/codes */
    if ((req32 & 0xFF) >= 0x20 && (req32 & 0xFF) <= 0x3F) {
        uint32_t ev_type = (req32 & 0xFF) - 0x20;
        uint32_t len = (req32 >> 16) & 0xFFFFu;
        if (!argp || len == 0) return 0;
        k_memset(argp, 0, len);

        if (ev_type == 0) {
            /* EV bitmask: report EV_SYN, EV_KEY, [EV_REL for mouse] */
            uint8_t* bits = (uint8_t*)argp;
            bits[0] |= (1 << EV_SYN) | (1 << EV_KEY);
            if (dev_idx == 1) bits[0] |= (1 << EV_REL);
        }
        return 0;
    }

    /* EVIOCGABS — absolute axis info (not supported for rel mouse) */
    if ((req32 & 0xFFu) >= 0x40 && (req32 & 0xFFu) <= 0x7F) {
        struct absinfo { int32_t value, minimum, maximum, fuzz, flat, resolution; };
        auto* ai = (absinfo*)argp;
        k_memset(ai, 0, sizeof(absinfo));
        uint32_t axis = (req32 & 0xFFu) - 0x40;
        if (axis == 0) { ai->maximum = (int32_t)fb_width; }
        if (axis == 1) { ai->maximum = (int32_t)fb_height; }
        return 0;
    }

    /* EVIOCGRAB — stub: success */
    if (req32 == 0x40044590u) return 0;

    return -25; /* ENOTTY */
}
