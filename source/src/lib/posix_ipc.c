#include "ams_syscall.h"
#include "linux_syscalls.h"
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>

void* ffi_type_void;
void* ffi_type_uint32;
void* ffi_type_uint64;
void* ffi_type_pointer;

static int neg_errno(long rc) {
    if (rc < 0) {
        return -1;
    }
    return (int)rc;
}

int socket(int domain, int type, int protocol) {
    return neg_errno((long)ams_syscall(SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol, 0, 0));
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return neg_errno((long)ams_syscall(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0));
}

int listen(int sockfd, int backlog) {
    return neg_errno((long)ams_syscall(SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog, 0, 0, 0));
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return neg_errno((long)ams_syscall(SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0));
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return neg_errno((long)ams_syscall(SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0));
}

int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    return neg_errno((long)ams_syscall(SYS_ACCEPT4, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, (uint64_t)flags, 0));
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return (ssize_t)ams_syscall(SYS_SENDMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return (ssize_t)ams_syscall(SYS_RECVMSG, (uint64_t)sockfd, (uint64_t)msg, (uint64_t)flags, 0, 0);
}

int shutdown(int sockfd, int how) {
    return neg_errno((long)ams_syscall(SYS_SHUTDOWN, (uint64_t)sockfd, (uint64_t)how, 0, 0, 0));
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return neg_errno((long)ams_syscall(SYS_GETSOCKNAME, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0));
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return neg_errno((long)ams_syscall(SYS_GETPEERNAME, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen, 0, 0));
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return (ssize_t)ams_syscall(SYS_READV, (uint64_t)fd, (uint64_t)iov, (uint64_t)iovcnt, 0, 0);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return (ssize_t)ams_syscall(SYS_WRITEV, (uint64_t)fd, (uint64_t)iov, (uint64_t)iovcnt, 0, 0);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return neg_errno((long)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout, 0, 0));
}

int epoll_create1(int flags) {
    return neg_errno((long)ams_syscall(SYS_EPOLL_CREATE1, (uint64_t)flags, 0, 0, 0, 0));
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    return neg_errno((long)ams_syscall(SYS_EPOLL_CTL, (uint64_t)epfd, (uint64_t)op, (uint64_t)fd, (uint64_t)event, 0));
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return neg_errno((long)ams_syscall(SYS_EPOLL_WAIT, (uint64_t)epfd, (uint64_t)events, (uint64_t)maxevents, (uint64_t)timeout, 0));
}

int eventfd(unsigned int initval, int flags) {
    return neg_errno((long)ams_syscall(SYS_EVENTFD2, (uint64_t)initval, (uint64_t)flags, 0, 0, 0));
}

int memfd_create(const char* name, unsigned int flags) {
    return neg_errno((long)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)name, (uint64_t)flags, 0, 0, 0));
}

int shm_open(const char* name, int oflag, mode_t mode) {
    (void)mode;
    unsigned int flags = 0;
    if (oflag & O_CLOEXEC) flags |= MFD_CLOEXEC;
    return memfd_create(name ? name : "shm", flags);
}

int shm_unlink(const char* name) {
    (void)name;
    return 0;
}
