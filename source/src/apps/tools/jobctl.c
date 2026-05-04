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

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        puts1("jobctl usage:");
        puts1("  jobctl test-wayland");
        puts1("  jobctl start-wayland");
        puts1("  jobctl ping-wayland");
        puts1("  jobctl stage-gcc");
        puts1("  jobctl gcc-version");
        puts1("  jobctl bash-version");
        puts1("  jobctl egl-smoke");
        puts1("  jobctl smoke-all");
        puts1("  jobctl gcc-native-version");
        puts1("  jobctl gcc-hello");
        puts1("  jobctl toolchain-status");
        puts1("  jobctl run-c <file.c>");
        return 1;
    }

    if (streq(argv[1], "test-wayland")) {
        char* wargv[2];
        wargv[0] = (char*)"/programs/wayland/wayland_smoke";
        wargv[1] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_smoke", 1, (uint64_t)wargv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: wayland smoke failed");
            return 2;
        }
        puts1("jobctl: wayland smoke passed");
        return 0;
    }

    if (streq(argv[1], "run-c")) {
        if (argc < 3) {
            puts1("jobctl: run-c requires input file");
            return 3;
        }
        char* targv[5];
        targv[0] = (char*)"/tools/compiler/tcc";
        targv[1] = (char*)"-run";
        targv[2] = (char*)"-nostdlib";
        targv[3] = argv[2];
        targv[4] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/compiler/tcc", 4, (uint64_t)targv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: run-c failed");
            return 4;
        }
        puts1("jobctl: run-c finished");
        return 0;
    }

    if (streq(argv[1], "start-wayland")) {
        char* argv2[2];
        argv2[0] = (char*)"/programs/wayland/wlroots-compositor";
        argv2[1] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/programs/wayland/wlroots-compositor", 1, (uint64_t)argv2, 0, 0);
        if (rc != 0) {
            puts1("jobctl: start-wayland failed");
            return 6;
        }
        puts1("jobctl: start-wayland finished");
        return 0;
    }

    if (streq(argv[1], "ping-wayland")) {
        char* argv2[2];
        argv2[0] = (char*)"/programs/wayland/wayland-smoke-client";
        argv2[1] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland-smoke-client", 1, (uint64_t)argv2, 0, 0);
        if (rc != 0) {
            puts1("jobctl: ping-wayland failed");
            return 7;
        }
        puts1("jobctl: ping-wayland finished");
        return 0;
    }

    if (streq(argv[1], "egl-smoke")) {
        char* argv2[2];
        argv2[0] = (char*)"/programs/wayland/wayland_egl_smoke";
        argv2[1] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_egl_smoke", 1, (uint64_t)argv2, 0, 0);
        if (rc != 0) {
            puts1("jobctl: egl-smoke failed");
            return 10;
        }
        puts1("jobctl: egl-smoke passed");
        return 0;
    }

    if (streq(argv[1], "stage-gcc")) {
        puts1("jobctl: gcc stage placeholder");
        puts1("jobctl: validating toolchain path via guest tcc run");
        char* targv[5];
        targv[0] = (char*)"/tools/compiler/tcc";
        targv[1] = (char*)"-run";
        targv[2] = (char*)"-nostdlib";
        targv[3] = (char*)"/tests/tcc/t1_basic.c";
        targv[4] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/compiler/tcc", 4, (uint64_t)targv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: stage-gcc failed (toolchain gate)");
            return 8;
        }
        puts1("jobctl: stage-gcc gate passed");
        return 0;
    }

    if (streq(argv[1], "gcc-version")) {
        char* gargv[3];
        gargv[0] = (char*)"/tools/compiler/gcc";
        gargv[1] = (char*)"--version";
        gargv[2] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/compiler/gcc", 2, (uint64_t)gargv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: gcc-version failed");
            return 9;
        }
        puts1("jobctl: gcc-version ok");
        return 0;
    }

    if (streq(argv[1], "gcc-native-version")) {
        char* gargv[3];
        gargv[0] = (char*)"/tools/toolchain/bin/x86_64-elf-gcc";
        gargv[1] = (char*)"--version";
        gargv[2] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/toolchain/bin/x86_64-elf-gcc", 2, (uint64_t)gargv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: gcc-native-version failed");
            return 11;
        }
        puts1("jobctl: gcc-native-version ok");
        return 0;
    }

    if (streq(argv[1], "bash-version")) {
        char* bargv[3];
        bargv[0] = (char*)"/tools/system/bash";
        bargv[1] = (char*)"--version";
        bargv[2] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/system/bash", 2, (uint64_t)bargv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: bash-version failed");
            return 12;
        }
        puts1("jobctl: bash-version ok");
        return 0;
    }

    if (streq(argv[1], "smoke-all")) {
        char* cmd1[2] = { (char*)"/programs/wayland/wayland_smoke", 0 };
        char* cmd2[2] = { (char*)"/programs/wayland/wayland_egl_smoke", 0 };
        char* cmd3[3] = { (char*)"/tools/compiler/gcc", (char*)"--version", 0 };
        if ((int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_smoke", 1, (uint64_t)cmd1, 0, 0) != 0) return 13;
        if ((int)ams_syscall(10, (uint64_t)"/programs/wayland/wayland_egl_smoke", 1, (uint64_t)cmd2, 0, 0) != 0) return 14;
        if ((int)ams_syscall(10, (uint64_t)"/tools/compiler/gcc", 2, (uint64_t)cmd3, 0, 0) != 0) return 15;
        puts1("jobctl: smoke-all passed");
        return 0;
    }

    if (streq(argv[1], "toolchain-status")) {
        puts1("jobctl: expected in-system payload:");
        puts1("  /tools/toolchain/bin");
        puts1("  /tools/toolchain/lib");
        puts1("  /programs/wayland/mesa");
        return 0;
    }

    if (streq(argv[1], "gcc-hello")) {
        char* gargv[3];
        gargv[0] = (char*)"gcc";
        gargv[1] = (char*)"/tests/toolchain/hello.c";
        gargv[2] = 0;
        int rc = (int)ams_syscall(10, (uint64_t)"/tools/compiler/gcc", 2, (uint64_t)gargv, 0, 0);
        if (rc != 0) {
            puts1("jobctl: gcc-hello failed");
            return 16;
        }
        puts1("jobctl: gcc-hello passed");
        return 0;
    }

    puts1("jobctl: unknown task");
    return 5;
}
