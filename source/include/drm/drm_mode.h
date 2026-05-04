/*
 * Minimal drm_mode UAPI shim. KMS subset: GETRESOURCES, GETCONNECTOR,
 * GETENCODER, GETCRTC, ADDFB, RMFB, SETCRTC, PAGEFLIP.
 */

#ifndef AMS_DRM_MODE_H
#define AMS_DRM_MODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
};

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char     name[32];
};

struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_modes;
    uint32_t count_props;
    uint32_t count_encoders;
    uint32_t encoder_id;
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mm_width, mm_height;
    uint32_t subpixel;
    uint32_t pad;
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

#define DRM_IOCTL_MODE_GETRESOURCES   0x6440 /* matches Linux 0x6440 */
#define DRM_IOCTL_MODE_GETCONNECTOR   0x6447
#define DRM_IOCTL_MODE_GETENCODER     0x6446
#define DRM_IOCTL_MODE_GETCRTC        0x6441
#define DRM_IOCTL_MODE_SETCRTC        0x6442
#define DRM_IOCTL_MODE_ADDFB          0x644E
#define DRM_IOCTL_MODE_RMFB           0x644F
#define DRM_IOCTL_MODE_PAGE_FLIP      0x644B

#ifdef __cplusplus
}
#endif

#endif
