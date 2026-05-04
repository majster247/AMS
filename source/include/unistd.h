#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int open(const char* path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char* pathname);
int usleep(unsigned int usec);

char* getcwd(char* buf, size_t size);
void* sbrk(intptr_t increment);
extern char** environ;

#ifdef __cplusplus
}
#endif

#endif