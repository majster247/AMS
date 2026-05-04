#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "poll.h"
#include "sys/epoll.h"
#include "sys/socket.h"
#include "sys/shm.h"
#include "fcntl.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern "C" uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);
extern "C" {

void exit(int code) {
    ams_syscall(SYS_EXIT, (uint64_t)code, 0, 0, 0, 0);
    while(1);
}

int sys_exec(const char* path, int argc, char** argv) {
    return (int)ams_syscall(SYS_EXEC, (uint64_t)path, (uint64_t)argc, (uint64_t)argv, 0, 0);
}

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)ams_syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

int open(const char* path, int flags, ...) {
    uint64_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return (int)ams_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, mode, 0, 0);
}

int close(int fd) {
    return (int)ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)ams_syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

long lseek(int fd, long offset, int whence) {
    return (long)ams_syscall(SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0);
}

int fcntl(int fd, int cmd, ...) {
    uint64_t arg = 0;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, uint64_t);
    va_end(ap);
    return (int)ams_syscall(SYS_FCNTL, (uint64_t)fd, (uint64_t)cmd, arg, 0, 0);
}

int openat(int dirfd, const char* path, int flags, ...) {
    uint64_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, unsigned int);
        va_end(ap);
    }
    return (int)ams_syscall(SYS_OPENAT, (uint64_t)dirfd, (uint64_t)path, (uint64_t)flags, mode, 0);
}

int ioctl(int fd, unsigned long req, ...) {
    uint64_t argp = 0;
    va_list ap;
    va_start(ap, req);
    argp = va_arg(ap, uint64_t);
    va_end(ap);
    return (int)ams_syscall(SYS_IOCTL, (uint64_t)fd, (uint64_t)req, argp, 0, 0);
}

int poll(struct pollfd* fds, size_t nfds, int timeout) {
    return (int)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout, 0, 0);
}

int ppoll(struct pollfd* fds, size_t nfds, const void* timeout_ts, const void* sigmask) {
    return (int)ams_syscall(SYS_PPOLL, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout_ts, (uint64_t)sigmask, 0);
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

int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int listen(int sockfd, int backlog) {
    return (int)ams_syscall(SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog, 0, 0, 0);
}

int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen) {
    return (int)ams_syscall(SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen) {
    return (int)ams_syscall(SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return (ssize_t)ams_syscall(SYS_SENDMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return (ssize_t)ams_syscall(SYS_RECVMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

int shm_open(const char* name, int oflag, unsigned int mode) {
    if (!name) return -1;
    return open(name, oflag, mode);
}

int shm_unlink(const char* name) {
    if (!name) return -1;
    // VFS unlink is not implemented yet in-kernel.
    (void)name;
    return 0;
}

int unlink(const char* pathname) {
    (void)pathname; 
    return 0; 
}

int get_key() {
    return (int)ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)ams_syscall(SYS_MMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)prot, (uint64_t)flags, (uint64_t)fd);
}

int munmap(void* addr, size_t length) {
    return (int)ams_syscall(SYS_MUNMAP, (uint64_t)addr, (uint64_t)length, 0, 0, 0);
}

// Zmienne globalne dla errno (wymagane przez niektóre biblioteki C)
int errno_val = 0;
int* __errno_location() { return &errno_val; }

}