#include "io.h"
#include "kernel.h"

extern "C" {
    uint64_t ticks = 0;

    void timer_init(uint32_t frequency) {
        uint32_t divisor = 1193180 / frequency;
        outb(0x43, 0x36);             // Command byte
        outb(0x40, (uint8_t)(divisor & 0xFF));      // Low byte
        outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
    }

    void timer_handler() {
        ticks++;
    }

    uint64_t get_system_ticks() {
        return ticks;
    }
}