/**
 * @file linux/input.h
 * @brief Linux evdev input event UAPI for AMS.
 *
 * Provides struct input_event and EV_*/KEY_*/BTN_*/REL_*/ABS_* constants
 * needed by libinput and wlroots.
 */
#ifndef _AMS_LINUX_INPUT_H
#define _AMS_LINUX_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

struct input_absinfo {
    int32_t value;
    int32_t minimum;
    int32_t maximum;
    int32_t fuzz;
    int32_t flat;
    int32_t resolution;
};

/* Event types */
#define EV_SYN       0x00
#define EV_KEY       0x01
#define EV_REL       0x02
#define EV_ABS       0x03
#define EV_MSC       0x04
#define EV_SW        0x05
#define EV_LED       0x11
#define EV_SND       0x12
#define EV_REP       0x14
#define EV_FF        0x15
#define EV_PWR       0x16
#define EV_FF_STATUS 0x17
#define EV_MAX       0x1f

/* Synchronization events */
#define SYN_REPORT    0
#define SYN_CONFIG    1
#define SYN_MT_REPORT 2
#define SYN_DROPPED   3

/* Relative axes */
#define REL_X      0x00
#define REL_Y      0x01
#define REL_Z      0x02
#define REL_WHEEL  0x08
#define REL_HWHEEL 0x06
#define REL_MAX    0x0f

/* Absolute axes */
#define ABS_X        0x00
#define ABS_Y        0x01
#define ABS_Z        0x02
#define ABS_MT_SLOT      0x2f
#define ABS_MT_POSITION_X 0x35
#define ABS_MT_POSITION_Y 0x36
#define ABS_MT_TRACKING_ID 0x39
#define ABS_MAX      0x3f

/* Key/button codes (subset) */
#define KEY_RESERVED   0
#define KEY_ESC        1
#define KEY_1          2
#define KEY_2          3
#define KEY_3          4
#define KEY_4          5
#define KEY_5          6
#define KEY_6          7
#define KEY_7          8
#define KEY_8          9
#define KEY_9          10
#define KEY_0          11
#define KEY_MINUS      12
#define KEY_EQUAL      13
#define KEY_BACKSPACE  14
#define KEY_TAB        15
#define KEY_Q          16
#define KEY_W          17
#define KEY_E          18
#define KEY_R          19
#define KEY_T          20
#define KEY_Y          21
#define KEY_U          22
#define KEY_I          23
#define KEY_O          24
#define KEY_P          25
#define KEY_LEFTBRACE  26
#define KEY_RIGHTBRACE 27
#define KEY_ENTER      28
#define KEY_LEFTCTRL   29
#define KEY_A          30
#define KEY_S          31
#define KEY_D          32
#define KEY_F          33
#define KEY_G          34
#define KEY_H          35
#define KEY_J          36
#define KEY_K          37
#define KEY_L          38
#define KEY_SEMICOLON  39
#define KEY_APOSTROPHE 40
#define KEY_GRAVE      41
#define KEY_LEFTSHIFT  42
#define KEY_BACKSLASH  43
#define KEY_Z          44
#define KEY_X          45
#define KEY_C          46
#define KEY_V          47
#define KEY_B          48
#define KEY_N          49
#define KEY_M          50
#define KEY_COMMA      51
#define KEY_DOT        52
#define KEY_SLASH      53
#define KEY_RIGHTSHIFT 54
#define KEY_KPASTERISK 55
#define KEY_LEFTALT    56
#define KEY_SPACE      57
#define KEY_CAPSLOCK   58
#define KEY_F1         59
#define KEY_F2         60
#define KEY_F3         61
#define KEY_F4         62
#define KEY_F5         63
#define KEY_F6         64
#define KEY_F7         65
#define KEY_F8         66
#define KEY_F9         67
#define KEY_F10        68
#define KEY_F11        87
#define KEY_F12        88
#define KEY_UP         103
#define KEY_LEFT       105
#define KEY_RIGHT      106
#define KEY_DOWN       108
#define KEY_DELETE     111
#define KEY_MAX        0x2ff

/* Mouse buttons */
#define BTN_MISC    0x100
#define BTN_0       0x100
#define BTN_MOUSE   0x110
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112
#define BTN_SIDE    0x113
#define BTN_EXTRA   0x114

/* Bus types */
#define BUS_PCI      0x01
#define BUS_ISA      0x02
#define BUS_USB      0x03
#define BUS_I8042    0x11
#define BUS_VIRTUAL  0x06

/* ioctl codes for EVIOCG* */
#define EVIOCGVERSION  _IOR('E', 0x01, int32_t)
#define EVIOCGID       _IOR('E', 0x02, struct input_id)
#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, (len))
#define EVIOCGPHYS(len) _IOC(_IOC_READ, 'E', 0x07, (len))
#define EVIOCGBIT(ev,len) _IOC(_IOC_READ, 'E', 0x20 + (ev), (len))
#define EVIOCGABS(abs)  _IOR('E', 0x40 + (abs), struct input_absinfo)
#define EVIOCGRAB       _IOW('E', 0x90, int32_t)

#ifndef _IOC
#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2
#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT 8
#define _IOC_SIZESHIFT 16
#define _IOC_DIRSHIFT  30
#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U
#define _IOC(dir,type,nr,size) \
    (((dir)<<_IOC_DIRSHIFT)|((type)<<_IOC_TYPESHIFT)|((nr)<<_IOC_NRSHIFT)|((size)<<_IOC_SIZESHIFT))
#define _IO(type,nr)       _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,sz)   _IOC(_IOC_READ,(type),(nr),sizeof(sz))
#define _IOW(type,nr,sz)   _IOC(_IOC_WRITE,(type),(nr),sizeof(sz))
#define _IOWR(type,nr,sz)  _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(sz))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _AMS_LINUX_INPUT_H */
