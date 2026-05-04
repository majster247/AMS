/**
 * @file evdev.cpp
 * @brief AMS kernel evdev compatibility layer.
 *
 * Translates PS/2 keyboard/mouse IRQ events into Linux-compatible
 * struct input_event streams readable from /dev/input/event0 (keyboard)
 * and /dev/input/event1 (mouse).
 *
 * The evdev fds use fd_kind == FD_KIND_EVDEV_{KB,MOUSE} and read()
 * returns sizeof(struct input_event) blocks.  ioctl implements
 * EVIOCGVERSION, EVIOCGID, EVIOCGNAME, EVIOCGBIT.
 */

#include "kernel.h"
#include "linux/input.h"
#include <stdint.h>

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" uint64_t get_time_ms();

/* Ring buffers for keyboard and mouse evdev events */
#define EVDEV_QUEUE_SIZE 256

struct evdev_queue {
    struct input_event events[EVDEV_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
};

static evdev_queue g_evdev_kb;
static evdev_queue g_evdev_mouse;

static void evdev_enqueue(evdev_queue* q, uint16_t type, uint16_t code, int32_t value) {
    uint32_t next = (q->head + 1) % EVDEV_QUEUE_SIZE;
    if (next == q->tail) return; /* drop on full */

    uint64_t ms = get_time_ms();
    struct input_event* ev = &q->events[q->head];
    ev->time_sec  = ms / 1000;
    ev->time_usec = (ms % 1000) * 1000;
    ev->type  = type;
    ev->code  = code;
    ev->value = value;
    q->head = next;
}

static void evdev_syn(evdev_queue* q) {
    evdev_enqueue(q, EV_SYN, SYN_REPORT, 0);
}

/* PS/2 scancode set 1 → Linux KEY_* mapping (first 128 keys) */
static const uint16_t scancode_to_keycode[128] = {
    KEY_RESERVED, KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I,
    KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL, KEY_A, KEY_S,
    KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,
    KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V,
    KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK,
    KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, /* 69..70 numlock/scrolllock */ 0, 0,
    /* 71..83 numpad/arrows */ KEY_UP, 0, 0, 0, KEY_LEFT, 0, KEY_RIGHT, 0,
    KEY_DOWN, 0, 0, KEY_DELETE, 0, 0, 0, KEY_F11,
    KEY_F12, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

extern "C" void evdev_report_key(uint8_t scancode, bool pressed) {
    uint16_t keycode = 0;
    if (scancode < 128) {
        keycode = scancode_to_keycode[scancode];
    }
    if (keycode == KEY_RESERVED) return;

    evdev_enqueue(&g_evdev_kb, EV_KEY, keycode, pressed ? 1 : 0);
    evdev_syn(&g_evdev_kb);
}

extern "C" void evdev_report_mouse(int32_t dx, int32_t dy, uint8_t buttons, uint8_t prev_buttons) {
    if (dx != 0) evdev_enqueue(&g_evdev_mouse, EV_REL, REL_X, dx);
    if (dy != 0) evdev_enqueue(&g_evdev_mouse, EV_REL, REL_Y, dy);

    /* button state changes */
    if ((buttons & 1) != (prev_buttons & 1))
        evdev_enqueue(&g_evdev_mouse, EV_KEY, BTN_LEFT,   (buttons & 1) ? 1 : 0);
    if ((buttons & 2) != (prev_buttons & 2))
        evdev_enqueue(&g_evdev_mouse, EV_KEY, BTN_RIGHT,  (buttons & 2) ? 1 : 0);
    if ((buttons & 4) != (prev_buttons & 4))
        evdev_enqueue(&g_evdev_mouse, EV_KEY, BTN_MIDDLE, (buttons & 4) ? 1 : 0);

    evdev_syn(&g_evdev_mouse);
}

extern "C" uint64_t evdev_read_kb(uint8_t* buf, uint64_t count) {
    uint64_t copied = 0;
    while (copied + sizeof(struct input_event) <= count && g_evdev_kb.tail != g_evdev_kb.head) {
        k_memcpy(buf + copied, &g_evdev_kb.events[g_evdev_kb.tail], sizeof(struct input_event));
        g_evdev_kb.tail = (g_evdev_kb.tail + 1) % EVDEV_QUEUE_SIZE;
        copied += sizeof(struct input_event);
    }
    return copied;
}

extern "C" uint64_t evdev_read_mouse(uint8_t* buf, uint64_t count) {
    uint64_t copied = 0;
    while (copied + sizeof(struct input_event) <= count && g_evdev_mouse.tail != g_evdev_mouse.head) {
        k_memcpy(buf + copied, &g_evdev_mouse.events[g_evdev_mouse.tail], sizeof(struct input_event));
        g_evdev_mouse.tail = (g_evdev_mouse.tail + 1) % EVDEV_QUEUE_SIZE;
        copied += sizeof(struct input_event);
    }
    return copied;
}

extern "C" bool evdev_kb_has_data() {
    return g_evdev_kb.tail != g_evdev_kb.head;
}

extern "C" bool evdev_mouse_has_data() {
    return g_evdev_mouse.tail != g_evdev_mouse.head;
}

extern "C" int64_t evdev_ioctl_kb(uint32_t cmd, void* arg) {
    uint32_t nr   = (cmd >> 0) & 0xFF;
    uint32_t type = (cmd >> 8) & 0xFF;
    (void)type;

    /* EVIOCGVERSION */
    if (nr == 0x01) {
        *(int32_t*)arg = 0x010001; /* 1.0.1 */
        return 0;
    }
    /* EVIOCGID */
    if (nr == 0x02) {
        struct input_id* id = (struct input_id*)arg;
        id->bustype = BUS_I8042;
        id->vendor  = 0x0001;
        id->product = 0x0001;
        id->version = 0x0001;
        return 0;
    }
    /* EVIOCGNAME */
    if (nr == 0x06) {
        const char* name = "AMS PS/2 Keyboard";
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        uint32_t len = 17;
        if (len >= sz) len = sz - 1;
        k_memcpy(arg, name, len);
        ((char*)arg)[len] = '\0';
        return (int64_t)len;
    }
    /* EVIOCGBIT(0, ...) → event types supported */
    if (nr == 0x20) {
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        if (sz > 0) {
            k_memset(arg, 0, sz);
            uint8_t* bits = (uint8_t*)arg;
            bits[EV_SYN / 8] |= (1 << (EV_SYN % 8));
            bits[EV_KEY / 8] |= (1 << (EV_KEY % 8));
            bits[EV_REP / 8] |= (1 << (EV_REP % 8));
            bits[EV_MSC / 8] |= (1 << (EV_MSC % 8));
        }
        return 0;
    }
    /* EVIOCGBIT(EV_KEY, ...) */
    if (nr == 0x21) {
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        if (sz > 0) {
            k_memset(arg, 0, sz);
            uint8_t* bits = (uint8_t*)arg;
            for (int k = KEY_ESC; k <= KEY_F12 && k / 8 < (int)sz; k++) {
                bits[k / 8] |= (1 << (k % 8));
            }
        }
        return 0;
    }
    /* EVIOCGRAB */
    if (nr == 0x90) return 0;

    return -25; /* ENOTTY */
}

extern "C" int64_t evdev_ioctl_mouse(uint32_t cmd, void* arg) {
    uint32_t nr   = (cmd >> 0) & 0xFF;
    uint32_t type = (cmd >> 8) & 0xFF;
    (void)type;

    if (nr == 0x01) {
        *(int32_t*)arg = 0x010001;
        return 0;
    }
    if (nr == 0x02) {
        struct input_id* id = (struct input_id*)arg;
        id->bustype = BUS_I8042;
        id->vendor  = 0x0002;
        id->product = 0x0001;
        id->version = 0x0001;
        return 0;
    }
    if (nr == 0x06) {
        const char* name = "AMS PS/2 Mouse";
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        uint32_t len = 14;
        if (len >= sz) len = sz - 1;
        k_memcpy(arg, name, len);
        ((char*)arg)[len] = '\0';
        return (int64_t)len;
    }
    /* EVIOCGBIT(0, ...) */
    if (nr == 0x20) {
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        if (sz > 0) {
            k_memset(arg, 0, sz);
            uint8_t* bits = (uint8_t*)arg;
            bits[EV_SYN / 8] |= (1 << (EV_SYN % 8));
            bits[EV_KEY / 8] |= (1 << (EV_KEY % 8));
            bits[EV_REL / 8] |= (1 << (EV_REL % 8));
        }
        return 0;
    }
    /* EVIOCGBIT(EV_KEY, ...) */
    if (nr == 0x21) {
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        if (sz > 0) {
            k_memset(arg, 0, sz);
            uint8_t* bits = (uint8_t*)arg;
            if (BTN_LEFT / 8 < sz)   bits[BTN_LEFT / 8]   |= (1 << (BTN_LEFT % 8));
            if (BTN_RIGHT / 8 < sz)  bits[BTN_RIGHT / 8]  |= (1 << (BTN_RIGHT % 8));
            if (BTN_MIDDLE / 8 < sz) bits[BTN_MIDDLE / 8] |= (1 << (BTN_MIDDLE % 8));
        }
        return 0;
    }
    /* EVIOCGBIT(EV_REL, ...) */
    if (nr == 0x22) {
        uint32_t sz = (cmd >> 16) & 0x3FFF;
        if (sz > 0) {
            k_memset(arg, 0, sz);
            uint8_t* bits = (uint8_t*)arg;
            bits[REL_X / 8] |= (1 << (REL_X % 8));
            bits[REL_Y / 8] |= (1 << (REL_Y % 8));
        }
        return 0;
    }
    if (nr == 0x90) return 0;

    return -25;
}

extern "C" void evdev_init() {
    k_memset(&g_evdev_kb, 0, sizeof(g_evdev_kb));
    k_memset(&g_evdev_mouse, 0, sizeof(g_evdev_mouse));
    write_serial_string("[EVDEV] Linux-compatible input subsystem initialized\n");
}
