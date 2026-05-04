#ifndef _AMS_DRM_H
#define _AMS_DRM_H

#include <stdint.h>

#define DRM_IOCTL_BASE           0x64
#define DRM_IOCTL_VERSION        0x00
#define DRM_IOCTL_GET_CAP        0x0C
#define DRM_IOCTL_MODE_GETRESOURCES  0xA0
#define DRM_IOCTL_MODE_GETCRTC       0xA1
#define DRM_IOCTL_MODE_SETCRTC       0xA2
#define DRM_IOCTL_MODE_GETENCODER    0xA6
#define DRM_IOCTL_MODE_GETCONNECTOR  0xA7
#define DRM_IOCTL_MODE_GETPLANERESOURCES 0xB5
#define DRM_IOCTL_MODE_GETPLANE      0xB6
#define DRM_IOCTL_MODE_ADDFB        0xAE
#define DRM_IOCTL_MODE_RMFB         0xAF
#define DRM_IOCTL_MODE_PAGE_FLIP    0xB0
#define DRM_IOCTL_MODE_ADDFB2       0xB8
#define DRM_IOCTL_MODE_GETPROPERTY  0xAA
#define DRM_IOCTL_MODE_SETPROPERTY  0xAB
#define DRM_IOCTL_SET_MASTER        0x1E
#define DRM_IOCTL_DROP_MASTER       0x1F

/* GEM ioctls */
#define DRM_IOCTL_GEM_OPEN       0x0B
#define DRM_IOCTL_GEM_CLOSE      0x09
#define DRM_IOCTL_GEM_FLINK      0x0A
#define DRM_IOCTL_MODE_CREATE_DUMB  0xB2
#define DRM_IOCTL_MODE_MAP_DUMB     0xB3
#define DRM_IOCTL_MODE_DESTROY_DUMB 0xB4

/* Capabilities */
#define DRM_CAP_DUMB_BUFFER        0x01
#define DRM_CAP_PRIME              0x02
#define DRM_CAP_TIMESTAMP_MONOTONIC 0x06
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12

/* Connector status */
#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

/* Connector type */
#define DRM_MODE_CONNECTOR_Unknown    0
#define DRM_MODE_CONNECTOR_VGA        1
#define DRM_MODE_CONNECTOR_DVII       2
#define DRM_MODE_CONNECTOR_DVID       3
#define DRM_MODE_CONNECTOR_DVIA       4
#define DRM_MODE_CONNECTOR_Composite  5
#define DRM_MODE_CONNECTOR_SVIDEO     6
#define DRM_MODE_CONNECTOR_LVDS       7
#define DRM_MODE_CONNECTOR_Component  8
#define DRM_MODE_CONNECTOR_9PinDIN    9
#define DRM_MODE_CONNECTOR_DisplayPort 10
#define DRM_MODE_CONNECTOR_HDMIA      11
#define DRM_MODE_CONNECTOR_HDMIB      12
#define DRM_MODE_CONNECTOR_TV         13
#define DRM_MODE_CONNECTOR_eDP        14
#define DRM_MODE_CONNECTOR_VIRTUAL    15

/* Encoder type */
#define DRM_MODE_ENCODER_NONE   0
#define DRM_MODE_ENCODER_DAC    1
#define DRM_MODE_ENCODER_TMDS   2
#define DRM_MODE_ENCODER_LVDS   3
#define DRM_MODE_ENCODER_TVDAC  4
#define DRM_MODE_ENCODER_VIRTUAL 5

/* Page flip flags */
#define DRM_MODE_PAGE_FLIP_EVENT    0x01
#define DRM_MODE_PAGE_FLIP_ASYNC    0x02

/* Pixel formats (fourcc) */
#define DRM_FORMAT_XRGB8888  0x34325258
#define DRM_FORMAT_ARGB8888  0x34325241

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[32];
};

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    uint32_t name_len;
    char* name;
    uint32_t date_len;
    char* date;
    uint32_t desc_len;
    char* desc;
};

struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

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

struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
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

struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
};

struct drm_gem_open {
    char name[64];
    uint32_t handle;
    uint64_t size;
};

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

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

#define DRM_EVENT_VBLANK        0x01
#define DRM_EVENT_FLIP_COMPLETE 0x02

#endif
