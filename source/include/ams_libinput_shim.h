#pragma once
#include <stdint.h>

/* Minimal libinput-like surface over AMS input syscalls (SYS_AMS_GET_MOUSE_EVENT, SYS_AMS_GET_KEY). */

enum ams_input_event_type {
    AMS_INPUT_NONE = 0,
    AMS_INPUT_KEY,
    AMS_INPUT_POINTER,
};

struct ams_input_event {
    uint32_t type;
    uint32_t time;
    int32_t key_code;
    int32_t dx;
    int32_t dy;
    uint32_t buttons;
};

void ams_libinput_init(void);
int ams_libinput_poll(struct ams_input_event* out);
