#include "ams_libinput_shim.h"
#include "ams_syscall.h"

void ams_libinput_init(void) {}

int ams_libinput_poll(struct ams_input_event* out) {
    if (!out) return 0;
    uint64_t ev = ams_syscall(SYS_AMS_GET_MOUSE_EVENT, 0, 0, 0, 0, 0);
    if (ev != 0) {
        int16_t dx = (int16_t)(ev & 0xFFFF);
        int16_t dy = (int16_t)((ev >> 16) & 0xFFFF);
        uint32_t buttons = (uint32_t)(ev >> 32);
        out->type = AMS_INPUT_POINTER;
        out->time = 0;
        out->dx = dx;
        out->dy = dy;
        out->buttons = buttons;
        out->key_code = 0;
        return 1;
    }
    int k = get_key();
    if (k != 0) {
        out->type = AMS_INPUT_KEY;
        out->time = 0;
        out->key_code = k;
        out->dx = 0;
        out->dy = 0;
        out->buttons = 0;
        return 1;
    }
    out->type = AMS_INPUT_NONE;
    return 0;
}
