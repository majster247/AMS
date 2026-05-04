/**
 * @file ams_drm.h
 * @brief Minimal in-kernel DRM/KMS/GEM/TTM façade exposed as /dev/dri/card0.
 *
 * The Wayland/Mesa3D stack expects a DRM device node providing:
 *  - GEM handles (per-FD lookup tables) for buffer objects.
 *  - TTM-style placement metadata (system / VRAM stub / GTT stub).
 *  - KMS modeset state: connector / CRTC / encoder / plane / framebuffer.
 *  - Dumb-buffer allocation (DRM_IOCTL_MODE_CREATE_DUMB) for software clients.
 *
 * AMS-OS does not (yet) drive a real GPU; this layer is the contract used by
 * libgbm/libdrm in software mode and by wlroots' drm backend. Allocations are
 * normal kernel pages mapped into the calling task on DRM_IOCTL_MODE_MAP_DUMB.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ioctl numbers — match libdrm's drm.h / drm_mode.h userspace ABI for the
 * subset we care about. Sizes use the linux _IOC encoding so userspace can
 * call into us without a glue layer. */
#define AMS_DRM_IOCTL_VERSION         0xC0406400u /* DRM_IOCTL_VERSION */
#define AMS_DRM_IOCTL_GET_CAP         0xC010640Cu /* DRM_IOCTL_GET_CAP */
#define AMS_DRM_IOCTL_SET_CLIENT_CAP  0x4010640Du /* DRM_IOCTL_SET_CLIENT_CAP */
#define AMS_DRM_IOCTL_GEM_CLOSE       0x40086409u
#define AMS_DRM_IOCTL_GEM_FLINK       0xC008640Au
#define AMS_DRM_IOCTL_GEM_OPEN        0xC010640Bu
#define AMS_DRM_IOCTL_PRIME_HANDLE_TO_FD 0xC00C642Du
#define AMS_DRM_IOCTL_PRIME_FD_TO_HANDLE 0xC00C642Eu

#define AMS_DRM_IOCTL_MODE_GETRESOURCES   0xC04064A0u
#define AMS_DRM_IOCTL_MODE_GETCRTC        0xC06864A1u
#define AMS_DRM_IOCTL_MODE_SETCRTC        0xC06864A2u
#define AMS_DRM_IOCTL_MODE_GETENCODER     0xC01464A6u
#define AMS_DRM_IOCTL_MODE_GETCONNECTOR   0xC05064A7u
#define AMS_DRM_IOCTL_MODE_GETPLANERESOURCES 0xC01064B5u
#define AMS_DRM_IOCTL_MODE_GETPLANE          0xC02064B6u
#define AMS_DRM_IOCTL_MODE_ADDFB             0xC01C64AEu
#define AMS_DRM_IOCTL_MODE_ADDFB2            0xC06864B8u
#define AMS_DRM_IOCTL_MODE_RMFB              0xC00464AFu
#define AMS_DRM_IOCTL_MODE_PAGE_FLIP         0xC02864B0u
#define AMS_DRM_IOCTL_MODE_CREATE_DUMB       0xC02064B2u
#define AMS_DRM_IOCTL_MODE_MAP_DUMB          0xC01064B3u
#define AMS_DRM_IOCTL_MODE_DESTROY_DUMB      0xC00464B4u
#define AMS_DRM_IOCTL_MODE_ATOMIC            0xC03864BCu

/* TTM placement constants exposed to userspace for parity with radeon/amdgpu
 * DRM headers. The compositor only inspects them; software path always uses
 * SYSTEM placement. */
#define AMS_TTM_PL_SYSTEM 0
#define AMS_TTM_PL_TT     1
#define AMS_TTM_PL_VRAM   2

/* Caps reported via DRM_IOCTL_GET_CAP. */
#define AMS_DRM_CAP_DUMB_BUFFER          0x1
#define AMS_DRM_CAP_PRIME                0x5
#define AMS_DRM_PRIME_CAP_IMPORT         0x1
#define AMS_DRM_PRIME_CAP_EXPORT         0x2

struct ams_drm_version {
    int32_t  version_major;
    int32_t  version_minor;
    int32_t  version_patchlevel;
    uint64_t name_len;
    char*    name;
    uint64_t date_len;
    char*    date;
    uint64_t desc_len;
    char*    desc;
};

struct ams_drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

struct ams_drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct ams_drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct ams_drm_mode_destroy_dumb {
    uint32_t handle;
};

struct ams_drm_mode_fb_cmd2 {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t flags;
    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint64_t modifier[4];
};

struct ams_drm_card_info {
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t bpp;
    uint64_t fb_phys; /* shadow framebuffer in TTM SYSTEM placement */
};

/* Kernel API */
void ams_drm_init(uint32_t fb_width, uint32_t fb_height, uint32_t fb_pitch);
int  ams_drm_open(int fd);
int  ams_drm_close(int fd);
int  ams_drm_ioctl(int fd, uint32_t cmd, void* argp);
/* Map a previously created dumb buffer at the linux mmap "offset" returned by
 * MAP_DUMB. Returns 0 / errno on failure. The kernel side ams_mmap path calls
 * this to translate a DRM offset into the GEM object's backing pages. */
int  ams_drm_mmap(uint64_t offset, uint64_t length, void** out_phys);
const struct ams_drm_card_info* ams_drm_get_card_info(void);

#ifdef __cplusplus
}
#endif
