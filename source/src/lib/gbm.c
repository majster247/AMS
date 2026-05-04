/**
 * @file gbm.c
 * @brief Minimal GBM implementation for AMS.
 *
 * Uses DRM dumb buffers via ioctl to allocate scanout-capable buffers.
 * This is sufficient for wlroots' GBM allocator backend.
 */

#include "drm/gbm.h"
#include "drm/drm.h"
#include "ams_syscall.h"
#include <stdlib.h>
#include <string.h>

struct gbm_device {
    int fd;
};

struct gbm_bo {
    struct gbm_device *gbm;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    uint32_t handle;
    uint64_t size;
    void    *user_data;
    void   (*destroy_user_data)(struct gbm_bo *, void *);
    void    *map_addr;
};

struct gbm_surface {
    struct gbm_device *gbm;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
    struct gbm_bo *front;
};

struct gbm_device *gbm_create_device(int fd) {
    struct gbm_device *dev = (struct gbm_device*)malloc(sizeof(struct gbm_device));
    if (!dev) return NULL;
    dev->fd = fd;
    return dev;
}

void gbm_device_destroy(struct gbm_device *gbm) {
    if (gbm) free(gbm);
}

int gbm_device_get_fd(struct gbm_device *gbm) {
    return gbm ? gbm->fd : -1;
}

int gbm_device_is_format_supported(struct gbm_device *gbm,
                                    uint32_t format, uint32_t usage) {
    (void)gbm; (void)usage;
    return (format == DRM_FORMAT_XRGB8888 || format == DRM_FORMAT_ARGB8888);
}

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm,
                              uint32_t width, uint32_t height,
                              uint32_t format, uint32_t flags) {
    (void)flags;
    if (!gbm) return NULL;

    struct drm_mode_create_dumb cd;
    memset(&cd, 0, sizeof(cd));
    cd.width = width;
    cd.height = height;
    cd.bpp = 32;

    int64_t ret = (int64_t)ams_syscall(16 /* SYS_IOCTL */, gbm->fd,
                                        DRM_IOCTL_MODE_CREATE_DUMB,
                                        (uint64_t)&cd, 0, 0);
    if (ret < 0) return NULL;

    struct gbm_bo *bo = (struct gbm_bo*)malloc(sizeof(struct gbm_bo));
    if (!bo) return NULL;
    memset(bo, 0, sizeof(*bo));
    bo->gbm = gbm;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->stride = cd.pitch;
    bo->handle = cd.handle;
    bo->size = cd.size;
    return bo;
}

void gbm_bo_destroy(struct gbm_bo *bo) {
    if (!bo) return;
    if (bo->destroy_user_data && bo->user_data)
        bo->destroy_user_data(bo, bo->user_data);

    if (bo->gbm) {
        struct drm_mode_destroy_dumb dd;
        dd.handle = bo->handle;
        ams_syscall(16, bo->gbm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, (uint64_t)&dd, 0, 0);
    }
    free(bo);
}

uint32_t gbm_bo_get_width(struct gbm_bo *bo)  { return bo ? bo->width : 0; }
uint32_t gbm_bo_get_height(struct gbm_bo *bo) { return bo ? bo->height : 0; }
uint32_t gbm_bo_get_stride(struct gbm_bo *bo) { return bo ? bo->stride : 0; }
uint32_t gbm_bo_get_format(struct gbm_bo *bo) { return bo ? bo->format : 0; }

union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo) {
    union gbm_bo_handle h;
    h.u32 = bo ? bo->handle : 0;
    return h;
}

int gbm_bo_get_fd(struct gbm_bo *bo) {
    (void)bo;
    return -1; /* PRIME export not supported */
}

void gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                            void (*destroy)(struct gbm_bo *, void *)) {
    if (!bo) return;
    bo->user_data = data;
    bo->destroy_user_data = destroy;
}

void *gbm_bo_get_user_data(struct gbm_bo *bo) {
    return bo ? bo->user_data : NULL;
}

int gbm_bo_write(struct gbm_bo *bo, const void *buf, uint64_t count) {
    (void)bo; (void)buf; (void)count;
    return -1;
}

struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                        uint32_t w, uint32_t h,
                                        uint32_t format, uint32_t flags) {
    struct gbm_surface *s = (struct gbm_surface*)malloc(sizeof(struct gbm_surface));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->gbm = gbm;
    s->width = w;
    s->height = h;
    s->format = format;
    s->flags = flags;
    return s;
}

void gbm_surface_destroy(struct gbm_surface *surface) {
    if (!surface) return;
    if (surface->front) gbm_bo_destroy(surface->front);
    free(surface);
}

struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface) {
    if (!surface) return NULL;
    if (!surface->front) {
        surface->front = gbm_bo_create(surface->gbm,
                                        surface->width, surface->height,
                                        surface->format, surface->flags);
    }
    return surface->front;
}

void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo) {
    (void)surface; (void)bo;
}

int gbm_surface_has_free_buffers(struct gbm_surface *surface) {
    (void)surface;
    return 1;
}
