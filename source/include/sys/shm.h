#ifndef _SYS_SHM_H
#define _SYS_SHM_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHM_RDONLY  0x1000
#define SHM_RND     0x2000

int shm_open(const char* name, int oflag, mode_t mode);
int shm_unlink(const char* name);

#ifdef __cplusplus
}
#endif

#endif
