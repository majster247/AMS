/*
 * AMS libwayland-server-ams: event loop.
 *
 * Wraps SYS_EPOLL_CREATE1 / SYS_EPOLL_CTL / SYS_EPOLL_WAIT and falls
 * back to SYS_POLL when epoll is unavailable.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ams_syscall.h"
#include "linux_syscalls.h"
#include <wayland/wayland-server-core.h>

#define MAX_SOURCES 64

#define EPOLLIN_FLAG  0x001u
#define EPOLLOUT_FLAG 0x004u
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

struct linux_epoll_event {
    uint32_t events;
    uint32_t __pad;
    uint64_t data;
} __attribute__((packed));

struct wl_event_source {
    int                       fd;
    uint32_t                  mask;
    wl_event_loop_fd_func_t   cb;
    void                     *data;
    int                       in_use;
};

struct wl_event_loop {
    int                     epfd;
    struct wl_event_source  sources[MAX_SOURCES];
};

struct wl_event_loop *wl_event_loop_create(void) {
    struct wl_event_loop *l = (struct wl_event_loop*)calloc(1, sizeof(*l));
    if (!l) return NULL;
    l->epfd = (int)ams_syscall(SYS_EPOLL_CREATE1, 0, 0, 0, 0, 0);
    return l;
}

void wl_event_loop_destroy(struct wl_event_loop *loop) {
    if (!loop) return;
    if (loop->epfd >= 0) ams_syscall(SYS_CLOSE, (uint64_t)loop->epfd, 0, 0, 0, 0);
    free(loop);
}

struct wl_event_source *wl_event_loop_add_fd(struct wl_event_loop *loop,
                                             int fd, uint32_t mask,
                                             wl_event_loop_fd_func_t cb, void *data) {
    if (!loop) return NULL;
    for (int i = 0; i < MAX_SOURCES; ++i) {
        if (!loop->sources[i].in_use) {
            loop->sources[i].fd = fd;
            loop->sources[i].mask = mask;
            loop->sources[i].cb = cb;
            loop->sources[i].data = data;
            loop->sources[i].in_use = 1;
            if (loop->epfd >= 0) {
                struct linux_epoll_event ev = {0};
                if (mask & WL_EVENT_READABLE) ev.events |= EPOLLIN_FLAG;
                if (mask & WL_EVENT_WRITABLE) ev.events |= EPOLLOUT_FLAG;
                ev.data = (uint64_t)(uintptr_t)&loop->sources[i];
                ams_syscall(SYS_EPOLL_CTL, (uint64_t)loop->epfd, EPOLL_CTL_ADD, (uint64_t)fd, 0, (uint64_t)&ev);
            }
            return &loop->sources[i];
        }
    }
    return NULL;
}

int wl_event_source_remove(struct wl_event_source *src) {
    if (!src || !src->in_use) return -1;
    src->in_use = 0;
    return 0;
}

int wl_event_loop_dispatch(struct wl_event_loop *loop, int timeout) {
    if (!loop) return -1;
    if (loop->epfd >= 0) {
        struct linux_epoll_event evs[16];
        int nf = (int)ams_syscall(SYS_EPOLL_WAIT, (uint64_t)loop->epfd,
                                  (uint64_t)evs, 16, (uint64_t)(unsigned)timeout, 0);
        for (int i = 0; i < nf; ++i) {
            struct wl_event_source *s = (struct wl_event_source*)(uintptr_t)evs[i].data;
            if (!s || !s->in_use || !s->cb) continue;
            uint32_t m = 0;
            if (evs[i].events & EPOLLIN_FLAG)  m |= WL_EVENT_READABLE;
            if (evs[i].events & EPOLLOUT_FLAG) m |= WL_EVENT_WRITABLE;
            s->cb(s->fd, m, s->data);
        }
        return nf;
    }
    return 0;
}
