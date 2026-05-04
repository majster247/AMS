#include "ams_syscall.h"
#include <libinput/libinput.h>
#include <stdint.h>

static void puts1(const char *s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static int open_restricted(const char *path, int flags, void *u) {
    (void)path; (void)flags; (void)u;
    return 0;
}
static void close_restricted(int fd, void *u) { (void)fd; (void)u; }

int main(void) {
    static const struct libinput_interface iface = {
        open_restricted, close_restricted
    };
    libinput *li = libinput_path_create_context(&iface, 0);
    if (!li) { puts1("libinput_smoke: ctx fail"); return 1; }
    libinput_path_add_device(li, "/dev/ams/keyboard0");
    libinput_path_add_device(li, "/dev/ams/mouse0");
    int produced = libinput_dispatch(li);
    (void)produced;
    libinput_event *ev;
    while ((ev = libinput_get_event(li)) != 0) {
        libinput_event_destroy(ev);
    }
    libinput_unref(li);
    puts1("libinput_smoke: PASS");
    return 0;
}
