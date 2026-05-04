#ifndef _SYS_SHM_H
#define _SYS_SHM_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int shm_open(const char* name, int oflag, mode_t mode);
int shm_unlink(const char* name);

#ifdef __cplusplus
}
#endif

#endif
