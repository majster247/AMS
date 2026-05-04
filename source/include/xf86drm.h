/**
 * @file xf86drm.h
 * @brief Userspace DRM interface for AMS-OS (compatible with libdrm)
 *
 * Provides the userspace API that Mesa, wlroots, and other DRM clients
 * use to communicate with the kernel DRM subsystem.
 */

#ifndef _XF86DRM_H_
#define _XF86DRM_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DRM ioctl numbers */
#define DRM_IOCTL_BASE             0x40
#define DRM_IOCTL_VERSION          (DRM_IOCTL_BASE + 0x00)
#define DRM_IOCTL_MODE_GETRESOURCES (DRM_IOCTL_BASE + 0x01)
#define DRM_IOCTL_MODE_GETCRTC     (DRM_IOCTL_BASE + 0x02)
#define DRM_IOCTL_MODE_SETCRTC     (DRM_IOCTL_BASE + 0x03)
#define DRM_IOCTL_MODE_GETCONNECTOR (DRM_IOCTL_BASE + 0x04)
#define DRM_IOCTL_MODE_GETENCODER  (DRM_IOCTL_BASE + 0x05)
#define DRM_IOCTL_MODE_CREATE_DUMB (DRM_IOCTL_BASE + 0x06)
#define DRM_IOCTL_MODE_MAP_DUMB    (DRM_IOCTL_BASE + 0x07)
#define DRM_IOCTL_MODE_DESTROY_DUMB (DRM_IOCTL_BASE + 0x08)
#define DRM_IOCTL_MODE_ADDFB       (DRM_IOCTL_BASE + 0x09)
#define DRM_IOCTL_MODE_RMFB        (DRM_IOCTL_BASE + 0x0A)
#define DRM_IOCTL_MODE_PAGE_FLIP   (DRM_IOCTL_BASE + 0x0B)
#define DRM_IOCTL_GEM_CLOSE        (DRM_IOCTL_BASE + 0x0C)
#define DRM_IOCTL_GEM_OPEN         (DRM_IOCTL_BASE + 0x0D)
#define DRM_IOCTL_SET_MASTER       (DRM_IOCTL_BASE + 0x0E)
#define DRM_IOCTL_DROP_MASTER      (DRM_IOCTL_BASE + 0x0F)
#define DRM_IOCTL_GEM_FLINK        (DRM_IOCTL_BASE + 0x10)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD (DRM_IOCTL_BASE + 0x11)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE (DRM_IOCTL_BASE + 0x12)

/* Connector status */
#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

/* Connector types */
#define DRM_MODE_CONNECTOR_VGA     1
#define DRM_MODE_CONNECTOR_DVII    2
#define DRM_MODE_CONNECTOR_DVID    3
#define DRM_MODE_CONNECTOR_DVIA    4
#define DRM_MODE_CONNECTOR_HDMIA   11
#define DRM_MODE_CONNECTOR_HDMIB   12
#define DRM_MODE_CONNECTOR_VIRTUAL 15
#define DRM_MODE_CONNECTOR_eDP     14

/* Page flip flags */
#define DRM_MODE_PAGE_FLIP_EVENT   0x01

/* DRM version */
typedef struct _drmVersion {
    int version_major;
    int version_minor;
    int version_patchlevel;
    int name_len;
    char *name;
    int date_len;
    char *date;
    int desc_len;
    char *desc;
} drmVersion, *drmVersionPtr;

/* Mode info */
typedef struct _drmModeModeInfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal;
    uint32_t hskew;
    uint32_t vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[32];
} drmModeModeInfo, *drmModeModeInfoPtr;

/* Resources */
typedef struct _drmModeRes {
    int count_fbs;
    uint32_t *fbs;
    int count_crtcs;
    uint32_t *crtcs;
    int count_connectors;
    uint32_t *connectors;
    int count_encoders;
    uint32_t *encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;

/* Connector */
typedef struct _drmModeConnector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mmWidth, mmHeight;
    uint32_t subpixel;
    int count_modes;
    drmModeModeInfoPtr modes;
    int count_props;
    int count_encoders;
    uint32_t *encoders;
} drmModeConnector, *drmModeConnectorPtr;

/* Encoder */
typedef struct _drmModeEncoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drmModeEncoder, *drmModeEncoderPtr;

/* CRTC */
typedef struct _drmModeCrtc {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x, y;
    uint32_t width, height;
    int mode_valid;
    drmModeModeInfo mode;
    int gamma_size;
} drmModeCrtc, *drmModeCrtcPtr;

/* Framebuffer */
typedef struct _drmModeFB {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
} drmModeFB, *drmModeFBPtr;

/* Create dumb buffer */
struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

/* Map dumb */
struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

/* Destroy dumb */
struct drm_mode_destroy_dumb {
    uint32_t handle;
};

/* Framebuffer command */
struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRM_H_ */
