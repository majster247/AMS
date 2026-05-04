#ifndef _POLL_H
#define _POLL_H

#include <sys/types.h>

#define POLLIN   0x001
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010

struct pollfd {
    int fd;
    short events;
    short revents;
};

#ifdef __cplusplus
extern "C" {
#endif

int poll(struct pollfd* fds, unsigned long nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif
