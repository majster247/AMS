#ifndef _GBM_H
#define _GBM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GBM_BO_USE_SCANOUT       (1 << 0)
#define GBM_BO_USE_CURSOR        (1 << 1)
#define GBM_BO_USE_RENDERING     (1 << 2)
#define GBM_BO_USE_WRITE         (1 << 3)
#define GBM_BO_USE_LINEAR        (1 << 4)

#define GBM_FORMAT_ARGB8888      0x34325241
#define GBM_FORMAT_XRGB8888      0x34325258
#define GBM_FORMAT_ABGR8888      0x34324241
#define GBM_FORMAT_XBGR8888      0x34324258

#define GBM_BO_TRANSFER_READ     (1 << 0)
#define GBM_BO_TRANSFER_WRITE    (1 << 1)
#define GBM_BO_TRANSFER_READ_WRITE (GBM_BO_TRANSFER_READ | GBM_BO_TRANSFER_WRITE)

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

struct gbm_device* gbm_create_device(int fd);
void gbm_device_destroy(struct gbm_device* gbm);
int  gbm_device_get_fd(struct gbm_device* gbm);
int  gbm_device_is_format_supported(struct gbm_device* gbm,
    uint32_t format, uint32_t usage);
const char* gbm_device_get_backend_name(struct gbm_device* gbm);

struct gbm_bo* gbm_bo_create(struct gbm_device* gbm,
    uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
void    gbm_bo_destroy(struct gbm_bo* bo);
uint32_t gbm_bo_get_width(struct gbm_bo* bo);
uint32_t gbm_bo_get_height(struct gbm_bo* bo);
uint32_t gbm_bo_get_stride(struct gbm_bo* bo);
uint32_t gbm_bo_get_format(struct gbm_bo* bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo* bo);
int      gbm_bo_get_fd(struct gbm_bo* bo);
void*    gbm_bo_map(struct gbm_bo* bo, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t flags, uint32_t* stride,
    void** map_data);
void     gbm_bo_unmap(struct gbm_bo* bo, void* map_data);
void     gbm_bo_set_user_data(struct gbm_bo* bo, void* data,
    void (*destroy_user_data)(struct gbm_bo*, void*));
void*    gbm_bo_get_user_data(struct gbm_bo* bo);

union gbm_bo_handle {
    void*    ptr;
    int32_t  s32;
    uint32_t u32;
    int64_t  s64;
    uint64_t u64;
};

struct gbm_surface* gbm_surface_create(struct gbm_device* gbm,
    uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
void gbm_surface_destroy(struct gbm_surface* surface);
struct gbm_bo* gbm_surface_lock_front_buffer(struct gbm_surface* surface);
void gbm_surface_release_buffer(struct gbm_surface* surface, struct gbm_bo* bo);
int  gbm_surface_has_free_buffers(struct gbm_surface* surface);

#ifdef __cplusplus
}
#endif

#endif
