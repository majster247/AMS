#include "kernel.h"
#include "graphics.h"
#include "drm/drm.h"
#include "drm/drm_mode.h"
#include <stdint.h>

extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);

extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t* backbuffer;
extern Framebuffer fb;
extern void graphics_flip();
extern "C" uint64_t g_kernel_cr3;

static constexpr uint32_t GEM_MAX_HANDLES = 64;
static constexpr uint32_t FB_MAX = 32;

struct gem_object {
    bool in_use;
    uint32_t handle;
    uint64_t size;
    uint32_t width, height, bpp, pitch;
    uint8_t* cpu_addr;
    uint64_t mmap_offset;
};

struct drm_framebuffer {
    bool in_use;
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp, depth;
    uint32_t pixel_format;
    uint32_t gem_handle;
};

static gem_object g_gems[GEM_MAX_HANDLES];
static drm_framebuffer g_fbs[FB_MAX];
static uint32_t g_next_handle = 1;
static uint32_t g_next_fb_id = 1;
static uint32_t g_active_fb_id = 0;
static bool g_drm_master = false;

static constexpr uint32_t CRTC_ID = 1;
static constexpr uint32_t CONNECTOR_ID = 1;
static constexpr uint32_t ENCODER_ID = 1;

static gem_object* find_gem(uint32_t handle) {
    for (uint32_t i = 0; i < GEM_MAX_HANDLES; ++i) {
        if (g_gems[i].in_use && g_gems[i].handle == handle)
            return &g_gems[i];
    }
    return nullptr;
}

static drm_framebuffer* find_fb(uint32_t fb_id) {
    for (uint32_t i = 0; i < FB_MAX; ++i) {
        if (g_fbs[i].in_use && g_fbs[i].fb_id == fb_id)
            return &g_fbs[i];
    }
    return nullptr;
}

static void fill_mode_info(drm_mode_modeinfo* mode) {
    k_memset(mode, 0, sizeof(*mode));
    mode->hdisplay = (uint16_t)fb_width;
    mode->hsync_start = (uint16_t)(fb_width + 48);
    mode->hsync_end = (uint16_t)(fb_width + 48 + 112);
    mode->htotal = (uint16_t)(fb_width + 48 + 112 + 248);
    mode->vdisplay = (uint16_t)fb_height;
    mode->vsync_start = (uint16_t)(fb_height + 1);
    mode->vsync_end = (uint16_t)(fb_height + 1 + 3);
    mode->vtotal = (uint16_t)(fb_height + 1 + 3 + 38);
    mode->vrefresh = 60;
    mode->clock = (uint32_t)(mode->htotal * mode->vtotal * 60 / 1000);
    mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
    mode->type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;

    char* n = mode->name;
    uint32_t w = fb_width, h = fb_height;
    int pos = 0;
    char tmp[16];
    int ti = 0;
    do { tmp[ti++] = '0' + (w % 10); w /= 10; } while (w);
    while (ti > 0) n[pos++] = tmp[--ti];
    n[pos++] = 'x';
    ti = 0;
    do { tmp[ti++] = '0' + (h % 10); h /= 10; } while (h);
    while (ti > 0) n[pos++] = tmp[--ti];
    n[pos] = '\0';
}

extern "C" int64_t drm_ioctl(uint32_t cmd, void* arg) {
    switch (cmd) {
    case DRM_IOCTL_VERSION: {
        auto* v = (drm_version*)arg;
        v->version_major = 1;
        v->version_minor = 0;
        v->version_patchlevel = 0;
        const char* drv_name = "ams-drm";
        const char* drv_date = "20260504";
        const char* drv_desc = "AMS-OS KMS/GEM DRM driver";
        if (v->name && v->name_len >= 8) k_memcpy(v->name, drv_name, 8);
        v->name_len = 7;
        if (v->date && v->date_len >= 9) k_memcpy(v->date, drv_date, 9);
        v->date_len = 8;
        if (v->desc && v->desc_len >= 26) k_memcpy(v->desc, drv_desc, 26);
        v->desc_len = 25;
        return 0;
    }

    case DRM_IOCTL_GET_CAP: {
        auto* c = (drm_get_cap*)arg;
        switch (c->capability) {
        case DRM_CAP_DUMB_BUFFER: c->value = 1; break;
        case DRM_CAP_PRIME: c->value = 0; break;
        case DRM_CAP_TIMESTAMP_MONOTONIC: c->value = 1; break;
        case DRM_CAP_CRTC_IN_VBLANK_EVENT: c->value = 1; break;
        default: c->value = 0; break;
        }
        return 0;
    }

    case DRM_IOCTL_SET_MASTER:
        g_drm_master = true;
        return 0;

    case DRM_IOCTL_DROP_MASTER:
        g_drm_master = false;
        return 0;

    case DRM_IOCTL_MODE_GETRESOURCES: {
        auto* r = (drm_mode_card_res*)arg;
        if (r->count_crtcs == 0 && r->count_connectors == 0 && r->count_encoders == 0) {
            r->count_fbs = 0;
            r->count_crtcs = 1;
            r->count_connectors = 1;
            r->count_encoders = 1;
            r->min_width = 0;
            r->max_width = 8192;
            r->min_height = 0;
            r->max_height = 8192;
        } else {
            if (r->count_crtcs >= 1 && r->crtc_id_ptr) {
                uint32_t* p = (uint32_t*)r->crtc_id_ptr;
                p[0] = CRTC_ID;
            }
            if (r->count_connectors >= 1 && r->connector_id_ptr) {
                uint32_t* p = (uint32_t*)r->connector_id_ptr;
                p[0] = CONNECTOR_ID;
            }
            if (r->count_encoders >= 1 && r->encoder_id_ptr) {
                uint32_t* p = (uint32_t*)r->encoder_id_ptr;
                p[0] = ENCODER_ID;
            }
            r->count_crtcs = 1;
            r->count_connectors = 1;
            r->count_encoders = 1;
        }
        return 0;
    }

    case DRM_IOCTL_MODE_GETCONNECTOR: {
        auto* c = (drm_mode_get_connector*)arg;
        c->connector_id = CONNECTOR_ID;
        c->connector_type = DRM_MODE_CONNECTOR_VIRTUAL;
        c->connector_type_id = 1;
        c->connection = DRM_MODE_CONNECTED;
        c->mm_width = (fb_width * 254) / 960;
        c->mm_height = (fb_height * 254) / 960;
        c->subpixel = DRM_MODE_SUBPIXEL_UNKNOWN;
        c->encoder_id = ENCODER_ID;
        if (c->count_modes == 0) {
            c->count_modes = 1;
            c->count_encoders = 1;
            c->count_props = 0;
        } else {
            if (c->count_modes >= 1 && c->modes_ptr) {
                fill_mode_info((drm_mode_modeinfo*)c->modes_ptr);
            }
            if (c->count_encoders >= 1 && c->encoders_ptr) {
                uint32_t* p = (uint32_t*)c->encoders_ptr;
                p[0] = ENCODER_ID;
            }
            c->count_modes = 1;
            c->count_encoders = 1;
        }
        return 0;
    }

    case DRM_IOCTL_MODE_GETENCODER: {
        auto* e = (drm_mode_get_encoder*)arg;
        e->encoder_id = ENCODER_ID;
        e->encoder_type = DRM_MODE_ENCODER_VIRTUAL;
        e->crtc_id = CRTC_ID;
        e->possible_crtcs = 1;
        e->possible_clones = 0;
        return 0;
    }

    case DRM_IOCTL_MODE_GETCRTC: {
        auto* c = (drm_mode_crtc*)arg;
        c->crtc_id = CRTC_ID;
        c->fb_id = g_active_fb_id;
        c->x = 0;
        c->y = 0;
        c->gamma_size = 256;
        c->mode_valid = 1;
        fill_mode_info(&c->mode);
        return 0;
    }

    case DRM_IOCTL_MODE_SETCRTC: {
        auto* c = (drm_mode_crtc*)arg;
        g_active_fb_id = c->fb_id;
        if (c->fb_id) {
            drm_framebuffer* dfb = find_fb(c->fb_id);
            if (dfb) {
                gem_object* gem = find_gem(dfb->gem_handle);
                if (gem && gem->cpu_addr && backbuffer) {
                    uint32_t copy_h = (dfb->height < fb_height) ? dfb->height : fb_height;
                    for (uint32_t y = 0; y < copy_h; ++y) {
                        uint32_t src_pitch = dfb->pitch;
                        uint32_t dst_pitch = fb_width * 4;
                        uint32_t row_bytes = (src_pitch < dst_pitch) ? src_pitch : dst_pitch;
                        k_memcpy(backbuffer + y * fb_width, gem->cpu_addr + y * src_pitch, row_bytes);
                    }
                    graphics_flip();
                }
            }
        }
        return 0;
    }

    case DRM_IOCTL_MODE_CREATE_DUMB: {
        auto* d = (drm_mode_create_dumb*)arg;
        d->pitch = ((d->width * d->bpp + 31) / 32) * 4;
        d->size = (uint64_t)d->pitch * d->height;
        d->size = (d->size + 4095) & ~4095ULL;

        gem_object* g = nullptr;
        for (uint32_t i = 0; i < GEM_MAX_HANDLES; ++i) {
            if (!g_gems[i].in_use) { g = &g_gems[i]; break; }
        }
        if (!g) return -28; // ENOSPC

        g->in_use = true;
        g->handle = g_next_handle++;
        g->size = d->size;
        g->width = d->width;
        g->height = d->height;
        g->bpp = d->bpp;
        g->pitch = d->pitch;
        g->cpu_addr = (uint8_t*)kmalloc(d->size);
        if (!g->cpu_addr) { g->in_use = false; return -12; }
        k_memset(g->cpu_addr, 0, d->size);
        g->mmap_offset = (uint64_t)g->cpu_addr;

        d->handle = g->handle;
        return 0;
    }

    case DRM_IOCTL_MODE_MAP_DUMB: {
        auto* m = (drm_mode_map_dumb*)arg;
        gem_object* g = find_gem(m->handle);
        if (!g) return -22;
        m->offset = g->mmap_offset;
        return 0;
    }

    case DRM_IOCTL_MODE_DESTROY_DUMB: {
        auto* d = (drm_mode_destroy_dumb*)arg;
        gem_object* g = find_gem(d->handle);
        if (!g) return -22;
        if (g->cpu_addr) kfree(g->cpu_addr);
        g->in_use = false;
        return 0;
    }

    case DRM_IOCTL_GEM_CLOSE: {
        auto* c = (drm_gem_close*)arg;
        gem_object* g = find_gem(c->handle);
        if (!g) return -22;
        if (g->cpu_addr) kfree(g->cpu_addr);
        g->in_use = false;
        return 0;
    }

    case DRM_IOCTL_MODE_ADDFB: {
        auto* f = (drm_mode_fb_cmd*)arg;
        drm_framebuffer* dfb = nullptr;
        for (uint32_t i = 0; i < FB_MAX; ++i) {
            if (!g_fbs[i].in_use) { dfb = &g_fbs[i]; break; }
        }
        if (!dfb) return -28;
        dfb->in_use = true;
        dfb->fb_id = g_next_fb_id++;
        dfb->width = f->width;
        dfb->height = f->height;
        dfb->pitch = f->pitch;
        dfb->bpp = f->bpp;
        dfb->depth = f->depth;
        dfb->gem_handle = f->handle;
        dfb->pixel_format = DRM_FORMAT_XRGB8888;
        f->fb_id = dfb->fb_id;
        return 0;
    }

    case DRM_IOCTL_MODE_ADDFB2: {
        auto* f = (drm_mode_fb_cmd2*)arg;
        drm_framebuffer* dfb = nullptr;
        for (uint32_t i = 0; i < FB_MAX; ++i) {
            if (!g_fbs[i].in_use) { dfb = &g_fbs[i]; break; }
        }
        if (!dfb) return -28;
        dfb->in_use = true;
        dfb->fb_id = g_next_fb_id++;
        dfb->width = f->width;
        dfb->height = f->height;
        dfb->pitch = f->pitches[0];
        dfb->pixel_format = f->pixel_format;
        dfb->gem_handle = f->handles[0];
        dfb->bpp = 32;
        dfb->depth = 24;
        f->fb_id = dfb->fb_id;
        return 0;
    }

    case DRM_IOCTL_MODE_RMFB: {
        uint32_t fb_id = *(uint32_t*)arg;
        drm_framebuffer* dfb = find_fb(fb_id);
        if (!dfb) return -22;
        dfb->in_use = false;
        if (g_active_fb_id == fb_id) g_active_fb_id = 0;
        return 0;
    }

    case DRM_IOCTL_MODE_PAGE_FLIP: {
        auto* pf = (drm_mode_crtc_page_flip*)arg;
        g_active_fb_id = pf->fb_id;
        drm_framebuffer* dfb = find_fb(pf->fb_id);
        if (dfb) {
            gem_object* gem = find_gem(dfb->gem_handle);
            if (gem && gem->cpu_addr && backbuffer) {
                uint32_t copy_h = (dfb->height < fb_height) ? dfb->height : fb_height;
                for (uint32_t y = 0; y < copy_h; ++y) {
                    uint32_t row_bytes = (dfb->pitch < fb_width * 4) ? dfb->pitch : fb_width * 4;
                    k_memcpy(backbuffer + y * fb_width, gem->cpu_addr + y * dfb->pitch, row_bytes);
                }
                graphics_flip();
            }
        }
        return 0;
    }

    case DRM_IOCTL_MODE_GETPLANERESOURCES: {
        uint32_t* count = (uint32_t*)arg;
        *count = 0;
        return 0;
    }

    default:
        write_serial_string("[DRM] unknown ioctl: 0x");
        write_serial_hex(cmd);
        write_serial_string("\n");
        return -22; // EINVAL
    }
}

extern "C" void drm_init() {
    k_memset(g_gems, 0, sizeof(g_gems));
    k_memset(g_fbs, 0, sizeof(g_fbs));
    g_next_handle = 1;
    g_next_fb_id = 1;
    g_active_fb_id = 0;
    g_drm_master = false;
    write_serial_string("[DRM] AMS-DRM KMS/GEM initialized\n");
}
