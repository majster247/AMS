#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char **environ;

int open(const char* path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char* pathname);
char *getcwd(char *buf, size_t size);
int usleep(unsigned int usec);
void* sbrk(intptr_t increment);

#ifdef __cplusplus
}
#endif

#endif