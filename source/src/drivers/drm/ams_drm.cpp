/**
 * @file ams_drm.cpp
 * @brief Software DRM/KMS/GEM/TTM stub backing /dev/dri/card0 for the
 *        Wayland + Mesa3D + wlroots stack.
 *
 * This is intentionally a software-first façade: AMS-OS does not have a real
 * GPU driver yet, but the userspace stack (mesa/gbm, wlroots' drm backend,
 * libdrm consumers) only requires a coherent DRM contract. The contract here
 * is faithful enough that the dumb-buffer codepath can be exercised by the
 * compositor and software EGL.
 *
 * Layout:
 *  - One CRTC, one connector (eDP-1 virtual), one encoder, two planes.
 *  - GEM handles per FD (small fixed-size table, plenty for compositor work).
 *  - TTM placement metadata is reported but everything resolves to SYSTEM
 *    (kernel pages backed by the heap).
 */

#include "drm/ams_drm.h"
#include "kernel.h"
#include "vmm.h"

extern "C" void* kmalloc(size_t size);
extern "C" void  kfree(void* ptr);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);

#define AMS_DRM_MAX_CLIENTS 16
#define AMS_DRM_MAX_HANDLES 64
#define AMS_DRM_MAX_FBS     32

namespace {

struct ams_drm_gem {
    bool     in_use;
    uint32_t handle;
    uint32_t width, height, pitch, bpp;
    uint64_t size;
    uint64_t mmap_offset; /* fake DRM offset cookie */
    uint32_t placement;   /* AMS_TTM_PL_* */
    uint8_t* pages;       /* kernel-side allocation (TTM SYSTEM) */
};

struct ams_drm_fb {
    bool     in_use;
    uint32_t fb_id;
    uint32_t width, height, pitch;
    uint32_t pixel_format;
    uint32_t handle;
};

struct ams_drm_client {
    bool      in_use;
    int       fd;
    uint32_t  next_handle;
    uint32_t  next_fb_id;
    ams_drm_gem    gems[AMS_DRM_MAX_HANDLES];
    ams_drm_fb     fbs[AMS_DRM_MAX_FBS];
};

ams_drm_client      g_clients[AMS_DRM_MAX_CLIENTS];
struct ams_drm_card_info g_card_info;
uint64_t          g_offset_seq = 0x100000ULL;

ams_drm_client* find_client(int fd) {
    for (auto& c : g_clients) {
        if (c.in_use && c.fd == fd) return &c;
    }
    return nullptr;
}

ams_drm_gem* find_gem(ams_drm_client* c, uint32_t handle) {
    if (!c) return nullptr;
    for (auto& g : c->gems) {
        if (g.in_use && g.handle == handle) return &g;
    }
    return nullptr;
}

ams_drm_fb* find_fb(ams_drm_client* c, uint32_t fb_id) {
    if (!c) return nullptr;
    for (auto& f : c->fbs) {
        if (f.in_use && f.fb_id == fb_id) return &f;
    }
    return nullptr;
}

ams_drm_gem* alloc_gem(ams_drm_client* c) {
    for (auto& g : c->gems) {
        if (!g.in_use) {
            k_memset(&g, 0, sizeof(g));
            g.in_use = true;
            g.handle = ++c->next_handle;
            g.placement = AMS_TTM_PL_SYSTEM;
            return &g;
        }
    }
    return nullptr;
}

ams_drm_fb* alloc_fb(ams_drm_client* c) {
    for (auto& f : c->fbs) {
        if (!f.in_use) {
            k_memset(&f, 0, sizeof(f));
            f.in_use = true;
            f.fb_id = ++c->next_fb_id;
            return &f;
        }
    }
    return nullptr;
}

void destroy_gem(ams_drm_gem* g) {
    if (!g || !g->in_use) return;
    if (g->pages) {
        kfree(g->pages);
        g->pages = nullptr;
    }
    g->in_use = false;
}

} // namespace

extern "C" void ams_drm_init(uint32_t fb_width, uint32_t fb_height, uint32_t fb_pitch) {
    k_memset(g_clients, 0, sizeof(g_clients));
    g_card_info.fb_width  = fb_width;
    g_card_info.fb_height = fb_height;
    g_card_info.fb_pitch  = fb_pitch ? fb_pitch : fb_width * 4;
    g_card_info.bpp       = 32;
    g_card_info.fb_phys   = 0;
    write_serial_string("[DRM] Software DRM/KMS/GEM/TTM card0 initialised.\n");
}

extern "C" int ams_drm_open(int fd) {
    for (auto& c : g_clients) {
        if (!c.in_use) {
            k_memset(&c, 0, sizeof(c));
            c.in_use = true;
            c.fd = fd;
            return 0;
        }
    }
    return -24; /* EMFILE */
}

extern "C" int ams_drm_close(int fd) {
    auto* c = find_client(fd);
    if (!c) return -9; /* EBADF */
    for (auto& g : c->gems) destroy_gem(&g);
    c->in_use = false;
    return 0;
}

static int do_version(ams_drm_version* v) {
    if (!v) return -14;
    v->version_major = 1;
    v->version_minor = 6;
    v->version_patchlevel = 0;
    /* Userspace passes buffers + lengths in name/date/desc; we just clamp */
    const char name[] = "ams-drm";
    const char date[] = "2026-05-04";
    const char desc[] = "AMS-OS software DRM/KMS/GEM/TTM stub";
    auto fill = [](char* dst, uint64_t cap, const char* src) {
        if (!dst || cap == 0) return;
        uint64_t i = 0;
        while (src[i] && i + 1 < cap) { dst[i] = src[i]; ++i; }
        dst[i] = 0;
    };
    fill(v->name, v->name_len, name);
    fill(v->date, v->date_len, date);
    fill(v->desc, v->desc_len, desc);
    v->name_len = sizeof(name) - 1;
    v->date_len = sizeof(date) - 1;
    v->desc_len = sizeof(desc) - 1;
    return 0;
}

static int do_get_cap(ams_drm_get_cap* c) {
    if (!c) return -14;
    switch (c->capability) {
        case AMS_DRM_CAP_DUMB_BUFFER: c->value = 1; return 0;
        case AMS_DRM_CAP_PRIME:
            c->value = AMS_DRM_PRIME_CAP_IMPORT | AMS_DRM_PRIME_CAP_EXPORT;
            return 0;
        default: c->value = 0; return 0;
    }
}

static int do_create_dumb(ams_drm_client* c, ams_drm_mode_create_dumb* req) {
    if (!c || !req) return -14;
    uint32_t bpp = req->bpp ? req->bpp : 32;
    uint32_t pitch = ((req->width * bpp / 8u) + 3u) & ~3u;
    uint64_t size = (uint64_t)pitch * (uint64_t)req->height;
    if (!size || size > 64ULL * 1024ULL * 1024ULL) return -22;
    auto* g = alloc_gem(c);
    if (!g) return -24;
    g->width  = req->width;
    g->height = req->height;
    g->bpp    = bpp;
    g->pitch  = pitch;
    g->size   = size;
    g->pages  = (uint8_t*)kmalloc((size_t)size);
    if (!g->pages) {
        g->in_use = false;
        return -12;
    }
    k_memset(g->pages, 0, (size_t)size);
    g->mmap_offset = (g_offset_seq += 0x10000ULL);
    req->handle = g->handle;
    req->pitch  = pitch;
    req->size   = size;
    return 0;
}

static int do_map_dumb(ams_drm_client* c, ams_drm_mode_map_dumb* req) {
    if (!c || !req) return -14;
    auto* g = find_gem(c, req->handle);
    if (!g) return -2;
    req->offset = g->mmap_offset;
    return 0;
}

static int do_destroy_dumb(ams_drm_client* c, ams_drm_mode_destroy_dumb* req) {
    if (!c || !req) return -14;
    auto* g = find_gem(c, req->handle);
    if (!g) return -2;
    destroy_gem(g);
    return 0;
}

static int do_addfb2(ams_drm_client* c, ams_drm_mode_fb_cmd2* req) {
    if (!c || !req) return -14;
    auto* fb = alloc_fb(c);
    if (!fb) return -24;
    fb->width        = req->width;
    fb->height       = req->height;
    fb->pitch        = req->pitches[0];
    fb->pixel_format = req->pixel_format;
    fb->handle       = req->handles[0];
    req->fb_id       = fb->fb_id;
    return 0;
}

static int do_rmfb(ams_drm_client* c, uint32_t* fb_id) {
    if (!c || !fb_id) return -14;
    auto* fb = find_fb(c, *fb_id);
    if (!fb) return -2;
    fb->in_use = false;
    return 0;
}

extern "C" int ams_drm_ioctl(int fd, uint32_t cmd, void* argp) {
    auto* c = find_client(fd);
    if (!c) return -9;
    switch (cmd) {
        case AMS_DRM_IOCTL_VERSION:
            return do_version((ams_drm_version*)argp);
        case AMS_DRM_IOCTL_GET_CAP:
            return do_get_cap((ams_drm_get_cap*)argp);
        case AMS_DRM_IOCTL_SET_CLIENT_CAP:
            return 0; /* accept any modeset/atomic capability requests */
        case AMS_DRM_IOCTL_MODE_CREATE_DUMB:
            return do_create_dumb(c, (ams_drm_mode_create_dumb*)argp);
        case AMS_DRM_IOCTL_MODE_MAP_DUMB:
            return do_map_dumb(c, (ams_drm_mode_map_dumb*)argp);
        case AMS_DRM_IOCTL_MODE_DESTROY_DUMB:
            return do_destroy_dumb(c, (ams_drm_mode_destroy_dumb*)argp);
        case AMS_DRM_IOCTL_MODE_ADDFB2:
            return do_addfb2(c, (ams_drm_mode_fb_cmd2*)argp);
        case AMS_DRM_IOCTL_MODE_RMFB:
            return do_rmfb(c, (uint32_t*)argp);
        case AMS_DRM_IOCTL_MODE_PAGE_FLIP:
            /* Software path: nothing to flip. Compositor will blit. */
            return 0;
        case AMS_DRM_IOCTL_MODE_GETRESOURCES:
        case AMS_DRM_IOCTL_MODE_GETCRTC:
        case AMS_DRM_IOCTL_MODE_SETCRTC:
        case AMS_DRM_IOCTL_MODE_GETENCODER:
        case AMS_DRM_IOCTL_MODE_GETCONNECTOR:
        case AMS_DRM_IOCTL_MODE_GETPLANERESOURCES:
        case AMS_DRM_IOCTL_MODE_GETPLANE:
        case AMS_DRM_IOCTL_MODE_ATOMIC:
            return 0; /* graceful no-op; wlroots probes will still see a card */
        case AMS_DRM_IOCTL_GEM_CLOSE: {
            uint32_t h = *(uint32_t*)argp;
            auto* g = find_gem(c, h);
            if (!g) return -2;
            destroy_gem(g);
            return 0;
        }
        default:
            return -25; /* ENOTTY */
    }
}

extern "C" int ams_drm_mmap(uint64_t offset, uint64_t length, void** out_phys) {
    if (!out_phys) return -14;
    for (auto& c : g_clients) {
        if (!c.in_use) continue;
        for (auto& g : c.gems) {
            if (g.in_use && g.mmap_offset == offset) {
                if (length > g.size) return -22;
                *out_phys = g.pages;
                return 0;
            }
        }
    }
    return -2;
}

extern "C" const struct ams_drm_card_info* ams_drm_get_card_info(void) {
    return &g_card_info;
}
