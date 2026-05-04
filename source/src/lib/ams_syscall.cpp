#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "poll.h"
#include "sys/epoll.h"
#include "sys/socket.h"
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

int sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return (int)ams_syscall(SYS_SENDMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

int recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return (int)ams_syscall(SYS_RECVMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

int read(int fd, void* buf, int count) {
    // Dodajemy dwa zera na końcu
    return (int)ams_syscall(0, fd, (uint64_t)buf, count, 0, 0); 
}

int poll(struct pollfd* fds, unsigned long nfds, int timeout) {
    return (int)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout, 0, 0);
}

int epoll_create1(int flags) {
    return (int)ams_syscall(SYS_EPOLL_CREATE1, (uint64_t)flags, 0, 0, 0, 0);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {
    return (int)ams_syscall(SYS_EPOLL_CTL, (uint64_t)epfd, (uint64_t)op, (uint64_t)fd, (uint64_t)event, 0);
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    return (int)ams_syscall(SYS_EPOLL_WAIT, (uint64_t)epfd, (uint64_t)events, (uint64_t)maxevents, (uint64_t)timeout, 0);
}

int socket(int domain, int type, int protocol) {
    return (int)ams_syscall(SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol, 0, 0);
}

int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int listen(int sockfd, int backlog) {
    return (int)ams_syscall(SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog, 0, 0, 0);
}

int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen) {
    return (int)ams_syscall(SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int shutdown(int sockfd, int how) {
    return (int)ams_syscall(SYS_SHUTDOWN, (uint64_t)sockfd, (uint64_t)how, 0, 0, 0);
}

int eventfd(unsigned int initval, int flags) {
    return (int)ams_syscall(SYS_EVENTFD2, (uint64_t)initval, (uint64_t)flags, 0, 0, 0);
}

int shm_open(const char* name, int oflag, unsigned int mode) {
    (void)mode;
    // AMS VFS is flat; emulate POSIX shm namespace using memfd_create.
    if (name && name[0] == '/' && name[1]) {
        return (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)(name + 1), 0, 0, 0, 0);
    }
    return (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)(name ? name : "shm"), 0, 0, 0, 0);
}

int shm_unlink(const char* name) {
    (void)name;
    // memfd objects are reference-counted; explicit unlink is a no-op here.
    return 0;
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

// Mmap na razie symulujemy malloc'iem (dla user space to bez różnicy na tym etapie)
void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    // 9 = SYS_MMAP (zgodnie z Linuxem)
    return (void*)ams_syscall(9, (uint64_t)addr, length, prot, flags, (uint64_t)fd);
}

int munmap(void* addr, size_t length) {
    (void)length;
    free(addr);
    return 0;
}

// Zmienne globalne dla errno (wymagane przez niektóre biblioteki C)
int errno_val = 0;
int* __errno_location() { return &errno_val; }

}