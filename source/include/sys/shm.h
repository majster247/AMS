#ifndef _AMS_SYS_SHM_H
#define _AMS_SYS_SHM_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * POSIX shm_open / shm_unlink (subset).
 * AMS implementation lives in src/lib/posix/shm.c. Names follow POSIX:
 * a leading '/' is required, the rest must not contain '/'. The shim
 * keeps a small in-process directory of (name -> memfd) mappings so that
 * the second shm_open() of the same name returns the same descriptor.
 */
int shm_open(const char *name, int oflag, unsigned int mode);
int shm_unlink(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_SYS_SHM_H */
