#include "ams_syscall.h"
#include "stdlib.h"  // Dla malloc i free
#include "string.h"  // Dla memset
#include <stdint.h>  // Dla uint64_t

// WAŻNE: To łączy nas z plikiem src/lib/syscall.s
// To ta funkcja wykonuje instrukcję CPU 'syscall'
extern "C" uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);
extern "C" {

void exit(int code) {
    // Dodajemy dwa zera na końcu (p4, p5)
    ams_syscall(60, code, 0, 0, 0, 0); 
    while(1);
}

int sys_exec(const char* path, int argc, char** argv) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(10, (uint64_t)path, (uint64_t)argc, (uint64_t)argv, 0, 0);
}

int write(int fd, const char* buf, int count) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(1, fd, (uint64_t)buf, count, 0, 0);
}

int open(const char* path, int flags) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(2, (uint64_t)path, flags, 0, 0, 0);
}

int close(int fd) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(3, fd, 0, 0, 0, 0);
}

int read(int fd, void* buf, int count) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(0, fd, (uint64_t)buf, count, 0, 0); 
}

long lseek(int fd, long offset, int whence) {
    // Dodajemy dwa zera na końcu
    return (long)ams_syscall(8, fd, (uint64_t)offset, whence, 0, 0); 
}
int unlink(const char* pathname) {
    (void)pathname; 
    return 0; 
}

int get_key() {
    return (int)ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
}

/*
 * mmap: full 6-argument syscall using inline assembly with explicit registers.
 * Linux x86-64 syscall mmap: rax=9, rdi=addr, rsi=len, rdx=prot,
 *                              r10=flags, r8=fd, r9=offset
 */
void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    void* result;
    register uint64_t r10 __asm__("r10") = (uint64_t)(uint32_t)flags;
    register uint64_t r8  __asm__("r8")  = (uint64_t)(uint32_t)fd;
    register uint64_t r9  __asm__("r9")  = (uint64_t)offset;
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "0"(9ULL),
          "D"(addr),
          "S"(length),
          "d"((uint64_t)(uint32_t)prot),
          "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return result;
}

int munmap(void* addr, size_t length) {
    (void)addr;
    (void)length;
    return (int)ams_syscall(11, (uint64_t)addr, (uint64_t)length, 0, 0, 0);
}

/* --- POSIX shared memory --- */
int shm_open(const char* name, int oflag, unsigned int mode) {
    return (int)ams_syscall(462, (uint64_t)name, (uint64_t)oflag, (uint64_t)mode, 0, 0);
}

int shm_unlink(const char* name) {
    return (int)ams_syscall(463, (uint64_t)name, 0, 0, 0, 0);
}

/* --- poll / ppoll wrappers --- */
int poll(struct pollfd_ams* fds, unsigned long nfds, int timeout) {
    return (int)ams_syscall(7, (uint64_t)fds, nfds, (uint64_t)timeout, 0, 0);
}

/* --- Zmienne globalne dla errno (wymagane przez niektóre biblioteki C) --- */
int errno_val = 0;
int* __errno_location() { return &errno_val; }

}