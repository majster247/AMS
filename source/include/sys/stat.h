#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_IFREG 0100000
#define S_IFDIR 0040000

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t __pad0;
    uint64_t st_rdev;
    uint64_t st_size;
};

int mkdir(const char* path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif