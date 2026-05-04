/**
 * Minimal GBM (Generic Buffer Management) for AMS.
 *
 * Allocates dumb buffers through our DRM ioctl layer.
 * Provides enough API surface for wlroots allocator.
 */

#include "gbm.h"
#include <stdint.h>
#include <stddef.h>

extern uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2,
                            uint64_t p3, uint64_t p4, uint64_t p5);
extern void* malloc(size_t size);
extern void  free(void* ptr);
extern void* memset(void* dest, int ch, size_t n);
extern int   ioctl(int fd, unsigned long request, ...);

#define SYS_MMAP 9
#define DRM_IOCTL_MODE_CREATE_DUMB  (0xC0000000u | (0x64u << 8) | 0xB2u)
#define DRM_IOCTL_MODE_MAP_DUMB     (0xC0000000u | (0x64u << 8) | 0xB3u)
#define DRM_IOCTL_MODE_DESTROY_DUMB (0xC0000000u | (0x64u << 8) | 0xB4u)

#define MAX_GBM_BO 64
#define GBM_SURFACE_BUFFERS 3

struct drm_create_dumb_req {
    uint32_t height, width, bpp, flags, handle, pitch;
    uint64_t size;
};

struct drm_map_dumb_req {
    uint32_t handle, pad;
    uint64_t offset;
};

struct drm_destroy_dumb_req {
    uint32_t handle;
};

struct gbm_device {
    int fd;
    int in_use;
};

struct gbm_bo {
    int in_use;
    struct gbm_device* device;
    uint32_t width, height, stride, format;
    uint32_t handle;
    uint64_t size;
    void*    mapped;
    void*    user_data;
    void     (*destroy_user_data)(struct gbm_bo*, void*);
};

struct gbm_surface {
    struct gbm_device* device;
    uint32_t width, height, format;
    struct gbm_bo* buffers[GBM_SURFACE_BUFFERS];
    int front;
    int in_use;
};

static struct gbm_device g_gbm_dev;
static struct gbm_bo g_bos[MAX_GBM_BO];
static struct gbm_surface g_surfaces[4];

struct gbm_device* gbm_create_device(int fd) {
    g_gbm_dev.fd = fd;
    g_gbm_dev.in_use = 1;
    return &g_gbm_dev;
}

void gbm_device_destroy(struct gbm_device* gbm) {
    if (gbm) gbm->in_use = 0;
}

int gbm_device_get_fd(struct gbm_device* gbm) {
    return gbm ? gbm->fd : -1;
}

int gbm_device_is_format_supported(struct gbm_device* gbm,
    uint32_t format, uint32_t usage)
{
    (void)gbm; (void)usage;
    return (format == GBM_FORMAT_ARGB8888 || format == GBM_FORMAT_XRGB8888) ? 1 : 0;
}

const char* gbm_device_get_backend_name(struct gbm_device* gbm) {
    (void)gbm;
    return "ams-drm";
}

static struct gbm_bo* alloc_bo(void) {
    for (int i = 0; i < MAX_GBM_BO; i++)
        if (!g_bos[i].in_use) return &g_bos[i];
    return (struct gbm_bo*)0;
}

struct gbm_bo* gbm_bo_create(struct gbm_device* gbm,
    uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
    (void)flags;
    if (!gbm) return (struct gbm_bo*)0;

    struct drm_create_dumb_req req;
    memset(&req, 0, sizeof(req));
    req.width = width;
    req.height = height;
    req.bpp = 32;

    if (ioctl(gbm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &req) < 0)
        return (struct gbm_bo*)0;

    struct gbm_bo* bo = alloc_bo();
    if (!bo) return (struct gbm_bo*)0;

    memset(bo, 0, sizeof(struct gbm_bo));
    bo->in_use = 1;
    bo->device = gbm;
    bo->width = width;
    bo->height = height;
    bo->stride = req.pitch;
    bo->format = format;
    bo->handle = req.handle;
    bo->size = req.size;
    return bo;
}

void gbm_bo_destroy(struct gbm_bo* bo) {
    if (!bo || !bo->in_use) return;
    if (bo->destroy_user_data && bo->user_data)
        bo->destroy_user_data(bo, bo->user_data);

    struct drm_destroy_dumb_req req;
    req.handle = bo->handle;
    ioctl(bo->device->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
    bo->in_use = 0;
}

uint32_t gbm_bo_get_width(struct gbm_bo* bo) { return bo ? bo->width : 0; }
uint32_t gbm_bo_get_height(struct gbm_bo* bo) { return bo ? bo->height : 0; }
uint32_t gbm_bo_get_stride(struct gbm_bo* bo) { return bo ? bo->stride : 0; }
uint32_t gbm_bo_get_format(struct gbm_bo* bo) { return bo ? bo->format : 0; }

union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo* bo) {
    union gbm_bo_handle h;
    h.u32 = bo ? bo->handle : 0;
    return h;
}

int gbm_bo_get_fd(struct gbm_bo* bo) {
    (void)bo;
    return -1; /* PRIME export not supported */
}

void* gbm_bo_map(struct gbm_bo* bo, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t flags, uint32_t* stride,
    void** map_data)
{
    (void)x; (void)y; (void)width; (void)height; (void)flags;
    if (!bo || !bo->in_use) return (void*)0;

    if (bo->mapped) {
        if (stride) *stride = bo->stride;
        if (map_data) *map_data = bo->mapped;
        return bo->mapped;
    }

    struct drm_map_dumb_req req;
    memset(&req, 0, sizeof(req));
    req.handle = bo->handle;
    if (ioctl(bo->device->fd, DRM_IOCTL_MODE_MAP_DUMB, &req) < 0)
        return (void*)0;

    void* ptr = (void*)ams_syscall(SYS_MMAP, 0, bo->size,
        0x1 | 0x2, /* PROT_READ | PROT_WRITE */
        0x01, /* MAP_SHARED */
        (uint64_t)bo->device->fd);

    if ((uint64_t)ptr > (uint64_t)-4096ULL) return (void*)0;

    bo->mapped = ptr;
    if (stride) *stride = bo->stride;
    if (map_data) *map_data = ptr;
    return ptr;
}

void gbm_bo_unmap(struct gbm_bo* bo, void* map_data) {
    (void)bo; (void)map_data;
}

void gbm_bo_set_user_data(struct gbm_bo* bo, void* data,
    void (*destroy_user_data)(struct gbm_bo*, void*))
{
    if (!bo) return;
    bo->user_data = data;
    bo->destroy_user_data = destroy_user_data;
}

void* gbm_bo_get_user_data(struct gbm_bo* bo) {
    return bo ? bo->user_data : (void*)0;
}

/* Surface (triple-buffered ring) */
struct gbm_surface* gbm_surface_create(struct gbm_device* gbm,
    uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
    struct gbm_surface* s = (struct gbm_surface*)0;
    for (int i = 0; i < 4; i++) {
        if (!g_surfaces[i].in_use) { s = &g_surfaces[i]; break; }
    }
    if (!s) return (struct gbm_surface*)0;

    memset(s, 0, sizeof(struct gbm_surface));
    s->device = gbm;
    s->width = width;
    s->height = height;
    s->format = format;
    s->in_use = 1;

    for (int i = 0; i < GBM_SURFACE_BUFFERS; i++) {
        s->buffers[i] = gbm_bo_create(gbm, width, height, format, flags);
    }
    return s;
}

void gbm_surface_destroy(struct gbm_surface* surface) {
    if (!surface) return;
    for (int i = 0; i < GBM_SURFACE_BUFFERS; i++) {
        if (surface->buffers[i]) gbm_bo_destroy(surface->buffers[i]);
    }
    surface->in_use = 0;
}

struct gbm_bo* gbm_surface_lock_front_buffer(struct gbm_surface* surface) {
    if (!surface) return (struct gbm_bo*)0;
    struct gbm_bo* bo = surface->buffers[surface->front];
    surface->front = (surface->front + 1) % GBM_SURFACE_BUFFERS;
    return bo;
}

void gbm_surface_release_buffer(struct gbm_surface* surface, struct gbm_bo* bo) {
    (void)surface; (void)bo;
}

int gbm_surface_has_free_buffers(struct gbm_surface* surface) {
    (void)surface;
    return 1;
}
