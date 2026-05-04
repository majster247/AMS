#include "ams_syscall.h"
#include <sys/shm.h>
#include <stdint.h>

static void puts1(const char *s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    int fd1 = shm_open("/ams-shm-smoke", 0, 0);
    int fd2 = shm_open("/ams-shm-smoke", 0, 0);
    if (fd1 < 0 || fd2 < 0) { puts1("shm_smoke: open fail"); return 1; }
    if (fd1 != fd2) { puts1("shm_smoke: identity fail"); return 2; }
    if (shm_unlink("/ams-shm-smoke") != 0) { puts1("shm_smoke: unlink fail"); return 3; }
    puts1("shm_smoke: PASS");
    return 0;
}
