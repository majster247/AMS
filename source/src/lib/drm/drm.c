/* AMS userspace libdrm implementation — wraps /dev/dri/card0 ioctls. */
#include "drm.h"
#include "drm_mode.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* AMS syscall stubs */
static long ams_open(const char* path, int flags) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(2), "D"(path), "S"(flags)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long ams_close(int fd) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "0"(3), "D"((long)fd) : "rcx", "r11");
    return ret;
}

static long ams_ioctl(int fd, unsigned long req, void* arg) {
    long ret;
    register long r10 __asm__("r10") = 0;
    register long r8  __asm__("r8")  = 0;
    register long r9  __asm__("r9")  = 0;
    (void)r10; (void)r8; (void)r9;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(16), "D"((long)fd), "S"(req), "d"((long)arg)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* Ioctl wire structs (mirror drm_virt.h kernel types) */
typedef struct { uint64_t capability; uint64_t value; } ams_drm_get_cap;
typedef struct { uint64_t capability; uint64_t value; } ams_drm_set_cap;
typedef struct {
    uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
    uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
    uint32_t min_width, max_width, min_height, max_height;
} ams_drm_res;
typedef struct { uint32_t encoder_id; uint32_t encoder_type; uint32_t crtc_id; uint32_t possible_crtcs; uint32_t possible_clones; } ams_drm_encoder;
typedef struct {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh, flags, type;
    char name[32];
} ams_drm_modeinfo;
typedef struct {
    uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
    uint32_t count_modes, count_props, count_encoders;
    uint32_t encoder_id, connector_id, connector_type, connector_type_id;
    uint32_t connection, mm_width, mm_height, subpixel, pad;
} ams_drm_connector;
typedef struct {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
    ams_drm_modeinfo mode;
} ams_drm_crtc;
typedef struct { uint32_t fb_id, width, height, pitch, bpp, depth, handle; } ams_drm_addfb;
typedef struct { uint32_t crtc_id, fb_id, flags, reserved; uint64_t user_data; } ams_drm_flip;
typedef struct { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; } ams_drm_dumb_create;
typedef struct { uint32_t handle, pad; uint64_t offset; } ams_drm_dumb_map;
typedef struct { uint32_t handle; } ams_drm_dumb_destroy;
typedef struct { uint32_t handle; uint32_t flags; int fd; } ams_drm_prime;

/* ioctl nr → request (simplified encoding) */
#define DRM_REQ(nr, sz) ((0xC000ULL | (((uint64_t)(sz)) << 16) | (((uint64_t)(nr)) << 8) | 'd'))

int drmOpen(const char* name, const char* busid) {
    (void)name; (void)busid;
    return (int)ams_open("/dev/dri/card0", 2 /*O_RDWR*/);
}

int drmClose(int fd) {
    return (int)ams_close(fd);
}

int drmGetMagic(int fd, drm_magic_t* magic) {
    (void)fd;
    if (magic) *magic = 1;
    return 0;
}

int drmAuthMagic(int fd, drm_magic_t magic) {
    (void)magic;
    uint32_t m = (uint32_t)magic;
    return (int)ams_ioctl(fd, DRM_REQ(0x11, sizeof(m)), &m);
}

int drmGetCap(int fd, uint64_t capability, uint64_t* value) {
    ams_drm_get_cap gc = { capability, 0 };
    long r = ams_ioctl(fd, DRM_REQ(0x0C, sizeof(gc)), &gc);
    if (r == 0 && value) *value = gc.value;
    return (int)r;
}

int drmSetClientCap(int fd, uint64_t capability, uint64_t value) {
    ams_drm_set_cap sc = { capability, value };
    return (int)ams_ioctl(fd, DRM_REQ(0x0D, sizeof(sc)), &sc);
}

drmModeResPtr drmModeGetResources(int fd) {
    ams_drm_res res;
    memset(&res, 0, sizeof(res));
    /* First call: get counts */
    ams_ioctl(fd, DRM_REQ(0xA0, sizeof(res)), &res);

    drmModeResPtr r = (drmModeResPtr)malloc(sizeof(drmModeRes));
    if (!r) return NULL;
    memset(r, 0, sizeof(*r));

    r->count_crtcs      = res.count_crtcs;
    r->count_connectors  = res.count_connectors;
    r->count_encoders    = res.count_encoders;
    r->min_width         = res.min_width;
    r->max_width         = res.max_width;
    r->min_height        = res.min_height;
    r->max_height        = res.max_height;

    /* Allocate and fill ID arrays */
    if (res.count_crtcs) {
        r->crtcs = (uint32_t*)malloc(res.count_crtcs * sizeof(uint32_t));
        res.crtc_id_ptr = (uint64_t)r->crtcs;
    }
    if (res.count_connectors) {
        r->connectors = (uint32_t*)malloc(res.count_connectors * sizeof(uint32_t));
        res.connector_id_ptr = (uint64_t)r->connectors;
    }
    if (res.count_encoders) {
        r->encoders = (uint32_t*)malloc(res.count_encoders * sizeof(uint32_t));
        res.encoder_id_ptr = (uint64_t)r->encoders;
    }
    ams_ioctl(fd, DRM_REQ(0xA0, sizeof(res)), &res);
    return r;
}

void drmModeFreeResources(drmModeResPtr ptr) {
    if (!ptr) return;
    free(ptr->fbs); free(ptr->crtcs); free(ptr->connectors); free(ptr->encoders);
    free(ptr);
}

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connector_id) {
    ams_drm_connector c;
    memset(&c, 0, sizeof(c));
    c.connector_id = connector_id;
    ams_ioctl(fd, DRM_REQ(0xA7, sizeof(c)), &c);

    drmModeConnectorPtr r = (drmModeConnectorPtr)malloc(sizeof(drmModeConnector));
    if (!r) return NULL;
    memset(r, 0, sizeof(*r));
    r->connector_id   = c.connector_id;
    r->encoder_id     = c.encoder_id;
    r->connector_type = c.connector_type;
    r->connector_type_id = c.connector_type_id;
    r->connection     = c.connection;
    r->mmWidth        = c.mm_width;
    r->mmHeight       = c.mm_height;
    r->count_modes    = (int)c.count_modes;
    r->count_encoders = (int)c.count_encoders;

    if (r->count_modes > 0) {
        r->modes = (drmModeModeInfo*)malloc((size_t)r->count_modes * sizeof(drmModeModeInfo));
        c.modes_ptr = (uint64_t)r->modes;
    }
    if (r->count_encoders > 0) {
        r->encoders = (uint32_t*)malloc((size_t)r->count_encoders * sizeof(uint32_t));
        c.encoders_ptr = (uint64_t)r->encoders;
    }
    ams_ioctl(fd, DRM_REQ(0xA7, sizeof(c)), &c);

    /* Copy mode data */
    if (r->modes && c.modes_ptr) {
        ams_drm_modeinfo* mi = (ams_drm_modeinfo*)(uint64_t)c.modes_ptr;
        for (int i = 0; i < r->count_modes; i++) {
            r->modes[i].clock       = mi[i].clock;
            r->modes[i].hdisplay    = mi[i].hdisplay;
            r->modes[i].vdisplay    = mi[i].vdisplay;
            r->modes[i].vrefresh    = mi[i].vrefresh;
            r->modes[i].flags       = mi[i].flags;
            r->modes[i].type        = mi[i].type;
            memcpy(r->modes[i].name, mi[i].name, DRM_DISPLAY_MODE_LEN);
        }
    }
    return r;
}

void drmModeFreeConnector(drmModeConnectorPtr ptr) {
    if (!ptr) return;
    free(ptr->modes); free(ptr->props); free(ptr->prop_values); free(ptr->encoders);
    free(ptr);
}

drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t encoder_id) {
    ams_drm_encoder e;
    memset(&e, 0, sizeof(e));
    e.encoder_id = encoder_id;
    ams_ioctl(fd, DRM_REQ(0xA6, sizeof(e)), &e);

    drmModeEncoderPtr r = (drmModeEncoderPtr)malloc(sizeof(drmModeEncoder));
    if (!r) return NULL;
    r->encoder_id      = e.encoder_id;
    r->encoder_type    = e.encoder_type;
    r->crtc_id         = e.crtc_id;
    r->possible_crtcs  = e.possible_crtcs;
    r->possible_clones = e.possible_clones;
    return r;
}

void drmModeFreeEncoder(drmModeEncoderPtr ptr) { free(ptr); }

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtc_id) {
    ams_drm_crtc c;
    memset(&c, 0, sizeof(c));
    c.crtc_id = crtc_id;
    ams_ioctl(fd, DRM_REQ(0xA1, sizeof(c)), &c);

    drmModeCrtcPtr r = (drmModeCrtcPtr)malloc(sizeof(drmModeCrtc));
    if (!r) return NULL;
    memset(r, 0, sizeof(*r));
    r->crtc_id    = c.crtc_id;
    r->buffer_id  = (int)c.fb_id;
    r->x          = c.x;
    r->y          = c.y;
    r->mode_valid = (int)c.mode_valid;
    r->gamma_size = (int)c.gamma_size;
    if (c.mode_valid) {
        r->mode.clock    = c.mode.clock;
        r->mode.hdisplay = c.mode.hdisplay;
        r->mode.vdisplay = c.mode.vdisplay;
        r->mode.vrefresh = c.mode.vrefresh;
        r->mode.flags    = c.mode.flags;
        r->mode.type     = c.mode.type;
        memcpy(r->mode.name, c.mode.name, DRM_DISPLAY_MODE_LEN);
    }
    return r;
}

int drmModeSetCrtc(int fd, uint32_t crtc_id, uint32_t fb_id,
                   uint32_t x, uint32_t y, uint32_t* connectors, int count,
                   drmModeModeInfoPtr mode) {
    ams_drm_crtc c;
    memset(&c, 0, sizeof(c));
    c.crtc_id             = crtc_id;
    c.fb_id               = fb_id;
    c.x                   = x;
    c.y                   = y;
    c.set_connectors_ptr  = (uint64_t)connectors;
    c.count_connectors    = (uint32_t)count;
    c.mode_valid          = (mode != NULL) ? 1 : 0;
    if (mode) {
        c.mode.clock    = mode->clock;
        c.mode.hdisplay = mode->hdisplay;
        c.mode.vdisplay = mode->vdisplay;
        c.mode.vrefresh = mode->vrefresh;
        c.mode.flags    = mode->flags;
        c.mode.type     = mode->type;
        memcpy(c.mode.name, mode->name, DRM_DISPLAY_MODE_LEN);
    }
    return (int)ams_ioctl(fd, DRM_REQ(0xA2, sizeof(c)), &c);
}

void drmModeFreeCrtc(drmModeCrtcPtr ptr) { free(ptr); }

int drmModeAddFB(int fd, uint32_t width, uint32_t height,
                 uint8_t depth, uint8_t bpp, uint32_t pitch,
                 uint32_t bo_handle, uint32_t* buf_id) {
    ams_drm_addfb fb = { 0, width, height, pitch, bpp, depth, bo_handle };
    long r = ams_ioctl(fd, DRM_REQ(0xAE, sizeof(fb)), &fb);
    if (r == 0 && buf_id) *buf_id = fb.fb_id;
    return (int)r;
}

int drmModeRmFB(int fd, uint32_t fb_id) {
    return (int)ams_ioctl(fd, DRM_REQ(0xAF, sizeof(fb_id)), &fb_id);
}

int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
                    uint32_t flags, void* user_data) {
    ams_drm_flip pf = { crtc_id, fb_id, flags, 0, (uint64_t)user_data };
    return (int)ams_ioctl(fd, DRM_REQ(0xB0, sizeof(pf)), &pf);
}

int drmModeCreateDumbBuffer(int fd, uint32_t width, uint32_t height,
                             uint32_t bpp, uint32_t flags,
                             uint32_t* handle, uint32_t* pitch, uint64_t* size) {
    ams_drm_dumb_create cd = { height, width, bpp, flags, 0, 0, 0 };
    long r = ams_ioctl(fd, DRM_REQ(0xB2, sizeof(cd)), &cd);
    if (r == 0) {
        if (handle) *handle = cd.handle;
        if (pitch)  *pitch  = cd.pitch;
        if (size)   *size   = cd.size;
    }
    return (int)r;
}

int drmModeDestroyDumbBuffer(int fd, uint32_t handle) {
    ams_drm_dumb_destroy dd = { handle };
    return (int)ams_ioctl(fd, DRM_REQ(0xB4, sizeof(dd)), &dd);
}

int drmModeMapDumbBuffer(int fd, uint32_t handle, uint64_t* offset) {
    ams_drm_dumb_map md = { handle, 0, 0 };
    long r = ams_ioctl(fd, DRM_REQ(0xB3, sizeof(md)), &md);
    if (r == 0 && offset) *offset = md.offset;
    return (int)r;
}

int drmIoctl(int fd, unsigned long request, void* arg) {
    return (int)ams_ioctl(fd, (unsigned long long)request, arg);
}

int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int* prime_fd) {
    ams_drm_prime p = { handle, flags, -1 };
    long r = ams_ioctl(fd, DRM_REQ(0x2D, sizeof(p)), &p);
    if (r == 0 && prime_fd) *prime_fd = p.fd;
    return (int)r;
}

int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t* handle) {
    ams_drm_prime p = { 0, 0, prime_fd };
    long r = ams_ioctl(fd, DRM_REQ(0x2E, sizeof(p)), &p);
    if (r == 0 && handle) *handle = p.handle;
    return (int)r;
}
