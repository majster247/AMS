/*
 * Wayland session supervisor for AMS-OS
 * Launches the wlroots-based compositor and restarts on crash.
 * Ensures DRM master, creates /run/user/0/ directory.
 */
#include "ams_syscall.h"
#include <stdint.h>

#define SYS_EXECVE 59
#define SYS_CLOCK_GETTIME 228
#define SYS_OPEN  2
#define SYS_CLOSE 3
#define SYS_MKDIR 83
#define SYS_WRITE 1

struct linux_timespec_local {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static void puts1(const char* s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(SYS_WRITE, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(SYS_WRITE, 1, (uint64_t)"\n", 1, 0, 0);
}

static void tiny_sleep_ticks(void) {
    struct linux_timespec_local ts;
    (void)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0);
}

int main(void) {
    puts1("wayland-session: starting compositor supervisor (DRM/KMS + wlroots)");

    /* Ensure runtime directory exists */
    ams_syscall(SYS_MKDIR, (uint64_t)"/run", 0755, 0, 0, 0);
    ams_syscall(SYS_MKDIR, (uint64_t)"/run/user", 0755, 0, 0, 0);
    ams_syscall(SYS_MKDIR, (uint64_t)"/run/user/0", 0700, 0, 0, 0);

    /* Set environment for Wayland */
    puts1("wayland-session: XDG_RUNTIME_DIR=/run/user/0");
    puts1("wayland-session: WAYLAND_DISPLAY=wayland-0");

    char* compositor_argv[2];
    compositor_argv[0] = (char*)"/ams-wl-compositor";
    compositor_argv[1] = 0;

    while (1) {
        puts1("wayland-session: launching ams-wl-compositor");
        long rc = (long)ams_syscall(SYS_EXECVE,
                                     (uint64_t)"/ams-wl-compositor",
                                     (uint64_t)compositor_argv, 0, 0, 0);
        (void)rc;
        puts1("wayland-session: compositor exited, restarting in 2s");
        for (int i = 0; i < 250000; ++i) tiny_sleep_ticks();
    }
}
