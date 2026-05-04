#include "ams_syscall.h"
#include <stdint.h>

#define SYS_POLL 7
#define SYS_WRITE 1

struct linux_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(SYS_WRITE, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(SYS_WRITE, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    puts1("wlroots-compositor: bootstrap start");
    puts1("wlroots-compositor: using AF_UNIX Wayland socket path /run/user/0/wayland-0");
    puts1("wlroots-compositor: expected stack = libdrm(GEM/TTM/KMS) + mesa(EGL/GBM) + wlroots");
    puts1("wlroots-compositor: required runtime helpers = libinput + pixman/cairo + wayland-scanner");
    puts1("wlroots-compositor: entering supervisor wait loop");

    // Keep process alive as a supervisor placeholder until native wlroots
    // binary from staged toolchain is available in AMS userspace.
    while (1) {
        struct linux_pollfd pfd;
        pfd.fd = -1;
        pfd.events = 0;
        pfd.revents = 0;
        (void)ams_syscall(SYS_POLL, (uint64_t)&pfd, 1, 200, 0, 0);
    }

    return 0;
}
