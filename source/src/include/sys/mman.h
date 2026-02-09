#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 32
#define MAP_FAILED ((void*)-1)

extern "C" void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
#endif
