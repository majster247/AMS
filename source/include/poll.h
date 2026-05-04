#ifndef _POLL_H
#define _POLL_H

#include <sys/types.h>
#include <time.h>

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd* fds, unsigned long nfds, int timeout);
int ppoll(struct pollfd* fds, unsigned long nfds, const struct timespec* timeout_ts, const void* sigmask);

#ifdef __cplusplus
}
#endif

#endif
