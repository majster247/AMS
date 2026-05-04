/**
 * @file libports_shm.c
 * @brief shm_open / shm_unlink shim for the AMS-OS userspace.
 *
 * The Wayland stack (libwayland-server's wl_shm, mesa's drisw_alloc, and
 * wlroots' wlr_shm_alloc) all reach for shm_open() to obtain an
 * anonymous, mappable file descriptor that they can ftruncate() and
 * mmap(MAP_SHARED). Linux exposes shm_open as a thin wrapper around
 * openat(/dev/shm/...). AMS-OS instead uses memfd_create() because it
 * doesn't have a tmpfs at /dev/shm. Both shapes give Wayland exactly
 * what it needs: an fd that can be sent across SCM_RIGHTS, mmap'd
 * shared, and torn down via close().
 */
#include "libports/libports.h"
#include "ams_syscall.h"
#include "linux_syscalls.h"

/* MFD_CLOEXEC | MFD_ALLOW_SEALING - inert flags here, kernel ignores. */
#define AMS_MFD_CLOEXEC      0x0001U
#define AMS_MFD_ALLOW_SEAL   0x0002U

int ams_shm_open(const char* name, int oflag, unsigned int mode) {
    (void)oflag; (void)mode;
    /* memfd_create is the universal shape under AMS-OS. */
    long rc = (long)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)name,
                                AMS_MFD_CLOEXEC, 0, 0, 0);
    return (int)rc;
}

int ams_shm_unlink(const char* name) {
    (void)name;
    /* memfds don't have a path; nothing to unlink. */
    return 0;
}

/* Glibc-compatible aliases so Wayland/Mesa link unchanged. */
int shm_open(const char* name, int oflag, unsigned int mode)
    __attribute__((weak, alias("ams_shm_open")));
int shm_unlink(const char* name)
    __attribute__((weak, alias("ams_shm_unlink")));
