#include "ams_syscall.h"
#include <stdint.h>
#include <string.h>

extern int sys_exec(const char* path, int argc, char** argv);

static void print_line(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        // Preferred path: execute real GCC payload staged inside AMS.
        int rc_native = sys_exec("/tools/toolchain/bin/x86_64-elf-gcc", argc, argv);
        if (rc_native == 0) return 0;

        // Fallback path: keep compatibility while ABI gaps are being closed.
        if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
            print_line("gcc (AMS compatibility shim) 17.0.0");
            print_line("native payload path: /tools/toolchain/bin/x86_64-elf-gcc");
            print_line("fallback active: ABI/loader path still incomplete");
            return 0;
        }
    }

    print_line("gcc: trying compatibility compile via tcc -run");
    if (argc >= 2) {
        char* tcc_argv[5];
        tcc_argv[0] = (char*)"/tools/compiler/tcc";
        tcc_argv[1] = (char*)"-run";
        tcc_argv[2] = (char*)"-nostdlib";
        tcc_argv[3] = argv[1];
        tcc_argv[4] = 0;
        int tr = sys_exec("/tools/compiler/tcc", 4, tcc_argv);
        if (tr == 0) return 0;
    }

    print_line("usage: gcc --version | gcc <file.c>");
    return 1;
}
