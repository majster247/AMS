#include "ams_syscall.h"
#include "stdlib.h"  // Dla malloc i free
#include "string.h"  // Dla memset
#include <stdint.h>  // Dla uint64_t

// WAŻNE: To łączy nas z plikiem src/lib/syscall.s
// To ta funkcja wykonuje instrukcję CPU 'syscall'
extern "C" uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);
extern "C" uint64_t ams_syscall6(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6);
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

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)ams_syscall6(9, (uint64_t)addr, length, prot, flags, (uint64_t)fd, (uint64_t)offset);
}

int munmap(void* addr, size_t length) {
    (void)length;
    free(addr);
    return 0;
}

int ioctl(int fd, unsigned long request, void* arg) {
    return (int)ams_syscall(16, fd, request, (uint64_t)arg, 0, 0);
}

int shm_open(const char* name, int oflag, unsigned int mode) {
    char path[256];
    const char* prefix = "/dev/shm";
    int i = 0;
    while (prefix[i]) { path[i] = prefix[i]; i++; }
    if (name[0] != '/') path[i++] = '/';
    int j = 0;
    while (name[j] && i < 254) { path[i++] = name[j++]; }
    path[i] = '\0';
    (void)mode;
    return (int)ams_syscall(2, (uint64_t)path, oflag | 0x40 /* O_CREAT */, 0, 0, 0);
}

int shm_unlink(const char* name) {
    (void)name;
    return 0;
}

int memfd_create(const char* name, unsigned int flags) {
    return (int)ams_syscall(319, (uint64_t)name, flags, 0, 0, 0);
}

int ftruncate(int fd, long length) {
    return (int)ams_syscall(77, fd, (uint64_t)length, 0, 0, 0);
}

int errno_val = 0;
int* __errno_location() { return &errno_val; }

}