#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "stdlib.h"
#include "string.h"
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

// WAŻNE: To łączy nas z plikiem src/lib/syscall.s
// To ta funkcja wykonuje instrukcję CPU 'syscall'
extern "C" uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);
extern "C" uint64_t ams_syscall6(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6);
extern "C" {

void exit(int code) {
    ams_syscall(SYS_EXIT, code, 0, 0, 0, 0);
    while(1);
}

int sys_exec(const char* path, int argc, char** argv) {
    return (int)ams_syscall(SYS_EXEC, (uint64_t)path, (uint64_t)argc, (uint64_t)argv, 0, 0);
}

int write(int fd, const void* buf, size_t count) {
    return (int)ams_syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

int open(const char* path, int flags, ...) {
    return (int)ams_syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

int close(int fd) {
    return (int)ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)ams_syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)ams_syscall(SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence, 0, 0);
}

int unlink(const char* pathname) {
    return (int)ams_syscall(SYS_UNLINK, (uint64_t)pathname, 0, 0, 0, 0);
}

char* getcwd(char* buf, size_t size) {
    return (char*)ams_syscall(SYS_GETCWD, (uint64_t)buf, (uint64_t)size, 0, 0, 0);
}

int socket(int domain, int type, int protocol) {
    return (int)ams_syscall(SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol, 0, 0);
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return (int)ams_syscall(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int listen(int sockfd, int backlog) {
    return (int)ams_syscall(SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog, 0, 0, 0);
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return (int)ams_syscall(SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return (int)ams_syscall(SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    return (ssize_t)ams_syscall(SYS_SENDMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    return (ssize_t)ams_syscall(SYS_RECVMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

int shutdown(int sockfd, int how) {
    return (int)ams_syscall(SYS_SHUTDOWN, (uint64_t)sockfd, (uint64_t)how, 0, 0, 0);
}

int memfd_create(const char* name, unsigned int flags) {
    return (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)name, (uint64_t)flags, 0, 0, 0);
}

int ftruncate(int fd, off_t length) {
    return (int)ams_syscall(SYS_FTRUNCATE, (uint64_t)fd, (uint64_t)length, 0, 0, 0);
}

int fcntl(int fd, int cmd, ...) {
    return (int)ams_syscall(SYS_FCNTL, (uint64_t)fd, (uint64_t)cmd, 0, 0, 0);
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
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

int eventfd(unsigned int initval, int flags) {
    return (int)ams_syscall(SYS_EVENTFD2, (uint64_t)initval, (uint64_t)flags, 0, 0, 0);
}

int get_key() {
    return (int)ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)ams_syscall6(SYS_MMAP, (uint64_t)addr, (uint64_t)length, (uint64_t)prot,
        (uint64_t)flags, (uint64_t)fd, (uint64_t)offset);
}

int munmap(void* addr, size_t length) {
    return (int)ams_syscall(SYS_MUNMAP, (uint64_t)addr, (uint64_t)length, 0, 0, 0);
}

int mprotect(void* addr, size_t length, int prot) {
    return (int)ams_syscall(SYS_MPROTECT, (uint64_t)addr, (uint64_t)length, (uint64_t)prot, 0, 0);
}

static int shm_counter = 1;

int shm_open(const char* name, int oflag, mode_t mode) {
    (void)mode;
    if (!name || !*name) return -1;
    if (name[0] == '/' && name[1] == '\0') return -1;

    char path[160];
    int at = 0;
    const char prefix[] = "/dev/shm/";
    while (prefix[at]) {
        path[at] = prefix[at];
        ++at;
    }
    int in = 0;
    if (name[0] == '/') in = 1;
    while (name[in] && at + 1 < (int)sizeof(path)) {
        path[at++] = name[in++];
    }
    path[at] = '\0';

    int fd = open(path, oflag, mode);
    if (fd >= 0) return fd;
    if (!(oflag & 0x40)) return fd;

    char fallback[64];
    const char prefix2[] = "/__shm_";
    int p = 0;
    while (prefix2[p]) {
        fallback[p] = prefix2[p];
        ++p;
    }
    int id = shm_counter++;
    char digits[16];
    int dn = 0;
    if (id == 0) digits[dn++] = '0';
    while (id > 0 && dn < (int)sizeof(digits)) {
        digits[dn++] = (char)('0' + (id % 10));
        id /= 10;
    }
    while (dn > 0 && p + 1 < (int)sizeof(fallback)) {
        fallback[p++] = digits[--dn];
    }
    fallback[p] = '\0';
    return open(fallback, oflag | 0x40, 0);
}

int shm_unlink(const char* name) {
    if (!name) return -1;
    char path[160];
    int at = 0;
    const char prefix[] = "/dev/shm/";
    while (prefix[at]) {
        path[at] = prefix[at];
        ++at;
    }
    int in = (name[0] == '/') ? 1 : 0;
    while (name[in] && at + 1 < (int)sizeof(path)) {
        path[at++] = name[in++];
    }
    path[at] = '\0';
    return unlink(path);
}

// Zmienne globalne dla errno (wymagane przez niektóre biblioteki C)
int errno_val = 0;
int* __errno_location() { return &errno_val; }

}