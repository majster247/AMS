#include "ams_syscall.h"
#include <stdint.h>

#define SYS_EXECVE 59
#define SYS_CLOCK_GETTIME 228

struct linux_timespec_local {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static void tiny_sleep_ticks(void) {
    struct linux_timespec_local ts;
    (void)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0);
}

int main(void) {
    char* compositor_argv[2];
    compositor_argv[0] = (char*)"/ams-wl-compositor";
    compositor_argv[1] = 0;

    puts1("wayland-session: starting compositor supervisor");
    while (1) {
        long rc = (long)ams_syscall(SYS_EXECVE, (uint64_t)"/ams-wl-compositor", (uint64_t)compositor_argv, 0, 0, 0);
        (void)rc;
        puts1("wayland-session: compositor exited, restarting");
        for (int i = 0; i < 250000; ++i) tiny_sleep_ticks();
    }
}
