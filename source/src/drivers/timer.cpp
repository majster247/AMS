#include "io.h"
#include "kernel.h"

extern "C" {
    uint64_t ticks = 0;

    void timer_init(uint32_t freq) {
        uint32_t divisor = 1193180 / freq;
        outb(0x43, 0x36);
        outb(0x40, divisor & 0xFF);
        outb(0x40, (divisor >> 8) & 0xFF);
    }

    void timer_handler() {
        ticks++;
    }

    uint64_t get_system_ticks() {
        return ticks;
    }
}