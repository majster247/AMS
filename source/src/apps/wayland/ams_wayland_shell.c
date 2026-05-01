#include "ams_syscall.h"
#include <stdint.h>
#include <string.h>

#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
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
struct linux_iovec { void* iov_base; uint64_t iov_len; };
struct linux_msghdr {
    void* msg_name; uint32_t msg_namelen; uint32_t __pad0;
    struct linux_iovec* msg_iov; uint64_t msg_iovlen;
    void* msg_control; uint64_t msg_controllen; uint32_t msg_flags; uint32_t __pad1;
};
struct linux_cmsghdr { uint64_t cmsg_len; int32_t cmsg_level; int32_t cmsg_type; };

static void wr_u32(uint8_t* p, uint32_t v) { p[0]=v&0xff; p[1]=(v>>8)&0xff; p[2]=(v>>16)&0xff; p[3]=(v>>24)&0xff; }
static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) {
    uint32_t n = 0; while (s[n]) ++n;
    at = append_u32(out, at, n + 1);
    memcpy(out + at, s, n); out[at + n] = 0; at += n + 1;
    while (at & 3U) out[at++] = 0;
    return at;
}
static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0}; struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control; msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

int main(void) {
    struct linux_sockaddr_un addr = {0}; addr.sun_family = AF_UNIX;
    const char* path = "/run/user/0/wayland-0";
    for (int i = 0; path[i] && i < 107; ++i) addr.sun_path[i] = path[i];
    int fd = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (fd < 0) return 1;
    if ((int)ams_syscall(SYS_CONNECT, (uint64_t)fd, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) return 2;

    const uint32_t registry_id = 70, compositor_id = 71, shm_id = 72, surface_id = 73, pool_id = 74, buffer_id = 75;
    const uint32_t xdg_wm_base_id = 76, xdg_surface_id = 77, xdg_toplevel_id = 78;
    uint8_t msg[512] = {0}; uint32_t at = 0; uint32_t h = 0;

    at = append_u32(msg, at, 1); at = append_u32(msg, at, (12U << 16) | 1U); at = append_u32(msg, at, registry_id); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, registry_id); h = at; at = append_u32(msg, at, 0); at = append_u32(msg, at, 1); at = append_string(msg, at, "wl_compositor"); at = append_u32(msg, at, 4); at = append_u32(msg, at, compositor_id); wr_u32(msg + h, (at << 16) | 0); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, registry_id); h = at; at = append_u32(msg, at, 0); at = append_u32(msg, at, 2); at = append_string(msg, at, "wl_shm"); at = append_u32(msg, at, 1); at = append_u32(msg, at, shm_id); wr_u32(msg + h, (at << 16) | 0); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, registry_id); h = at; at = append_u32(msg, at, 0); at = append_u32(msg, at, 5); at = append_string(msg, at, "xdg_wm_base"); at = append_u32(msg, at, 1); at = append_u32(msg, at, xdg_wm_base_id); wr_u32(msg + h, (at << 16) | 0); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, compositor_id); at = append_u32(msg, at, (12U << 16) | 0U); at = append_u32(msg, at, surface_id); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, xdg_wm_base_id); at = append_u32(msg, at, (16U << 16) | 1U); at = append_u32(msg, at, xdg_surface_id); at = append_u32(msg, at, surface_id); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, xdg_surface_id); at = append_u32(msg, at, (12U << 16) | 1U); at = append_u32(msg, at, xdg_toplevel_id); (void)send_packet(fd, msg, at, -1);

    const int width = 800, height = 450, stride = width * 4, size = stride * height;
    int shmfd = (int)ams_syscall(SYS_MEMFD_CREATE, (uint64_t)"ams-wayland-shell", 0, 0, 0, 0);
    if (shmfd < 0) return 3;
    if ((int)ams_syscall(SYS_FTRUNCATE, (uint64_t)shmfd, (uint64_t)size, 0, 0, 0) < 0) return 4;
    uint32_t* pix = (uint32_t*)mmap(0, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if ((uint64_t)pix > (uint64_t)-4096LL) return 5;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) pix[y * width + x] = (y < 38) ? 0x2E3D52 : 0x1A2232;

    at = 0; at = append_u32(msg, at, shm_id); at = append_u32(msg, at, (16U << 16) | 0U); at = append_u32(msg, at, pool_id); at = append_u32(msg, at, (uint32_t)size); (void)send_packet(fd, msg, at, shmfd);
    at = 0; at = append_u32(msg, at, pool_id); at = append_u32(msg, at, (32U << 16) | 0U); at = append_u32(msg, at, buffer_id); at = append_u32(msg, at, 0); at = append_u32(msg, at, (uint32_t)width); at = append_u32(msg, at, (uint32_t)height); at = append_u32(msg, at, (uint32_t)stride); at = append_u32(msg, at, 0); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, surface_id); at = append_u32(msg, at, (20U << 16) | 1U); at = append_u32(msg, at, buffer_id); at = append_u32(msg, at, 0); at = append_u32(msg, at, 0); (void)send_packet(fd, msg, at, -1);
    at = 0; at = append_u32(msg, at, surface_id); at = append_u32(msg, at, (8U << 16) | 6U); (void)send_packet(fd, msg, at, -1);
    return 0;
}
