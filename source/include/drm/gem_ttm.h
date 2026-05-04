/**
 * @file drm/gem_ttm.h
 * @brief GEM (Graphics Execution Manager) + TTM (Translation Table Manager)
 *        kernel subsystem for AMS-OS.
 *
 * Provides:
 *  - GEM buffer objects backed by physical RAM pages (TTM placement)
 *  - KMS connector / CRTC / plane primitives over the multiboot2 linear framebuffer
 *  - DRM-fd interface exposing GEM_CREATE / GEM_MMAP / PAGE_FLIP ioctls
 *  - Scanout engine: blits a designated GEM BO to the hardware LFB on flip
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Configuration ---- */
#define GEM_MAX_BOS        256   /* max simultaneous GEM BOs                 */
#define GEM_MAX_HANDLES    64    /* max GEM handles per DRM fd                */
#define GEM_MMAP_BASE      0x600000000ULL  /* user-space mmap arena for GEM   */
#define GEM_MMAP_STRIDE    0x10000000ULL   /* 256 MB per BO slot               */

/* ---- TTM memory placement flags ---- */
#define TTM_PL_FLAG_VRAM   (1u << 0)  /* place in VRAM (framebuffer region)   */
#define TTM_PL_FLAG_TT     (1u << 1)  /* place in system RAM (TT aperture)    */
#define TTM_PL_FLAG_CACHED (1u << 2)  /* allow CPU caching                    */
#define TTM_PL_FLAG_WC     (1u << 3)  /* write-combine mapping                */

/* ---- GEM buffer object ---- */
typedef struct gem_bo {
    uint32_t  handle;         /* unique handle (1-based, 0=invalid)           */
    uint32_t  placement;      /* TTM_PL_FLAG_* bitmask                        */
    uint64_t  size;           /* byte size (page-aligned)                     */
    uint64_t  phys_base;      /* first physical page address                  */
    uint64_t  user_vaddr;     /* user-space virtual address after mmap        */
    uint32_t  refcount;
    uint32_t  width;          /* optional: surface geometry for KMS           */
    uint32_t  height;
    uint32_t  pitch;
    uint32_t  format;         /* DRM_FORMAT_* – 0 = raw/untyped               */
    int       dma_buf_fd;     /* -1 when not exported as dma-buf              */
    uint8_t   in_use;
} gem_bo;

/* ---- KMS connector state ---- */
typedef struct kms_connector {
    uint32_t  id;
    uint32_t  width_mm;
    uint32_t  height_mm;
    uint32_t  hdisplay;
    uint32_t  vdisplay;
    uint32_t  vrefresh;   /* mHz */
    uint8_t   connected;
} kms_connector;

/* ---- KMS CRTC state ---- */
typedef struct kms_crtc {
    uint32_t  id;
    uint32_t  active;
    uint32_t  scanout_bo_handle;  /* GEM handle of current scanout surface    */
    uint32_t  x, y;              /* scanout origin                            */
    uint32_t  mode_width;
    uint32_t  mode_height;
} kms_crtc;

/* ---- DRM ioctl numbers (custom, Linux-compatible offsets) ---- */
#define DRM_IOCTL_BASE             0x64  /* 'd' */

#define DRM_IOCTL_VERSION          _DRM_IO(DRM_IOCTL_BASE, 0x00)
#define DRM_IOCTL_GET_UNIQUE       _DRM_IO(DRM_IOCTL_BASE, 0x01)
#define DRM_IOCTL_GEM_CLOSE        _DRM_IO(DRM_IOCTL_BASE, 0x09)
#define DRM_IOCTL_GEM_FLINK        _DRM_IO(DRM_IOCTL_BASE, 0x0a)
#define DRM_IOCTL_GEM_OPEN         _DRM_IO(DRM_IOCTL_BASE, 0x0b)
#define DRM_IOCTL_GET_CAP          _DRM_IO(DRM_IOCTL_BASE, 0x0c)
#define DRM_IOCTL_MODE_GETRESOURCES _DRM_IO(DRM_IOCTL_BASE, 0xa0)
#define DRM_IOCTL_MODE_GETCRTC     _DRM_IO(DRM_IOCTL_BASE, 0xa1)
#define DRM_IOCTL_MODE_SETCRTC     _DRM_IO(DRM_IOCTL_BASE, 0xa2)
#define DRM_IOCTL_MODE_PAGE_FLIP   _DRM_IO(DRM_IOCTL_BASE, 0xb0)
#define DRM_IOCTL_MODE_CREATE_DUMB _DRM_IO(DRM_IOCTL_BASE, 0xb2)
#define DRM_IOCTL_MODE_MAP_DUMB    _DRM_IO(DRM_IOCTL_BASE, 0xb3)
#define DRM_IOCTL_MODE_DESTROY_DUMB _DRM_IO(DRM_IOCTL_BASE, 0xb4)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD _DRM_IO(DRM_IOCTL_BASE, 0x2d)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE _DRM_IO(DRM_IOCTL_BASE, 0x2e)

#define _DRM_IO(base, nr)   (((base) << 8) | (nr))

/* ---- ioctl argument structures ---- */

typedef struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
} drm_gem_close;

typedef struct drm_gem_flink {
    uint32_t handle;
    uint32_t name;
} drm_gem_flink;

typedef struct drm_gem_open {
    uint32_t name;
    uint32_t handle;
    uint64_t size;
} drm_gem_open;

typedef struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
} drm_get_cap;

typedef struct drm_mode_resources {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
} drm_mode_resources;

typedef struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[32];
} drm_mode_modeinfo;

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

typedef struct drm_mode_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
} drm_mode_page_flip;

typedef struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
} drm_prime_handle;

/* ---- Public API ---- */

/** @brief Initialise GEM/TTM subsystem – called from kernel init */
void gem_ttm_init(void);

/** @brief Allocate a new GEM BO of @p size bytes with @p placement flags */
gem_bo* gem_create(uint64_t size, uint32_t placement);

/** @brief Increment ref-count */
void gem_get(gem_bo* bo);

/** @brief Decrement ref-count; frees physical pages when it reaches zero */
void gem_put(gem_bo* bo);

/** @brief Look up a BO by handle (returns NULL if not found) */
gem_bo* gem_lookup(uint32_t handle);

/**
 * @brief Map a GEM BO into the current user task's address space.
 *        Uses write-combine physical mapping for performance.
 * @return User virtual address or 0 on failure.
 */
uint64_t gem_mmap_bo(gem_bo* bo);

/** @brief Page-flip: set @p bo as the new scanout and blit to LFB */
int kms_page_flip(uint32_t crtc_id, gem_bo* bo);

/** @brief DRM ioctl dispatcher – called from sys_ioctl for DRM fds */
uint64_t drm_ioctl(int drm_fd, uint64_t request, uint64_t arg_ptr);

/** @brief Called by sys_open when path is "/dev/dri/card0" */
int drm_open(void);

/** @brief Called by sys_close on a DRM fd */
void drm_close(int fd);

/** @brief Return the singleton KMS connector (primary display) */
kms_connector* kms_get_connector(void);

/** @brief Return the singleton KMS CRTC */
kms_crtc* kms_get_crtc(void);

#ifdef __cplusplus
}
#endif
