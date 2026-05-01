#include "ams_syscall.h"
#include <stdint.h>
#include <string.h>

static void print_line(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "--version")) {
        print_line("GNU bash, version 5.2.37(ams-mvp)");
        return 0;
    }

    print_line("bash: AMS compatibility shell entrypoint (MVP).");
    print_line("Run commands from AMS terminal, or use: bash --version");
    (void)argv;
    return 0;
}
