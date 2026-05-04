/**
 * @file libports_unix.c
 * @brief AF_UNIX socket helpers for the AMS-OS userspace.
 *
 * libwayland-server creates the compositor socket at runtime, and the
 * compositor binds it under $XDG_RUNTIME_DIR (typically /run/user/0).
 * This shim wraps socket()/bind()/listen()/accept()/connect() with the
 * AMS kernel syscall ABI and exposes glibc-compatible symbols.
 */
#include "libports/libports.h"
#include "ams_syscall.h"
#include "linux_syscalls.h"

#define AMS_AF_UNIX     1
#define AMS_SOCK_STREAM 1

struct ams_sockaddr_un {
    uint16_t sun_family;
    char     sun_path[108];
};

static int copy_path(struct ams_sockaddr_un* dst, const char* src) {
    int i = 0;
    if (!src) return -1;
    dst->sun_family = AMS_AF_UNIX;
    for (; src[i] && i < 107; ++i) dst->sun_path[i] = src[i];
    dst->sun_path[i] = 0;
    return 0;
}

int ams_unix_socket(int type) {
    int real = type ? type : AMS_SOCK_STREAM;
    return (int)(long)ams_syscall(SYS_SOCKET, AMS_AF_UNIX,
                                  (uint64_t)(int64_t)real, 0, 0, 0);
}

int ams_unix_bind(int fd, const char* path) {
    struct ams_sockaddr_un addr = {0};
    if (copy_path(&addr, path) != 0) return -1;
    return (int)(long)ams_syscall(SYS_BIND, (uint64_t)(int64_t)fd,
                                  (uint64_t)&addr, sizeof(addr), 0, 0);
}

int ams_unix_listen(int fd, int backlog) {
    return (int)(long)ams_syscall(SYS_LISTEN, (uint64_t)(int64_t)fd,
                                  (uint64_t)(int64_t)backlog, 0, 0, 0);
}

int ams_unix_accept(int fd) {
    return (int)(long)ams_syscall(SYS_ACCEPT, (uint64_t)(int64_t)fd, 0, 0, 0, 0);
}

int ams_unix_connect(int fd, const char* path) {
    struct ams_sockaddr_un addr = {0};
    if (copy_path(&addr, path) != 0) return -1;
    return (int)(long)ams_syscall(SYS_CONNECT, (uint64_t)(int64_t)fd,
                                  (uint64_t)&addr, sizeof(addr), 0, 0);
}
