/* AMS evdev layer — bridges PS/2 keyboard + mouse to Linux-compatible
 * /dev/input/event0 (keyboard) and /dev/input/event1 (mouse) devices.
 *
 * Each device maintains a ring buffer of struct input_event records.
 * Userspace reads them via SYS_READ on the corresponding VFS node FD.
 */
#include "evdev.h"
#include "kernel.h"
#include "vfs.h"
#include <stdint.h>
#include <stddef.h>

extern "C" void* kmalloc(size_t);
extern "C" void  kfree(void*);
extern "C" void* k_memset(void*, int, size_t);
extern "C" void* k_memcpy(void*, const void*, size_t);
extern "C" int   k_strlen(const char*);
extern "C" char* k_strcpy(char*, const char*);
extern uint64_t  get_system_ticks();
extern vfs_node* vfs_root;

/* ---- ring buffer per device ---- */
#define EVDEV_NDEV 2   /* event0 = keyboard, event1 = mouse */

static struct input_event evdev_ring[EVDEV_NDEV][EVDEV_RING_SIZE];
static volatile int evdev_head[EVDEV_NDEV];
static volatile int evdev_tail[EVDEV_NDEV];

/* VFS nodes for /dev/input/event0 and /dev/input/event1 */
static vfs_node evdev_nodes[EVDEV_NDEV];

/* Scancode → Linux KEY_* translation table (128 entries, PC-AT set 1) */
static const uint16_t sc_to_linux[128] = {
    0,          /* 0x00 */
    KEY_ESC,    /* 0x01 */
    KEY_1,      /* 0x02 */
    KEY_2,      KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS,  KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK,
    KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLOCK, KEY_SCROLLLOCK,
    /* 0x47–0x53: numpad — map to arrows/home etc. */
    KEY_HOME, KEY_UP, KEY_PAGEUP, 0, KEY_LEFT, 0, KEY_RIGHT, 0,
    KEY_END, KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE,
    0, 0, 0,
    KEY_F11,    /* 0x57 */
    KEY_F12,    /* 0x58 */
    /* 0x59–0x7F: unused */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0
};

/* ---- internal helpers ---- */

static void push_event(int dev, uint16_t type, uint16_t code, int32_t value) {
    int next = (evdev_head[dev] + 1) % EVDEV_RING_SIZE;
    if (next == evdev_tail[dev]) return; /* ring full — drop */

    struct input_event* ev = &evdev_ring[dev][evdev_head[dev]];
    uint64_t ticks = get_system_ticks();
    ev->tv_sec  = (int64_t)(ticks / 100);
    ev->tv_usec = (int64_t)((ticks % 100) * 10000);
    ev->type    = type;
    ev->code    = code;
    ev->value   = value;
    evdev_head[dev] = next;
}

static void push_syn(int dev) {
    push_event(dev, EV_SYN, SYN_REPORT, 0);
}

/* ---- VFS read callback (called from sys_read) ---- */

static uint64_t evdev_vfs_read(vfs_node* node, uint64_t /*offset*/, uint64_t len, uint8_t* buf) {
    int dev = (int)(uint64_t)node->tar_data; /* we stash device index here */
    if (len < sizeof(struct input_event)) return 0;

    uint64_t count = 0;
    while (count + sizeof(struct input_event) <= len) {
        if (evdev_head[dev] == evdev_tail[dev]) break;
        struct input_event* ev = &evdev_ring[dev][evdev_tail[dev]];
        k_memcpy(buf + count, ev, sizeof(struct input_event));
        evdev_tail[dev] = (evdev_tail[dev] + 1) % EVDEV_RING_SIZE;
        count += sizeof(struct input_event);
    }
    return count;
}

/* ---- Public API ---- */

void evdev_init(void) {
    for (int d = 0; d < EVDEV_NDEV; d++) {
        evdev_head[d] = evdev_tail[d] = 0;
    }

    /* Register /dev/input/event0 (keyboard) */
    k_memset(&evdev_nodes[0], 0, sizeof(vfs_node));
    k_strcpy(evdev_nodes[0].name, "event0");
    evdev_nodes[0].type         = FS_FILE;
    evdev_nodes[0].source       = FS_TAR;
    evdev_nodes[0].read         = evdev_vfs_read;
    evdev_nodes[0].tar_data     = (uint8_t*)(uint64_t)0; /* dev index 0 */

    /* Register /dev/input/event1 (pointer) */
    k_memset(&evdev_nodes[1], 0, sizeof(vfs_node));
    k_strcpy(evdev_nodes[1].name, "event1");
    evdev_nodes[1].type         = FS_FILE;
    evdev_nodes[1].source       = FS_TAR;
    evdev_nodes[1].read         = evdev_vfs_read;
    evdev_nodes[1].tar_data     = (uint8_t*)(uint64_t)1; /* dev index 1 */

    /* Prepend both nodes to VFS root list */
    evdev_nodes[1].next = vfs_root;
    evdev_nodes[0].next = &evdev_nodes[1];
    vfs_root = &evdev_nodes[0];
}

/* Called by keyboard_handler with PS/2 scancode (set 1) */
extern "C" void evdev_push_scancode(uint8_t scancode) {
    bool release = (scancode & 0x80) != 0;
    uint8_t sc   = scancode & 0x7F;
    if (sc >= 128) return;

    uint16_t keycode = sc_to_linux[sc];
    if (keycode == 0) return;

    push_event(0, EV_KEY, keycode, release ? KEY_RELEASED : KEY_PRESSED);
    push_syn(0);
}

void evdev_push_key(uint16_t linux_keycode, int value) {
    push_event(0, EV_KEY, linux_keycode, value);
    push_syn(0);
}

void evdev_push_rel(int32_t dx, int32_t dy) {
    if (dx) push_event(1, EV_REL, REL_X, dx);
    if (dy) push_event(1, EV_REL, REL_Y, dy);
    push_syn(1);
}

void evdev_push_btn(uint16_t btn_code, int value) {
    push_event(1, EV_KEY, btn_code, value);
    push_syn(1);
}

int64_t evdev_read(int device_idx, uint8_t* buf, uint64_t len) {
    if (device_idx < 0 || device_idx >= EVDEV_NDEV) return -1;
    return (int64_t)evdev_vfs_read(&evdev_nodes[device_idx], 0, len, buf);
}

int evdev_poll_ready(int device_idx) {
    if (device_idx < 0 || device_idx >= EVDEV_NDEV) return 0;
    return (evdev_head[device_idx] != evdev_tail[device_idx]) ? 1 : 0;
}

int evdev_index_from_name(const char* name) {
    if (!name) return -1;
    /* match "event0" → 0, "event1" → 1 */
    int len = k_strlen(name);
    if (len < 6) return -1;
    const char* tail = name + (len - 6);
    if (tail[0]=='e' && tail[1]=='v' && tail[2]=='e' &&
        tail[3]=='n' && tail[4]=='t') {
        int idx = tail[5] - '0';
        if (idx >= 0 && idx < EVDEV_NDEV) return idx;
    }
    return -1;
}
