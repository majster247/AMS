/**
 * @file drm.h
 * @brief AMS-OS DRM/KMS subsystem with GEM/TTM buffer management
 *
 * Provides a minimal DRM (Direct Rendering Manager) implementation
 * compatible with the Linux DRM/KMS ABI, enabling:
 *   - KMS (Kernel Mode Setting) for display configuration
 *   - GEM (Graphics Execution Manager) for GPU buffer objects
 *   - TTM (Translation Table Manager) for buffer placement
 *   - Dumb buffer allocation for software rendering
 *   - DRM framebuffer management
 */

#ifndef AMS_DRM_H
#define AMS_DRM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- DRM ioctl numbers --- */
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
#define DRM_MODE_CONNECTOR_HDMIA   11
#define DRM_MODE_CONNECTOR_VIRTUAL 15

/* Mode type flags */
#define DRM_MODE_TYPE_PREFERRED    (1 << 3)
#define DRM_MODE_TYPE_DRIVER       (1 << 6)

/* Mode flags */
#define DRM_MODE_FLAG_PHSYNC       (1 << 0)
#define DRM_MODE_FLAG_NHSYNC       (1 << 1)
#define DRM_MODE_FLAG_PVSYNC       (1 << 2)
#define DRM_MODE_FLAG_NVSYNC       (1 << 3)

/* Page flip flags */
#define DRM_MODE_PAGE_FLIP_EVENT   0x01

/* GEM object (kernel side) */
#define DRM_GEM_MAX_OBJECTS 64

struct drm_gem_object {
    uint32_t handle;
    uint32_t name;         /* flink global name */
    uint64_t size;
    void*    vaddr;        /* kernel virtual address */
    uint64_t phys_addr;    /* physical backing */
    uint32_t ref_count;
    uint32_t in_use;
};

/* TTM placement */
#define TTM_PL_SYSTEM  0
#define TTM_PL_VRAM    1
#define TTM_PL_TT      2   /* system pages mapped via GART/GTT */

struct drm_ttm_buffer {
    uint32_t placement;    /* TTM_PL_* */
    uint32_t gem_handle;
    uint64_t size;
    void*    pages;        /* page list */
    uint32_t num_pages;
    uint32_t in_use;
};

/* Mode info */
struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal;
    uint32_t flags;
    uint32_t type;
    char name[32];
};

/* Resources */
struct drm_mode_card_res {
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t count_fbs;
    uint32_t crtc_ids[8];
    uint32_t connector_ids[8];
    uint32_t encoder_ids[8];
};

/* Connector */
struct drm_mode_get_connector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connection;
    uint32_t count_modes;
    struct drm_mode_modeinfo modes[16];
    uint32_t mm_width, mm_height;
};

/* Encoder */
struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

/* CRTC */
struct drm_mode_crtc {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

/* Dumb buffer create */
struct drm_mode_create_dumb {
    uint32_t width;
    uint32_t height;
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

/* Framebuffer */
struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

/* Page flip */
struct drm_mode_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

/* GEM close */
struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
};

/* GEM open (by name) */
struct drm_gem_open {
    uint32_t name;
    uint32_t handle;
    uint64_t size;
};

/* GEM flink */
struct drm_gem_flink {
    uint32_t handle;
    uint32_t name;
};

/* PRIME handle<->fd */
struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
};

/* DRM version */
struct drm_version {
    int32_t version_major;
    int32_t version_minor;
    int32_t version_patchlevel;
    uint32_t name_len;
    char name[64];
    uint32_t date_len;
    char date[32];
    uint32_t desc_len;
    char desc[128];
};

/* --- DRM subsystem state --- */
struct drm_device {
    uint32_t initialized;

    /* GEM objects */
    struct drm_gem_object gems[DRM_GEM_MAX_OBJECTS];
    uint32_t next_gem_handle;
    uint32_t next_gem_name;

    /* TTM buffers */
    struct drm_ttm_buffer ttm_bufs[DRM_GEM_MAX_OBJECTS];

    /* KMS state */
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    void*    fb_base;       /* mapped LFB */
    uint32_t active_fb_id;
    uint32_t next_fb_id;

    /* Framebuffer objects */
    struct drm_mode_fb_cmd fbs[16];
    uint32_t fb_count;

    /* CRTC state */
    struct drm_mode_crtc crtcs[4];
    uint32_t crtc_count;

    /* Master state */
    int master_fd;
};

/* Global DRM device */
extern struct drm_device g_drm_dev;

/* Initialize DRM subsystem from Multiboot framebuffer info */
void drm_init(uint64_t fb_addr, uint32_t width, uint32_t height,
              uint32_t pitch, uint8_t bpp);

/* Handle DRM ioctl from userspace */
int64_t drm_ioctl(int fd, uint64_t request, uint64_t arg);

/* GEM operations */
uint32_t drm_gem_create(uint64_t size);
void     drm_gem_destroy(uint32_t handle);
struct drm_gem_object* drm_gem_lookup(uint32_t handle);

/* TTM operations */
int drm_ttm_alloc(uint32_t gem_handle, uint32_t placement);
void drm_ttm_free(uint32_t gem_handle);

#ifdef __cplusplus
}
#endif

#endif /* AMS_DRM_H */
