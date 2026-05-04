/*
 * AMS sysdeps shim for mlibc.
 *
 * mlibc uses sysdeps/<platform>/ to bridge libc to a kernel ABI. This
 * file provides those bridge symbols against AMS' ams_syscall(...).
 *
 * It is NOT linked by the regular AMS apps yet (they use src/lib).
 * It exists so a follow-up `meson` cross-build of mlibc against
 * external/mlibc-stage can link successfully.
 */

#include <stdint.h>
#include <stddef.h>
#include "ams_syscall.h"
#include "linux_syscalls.h"

typedef long ssize_t;
typedef long off_t;
typedef long time_t;

int sys_open(const char *path, int flags, int mode, int *fd_out) {
    int fd = (int)ams_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode, 0, 0);
    if (fd < 0) return -fd;
    *fd_out = fd;
    return 0;
}

int sys_close(int fd) {
    return (int)ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

int sys_read(int fd, void *buf, size_t len, ssize_t *out) {
    long rc = (long)ams_syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)len, 0, 0);
    if (rc < 0) return (int)-rc;
    *out = (ssize_t)rc;
    return 0;
}

int sys_write(int fd, const void *buf, size_t len, ssize_t *out) {
    long rc = (long)ams_syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)len, 0, 0);
    if (rc < 0) return (int)-rc;
    *out = (ssize_t)rc;
    return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
    long rc = (long)ams_syscall(SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0);
    if (rc < 0) return (int)-rc;
    *new_offset = (off_t)rc;
    return 0;
}

int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
    void *p = (void*)ams_syscall(SYS_MMAP, (uint64_t)hint, (uint64_t)size, (uint64_t)prot, (uint64_t)flags, (uint64_t)fd);
    if ((uint64_t)p > (uint64_t)-4096LL) return (int)-(uint64_t)p;
    *window = p;
    (void)offset;
    return 0;
}

int sys_vm_unmap(void *addr, size_t size) {
    return (int)ams_syscall(SYS_MUNMAP, (uint64_t)addr, (uint64_t)size, 0, 0, 0);
}

int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window);

int sys_anon_allocate(size_t size, void **window) {
    return sys_vm_map(0, size, 3, 0x22, -1, 0, window);
}

void sys_exit(int status) {
    ams_syscall(SYS_EXIT, (uint64_t)status, 0, 0, 0, 0);
    while (1) {}
}

int sys_clock_get(int clock, time_t *secs, long *nanos) {
    struct ts { int64_t s; int64_t ns; } t;
    long rc = (long)ams_syscall(SYS_CLOCK_GETTIME, (uint64_t)clock, (uint64_t)&t, 0, 0, 0);
    if (rc < 0) return (int)-rc;
    if (secs)  *secs  = (time_t)t.s;
    if (nanos) *nanos = (long)t.ns;
    return 0;
}
