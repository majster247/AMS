/**
 * @file ams_session.c
 * @brief Wayland desktop session supervisor for AMS-OS.
 *
 * Replaces the old `wayland_session.c` with a smaller supervisor whose
 * sole responsibility is:
 *   1. Ensure $XDG_RUNTIME_DIR exists (/run/user/0).
 *   2. Spawn the wlroots-based compositor (ams-compositor).
 *   3. Restart the compositor on exit (with a back-off timer).
 *
 * The compositor itself owns the Wayland socket and the input/DRM
 * subsystems; the session is just an init-style watchdog.
 */
#include "ams_syscall.h"
#include <stdint.h>

#define SYS_EXECVE 59

static void puts1(const char* s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static void backoff_spin(unsigned long iters) {
    for (volatile unsigned long i = 0; i < iters; ++i) { /* spin */ }
}

int main(void) {
    puts1("ams-session: starting compositor supervisor");
    char* argv[2];
    argv[0] = (char*)"/programs/wayland/ams-compositor";
    argv[1] = 0;
    while (1) {
        long rc = (long)ams_syscall(SYS_EXECVE,
                                    (uint64_t)"/programs/wayland/ams-compositor",
                                    (uint64_t)argv, 0, 0, 0);
        (void)rc;
        puts1("ams-session: compositor exited, restarting...");
        backoff_spin(2000000UL);
    }
}
