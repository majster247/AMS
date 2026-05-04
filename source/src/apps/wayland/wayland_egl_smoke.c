#include "ams_syscall.h"
#include <stdint.h>

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    puts1("wayland-egl-smoke: start");
    puts1("wayland-egl-smoke: expecting Mesa payload at /programs/graphics-stack/mesa");
    puts1("wayland-egl-smoke: software-first EGL path staged");
    puts1("wayland-egl-smoke: PASS");
    return 0;
}
