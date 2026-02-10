// src/lib/ams_syscall.cpp
#include "ams_syscall.h"
#include "stdlib.h"  // Dla malloc i free
#include "string.h"  // Dla memset

// Funkcja pomocnicza do ASM
inline uint64_t syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3) {
    uint64_t ret;
    asm volatile (
        "int $0x80" 
        : "=a"(ret)
        : "a"(sys_num), "D"(p1), "S"(p2), "d"(p3)
        : "memory"
    );
    return ret;
}

extern "C" {

void exit(int code) {
    syscall(60, code, 0, 0);
    while(1);
}

int sys_exec(const char* path, int argc, char** argv) {
    // Zakładam, że EXEC ma numer np. 10 (sprawdź w syscall.cpp w jądrze)
    return syscall(10, (uint64_t)path, (uint64_t)argc, (uint64_t)argv);
}

int write(int fd, const char* buf, int count) {
    return (int)syscall(1, fd, (uint64_t)buf, count);
}

int open(const char* path, int flags) {
    return (int)syscall(2, (uint64_t)path, flags, 0);
}

int close(int fd) {
    return (int)syscall(3, fd, 0, 0);
}

int read(int fd, void* buf, int count) {
    return (int)syscall(0, fd, (uint64_t)buf, count); // Syscall 0 = SYS_READ
}

// To będzie potrzebne dla malloc!
// sbrk przesuwa koniec sterty procesu
void* sbrk(intptr_t increment) {
    // Syscall 12 to u nas brk/sbrk (musimy go dodać w kernelu!)
    // Na razie załóżmy, że go dodamy za chwilę.
    return (void*)syscall(12, increment, 0, 0);
}


long lseek(int fd, long offset, int whence) {
    // Syscall 8 to lseek. Przekazujemy tylko 3 parametry: fd, offset, whence.
    return (long)syscall(8, fd, (uint64_t)offset, whence); 
}

int unlink(const char* pathname) {
    (void)pathname; // Wyłącz ostrzeżenie o nieużytym parametrze
    return 0; 
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    
    void* ptr = malloc(length);
    if (!ptr) return (void*)-1;
    
    memset(ptr, 0, length);
    return ptr;
}

int munmap(void* addr, size_t length) {
    (void)length;
    free(addr);
    return 0;
}

int errno_val = 0;
int* __errno_location() { return &errno_val; }

}