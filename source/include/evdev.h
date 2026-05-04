/**
 * @file evdev.h
 * @brief AMS-OS evdev input device layer for libinput compatibility
 *
 * Provides a minimal Linux evdev-compatible interface that translates
 * AMS-OS keyboard and mouse events into struct input_event format,
 * readable from /dev/input/event0 (keyboard) and /dev/input/event1 (mouse).
 */

#ifndef AMS_EVDEV_H
#define AMS_EVDEV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linux input event types */
#define EV_SYN     0x00
#define EV_KEY     0x01
#define EV_REL     0x02
#define EV_ABS     0x03

/* Relative axes */
#define REL_X      0x00
#define REL_Y      0x01
#define REL_WHEEL  0x08

/* Absolute axes */
#define ABS_X      0x00
#define ABS_Y      0x01

/* Sync events */
#define SYN_REPORT 0x00

/* Key codes (subset matching PS/2 scancodes) */
#define KEY_ESC    1
#define KEY_1      2
#define KEY_2      3
#define KEY_3      4
#define KEY_4      5
#define KEY_5      6
#define KEY_6      7
#define KEY_7      8
#define KEY_8      9
#define KEY_9      10
#define KEY_0      11
#define KEY_MINUS  12
#define KEY_EQUAL  13
#define KEY_BACKSPACE 14
#define KEY_TAB    15
#define KEY_Q      16
#define KEY_W      17
#define KEY_E      18
#define KEY_R      19
#define KEY_T      20
#define KEY_Y      21
#define KEY_U      22
#define KEY_I      23
#define KEY_O      24
#define KEY_P      25
#define KEY_ENTER  28
#define KEY_A      30
#define KEY_S      31
#define KEY_D      32
#define KEY_F      33
#define KEY_G      34
#define KEY_H      35
#define KEY_J      36
#define KEY_K      37
#define KEY_L      38
#define KEY_SPACE  57
#define KEY_F1     59
#define KEY_F2     60
#define KEY_F3     61
#define KEY_F4     62
#define KEY_UP     103
#define KEY_LEFT   105
#define KEY_RIGHT  106
#define KEY_DOWN   108

/* Mouse buttons */
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/* Input event (matches Linux struct input_event) */
struct input_event {
    int64_t  time_sec;
    int64_t  time_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

/* Input device info for EVIOCGNAME etc. */
struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

#define BUS_USB    0x03
#define BUS_I8042  0x11

/* Evdev ring buffer for kernel->userspace event delivery */
#define EVDEV_RING_SIZE 256

struct evdev_device {
    uint32_t in_use;
    uint32_t dev_type;    /* 0=keyboard, 1=mouse */
    struct input_event ring[EVDEV_RING_SIZE];
    uint32_t head;
    uint32_t tail;
    struct input_id id;
    char name[64];
};

#define EVDEV_MAX_DEVICES 4

extern struct evdev_device g_evdev_devices[EVDEV_MAX_DEVICES];

/* Initialize evdev subsystem */
void evdev_init(void);

/* Push an event into device's ring buffer */
void evdev_push_event(uint32_t dev_idx, uint16_t type, uint16_t code, int32_t value);

/* Push a key event + SYN */
void evdev_push_key(uint32_t scancode, int pressed);

/* Push mouse movement + SYN */
void evdev_push_mouse_rel(int32_t dx, int32_t dy);

/* Push mouse absolute position + SYN */
void evdev_push_mouse_abs(int32_t x, int32_t y);

/* Push mouse button + SYN */
void evdev_push_mouse_button(uint16_t button, int pressed);

/* Read events from userspace (returns bytes read) */
int evdev_read(uint32_t dev_idx, void* buf, uint32_t count);

/* ioctl for device info */
int evdev_ioctl(uint32_t dev_idx, uint64_t request, uint64_t arg);

#ifdef __cplusplus
}
#endif

#endif /* AMS_EVDEV_H */
