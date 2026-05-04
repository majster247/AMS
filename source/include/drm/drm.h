/**
 * @file drm/drm.h
 * @brief Linux-compatible DRM (Direct Rendering Manager) UAPI header for AMS.
 *
 * Provides ioctl definitions, structures and constants compatible with
 * Linux DRM so that libdrm / wlroots can compile against the AMS kernel.
 * The actual driver wraps the Multiboot2 linear framebuffer as a single
 * CRTC + connector + plane exposed via /dev/dri/card0.
 */
#ifndef _AMS_DRM_H
#define _AMS_DRM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- ioctl encoding (Linux convention) ---- */
#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT   + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U

#define _IOC(dir,type,nr,size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT)| \
     ((nr)   << _IOC_NRSHIFT)  | \
     ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,sz)    _IOC(_IOC_READ,(type),(nr),sizeof(sz))
#define _IOW(type,nr,sz)    _IOC(_IOC_WRITE,(type),(nr),sizeof(sz))
#define _IOWR(type,nr,sz)   _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(sz))

/* ---- DRM ioctl base ---- */
#define DRM_IOCTL_BASE 'd'
#define DRM_IO(nr)          _IO(DRM_IOCTL_BASE, nr)
#define DRM_IOR(nr, type)   _IOR(DRM_IOCTL_BASE, nr, type)
#define DRM_IOW(nr, type)   _IOW(DRM_IOCTL_BASE, nr, type)
#define DRM_IOWR(nr, type)  _IOWR(DRM_IOCTL_BASE, nr, type)

/* ---- Capability tokens ---- */
#define DRM_CAP_DUMB_BUFFER          0x1
#define DRM_CAP_PRIME                0x2
#define DRM_CAP_TIMESTAMP_MONOTONIC  0x6
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC           3

/* ---- Connector types (subset) ---- */
#define DRM_MODE_CONNECTOR_Unknown     0
#define DRM_MODE_CONNECTOR_VGA         1
#define DRM_MODE_CONNECTOR_HDMIA      11
#define DRM_MODE_CONNECTOR_Virtual    15

/* ---- Connector status ---- */
#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

/* ---- Encoder types ---- */
#define DRM_MODE_ENCODER_NONE  0
#define DRM_MODE_ENCODER_DAC   1
#define DRM_MODE_ENCODER_VIRTUAL 7

/* ---- Plane types ---- */
#define DRM_PLANE_TYPE_OVERLAY  0
#define DRM_PLANE_TYPE_PRIMARY  1
#define DRM_PLANE_TYPE_CURSOR   2

/* ---- Mode info ---- */
#define DRM_DISPLAY_MODE_LEN 32

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[DRM_DISPLAY_MODE_LEN];
};

/* ---- Resources ---- */
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

/* ---- Connector ---- */
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

/* ---- Encoder ---- */
struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

/* ---- CRTC ---- */
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

/* ---- Dumb buffer ---- */
struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_mode_destroy_dumb {
    uint32_t handle;
};

/* ---- Framebuffer ---- */
struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_fb_cmd2 {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pixel_format;
    uint32_t flags;
    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint64_t modifier[4];
};

/* ---- GEM ---- */
struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
};

struct drm_gem_flink {
    uint32_t handle;
    uint32_t name;
};

struct drm_gem_open {
    uint32_t name;
    uint32_t handle;
    uint64_t size;
};

/* ---- Capabilities ---- */
struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_set_client_cap {
    uint64_t capability;
    uint64_t value;
};

/* ---- Version ---- */
struct drm_version {
    int32_t  version_major;
    int32_t  version_minor;
    int32_t  version_patchlevel;
    uint64_t name_len;
    uint64_t name;
    uint64_t date_len;
    uint64_t date;
    uint64_t desc_len;
    uint64_t desc;
};

/* ---- Page flip ---- */
struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

/* ---- VBlank ---- */
#define DRM_EVENT_VBLANK         0x01
#define DRM_EVENT_FLIP_COMPLETE  0x02

struct drm_event {
    uint32_t type;
    uint32_t length;
};

struct drm_event_vblank {
    struct drm_event base;
    uint64_t user_data;
    uint32_t tv_sec;
    uint32_t tv_usec;
    uint32_t sequence;
    uint32_t crtc_id;
};

/* ---- DRM ioctl numbers ---- */
#define DRM_IOCTL_VERSION         DRM_IOWR(0x00, struct drm_version)
#define DRM_IOCTL_GET_CAP         DRM_IOWR(0x0C, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP  DRM_IOW (0x0D, struct drm_set_client_cap)
#define DRM_IOCTL_GEM_CLOSE       DRM_IOW (0x09, struct drm_gem_close)
#define DRM_IOCTL_GEM_FLINK       DRM_IOWR(0x0A, struct drm_gem_flink)
#define DRM_IOCTL_GEM_OPEN        DRM_IOWR(0x0B, struct drm_gem_open)

#define DRM_IOCTL_MODE_GETRESOURCES   DRM_IOWR(0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC        DRM_IOWR(0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC        DRM_IOWR(0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER     DRM_IOWR(0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR   DRM_IOWR(0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_ADDFB         DRM_IOWR(0xAE, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_RMFB          DRM_IOWR(0xAF, uint32_t)
#define DRM_IOCTL_MODE_PAGE_FLIP     DRM_IOWR(0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_CREATE_DUMB   DRM_IOWR(0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB      DRM_IOWR(0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB  DRM_IOWR(0xB4, struct drm_mode_destroy_dumb)

/* ---- fourcc pixel formats (subset) ---- */
#define DRM_FORMAT_XRGB8888 0x34325258  /* XR24 */
#define DRM_FORMAT_ARGB8888 0x34325241  /* AR24 */
#define DRM_FORMAT_RGB565   0x36314752  /* RG16 */

#ifdef __cplusplus
}
#endif

#endif /* _AMS_DRM_H */
