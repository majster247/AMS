/**
 * @file libports.h
 * @brief AMS-OS userspace porting layer.
 *
 * The Linux desktop graphics stack (libwayland, mlibc, mesa, wlroots,
 * libinput, libxkbcommon, pixman, cairo, libffi) expects a POSIX-shaped
 * runtime: shm_open(), poll/epoll_wait, mmap on shared memory fds,
 * AF_UNIX sockets with sendmsg/recvmsg + SCM_RIGHTS, libffi closures,
 * /dev/dri/card0, /dev/input/event*. AMS-OS provides these as kernel
 * syscalls, but the Linux glibc ABI symbols still need to exist in
 * userspace.
 *
 * libports is the small, self-contained shim that exposes those symbols
 * by translating to AMS syscall numbers (see include/linux_syscalls.h)
 * and minimal logic on top.
 *
 * Linkage: every Wayland-stack ELF (compositor, smoke clients, sample
 * apps) is linked with libports.a in addition to mlibc, so undefined
 * symbols like shm_open are satisfied here. The shim is intentionally
 * tiny so it fits in any user binary.
 */
#ifndef _AMS_LIBPORTS_H
#define _AMS_LIBPORTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === POSIX shared memory (shm_open / shm_unlink) =========================
 *
 * AMS-OS treats shm_open as memfd_create() with the supplied name. The
 * returned fd supports ftruncate()/mmap(MAP_SHARED) and may be passed
 * across AF_UNIX sockets via SCM_RIGHTS. shm_unlink is a no-op because
 * the memfd is anonymous. */
int  ams_shm_open(const char* name, int oflag, unsigned int mode);
int  ams_shm_unlink(const char* name);

/* === poll / epoll convenience ============================================
 *
 * libports keeps a 1:1 wrapper over the kernel poll/epoll primitives so
 * that mlibc/Mesa/wlroots can resolve the symbols even on freestanding
 * targets. */
struct ams_pollfd { int fd; short events; short revents; };
int  ams_poll(struct ams_pollfd* fds, unsigned int nfds, int timeout_ms);
int  ams_epoll_create1(int flags);
int  ams_epoll_ctl(int epfd, int op, int fd, void* event);
int  ams_epoll_wait(int epfd, void* events, int maxevents, int timeout_ms);

/* === AF_UNIX helpers =====================================================
 *
 * Wayland's `wl_socket_*` API is happiest when libports exposes a
 * `socket()` + `bind()` + `listen()` + `accept()` quartet. */
int  ams_unix_socket(int type);
int  ams_unix_bind(int fd, const char* path);
int  ams_unix_listen(int fd, int backlog);
int  ams_unix_accept(int fd);
int  ams_unix_connect(int fd, const char* path);

/* === libffi / closures ===================================================
 *
 * libwayland uses libffi for marshalling. libports does NOT reimplement
 * libffi; it only ensures the upstream libffi (built from
 * external/wayland-stack/libffi) finds mmap/mprotect/__getauxval. */
void* ams_ffi_get_auxval(unsigned long type);

/* === DRM / GBM helpers ===================================================
 *
 * Open the AMS DRM card, allocate a dumb buffer, mmap it. Used by the
 * GBM port and the wlroots drm backend. Returns 0 on success.
 *
 * `mmap_offset_out` is the cookie returned by DRM_IOCTL_MODE_MAP_DUMB
 * which userspace then passes to mmap() to map the GEM into its address
 * space. */
int  ams_drm_open_card(void);
int  ams_drm_create_dumb(int fd, uint32_t w, uint32_t h, uint32_t bpp,
                         uint32_t* handle_out, uint32_t* pitch_out,
                         uint64_t* size_out);
int  ams_drm_map_dumb(int fd, uint32_t handle, uint64_t* offset_out);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_LIBPORTS_H */
