#ifndef _AMS_LINUX_INPUT_H
#define _AMS_LINUX_INPUT_H

#include <stdint.h>

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

#define EV_SYN       0x00
#define EV_KEY       0x01
#define EV_REL       0x02
#define EV_ABS       0x03
#define EV_MSC       0x04

#define SYN_REPORT   0

#define REL_X        0x00
#define REL_Y        0x01
#define REL_WHEEL    0x08

#define ABS_X        0x00
#define ABS_Y        0x01

#define BTN_LEFT     0x110
#define BTN_RIGHT    0x111
#define BTN_MIDDLE   0x112
#define BTN_MOUSE    0x110

#define KEY_MAX      0x2FF
#define REL_MAX      0x0F
#define ABS_MAX      0x3F
#define EV_MAX       0x1F

#define BUS_USB       0x03
#define BUS_VIRTUAL   0x06

/* ioctls */
#define EVIOCGVERSION  0x80044501u
#define EVIOCGID       0x80084502u
#define EVIOCGNAME(len) (0x80004506u | ((uint32_t)(len) << 16))
#define EVIOCGBIT(ev,len) (0x80004520u | ((uint32_t)(ev) << 0) | ((uint32_t)(len) << 16))
#define EVIOCGABS(abs) (0x80184540u | (uint32_t)(abs))
#define EVIOCGRAB      0x40044590u

#endif
