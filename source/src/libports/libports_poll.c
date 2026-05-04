/**
 * @file libports_poll.c
 * @brief Thin libports wrappers over the AMS-OS poll/ppoll/epoll syscalls.
 *
 * mlibc and libwayland both expect glibc-shaped poll() / epoll_create1()
 * symbols. Rather than rebuilding mlibc to know about AMS-OS, we
 * provide the symbols here and translate to the kernel ABI declared in
 * include/linux_syscalls.h.
 *
 * AMS-OS poll() is non-blocking under the hood: if no events are
 * pending the kernel returns EAGAIN (which we promote to 0 as POSIX
 * mandates) so libwayland's event loop spins until events arrive over
 * the AF_UNIX channel. Compositor main loops also use epoll, so we
 * forward those too.
 */
#include "libports/libports.h"
#include "ams_syscall.h"
#include "linux_syscalls.h"

int ams_poll(struct ams_pollfd* fds, unsigned int nfds, int timeout_ms) {
    long rc = (long)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds,
                                (uint64_t)(int64_t)timeout_ms, 0, 0);
    if (rc == -11 /* EAGAIN */) return 0;
    return (int)rc;
}

int ams_epoll_create1(int flags) {
    return (int)(long)ams_syscall(SYS_EPOLL_CREATE1,
                                  (uint64_t)(int64_t)flags, 0, 0, 0, 0);
}

int ams_epoll_ctl(int epfd, int op, int fd, void* event) {
    return (int)(long)ams_syscall(SYS_EPOLL_CTL,
                                  (uint64_t)(int64_t)epfd,
                                  (uint64_t)(int64_t)op,
                                  (uint64_t)(int64_t)fd,
                                  (uint64_t)event, 0);
}

int ams_epoll_wait(int epfd, void* events, int maxevents, int timeout_ms) {
    long rc = (long)ams_syscall(SYS_EPOLL_WAIT,
                                (uint64_t)(int64_t)epfd,
                                (uint64_t)events,
                                (uint64_t)(int64_t)maxevents,
                                (uint64_t)(int64_t)timeout_ms, 0);
    if (rc == -11) return 0;
    return (int)rc;
}

int poll(struct ams_pollfd* fds, unsigned int nfds, int timeout_ms)
    __attribute__((weak, alias("ams_poll")));
int epoll_create1(int flags)
    __attribute__((weak, alias("ams_epoll_create1")));
int epoll_ctl(int epfd, int op, int fd, void* event)
    __attribute__((weak, alias("ams_epoll_ctl")));
int epoll_wait(int epfd, void* events, int maxevents, int timeout_ms)
    __attribute__((weak, alias("ams_epoll_wait")));
