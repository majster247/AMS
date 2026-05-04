#ifndef _DIRENT_H
#define _DIRENT_H

#include <stdint.h>

#define DT_UNKNOWN 0
#define DT_REG     8
#define DT_DIR     4
#define DT_LNK    10

struct dirent {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
};

typedef struct {
    int fd;
    int pos;
} DIR;

#ifdef __cplusplus
extern "C" {
#endif

DIR*           opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int            closedir(DIR* dirp);

#ifdef __cplusplus
}
#endif

#endif
