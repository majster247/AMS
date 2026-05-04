/**
 * @file drm.cpp
 * @brief AMS kernel DRM/KMS subsystem.
 *
 * Wraps the Multiboot2 linear framebuffer as a minimal DRM device
 * (/dev/dri/card0) with:
 *   - 1 CRTC  (id 50)
 *   - 1 encoder (id 51)
 *   - 1 connector (id 52, type Virtual, connected)
 *   - dumb-buffer create / map / destroy
 *   - GEM handle bookkeeping
 *   - mode-set (SETCRTC) → copies dumb buffer to hw fb via graphics_flip
 *   - page-flip with synthetic vblank event
 *
 * The ioctl dispatch is called from sys_ioctl when fd_kind == FD_KIND_DRM.
 */

#include "kernel.h"
#include "graphics.h"
#include "vmm.h"
#include <stdint.h>

/* pull in the UAPI header for struct definitions */
#include "drm/drm.h"
#include "drm/drm_mode.h"

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void* kmalloc(size_t size);
extern "C" void  kfree(void* ptr);
extern "C" int   k_strlen(const char* str);
extern "C" char* k_strcpy(char* dest, const char* src);
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t* backbuffer;
extern Framebuffer fb;
extern "C" void graphics_flip();
extern "C" uint64_t get_time_ms();

/* ---- internal bookkeeping ---- */

#define DRM_MAX_GEM 64

struct drm_gem_obj {
    bool     in_use;
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint64_t size;
    void*    cpu_addr;       /* kernel-virtual (HHDM) */
    uint64_t mmap_offset;    /* fake offset returned by MAP_DUMB */
};

struct drm_fb_obj {
    bool     in_use;
    uint32_t fb_id;
    uint32_t gem_handle;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

#define DRM_MAX_FB 16

static drm_gem_obj  g_gems[DRM_MAX_GEM];
static drm_fb_obj   g_fbs[DRM_MAX_FB];
static uint32_t     g_next_gem_handle = 1;
static uint32_t     g_next_fb_id = 100;
static uint32_t     g_active_fb_id = 0;

/* IDs for the single output pipeline */
static const uint32_t DRM_CRTC_ID      = 50;
static const uint32_t DRM_ENCODER_ID   = 51;
static const uint32_t DRM_CONNECTOR_ID = 52;
static const uint32_t DRM_PLANE_ID     = 60;

/* ---- helpers ---- */

static drm_gem_obj* gem_find(uint32_t handle) {
    for (int i = 0; i < DRM_MAX_GEM; i++) {
        if (g_gems[i].in_use && g_gems[i].handle == handle)
            return &g_gems[i];
    }
    return nullptr;
}

static drm_gem_obj* gem_alloc_slot() {
    for (int i = 0; i < DRM_MAX_GEM; i++) {
        if (!g_gems[i].in_use)
            return &g_gems[i];
    }
    return nullptr;
}

static drm_fb_obj* fb_find(uint32_t fb_id) {
    for (int i = 0; i < DRM_MAX_FB; i++) {
        if (g_fbs[i].in_use && g_fbs[i].fb_id == fb_id)
            return &g_fbs[i];
    }
    return nullptr;
}

static drm_fb_obj* fb_alloc_slot() {
    for (int i = 0; i < DRM_MAX_FB; i++) {
        if (!g_fbs[i].in_use)
            return &g_fbs[i];
    }
    return nullptr;
}

static void build_current_mode(struct drm_mode_modeinfo* m) {
    k_memset(m, 0, sizeof(*m));
    m->hdisplay = fb_width;
    m->hsync_start = fb_width + 48;
    m->hsync_end = fb_width + 48 + 112;
    m->htotal = fb_width + 48 + 112 + 80;
    m->vdisplay = fb_height;
    m->vsync_start = fb_height + 3;
    m->vsync_end = fb_height + 3 + 6;
    m->vtotal = fb_height + 3 + 6 + 25;
    m->vrefresh = 60;
    m->clock = (m->htotal * m->vtotal * 60) / 1000;
    m->type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;
    m->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;

    /* build name like "1024x768" */
    char* p = m->name;
    uint32_t w = fb_width, h = fb_height;
    char wbuf[8], hbuf[8];
    int wi = 0, hi = 0;
    if (w == 0) { wbuf[wi++] = '0'; } else { uint32_t t = w; while (t) { wbuf[wi++] = '0' + (t % 10); t /= 10; } }
    if (h == 0) { hbuf[hi++] = '0'; } else { uint32_t t = h; while (t) { hbuf[hi++] = '0' + (t % 10); t /= 10; } }
    for (int i = wi - 1; i >= 0; i--) *p++ = wbuf[i];
    *p++ = 'x';
    for (int i = hi - 1; i >= 0; i--) *p++ = hbuf[i];
    *p = '\0';
}

/* Copy a GEM dumb buffer into the backbuffer and flip. */
static void drm_scanout(drm_gem_obj* gem) {
    if (!gem || !gem->cpu_addr || !backbuffer) return;

    uint32_t* src = (uint32_t*)gem->cpu_addr;
    uint32_t copy_w = gem->width < fb_width ? gem->width : fb_width;
    uint32_t copy_h = gem->height < fb_height ? gem->height : fb_height;

    for (uint32_t y = 0; y < copy_h; y++) {
        k_memcpy(backbuffer + y * fb_width,
                 src + y * (gem->pitch / 4),
                 copy_w * 4);
    }
    graphics_flip();
}

/* ---- public ioctl dispatcher (called from syscall.cpp) ---- */

extern "C" int64_t drm_ioctl(uint32_t cmd, void* arg) {
    if (!arg) return -14; /* EFAULT */

    switch (cmd) {

    /* ---- DRM_IOCTL_VERSION ---- */
    case DRM_IOCTL_VERSION: {
        struct drm_version* v = (struct drm_version*)arg;
        v->version_major = 1;
        v->version_minor = 0;
        v->version_patchlevel = 0;
        const char* drv_name = "ams-drm";
        const char* drv_date = "20260504";
        const char* drv_desc = "AMS Multiboot2 FB DRM";
        if (v->name && v->name_len >= 8) {
            k_memcpy((void*)v->name, drv_name, 8);
            v->name_len = 7;
        } else {
            v->name_len = 7;
        }
        if (v->date && v->date_len >= 9) {
            k_memcpy((void*)v->date, drv_date, 9);
            v->date_len = 8;
        } else {
            v->date_len = 8;
        }
        if (v->desc && v->desc_len >= 22) {
            k_memcpy((void*)v->desc, drv_desc, 22);
            v->desc_len = 21;
        } else {
            v->desc_len = 21;
        }
        return 0;
    }

    /* ---- DRM_IOCTL_GET_CAP ---- */
    case DRM_IOCTL_GET_CAP: {
        struct drm_get_cap* gc = (struct drm_get_cap*)arg;
        switch (gc->capability) {
            case DRM_CAP_DUMB_BUFFER:         gc->value = 1; break;
            case DRM_CAP_PRIME:               gc->value = 0; break;
            case DRM_CAP_TIMESTAMP_MONOTONIC: gc->value = 1; break;
            case DRM_CAP_CRTC_IN_VBLANK_EVENT:gc->value = 1; break;
            default:                          gc->value = 0; break;
        }
        return 0;
    }

    /* ---- DRM_IOCTL_SET_CLIENT_CAP ---- */
    case DRM_IOCTL_SET_CLIENT_CAP:
        return 0;

    /* ---- DRM_IOCTL_MODE_GETRESOURCES ---- */
    case DRM_IOCTL_MODE_GETRESOURCES: {
        struct drm_mode_card_res* res = (struct drm_mode_card_res*)arg;
        if (res->crtc_id_ptr && res->count_crtcs >= 1) {
            uint32_t* p = (uint32_t*)(uintptr_t)res->crtc_id_ptr;
            p[0] = DRM_CRTC_ID;
        }
        if (res->encoder_id_ptr && res->count_encoders >= 1) {
            uint32_t* p = (uint32_t*)(uintptr_t)res->encoder_id_ptr;
            p[0] = DRM_ENCODER_ID;
        }
        if (res->connector_id_ptr && res->count_connectors >= 1) {
            uint32_t* p = (uint32_t*)(uintptr_t)res->connector_id_ptr;
            p[0] = DRM_CONNECTOR_ID;
        }
        res->count_fbs = 0;
        res->count_crtcs = 1;
        res->count_connectors = 1;
        res->count_encoders = 1;
        res->min_width = 1;
        res->max_width = 8192;
        res->min_height = 1;
        res->max_height = 8192;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_GETCONNECTOR ---- */
    case DRM_IOCTL_MODE_GETCONNECTOR: {
        struct drm_mode_get_connector* c = (struct drm_mode_get_connector*)arg;
        c->connector_id = DRM_CONNECTOR_ID;
        c->connector_type = DRM_MODE_CONNECTOR_Virtual;
        c->connector_type_id = 1;
        c->connection = DRM_MODE_CONNECTED;
        c->encoder_id = DRM_ENCODER_ID;
        c->mm_width = (fb_width * 254) / 960;
        c->mm_height = (fb_height * 254) / 960;
        c->subpixel = 0;

        if (c->modes_ptr && c->count_modes >= 1) {
            struct drm_mode_modeinfo* m = (struct drm_mode_modeinfo*)(uintptr_t)c->modes_ptr;
            build_current_mode(m);
        }
        c->count_modes = 1;

        if (c->encoders_ptr && c->count_encoders >= 1) {
            uint32_t* e = (uint32_t*)(uintptr_t)c->encoders_ptr;
            e[0] = DRM_ENCODER_ID;
        }
        c->count_encoders = 1;
        c->count_props = 0;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_GETENCODER ---- */
    case DRM_IOCTL_MODE_GETENCODER: {
        struct drm_mode_get_encoder* e = (struct drm_mode_get_encoder*)arg;
        e->encoder_id = DRM_ENCODER_ID;
        e->encoder_type = DRM_MODE_ENCODER_VIRTUAL;
        e->crtc_id = DRM_CRTC_ID;
        e->possible_crtcs = 1;
        e->possible_clones = 0;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_GETCRTC ---- */
    case DRM_IOCTL_MODE_GETCRTC: {
        struct drm_mode_crtc* cr = (struct drm_mode_crtc*)arg;
        cr->crtc_id = DRM_CRTC_ID;
        cr->fb_id = g_active_fb_id;
        cr->x = 0;
        cr->y = 0;
        cr->gamma_size = 0;
        cr->mode_valid = 1;
        build_current_mode(&cr->mode);
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_SETCRTC ---- */
    case DRM_IOCTL_MODE_SETCRTC: {
        struct drm_mode_crtc* cr = (struct drm_mode_crtc*)arg;
        if (cr->fb_id) {
            drm_fb_obj* fbo = fb_find(cr->fb_id);
            if (!fbo) return -22; /* EINVAL */
            drm_gem_obj* gem = gem_find(fbo->gem_handle);
            g_active_fb_id = cr->fb_id;
            if (gem) drm_scanout(gem);
        }
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_CREATE_DUMB ---- */
    case DRM_IOCTL_MODE_CREATE_DUMB: {
        struct drm_mode_create_dumb* cd = (struct drm_mode_create_dumb*)arg;
        drm_gem_obj* gem = gem_alloc_slot();
        if (!gem) return -12; /* ENOMEM */

        uint32_t bpp = cd->bpp ? cd->bpp : 32;
        uint32_t pitch = ((cd->width * bpp + 7) / 8 + 63) & ~63u;
        uint64_t size = (uint64_t)pitch * cd->height;
        size = (size + 4095) & ~4095ULL;

        void* buf = kmalloc(size);
        if (!buf) return -12;
        k_memset(buf, 0, size);

        gem->in_use = true;
        gem->handle = g_next_gem_handle++;
        gem->width  = cd->width;
        gem->height = cd->height;
        gem->bpp    = bpp;
        gem->pitch  = pitch;
        gem->size   = size;
        gem->cpu_addr = buf;
        gem->mmap_offset = (uint64_t)gem->handle << 12;

        cd->handle = gem->handle;
        cd->pitch  = pitch;
        cd->size   = size;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_MAP_DUMB ---- */
    case DRM_IOCTL_MODE_MAP_DUMB: {
        struct drm_mode_map_dumb* md = (struct drm_mode_map_dumb*)arg;
        drm_gem_obj* gem = gem_find(md->handle);
        if (!gem) return -22;
        md->offset = gem->mmap_offset;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_DESTROY_DUMB ---- */
    case DRM_IOCTL_MODE_DESTROY_DUMB: {
        struct drm_mode_destroy_dumb* dd = (struct drm_mode_destroy_dumb*)arg;
        drm_gem_obj* gem = gem_find(dd->handle);
        if (!gem) return -22;
        if (gem->cpu_addr) kfree(gem->cpu_addr);
        k_memset(gem, 0, sizeof(*gem));
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_ADDFB ---- */
    case DRM_IOCTL_MODE_ADDFB: {
        struct drm_mode_fb_cmd* fc = (struct drm_mode_fb_cmd*)arg;
        drm_gem_obj* gem = gem_find(fc->handle);
        if (!gem) return -22;
        drm_fb_obj* fbo = fb_alloc_slot();
        if (!fbo) return -12;

        fbo->in_use    = true;
        fbo->fb_id     = g_next_fb_id++;
        fbo->gem_handle = fc->handle;
        fbo->width     = fc->width;
        fbo->height    = fc->height;
        fbo->pitch     = fc->pitch;
        fbo->bpp       = fc->bpp;

        fc->fb_id = fbo->fb_id;
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_RMFB ---- */
    case DRM_IOCTL_MODE_RMFB: {
        uint32_t id = *(uint32_t*)arg;
        drm_fb_obj* fbo = fb_find(id);
        if (!fbo) return -22;
        if (g_active_fb_id == id) g_active_fb_id = 0;
        k_memset(fbo, 0, sizeof(*fbo));
        return 0;
    }

    /* ---- DRM_IOCTL_MODE_PAGE_FLIP ---- */
    case DRM_IOCTL_MODE_PAGE_FLIP: {
        struct drm_mode_crtc_page_flip* pf = (struct drm_mode_crtc_page_flip*)arg;
        drm_fb_obj* fbo = fb_find(pf->fb_id);
        if (!fbo) return -22;
        drm_gem_obj* gem = gem_find(fbo->gem_handle);
        g_active_fb_id = pf->fb_id;
        if (gem) drm_scanout(gem);
        /* vblank event would be queued here for non-blocking flip */
        return 0;
    }

    /* ---- DRM_IOCTL_GEM_CLOSE ---- */
    case DRM_IOCTL_GEM_CLOSE: {
        struct drm_gem_close* gc = (struct drm_gem_close*)arg;
        drm_gem_obj* gem = gem_find(gc->handle);
        if (!gem) return -22;
        if (gem->cpu_addr) kfree(gem->cpu_addr);
        k_memset(gem, 0, sizeof(*gem));
        return 0;
    }

    default:
        write_serial_string("[DRM] unknown ioctl cmd=0x");
        write_serial_hex(cmd);
        write_serial_string("\n");
        return -25; /* ENOTTY */
    }
}

extern "C" void drm_init() {
    k_memset(g_gems, 0, sizeof(g_gems));
    k_memset(g_fbs, 0, sizeof(g_fbs));
    g_next_gem_handle = 1;
    g_next_fb_id = 100;
    g_active_fb_id = 0;
    write_serial_string("[DRM] AMS DRM/KMS subsystem initialized (");
    write_serial_dec(fb_width);
    write_serial_string("x");
    write_serial_dec(fb_height);
    write_serial_string(")\n");
}

/* Called from sys_mmap when mapping a DRM dumb buffer offset. */
extern "C" drm_gem_obj* drm_find_gem_by_offset(uint64_t offset) {
    for (int i = 0; i < DRM_MAX_GEM; i++) {
        if (g_gems[i].in_use && g_gems[i].mmap_offset == offset)
            return &g_gems[i];
    }
    return nullptr;
}
