#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "stdlib.h"  // Dla malloc i free
#include "string.h"  // Dla memset
#include <stdint.h>  // Dla uint64_t

struct linux_iovec_local {
    void* iov_base;
    uint64_t iov_len;
};

struct linux_msghdr_local {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    void* msg_iov;
    uint64_t msg_iovlen;
    void* msg_control;
    uint64_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct linux_pollfd_local {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct linux_epoll_event_local {
    uint32_t events;
    uint64_t data;
} __attribute__((packed));

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
    return (int)ams_syscall(SYS_UNLINK, (uint64_t)pathname, 0, 0, 0, 0);
}

int get_key() {
    return (int)ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    (void)offset;
    return (void*)ams_syscall(SYS_MMAP, (uint64_t)addr, length, prot, flags, (uint64_t)fd);
}

int munmap(void* addr, size_t length) {
    return (int)ams_syscall(SYS_MUNMAP, (uint64_t)addr, length, 0, 0, 0);
}

int mprotect(void* addr, size_t length, int prot) {
    return (int)ams_syscall(SYS_MPROTECT, (uint64_t)addr, length, prot, 0, 0);
}

int socket(int domain, int type, int protocol) {
    return (int)ams_syscall(SYS_SOCKET, domain, type, protocol, 0, 0);
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    return (int)ams_syscall(SYS_SOCKETPAIR, domain, type, protocol, (uint64_t)sv, 0);
}

int bind(int fd, const void* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_BIND, fd, (uint64_t)addr, addrlen, 0, 0);
}

int listen(int fd, int backlog) {
    return (int)ams_syscall(SYS_LISTEN, fd, backlog, 0, 0, 0);
}

int connect(int fd, const void* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_CONNECT, fd, (uint64_t)addr, addrlen, 0, 0);
}

int accept(int fd, void* addr, unsigned int* addrlen) {
    return (int)ams_syscall(SYS_ACCEPT, fd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int accept4(int fd, void* addr, unsigned int* addrlen, int flags) {
    return (int)ams_syscall(SYS_ACCEPT4, fd, (uint64_t)addr, (uint64_t)addrlen, flags, 0);
}

int shutdown(int fd, int how) {
    return (int)ams_syscall(SYS_SHUTDOWN, fd, how, 0, 0, 0);
}

int getsockname(int fd, void* addr, unsigned int* addrlen) {
    return (int)ams_syscall(SYS_GETSOCKNAME, fd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int getpeername(int fd, void* addr, unsigned int* addrlen) {
    return (int)ams_syscall(SYS_GETPEERNAME, fd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

long sendmsg(int fd, const void* msg, int flags) {
    return (long)ams_syscall(SYS_SENDMSG, fd, (uint64_t)msg, flags, 0, 0);
}

long recvmsg(int fd, void* msg, int flags) {
    return (long)ams_syscall(SYS_RECVMSG, fd, (uint64_t)msg, flags, 0, 0);
}

int poll(struct linux_pollfd_local* fds, unsigned long nfds, int timeout) {
    return (int)ams_syscall(SYS_POLL, (uint64_t)fds, nfds, timeout, 0, 0);
}

int ppoll(struct linux_pollfd_local* fds, unsigned long nfds, const void* timeout_ts, const void* sigmask) {
    (void)sigmask;
    return (int)ams_syscall(SYS_PPOLL, (uint64_t)fds, nfds, (uint64_t)timeout_ts, 0, 0);
}

int epoll_create1(int flags) {
    return (int)ams_syscall(SYS_EPOLL_CREATE1, flags, 0, 0, 0, 0);
}

int epoll_ctl(int epfd, int op, int fd, struct linux_epoll_event_local* event) {
    return (int)ams_syscall(SYS_EPOLL_CTL, epfd, op, fd, (uint64_t)event, 0);
}

int epoll_wait(int epfd, struct linux_epoll_event_local* events, int maxevents, int timeout) {
    return (int)ams_syscall(SYS_EPOLL_WAIT, epfd, (uint64_t)events, maxevents, timeout, 0);
}

int eventfd(unsigned int initval, int flags) {
    return (int)ams_syscall(SYS_EVENTFD2, initval, flags, 0, 0, 0);
}

int memfd_create(const char* name, unsigned int flags) {
    return (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)name, flags, 0, 0, 0);
}

int shm_open(const char* name, int oflag, unsigned int mode) {
    (void)mode;
    return open(name, oflag);
}

int shm_unlink(const char* name) {
    return unlink(name);
}

// Zmienne globalne dla errno (wymagane przez niektóre biblioteki C)
int errno_val = 0;
int* __errno_location() { return &errno_val; }

}