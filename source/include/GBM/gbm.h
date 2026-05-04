/*
 * Minimal GBM ABI shim for AMS.
 */

#ifndef AMS_GBM_H
#define AMS_GBM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gbm_device;
struct gbm_surface;
struct gbm_bo;

#define GBM_FORMAT_ARGB8888 0x34325241u
#define GBM_FORMAT_XRGB8888 0x34325258u

#define GBM_BO_USE_RENDERING (1 << 2)
#define GBM_BO_USE_LINEAR    (1 << 4)

struct gbm_device   *gbm_create_device(int fd);
void                 gbm_device_destroy(struct gbm_device *gbm);
int                  gbm_device_get_fd(struct gbm_device *gbm);

struct gbm_surface  *gbm_surface_create(struct gbm_device *gbm, uint32_t w, uint32_t h, uint32_t format, uint32_t flags);
void                 gbm_surface_destroy(struct gbm_surface *surface);
struct gbm_bo       *gbm_surface_lock_front_buffer(struct gbm_surface *surface);
int                  gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo);
uint32_t             gbm_bo_get_width(struct gbm_bo *bo);
uint32_t             gbm_bo_get_height(struct gbm_bo *bo);
uint32_t             gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t             gbm_bo_get_handle(struct gbm_bo *bo);

#ifdef __cplusplus
}
#endif

#endif
