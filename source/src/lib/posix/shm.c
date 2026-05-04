/*
 * AMS POSIX shm_open / shm_unlink (subset).
 *
 * Backed by memfd_create + ftruncate. Names are stored in an in-process
 * lookup table so that opening the same name twice returns the same fd.
 *
 * Limitations vs glibc:
 *  - per-process registry (no system-wide /dev/shm). Cross-process sharing
 *    is solved via SCM_RIGHTS (already used by Wayland code).
 *  - mode argument is ignored beyond presence.
 */

#include <stdint.h>
#include <stddef.h>
#include "ams_syscall.h"
#include "linux_syscalls.h"
#include <sys/shm.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#define AMS_SHM_MAX_ENTRIES 32
#define AMS_SHM_NAME_MAX    63

struct ams_shm_entry {
    int fd;
    char name[AMS_SHM_NAME_MAX + 1];
};

static struct ams_shm_entry g_shm_table[AMS_SHM_MAX_ENTRIES];

static int ams_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

static size_t ams_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void ams_strcpy_safe(char *dst, const char *src, size_t cap) {
    size_t i = 0;
    if (cap == 0) return;
    while (i + 1 < cap && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

static int validate_name(const char *name) {
    if (!name) return 0;
    if (name[0] != '/') return 0;
    if (name[1] == '\0') return 0;
    for (size_t i = 1; name[i]; ++i) {
        if (name[i] == '/') return 0;
    }
    return 1;
}

int shm_open(const char *name, int oflag, unsigned int mode) {
    (void)oflag;
    (void)mode;
    if (!validate_name(name)) return -1;
    if (ams_strlen(name) > AMS_SHM_NAME_MAX) return -1;

    for (int i = 0; i < AMS_SHM_MAX_ENTRIES; ++i) {
        if (g_shm_table[i].fd > 0 && ams_strcmp(g_shm_table[i].name, name) == 0) {
            return g_shm_table[i].fd;
        }
    }

    int fd = (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)(name + 1),
                              MFD_CLOEXEC, 0, 0, 0);
    if (fd < 0) return fd;

    for (int i = 0; i < AMS_SHM_MAX_ENTRIES; ++i) {
        if (g_shm_table[i].fd <= 0) {
            g_shm_table[i].fd = fd;
            ams_strcpy_safe(g_shm_table[i].name, name, sizeof(g_shm_table[i].name));
            return fd;
        }
    }

    return fd;
}

int shm_unlink(const char *name) {
    if (!validate_name(name)) return -1;
    for (int i = 0; i < AMS_SHM_MAX_ENTRIES; ++i) {
        if (g_shm_table[i].fd > 0 && ams_strcmp(g_shm_table[i].name, name) == 0) {
            g_shm_table[i].fd = 0;
            g_shm_table[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}
