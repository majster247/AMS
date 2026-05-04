#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${ROOT_DIR}/external/mlibc-stack"
MLIBC_DIR="${DEPS_DIR}/mlibc"

mkdir -p "${DEPS_DIR}"

clone_or_update() {
  local repo_url="$1"
  local dst="$2"
  if [[ ! -d "${dst}/.git" ]]; then
    git clone --depth 1 --single-branch "${repo_url}" "${dst}"
  else
    git -C "${dst}" fetch --depth 1 origin
    git -C "${dst}" reset --hard origin/HEAD
  fi
}

clone_or_update "https://github.com/managarm/mlibc.git" "${MLIBC_DIR}"

echo "[mlibc-stage] repository ready: ${MLIBC_DIR}"

# Create AMS-OS sysdeps directory for mlibc
SYSDEPS="${DEPS_DIR}/ams-sysdeps"
mkdir -p "${SYSDEPS}/include" "${SYSDEPS}/src"

cat > "${SYSDEPS}/include/ams/syscall.h" << 'SYSDEP_HDR'
#ifndef AMS_MLIBC_SYSCALL_H
#define AMS_MLIBC_SYSCALL_H

#include <stdint.h>

static inline long ams_syscall(long num, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 asm("r10") = a4;
    register long r8 asm("r8") = a5;
    asm volatile("syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory");
    return ret;
}

#endif
SYSDEP_HDR

cat > "${SYSDEPS}/src/entry.cpp" << 'SYSDEP_ENTRY'
#include <bits/ensure.h>
#include <mlibc/all-sysdeps.hpp>
#include <ams/syscall.h>

extern "C" void __mlibc_entry(int (*main_fn)(int, char **, char **)) {
    auto result = main_fn(0, nullptr, nullptr);
    ams_syscall(60 /* SYS_EXIT */, result, 0, 0, 0, 0);
    __builtin_unreachable();
}
SYSDEP_ENTRY

cat > "${SYSDEPS}/src/generic.cpp" << 'SYSDEP_GENERIC'
#include <bits/ensure.h>
#include <mlibc/all-sysdeps.hpp>
#include <ams/syscall.h>
#include <errno.h>

namespace mlibc {

int sys_write(int fd, const void *buf, size_t count, ssize_t *bytes_written) {
    auto ret = ams_syscall(1, fd, (long)buf, count, 0, 0);
    if (ret < 0) return -ret;
    *bytes_written = ret;
    return 0;
}

int sys_read(int fd, void *buf, size_t count, ssize_t *bytes_read) {
    auto ret = ams_syscall(0, fd, (long)buf, count, 0, 0);
    if (ret < 0) return -ret;
    *bytes_read = ret;
    return 0;
}

int sys_open(const char *path, int flags, mode_t mode, int *fd) {
    auto ret = ams_syscall(2, (long)path, flags, mode, 0, 0);
    if (ret < 0) return -ret;
    *fd = ret;
    return 0;
}

int sys_close(int fd) {
    auto ret = ams_syscall(3, fd, 0, 0, 0, 0);
    if (ret < 0) return -ret;
    return 0;
}

int sys_seek(int fd, off_t offset, int whence, off_t *new_offset) {
    auto ret = ams_syscall(8, fd, offset, whence, 0, 0);
    if (ret < 0) return -ret;
    *new_offset = ret;
    return 0;
}

int sys_vm_map(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
    auto ret = ams_syscall(9, (long)hint, size, prot, flags | ((long)fd << 32), offset, 0);
    if (ret < 0 || (unsigned long)ret > (unsigned long)-4096ULL) return ENOMEM;
    *window = (void *)ret;
    return 0;
}

int sys_vm_unmap(void *pointer, size_t size) {
    ams_syscall(11, (long)pointer, size, 0, 0, 0);
    return 0;
}

int sys_anon_allocate(size_t size, void **pointer) {
    return sys_vm_map(nullptr, size, 3, 0x22, -1, 0, pointer);
}

int sys_anon_free(void *pointer, size_t size) {
    return sys_vm_unmap(pointer, size);
}

void sys_exit(int status) {
    ams_syscall(60, status, 0, 0, 0, 0);
    __builtin_unreachable();
}

int sys_clock_get(int clock, time_t *secs, long *nanos) {
    struct { long tv_sec; long tv_nsec; } ts;
    auto ret = ams_syscall(228, clock, (long)&ts, 0, 0, 0);
    if (ret < 0) return -ret;
    *secs = ts.tv_sec;
    *nanos = ts.tv_nsec;
    return 0;
}

int sys_socket(int domain, int type, int protocol, int *fd) {
    auto ret = ams_syscall(41, domain, type, protocol, 0, 0);
    if (ret < 0) return -ret;
    *fd = ret;
    return 0;
}

int sys_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    auto ret = ams_syscall(49, fd, (long)addr, addrlen, 0, 0);
    if (ret < 0) return -ret;
    return 0;
}

int sys_listen(int fd, int backlog) {
    auto ret = ams_syscall(50, fd, backlog, 0, 0, 0);
    if (ret < 0) return -ret;
    return 0;
}

int sys_accept(int fd, int *newfd, struct sockaddr *addr, socklen_t *addrlen) {
    auto ret = ams_syscall(43, fd, (long)addr, (long)addrlen, 0, 0);
    if (ret < 0) return -ret;
    *newfd = ret;
    return 0;
}

int sys_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    auto ret = ams_syscall(42, fd, (long)addr, addrlen, 0, 0);
    if (ret < 0) return -ret;
    return 0;
}

int sys_ioctl(int fd, unsigned long request, void *arg, int *result) {
    auto ret = ams_syscall(16, fd, request, (long)arg, 0, 0);
    if (ret < 0) return -ret;
    *result = ret;
    return 0;
}

int sys_poll(struct pollfd *fds, nfds_t count, int timeout, int *num_events) {
    auto ret = ams_syscall(7, (long)fds, count, timeout, 0, 0);
    if (ret < 0) return -ret;
    *num_events = ret;
    return 0;
}

pid_t sys_getpid() {
    return ams_syscall(39, 0, 0, 0, 0, 0);
}

pid_t sys_getppid() {
    return ams_syscall(110, 0, 0, 0, 0, 0);
}

uid_t sys_getuid() {
    return ams_syscall(102, 0, 0, 0, 0, 0);
}

gid_t sys_getgid() {
    return ams_syscall(104, 0, 0, 0, 0, 0);
}

uid_t sys_geteuid() {
    return ams_syscall(107, 0, 0, 0, 0, 0);
}

gid_t sys_getegid() {
    return ams_syscall(108, 0, 0, 0, 0, 0);
}

} // namespace mlibc
SYSDEP_GENERIC

echo "[mlibc-stage] AMS sysdeps created at ${SYSDEPS}"
echo "[mlibc-stage] To build: configure mlibc meson with -Dlinux_kernel_headers=... --cross-file=..."
