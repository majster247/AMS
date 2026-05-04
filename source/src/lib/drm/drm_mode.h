/* AMS userspace DRM mode structures — mirrors Linux kernel uAPI. */
#pragma once
#include <stdint.h>

#define DRM_DISPLAY_MODE_LEN 32

#define DRM_MODE_CONNECTED     1
#define DRM_MODE_DISCONNECTED  2

#define DRM_MODE_TYPE_PREFERRED (1 << 3)
#define DRM_MODE_TYPE_DRIVER    (1 << 6)

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

#define DRM_CLIENT_CAP_STEREO_3D     1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC        3

#define DRM_MODE_PAGE_FLIP_EVENT  0x01
#define DRM_MODE_PAGE_FLIP_ASYNC  0x02

#define DRM_MODE_CONNECTOR_Unknown  0
#define DRM_MODE_CONNECTOR_HDMIA    11

#define DRM_MODE_ENCODER_NONE  0
#define DRM_MODE_ENCODER_TMDS  3

typedef struct _drmModeModeInfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char     name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo, *drmModeModeInfoPtr;

typedef struct {
    uint32_t fb_id;
    uint32_t count_fbs;
    uint32_t* fbs;
    uint32_t count_crtcs;
    uint32_t* crtcs;
    uint32_t count_connectors;
    uint32_t* connectors;
    uint32_t count_encoders;
    uint32_t* encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;

typedef struct {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drmModeEncoder, *drmModeEncoderPtr;

typedef struct {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mmWidth, mmHeight;
    uint32_t subpixel;
    int      count_modes;
    drmModeModeInfo* modes;
    int      count_props;
    uint32_t* props;
    uint64_t* prop_values;
    int      count_encoders;
    uint32_t* encoders;
} drmModeConnector, *drmModeConnectorPtr;

typedef struct {
    uint32_t crtc_id;
    int      buffer_id;
    uint32_t x, y;
    uint32_t width, height;
    int      mode_valid;
    drmModeModeInfo mode;
    int      gamma_size;
} drmModeCrtc, *drmModeCrtcPtr;
