/* AMS virtual DRM/KMS subsystem header.
 * Exposes /dev/dri/card0 with dumb-buffer support backed by the multiboot
 * linear framebuffer.  No real GPU — software scanout only.
 */
#pragma once
#include <stdint.h>

/* ---- DRM ioctl encoding (matches Linux drm.h) ---- */
#define DRM_IOCTL_BASE   'd'
#define DRM_IO(nr)       ((uint64_t)(0x0000 | ((nr) << 8) | DRM_IOCTL_BASE))
#define DRM_IOR(nr,sz)   ((uint64_t)(0x8000 | ((sz) << 16) | ((nr) << 8) | DRM_IOCTL_BASE))
#define DRM_IOW(nr,sz)   ((uint64_t)(0x4000 | ((sz) << 16) | ((nr) << 8) | DRM_IOCTL_BASE))
#define DRM_IOWR(nr,sz)  ((uint64_t)(0xC000 | ((sz) << 16) | ((nr) << 8) | DRM_IOCTL_BASE))

/* DRM ioctl numbers */
#define DRM_IOCTL_VERSION          DRM_IOWR(0x00, sizeof(drm_version))
#define DRM_IOCTL_GET_UNIQUE       DRM_IOWR(0x01, sizeof(drm_unique))
#define DRM_IOCTL_GET_CAP          DRM_IOWR(0x0C, sizeof(drm_get_cap))
#define DRM_IOCTL_SET_CLIENT_CAP   DRM_IOW (0x0D, sizeof(drm_set_client_cap))
#define DRM_IOCTL_AUTH_MAGIC       DRM_IOW (0x11, sizeof(drm_auth))
#define DRM_IOCTL_MODE_GETRESOURCES  DRM_IOWR(0xA0, sizeof(drm_mode_card_res))
#define DRM_IOCTL_MODE_GETCRTC       DRM_IOWR(0xA1, sizeof(drm_mode_crtc))
#define DRM_IOCTL_MODE_SETCRTC       DRM_IOWR(0xA2, sizeof(drm_mode_crtc))
#define DRM_IOCTL_MODE_CURSOR        DRM_IOWR(0xA3, sizeof(drm_mode_cursor))
#define DRM_IOCTL_MODE_GETGAMMA      DRM_IOWR(0xA4, sizeof(drm_mode_crtc_lut))
#define DRM_IOCTL_MODE_SETGAMMA      DRM_IOWR(0xA5, sizeof(drm_mode_crtc_lut))
#define DRM_IOCTL_MODE_GETENCODER    DRM_IOWR(0xA6, sizeof(drm_mode_get_encoder))
#define DRM_IOCTL_MODE_GETCONNECTOR  DRM_IOWR(0xA7, sizeof(drm_mode_get_connector))
#define DRM_IOCTL_MODE_ADDFB         DRM_IOWR(0xAE, sizeof(drm_mode_fb_cmd))
#define DRM_IOCTL_MODE_RMFB          DRM_IOWR(0xAF, sizeof(uint32_t))
#define DRM_IOCTL_MODE_PAGE_FLIP     DRM_IOWR(0xB0, sizeof(drm_mode_crtc_page_flip))
#define DRM_IOCTL_MODE_DIRTYFB       DRM_IOWR(0xB1, sizeof(drm_mode_fb_dirty_cmd))
#define DRM_IOCTL_MODE_CREATE_DUMB   DRM_IOWR(0xB2, sizeof(drm_mode_create_dumb))
#define DRM_IOCTL_MODE_MAP_DUMB      DRM_IOWR(0xB3, sizeof(drm_mode_map_dumb))
#define DRM_IOCTL_MODE_DESTROY_DUMB  DRM_IOWR(0xB4, sizeof(drm_mode_destroy_dumb))
#define DRM_IOCTL_PRIME_HANDLE_TO_FD DRM_IOWR(0x2D, sizeof(drm_prime_handle))
#define DRM_IOCTL_PRIME_FD_TO_HANDLE DRM_IOWR(0x2E, sizeof(drm_prime_handle))

/* DRM capability IDs */
#define DRM_CAP_DUMB_BUFFER          0x1
#define DRM_CAP_VBLANK_HIGH_CRTC    0x2
#define DRM_CAP_DUMB_PREFERRED_DEPTH 0x3
#define DRM_CAP_DUMB_PREFER_SHADOW   0x4
#define DRM_CAP_PRIME               0x5
#define DRM_CAP_TIMESTAMP_MONOTONIC  0x6
#define DRM_CAP_ASYNC_PAGE_FLIP      0x7
#define DRM_CAP_CURSOR_WIDTH         0x8
#define DRM_CAP_CURSOR_HEIGHT        0x9
#define DRM_CAP_ADDFB2_MODIFIERS     0x10
#define DRM_CAP_PAGE_FLIP_TARGET     0x11
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#define DRM_CAP_SYNCOBJ             0x13
#define DRM_CAP_SYNCOBJ_TIMELINE     0x14

/* Client cap IDs */
#define DRM_CLIENT_CAP_STEREO_3D     1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC        3
#define DRM_CLIENT_CAP_ASPECT_RATIO  4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5

/* DRM connector/encoder types */
#define DRM_MODE_CONNECTOR_Unknown   0
#define DRM_MODE_CONNECTOR_HDMIA     11
#define DRM_MODE_ENCODER_NONE        0
#define DRM_MODE_ENCODER_TMDS        3

/* DRM connector status */
#define DRM_MODE_CONNECTED           1
#define DRM_MODE_DISCONNECTED        2

/* DRM mode type/flags */
#define DRM_MODE_TYPE_PREFERRED      (1<<3)
#define DRM_MODE_TYPE_DRIVER         (1<<6)
#define DRM_MODE_FLAG_NHSYNC         (1<<0)
#define DRM_MODE_FLAG_NVSYNC         (1<<2)

/* Page flip flags */
#define DRM_MODE_PAGE_FLIP_EVENT     0x01
#define DRM_MODE_PAGE_FLIP_ASYNC     0x02

/* ---- DRM structs (wire-compatible with Linux uAPI) ---- */

typedef struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    uint64_t name_len;
    uint64_t name;        /* ptr */
    uint64_t date_len;
    uint64_t date;        /* ptr */
    uint64_t desc_len;
    uint64_t desc;        /* ptr */
} drm_version;

typedef struct drm_unique {
    uint64_t unique_len;
    uint64_t unique;      /* ptr */
} drm_unique;

typedef struct drm_auth {
    uint32_t magic;
} drm_auth;

typedef struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
} drm_get_cap;

typedef struct drm_set_client_cap {
    uint64_t capability;
    uint64_t value;
} drm_set_client_cap;

typedef struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char     name[32];
} drm_mode_modeinfo;

typedef struct drm_mode_card_res {
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
} drm_mode_card_res;

typedef struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    drm_mode_modeinfo mode;
} drm_mode_crtc;

typedef struct drm_mode_cursor {
    uint32_t flags;
    uint32_t crtc_id;
    int32_t  x, y;
    uint32_t width, height;
    uint32_t handle;
    uint64_t user_data;
} drm_mode_cursor;

typedef struct drm_mode_crtc_lut {
    uint32_t crtc_id;
    uint32_t gamma_size;
    uint64_t red, green, blue;
} drm_mode_crtc_lut;

typedef struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drm_mode_get_encoder;

typedef struct drm_mode_get_connector {
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
} drm_mode_get_connector;

typedef struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
} drm_mode_fb_cmd;

typedef struct drm_mode_fb_dirty_cmd {
    uint32_t fb_id;
    uint32_t flags;
    uint32_t color;
    uint32_t num_clips;
    uint64_t clips_ptr;
} drm_mode_fb_dirty_cmd;

typedef struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
} drm_mode_crtc_page_flip;

typedef struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
} drm_mode_create_dumb;

typedef struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
} drm_mode_map_dumb;

typedef struct drm_mode_destroy_dumb {
    uint32_t handle;
} drm_mode_destroy_dumb;

typedef struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
} drm_prime_handle;

/* ---- AMS-internal dumb buffer descriptor ---- */
#define DRM_MAX_DUMB_BUFS  16

typedef struct ams_dumb_buf {
    uint32_t  handle;     /* 1-based index */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint64_t  size;       /* bytes, page-aligned */
    uint64_t  map_offset; /* mmap offset token = handle << 12 */
    uint64_t* phys_pages; /* array of physical page addresses */
    uint32_t  n_pages;
    uint32_t  fb_id;      /* non-zero when associated with a framebuffer */
} ams_dumb_buf;

/* ---- Public kernel API ---- */
#ifdef __cplusplus
extern "C" {
#endif

void drm_virt_init(void);
uint64_t drm_virt_ioctl(int fd, uint64_t request, uint64_t arg_ptr);

/* Called from sys_mmap when offset matches a dumb-buffer map_offset */
uint64_t drm_virt_mmap_dumb(uint64_t map_offset, uint64_t length);

/* Retrieve physical pages for a dumb buffer (for direct blit) */
ams_dumb_buf* drm_virt_get_buf(uint32_t handle);

#ifdef __cplusplus
}
#endif
