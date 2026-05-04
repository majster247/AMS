/**
 * @file libports_drm.c
 * @brief DRM/GBM helper used by mesa3d's swrast and wlroots' drm backend.
 *
 * The AMS-OS DRM driver lives in the kernel (src/drivers/drm/ams_drm.cpp)
 * and is exposed as /dev/dri/card0. Userspace opens the node, performs
 * ioctls (DRM_IOCTL_MODE_CREATE_DUMB / MAP_DUMB) and mmaps the returned
 * offset. This file wraps that flow so that the upstream code in
 * external/wayland-stack/mesa/src/gbm/backends/dri can reuse it
 * verbatim (we patch the GBM backend to call ams_drm_* helpers when
 * targeting AMS-OS).
 */
#include "libports/libports.h"
#include "ams_syscall.h"
#include "linux_syscalls.h"
#include "drm/ams_drm.h"

#define AMS_O_RDWR 2

int ams_drm_open_card(void) {
    return (int)(long)ams_syscall(SYS_OPENAT, (uint64_t)(int64_t)-100,
                                  (uint64_t)"/dev/dri/card0", AMS_O_RDWR, 0, 0);
}

int ams_drm_create_dumb(int fd, uint32_t w, uint32_t h, uint32_t bpp,
                        uint32_t* handle_out, uint32_t* pitch_out,
                        uint64_t* size_out)
{
    struct ams_drm_mode_create_dumb req = {0};
    req.width  = w;
    req.height = h;
    req.bpp    = bpp ? bpp : 32;
    long rc = (long)ams_syscall(SYS_IOCTL, (uint64_t)(int64_t)fd,
                                AMS_DRM_IOCTL_MODE_CREATE_DUMB,
                                (uint64_t)&req, 0, 0);
    if (rc != 0) return (int)rc;
    if (handle_out) *handle_out = req.handle;
    if (pitch_out)  *pitch_out  = req.pitch;
    if (size_out)   *size_out   = req.size;
    return 0;
}

int ams_drm_map_dumb(int fd, uint32_t handle, uint64_t* offset_out) {
    struct ams_drm_mode_map_dumb req = {0};
    req.handle = handle;
    long rc = (long)ams_syscall(SYS_IOCTL, (uint64_t)(int64_t)fd,
                                AMS_DRM_IOCTL_MODE_MAP_DUMB,
                                (uint64_t)&req, 0, 0);
    if (rc != 0) return (int)rc;
    if (offset_out) *offset_out = req.offset;
    return 0;
}
