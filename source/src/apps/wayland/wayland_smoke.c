#include "ams_syscall.h"
#include <stdint.h>

#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_ACCEPT 43
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_EVENTFD2 290

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1

struct linux_sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

struct linux_iovec {
    void* iov_base;
    uint64_t iov_len;
};

struct linux_msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    struct linux_iovec* msg_iov;
    uint64_t msg_iovlen;
    void* msg_control;
    uint64_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct linux_cmsghdr {
    uint64_t cmsg_len;
    int32_t cmsg_level;
    int32_t cmsg_type;
};

static void log_s(const char* s) {
    ams_syscall(1, 1, (uint64_t)s, 0, 0, 0);
}

static void log_ln(const char* s) {
    const char nl[] = "\n";
    int n = 0;
    while (s[n]) n++;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)nl, 1, 0, 0);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t rd_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int starts_with(const char* a, const char* b) {
    while (*b) {
        if (*a++ != *b++) return 0;
    }
    return 1;
}

int main(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    const char* path = "/run/user/0/wayland-0";
    for (int i = 0; path[i] && i < 107; ++i) addr.sun_path[i] = path[i];

    int srv = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    int cli = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (srv < 0 || cli < 0) {
        log_ln("wayland_smoke: socket failed");
        return 1;
    }
    if ((int)ams_syscall(SYS_BIND, srv, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        log_ln("wayland_smoke: bind failed");
        return 2;
    }
    ams_syscall(SYS_LISTEN, srv, 4, 0, 0, 0);
    if ((int)ams_syscall(SYS_CONNECT, cli, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        log_ln("wayland_smoke: connect failed");
        return 3;
    }
    int acc = (int)ams_syscall(SYS_ACCEPT, srv, 0, 0, 0, 0);
    if (acc < 0) {
        log_ln("wayland_smoke: accept failed");
        return 4;
    }

    int efd = (int)ams_syscall(SYS_EVENTFD2, 1, 0, 0, 0, 0);
    if (efd < 0) {
        log_ln("wayland_smoke: eventfd failed");
        return 5;
    }

    char payload[] = "wl-hello";
    struct linux_iovec iov = { payload, sizeof(payload) - 1 };
    uint8_t control[64] = {0};
    struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
    ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
    ch->cmsg_level = SOL_SOCKET;
    ch->cmsg_type = SCM_RIGHTS;
    *(int*)(control + sizeof(struct linux_cmsghdr)) = efd;

    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(struct linux_cmsghdr) + sizeof(int);

    if ((int)ams_syscall(SYS_SENDMSG, cli, (uint64_t)&msg, 0, 0, 0) < 0) {
        log_ln("wayland_smoke: sendmsg failed");
        return 6;
    }

    char recvbuf[32] = {0};
    struct linux_iovec riov = { recvbuf, sizeof(recvbuf) };
    uint8_t rcontrol[64] = {0};
    struct linux_msghdr rmsg = {0};
    rmsg.msg_iov = &riov;
    rmsg.msg_iovlen = 1;
    rmsg.msg_control = rcontrol;
    rmsg.msg_controllen = sizeof(rcontrol);

    int rr = (int)ams_syscall(SYS_RECVMSG, acc, (uint64_t)&rmsg, 0, 0, 0);
    if (rr < 0) {
        log_ln("wayland_smoke: recvmsg failed");
        return 7;
    }

    uint8_t registry_req[16] = {0};
    wr_u32(registry_req + 0, 1);
    wr_u32(registry_req + 4, (12U << 16) | 1U); // wl_display.get_registry
    wr_u32(registry_req + 8, 90);
    struct linux_iovec giov = { registry_req, 12 };
    struct linux_msghdr gmsg = {0};
    gmsg.msg_iov = &giov;
    gmsg.msg_iovlen = 1;
    if ((int)ams_syscall(SYS_SENDMSG, cli, (uint64_t)&gmsg, 0, 0, 0) < 0) {
        log_ln("wayland_smoke: get_registry failed");
        return 8;
    }

    uint8_t globals[1024] = {0};
    struct linux_iovec giov_in = { globals, sizeof(globals) };
    struct linux_msghdr gmsg_in = {0};
    gmsg_in.msg_iov = &giov_in;
    gmsg_in.msg_iovlen = 1;
    int gl = (int)ams_syscall(SYS_RECVMSG, acc, (uint64_t)&gmsg_in, 0, 0, 0);
    if (gl <= 0) {
        log_ln("wayland_smoke: no registry globals");
        return 9;
    }
    int has_output = 0, has_seat = 0, has_xdg = 0;
    uint32_t off = 0;
    while (off + 8U <= (uint32_t)gl) {
        uint32_t oid = rd_u32(globals + off + 0);
        uint32_t hdr = rd_u32(globals + off + 4);
        uint16_t opcode = (uint16_t)(hdr & 0xFFFFU);
        uint16_t size = (uint16_t)(hdr >> 16);
        if (size < 8 || off + size > (uint32_t)gl) break;
        if (oid == 90 && opcode == 0 && size > 16) {
            uint8_t* p = globals + off + 8;
            uint32_t slen = rd_u32(p + 4);
            const char* iface = (const char*)(p + 8);
            if (slen > 0 && slen < 128) {
                if (starts_with(iface, "wl_output")) has_output = 1;
                if (starts_with(iface, "wl_seat")) has_seat = 1;
                if (starts_with(iface, "xdg_wm_base")) has_xdg = 1;
            }
        }
        off += size;
    }
    if (!has_output || !has_seat || !has_xdg) {
        log_ln("wayland_smoke: missing desktop globals");
        return 10;
    }

    log_s("wayland_smoke: ok msg=\"");
    ams_syscall(1, 1, (uint64_t)recvbuf, (uint64_t)rr, 0, 0);
    log_ln("\"");
    log_ln("wayland_smoke: globals ok (output/seat/xdg)");
    return 0;
}
