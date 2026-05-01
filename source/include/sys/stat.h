#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stat {
    uint32_t st_mode;
    uint64_t st_size;
};

int mkdir(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif