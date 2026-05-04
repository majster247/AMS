/**
 * @file drm/gbm.h
 * @brief Generic Buffer Manager UAPI header for AMS.
 *
 * Provides a minimal GBM interface sufficient for wlroots to allocate
 * dumb/scanout buffers via the DRM dumb buffer path.
 */
#ifndef _AMS_GBM_H
#define _AMS_GBM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gbm_device;
struct gbm_surface;
struct gbm_bo;

enum gbm_bo_format {
    GBM_BO_FORMAT_XRGB8888 = 0,
    GBM_BO_FORMAT_ARGB8888 = 1,
};

enum gbm_bo_flags {
    GBM_BO_USE_SCANOUT      = (1 << 0),
    GBM_BO_USE_CURSOR       = (1 << 1),
    GBM_BO_USE_RENDERING    = (1 << 2),
    GBM_BO_USE_WRITE        = (1 << 3),
    GBM_BO_USE_LINEAR       = (1 << 4),
};

struct gbm_device *gbm_create_device(int fd);
void               gbm_device_destroy(struct gbm_device *gbm);
int                gbm_device_get_fd(struct gbm_device *gbm);
int                gbm_device_is_format_supported(struct gbm_device *gbm,
                                                   uint32_t format,
                                                   uint32_t usage);

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                              uint32_t width, uint32_t height,
                              uint32_t format, uint32_t flags);
void           gbm_bo_destroy(struct gbm_bo *bo);
uint32_t       gbm_bo_get_width(struct gbm_bo *bo);
uint32_t       gbm_bo_get_height(struct gbm_bo *bo);
uint32_t       gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t       gbm_bo_get_format(struct gbm_bo *bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
int            gbm_bo_get_fd(struct gbm_bo *bo);
void           gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                                     void (*destroy_user_data)(struct gbm_bo *, void *));
void *         gbm_bo_get_user_data(struct gbm_bo *bo);
int            gbm_bo_write(struct gbm_bo *bo, const void *buf, uint64_t count);

union gbm_bo_handle {
    void     *ptr;
    int32_t   s32;
    uint32_t  u32;
    int64_t   s64;
    uint64_t  u64;
};

struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                        uint32_t width, uint32_t height,
                                        uint32_t format, uint32_t flags);
void                gbm_surface_destroy(struct gbm_surface *surface);
struct gbm_bo *     gbm_surface_lock_front_buffer(struct gbm_surface *surface);
void                gbm_surface_release_buffer(struct gbm_surface *surface,
                                                struct gbm_bo *bo);
int                 gbm_surface_has_free_buffers(struct gbm_surface *surface);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_GBM_H */
