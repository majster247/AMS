#include "ams_syscall.h"

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    puts1("ams-wl-compositor: legacy in-tree compositor was removed.");
    puts1("ams-wl-compositor: use the staged wlroots compositor port instead.");
    puts1("ams-wl-compositor: run `make wlroots_stage wayland_port_preflight` on the host.");
    return 78;
}
