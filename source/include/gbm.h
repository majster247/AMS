/**
 * @file gbm.h
 * @brief Generic Buffer Manager (GBM) interface for AMS-OS
 *
 * GBM is the abstraction layer between Mesa EGL and the DRM/KMS
 * kernel subsystem. It provides buffer allocation that can be
 * used for both rendering (via EGL) and scanout (via KMS).
 *
 * This header is compatible with Mesa's GBM expectations.
 */

#ifndef _GBM_H_
#define _GBM_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GBM buffer object flags */
#define GBM_BO_USE_SCANOUT      (1 << 0)
#define GBM_BO_USE_CURSOR       (1 << 1)
#define GBM_BO_USE_RENDERING    (1 << 2)
#define GBM_BO_USE_WRITE        (1 << 3)
#define GBM_BO_USE_LINEAR       (1 << 4)

/* GBM formats (DRM fourcc compatible) */
#define GBM_FORMAT_XRGB8888     0x34325258  /* XR24 */
#define GBM_FORMAT_ARGB8888     0x34325241  /* AR24 */
#define GBM_FORMAT_RGB888       0x34324752  /* RG24 */
#define GBM_FORMAT_RGB565       0x36314752  /* RG16 */
#define GBM_FORMAT_XBGR8888     0x34324258  /* XB24 */
#define GBM_FORMAT_ABGR8888     0x34324241  /* AB24 */

/* GBM BO transfer flags for map/unmap */
#define GBM_BO_TRANSFER_READ            (1 << 0)
#define GBM_BO_TRANSFER_WRITE           (1 << 1)
#define GBM_BO_TRANSFER_READ_WRITE      (GBM_BO_TRANSFER_READ | GBM_BO_TRANSFER_WRITE)

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

/* Device creation/destruction */
struct gbm_device *gbm_create_device(int fd);
void gbm_device_destroy(struct gbm_device *gbm);
int gbm_device_get_fd(struct gbm_device *gbm);
const char *gbm_device_get_backend_name(struct gbm_device *gbm);
int gbm_device_is_format_supported(struct gbm_device *gbm,
                                    uint32_t format, uint32_t usage);

/* Buffer object operations */
struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                              uint32_t width, uint32_t height,
                              uint32_t format, uint32_t flags);
void gbm_bo_destroy(struct gbm_bo *bo);

uint32_t gbm_bo_get_width(struct gbm_bo *bo);
uint32_t gbm_bo_get_height(struct gbm_bo *bo);
uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t gbm_bo_get_format(struct gbm_bo *bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
int gbm_bo_get_fd(struct gbm_bo *bo);
struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo);

void gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                           void (*destroy_user_data)(struct gbm_bo *, void *));
void *gbm_bo_get_user_data(struct gbm_bo *bo);

void *gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height, uint32_t flags,
                  uint32_t *stride, void **map_data);
void gbm_bo_unmap(struct gbm_bo *bo, void *map_data);
int gbm_bo_write(struct gbm_bo *bo, const void *buf, size_t count);

/* Surface operations (for EGL integration) */
struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                        uint32_t width, uint32_t height,
                                        uint32_t format, uint32_t flags);
void gbm_surface_destroy(struct gbm_surface *surface);
struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface);
void gbm_surface_release_buffer(struct gbm_surface *surface,
                                 struct gbm_bo *bo);
int gbm_surface_has_free_buffers(struct gbm_surface *surface);

/* BO handle union */
union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
};

#ifdef __cplusplus
}
#endif

#endif /* _GBM_H_ */
