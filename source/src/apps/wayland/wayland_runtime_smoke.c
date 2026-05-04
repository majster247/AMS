#include "ams_syscall.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void puts1(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    (void)write(1, s, n);
    (void)write(1, "\n", 1);
}

static int expect(int cond, const char* msg, int code) {
    if (cond) return 0;
    puts1(msg);
    return code;
}

int main(void) {
    const char* shm_name = "/wl-runtime-smoke";
    int shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (expect(shmfd >= 0, "runtime-smoke: shm_open failed", 1)) return 1;
    if (expect(ftruncate(shmfd, 4096) == 0, "runtime-smoke: ftruncate failed", 2)) return 2;

    uint32_t* pix = (uint32_t*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (expect(pix != MAP_FAILED, "runtime-smoke: mmap failed", 3)) return 3;
    pix[0] = 0x12345678u;
    pix[1] = 0x90abcdefu;

    int evfd = eventfd(0, 0);
    if (expect(evfd >= 0, "runtime-smoke: eventfd failed", 4)) return 4;

    struct pollfd pfd = { evfd, POLLIN, 0 };
    if (expect(poll(&pfd, 1, 0) == 0, "runtime-smoke: poll precondition failed", 5)) return 5;

    uint64_t one = 1;
    if (expect(write(evfd, &one, sizeof(one)) == (ssize_t)sizeof(one), "runtime-smoke: eventfd write failed", 6)) return 6;
    if (expect(poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN), "runtime-smoke: poll readiness failed", 7)) return 7;

    int epfd = epoll_create1(0);
    if (expect(epfd >= 0, "runtime-smoke: epoll_create1 failed", 8)) return 8;
    struct epoll_event ev = { EPOLLIN, 0xBEEFu };
    if (expect(epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &ev) == 0, "runtime-smoke: epoll_ctl failed", 9)) return 9;
    struct epoll_event out = {0};
    if (expect(epoll_wait(epfd, &out, 1, 0) == 1 && (out.events & EPOLLIN), "runtime-smoke: epoll_wait failed", 10)) return 10;

    uint64_t got = 0;
    if (expect(read(evfd, &got, sizeof(got)) == (ssize_t)sizeof(got) && got == 1, "runtime-smoke: eventfd read failed", 11)) return 11;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/run/user/0/runtime-smoke");

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    int cli = socket(AF_UNIX, SOCK_STREAM, 0);
    if (expect(srv >= 0 && cli >= 0, "runtime-smoke: socket failed", 12)) return 12;
    if (expect(bind(srv, (const struct sockaddr*)&addr, sizeof(addr)) == 0, "runtime-smoke: bind failed", 13)) return 13;
    if (expect(listen(srv, 1) == 0, "runtime-smoke: listen failed", 14)) return 14;
    if (expect(connect(cli, (const struct sockaddr*)&addr, sizeof(addr)) == 0, "runtime-smoke: connect failed", 15)) return 15;

    int acc = accept(srv, 0, 0);
    if (expect(acc >= 0, "runtime-smoke: accept failed", 16)) return 16;

    char payload[] = "ok";
    struct iovec iov = { payload, sizeof(payload) - 1 };
    uint8_t control[64];
    memset(control, 0, sizeof(control));
    struct cmsghdr* ch = (struct cmsghdr*)control;
    ch->cmsg_len = sizeof(struct cmsghdr) + sizeof(int);
    ch->cmsg_level = SOL_SOCKET;
    ch->cmsg_type = SCM_RIGHTS;
    *(int*)(control + sizeof(struct cmsghdr)) = shmfd;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = ch->cmsg_len;
    if (expect(sendmsg(cli, &msg, 0) == (ssize_t)(sizeof(payload) - 1), "runtime-smoke: sendmsg failed", 17)) return 17;

    char recvbuf[8] = {0};
    struct iovec riov = { recvbuf, sizeof(recvbuf) };
    uint8_t rcontrol[64];
    memset(rcontrol, 0, sizeof(rcontrol));
    struct msghdr rmsg;
    memset(&rmsg, 0, sizeof(rmsg));
    rmsg.msg_iov = &riov;
    rmsg.msg_iovlen = 1;
    rmsg.msg_control = rcontrol;
    rmsg.msg_controllen = sizeof(rcontrol);

    if (expect(recvmsg(acc, &rmsg, 0) == 2, "runtime-smoke: recvmsg failed", 18)) return 18;
    if (expect(recvbuf[0] == 'o' && recvbuf[1] == 'k', "runtime-smoke: payload mismatch", 19)) return 19;
    if (expect(rmsg.msg_controllen >= sizeof(struct cmsghdr) + sizeof(int), "runtime-smoke: missing fd rights", 20)) return 20;

    int passed_fd = *(int*)(rcontrol + sizeof(struct cmsghdr));
    uint32_t* pix2 = (uint32_t*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, passed_fd, 0);
    if (expect(pix2 != MAP_FAILED, "runtime-smoke: mmap passed fd failed", 21)) return 21;
    if (expect(pix2[0] == 0x12345678u && pix2[1] == 0x90abcdefu, "runtime-smoke: shared memory mismatch", 22)) return 22;

    (void)munmap(pix2, 4096);
    (void)munmap(pix, 4096);
    (void)close(passed_fd);
    (void)close(acc);
    (void)close(cli);
    (void)close(srv);
    (void)close(epfd);
    (void)close(evfd);
    (void)close(shmfd);
    (void)shm_unlink(shm_name);

    puts1("runtime-smoke: ok");
    return 0;
}
