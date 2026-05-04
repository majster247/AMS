#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#include <sys/types.h>
#include <stddef.h>
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 32
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void*)-1)

extern "C" void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
extern "C" int munmap(void* addr, size_t length);
extern "C" int mprotect(void* addr, size_t len, int prot);
#endif
