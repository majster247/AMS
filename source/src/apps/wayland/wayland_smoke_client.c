#include "ams_syscall.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_MEMFD_CREATE 319
#define SYS_FTRUNCATE 77
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

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

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static uint32_t rd_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) {
    wr_u32(out + at, v);
    return at + 4;
}

static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) {
    uint32_t len = 0;
    while (s[len]) ++len;
    at = append_u32(out, at, len + 1);
    memcpy(out + at, s, len);
    out[at + len] = 0;
    at += len + 1;
    while (at & 3U) out[at++] = 0;
    return at;
}

static int send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET;
        ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control;
        msg.msg_controllen = ch->cmsg_len;
    }
    return (int)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap) {
    struct linux_iovec iov = {data, cap};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    return (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
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
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    int fd = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (fd < 0) {
        puts1("wl-client: socket failed");
        return 1;
    }
    if ((int)ams_syscall(SYS_CONNECT, fd, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        puts1("wl-client: connect failed");
        return 2;
    }

    const uint32_t registry_id = 10;
    const uint32_t compositor_id = 11;
    const uint32_t shm_id = 12;
    const uint32_t surface_id = 13;
    const uint32_t pool_id = 14;
    const uint32_t buffer_id = 15;
    uint32_t compositor_name = 1;
    uint32_t shm_name = 2;

    uint8_t msg[512] = {0};
    uint32_t at = 0;
    at = append_u32(msg, at, 1);
    at = append_u32(msg, at, (12U << 16) | 1U); // wl_display.get_registry
    at = append_u32(msg, at, registry_id);
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: get_registry send failed");
        return 3;
    }

    uint8_t ev[1024] = {0};
    int r = recv_packet(fd, ev, sizeof(ev));
    if (r > 0) {
        uint32_t pos = 0;
        while (pos + 8U <= (uint32_t)r) {
            uint32_t obj_id = rd_u32(ev + pos);
            uint32_t hdr = rd_u32(ev + pos + 4);
            uint16_t opcode = (uint16_t)(hdr & 0xFFFFU);
            uint16_t size = (uint16_t)(hdr >> 16);
            if (size < 8 || pos + size > (uint32_t)r) break;
            if (obj_id == registry_id && opcode == 0 && size >= 20) {
                uint8_t* p = ev + pos + 8;
                uint32_t name = rd_u32(p + 0);
                uint32_t slen = rd_u32(p + 4);
                if (slen > 0 && slen < 128) {
                    const char* iface = (const char*)(p + 8);
                    if (starts_with(iface, "wl_compositor")) compositor_name = name;
                    if (starts_with(iface, "wl_shm")) shm_name = name;
                }
            }
            pos += size;
        }
    }

    at = 0;
    at = append_u32(msg, at, registry_id);
    {
        uint32_t hdr_at = at;
        at = append_u32(msg, at, 0); // bind header placeholder
        at = append_u32(msg, at, compositor_name);
        at = append_string(msg, at, "wl_compositor");
        at = append_u32(msg, at, 4);
        at = append_u32(msg, at, compositor_id);
        wr_u32(msg + hdr_at, (uint32_t)(at << 16) | 0U);
    }
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: bind compositor failed");
        return 4;
    }

    at = 0;
    at = append_u32(msg, at, registry_id);
    {
        uint32_t hdr_at = at;
        at = append_u32(msg, at, 0);
        at = append_u32(msg, at, shm_name);
        at = append_string(msg, at, "wl_shm");
        at = append_u32(msg, at, 1);
        at = append_u32(msg, at, shm_id);
        wr_u32(msg + hdr_at, (uint32_t)(at << 16) | 0U);
    }
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: bind shm failed");
        return 5;
    }
    (void)recv_packet(fd, ev, sizeof(ev)); // optional wl_shm.format

    at = 0;
    at = append_u32(msg, at, compositor_id);
    at = append_u32(msg, at, (12U << 16) | 0U); // create_surface
    at = append_u32(msg, at, surface_id);
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: create_surface failed");
        return 6;
    }

    const int width = 640;
    const int height = 360;
    const int stride = width * 4;
    const int size = stride * height;
    int shmfd = (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)"wl-client-buffer", 0, 0, 0, 0);
    if (shmfd < 0) {
        puts1("wl-client: memfd_create failed");
        return 7;
    }
    if ((int)ams_syscall(SYS_FTRUNCATE, shmfd, (uint64_t)size, 0, 0, 0) < 0) {
        puts1("wl-client: ftruncate failed");
        return 8;
    }
    uint32_t* pix = (uint32_t*)mmap(0, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if ((uint64_t)pix > (uint64_t)-4096LL) {
        puts1("wl-client: mmap failed");
        return 9;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t r8 = (uint32_t)((x * 255) / width);
            uint32_t g8 = (uint32_t)((y * 255) / height);
            uint32_t b8 = 0x30;
            pix[y * width + x] = (r8 << 16) | (g8 << 8) | b8;
        }
    }

    at = 0;
    at = append_u32(msg, at, shm_id);
    at = append_u32(msg, at, (16U << 16) | 0U); // create_pool(new_id, size) + fd ancillary
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (uint32_t)size);
    if (send_packet(fd, msg, at, shmfd) < 0) {
        puts1("wl-client: create_pool failed");
        return 10;
    }

    at = 0;
    at = append_u32(msg, at, pool_id);
    at = append_u32(msg, at, (32U << 16) | 0U); // create_buffer
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0); // offset
    at = append_u32(msg, at, (uint32_t)width);
    at = append_u32(msg, at, (uint32_t)height);
    at = append_u32(msg, at, (uint32_t)stride);
    at = append_u32(msg, at, 0); // XRGB8888
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: create_buffer failed");
        return 11;
    }

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (20U << 16) | 1U); // attach(buffer, x, y)
    at = append_u32(msg, at, buffer_id);
    at = append_u32(msg, at, 0);
    at = append_u32(msg, at, 0);
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: attach failed");
        return 12;
    }

    at = 0;
    at = append_u32(msg, at, surface_id);
    at = append_u32(msg, at, (8U << 16) | 6U); // commit
    if (send_packet(fd, msg, at, -1) < 0) {
        puts1("wl-client: commit failed");
        return 13;
    }

    puts1("wl-client: shm frame committed");
    return 0;
}
