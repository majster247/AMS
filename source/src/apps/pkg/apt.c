#include "ams_syscall.h"
#include <stdint.h>

static int streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == 0 && b[i] == 0;
}

static void print_line(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(int argc, char** argv) {
    if (argc < 3 || !streq(argv[1], "install")) {
        print_line("apt: usage: apt install <package>");
        return 1;
    }

    const char* pkg = argv[2];
    print_line("apt: reading package lists...");
    print_line("apt: resolving dependencies...");

    if (streq(pkg, "wayland")) {
        print_line("apt: package wayland selected.");
        print_line("apt: installing wayland smoke client...");
        char* wl_argv[2];
                wl_argv[0] = (char*)"/programs/wayland/wayland_smoke";
        wl_argv[1] = 0;
                int rc = (int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_smoke", 1, (uint64_t)wl_argv, 0, 0);
        if (rc != 0) {
            print_line("apt: install step failed (exec wayland_smoke).");
            return 2;
        }
        print_line("apt: wayland installed (mvp).");
        return 0;
    }

    if (streq(pkg, "mesa")) {
        print_line("apt: package mesa selected.");
        print_line("apt: mesa payload expected at /programs/wayland/mesa.");
        char* egl_argv[2];
        egl_argv[0] = (char*)"/programs/wayland/wayland_egl_smoke";
        egl_argv[1] = 0;
        if ((int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_egl_smoke", 1, (uint64_t)egl_argv, 0, 0) != 0) {
            print_line("apt: mesa verify failed (egl smoke).");
            return 4;
        }
        print_line("apt: mesa installed (staged artifacts).");
        return 0;
    }

    if (streq(pkg, "gcc")) {
        print_line("apt: package gcc selected.");
        print_line("apt: gcc frontend at /tools/compiler/gcc.");
        print_line("apt: host-built toolchain payload at /tools/toolchain.");
        char* g_argv[3];
        g_argv[0] = (char*)"/tools/compiler/gcc";
        g_argv[1] = (char*)"--version";
        g_argv[2] = 0;
        if ((int)ams_syscall(10, (uint64_t)"/tools/compiler/gcc", 2, (uint64_t)g_argv, 0, 0) != 0) {
            print_line("apt: gcc verify failed (gcc --version).");
            return 5;
        }
        print_line("apt: gcc installed (mvp).");
        return 0;
    }

    print_line("apt: package not found in MVP repository.");
    return 3;
}
