#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>  /* <--- TU BYŁ PIES POGRZEBANY (definicja off_t) */
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
int ftruncate(int fd, off_t length);
//int fileno(void *stream); /* To też się przyda dla hello_user */

/* Dodajmy też sbrk, bo często tu szukają */
void* sbrk(intptr_t increment);

#ifdef __cplusplus
}
#endif

#endif
/* Ratunkowe deklaracje dla TCC */
char *getcwd(char *buf, size_t size);
int unlink(const char *pathname);
extern char **environ;