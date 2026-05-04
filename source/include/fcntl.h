#ifndef _FCNTL_H
#define _FCNTL_H
#include <sys/types.h>

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   64
#define O_TRUNC   512
#define O_BINARY  0 
#define O_CLOEXEC 02000000
#define O_NONBLOCK 04000

#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

#define FD_CLOEXEC 1

#ifdef __cplusplus
extern "C" {
#endif

int fcntl(int fd, int cmd, ...);
int openat(int dirfd, const char* path, int flags, ...);

#ifdef __cplusplus
}
#endif
#endif
