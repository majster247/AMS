/**
 * DRM/KMS compatibility layer for AMS.
 *
 * Maps a minimal set of DRM ioctls onto the existing AMS framebuffer,
 * allowing libraries like libdrm / wlroots to talk to a virtual GPU
 * through /dev/dri/card0.
 */

#include "graphics.h"
#include <stdint.h>

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void* kmalloc(size_t size);
extern "C" void  kfree(void* ptr);
extern "C" int   k_strlen(const char* str);
extern "C" char* k_strcpy(char* dest, const char* src);
extern "C" void  write_serial_string(const char* s);
extern "C" void  write_serial_hex(uint64_t val);

/* ---------- internal constants --------- */
static constexpr uint32_t DRM_AMS_CRTC_ID      = 1;
static constexpr uint32_t DRM_AMS_ENCODER_ID   = 1;
static constexpr uint32_t DRM_AMS_CONNECTOR_ID = 1;
static constexpr uint32_t DRM_AMS_MAX_DUMB     = 32;
static constexpr uint32_t DRM_AMS_MAX_FB       = 32;

/* ---------- dumb buffer book-keeping --- */
struct drm_dumb_bo {
    bool     in_use;
    uint32_t handle;
    uint32_t width, height, bpp, pitch;
    uint64_t size;
    uint8_t* data;
    uint64_t map_offset;
};

struct drm_fb_obj {
    bool     in_use;
    uint32_t fb_id;
    uint32_t width, height, pitch, bpp, depth;
    uint32_t handle;
};

static drm_dumb_bo  g_dumb[DRM_AMS_MAX_DUMB];
static drm_fb_obj   g_fbs[DRM_AMS_MAX_FB];
static uint32_t     g_next_handle = 1;
static uint32_t     g_next_fb_id = 1;
static uint32_t     g_active_fb  = 0;

static drm_dumb_bo* find_bo(uint32_t handle) {
    for (uint32_t i = 0; i < DRM_AMS_MAX_DUMB; i++)
        if (g_dumb[i].in_use && g_dumb[i].handle == handle) return &g_dumb[i];
    return nullptr;
}
static drm_fb_obj* find_fb(uint32_t id) {
    for (uint32_t i = 0; i < DRM_AMS_MAX_FB; i++)
        if (g_fbs[i].in_use && g_fbs[i].fb_id == id) return &g_fbs[i];
    return nullptr;
}

/* -------- ioctl dispatch helpers (called from kernel sys_ioctl) -------- */

static int64_t drm_ioctl_version(void* argp) {
    struct drm_version_k {
        int ver_major, ver_minor, ver_patch;
        uint64_t name_len; char* name;
        uint64_t date_len; char* date;
        uint64_t desc_len; char* desc;
    };
    auto* v = (drm_version_k*)argp;
    if (!v) return -14;
    v->ver_major = 1; v->ver_minor = 0; v->ver_patch = 0;
    const char* nm = "ams-drm";
    const char* dt = "20260504";
    const char* ds = "AMS virtual DRM";
    if (v->name && v->name_len >= 8) { k_memcpy(v->name, nm, 8); }
    v->name_len = 7;
    if (v->date && v->date_len >= 9) { k_memcpy(v->date, dt, 9); }
    v->date_len = 8;
    if (v->desc && v->desc_len >= 16) { k_memcpy(v->desc, ds, 16); }
    v->desc_len = 15;
    return 0;
}

static int64_t drm_ioctl_get_cap(void* argp) {
    struct drm_get_cap_k { uint64_t cap; uint64_t value; };
    auto* gc = (drm_get_cap_k*)argp;
    if (!gc) return -14;
    switch (gc->cap) {
        case 0x1: gc->value = 1; break; /* DUMB_BUFFER */
        case 0x6: gc->value = 1; break; /* TIMESTAMP_MONOTONIC */
        default:  gc->value = 0; break;
    }
    return 0;
}

static int64_t drm_ioctl_set_client_cap(void* argp) {
    (void)argp;
    return 0;
}

static void fill_current_mode(void* mode_ptr) {
    struct modeinfo {
        uint32_t clock;
        uint16_t hd,hss,hse,ht,hsk;
        uint16_t vd,vss,vse,vt,vscan;
        uint32_t vrefresh, flags, type;
        char name[32];
    };
    auto* m = (modeinfo*)mode_ptr;
    k_memset(m, 0, sizeof(modeinfo));
    m->clock = fb_width * fb_height * 60 / 1000;
    m->hd = (uint16_t)fb_width; m->hss = m->hd + 1; m->hse = m->hd + 2; m->ht = m->hd + 3;
    m->vd = (uint16_t)fb_height; m->vss = m->vd + 1; m->vse = m->vd + 2; m->vt = m->vd + 3;
    m->vrefresh = 60;
    m->flags = 0x05; /* PHSYNC | PVSYNC */
    m->type = (1 << 3); /* PREFERRED */
    const char* nm = "AMS";
    k_memcpy(m->name, nm, 4);
}

static int64_t drm_ioctl_mode_getresources(void* argp) {
    struct card_res {
        uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
        uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
        uint32_t min_w, max_w, min_h, max_h;
    };
    auto* r = (card_res*)argp;
    if (!r) return -14;
    if (r->count_crtcs >= 1 && r->crtc_id_ptr) {
        uint32_t id = DRM_AMS_CRTC_ID;
        k_memcpy((void*)r->crtc_id_ptr, &id, 4);
    }
    r->count_crtcs = 1;
    if (r->count_connectors >= 1 && r->connector_id_ptr) {
        uint32_t id = DRM_AMS_CONNECTOR_ID;
        k_memcpy((void*)r->connector_id_ptr, &id, 4);
    }
    r->count_connectors = 1;
    if (r->count_encoders >= 1 && r->encoder_id_ptr) {
        uint32_t id = DRM_AMS_ENCODER_ID;
        k_memcpy((void*)r->encoder_id_ptr, &id, 4);
    }
    r->count_encoders = 1;
    r->count_fbs = 0;
    r->min_w = 1; r->max_w = fb_width;
    r->min_h = 1; r->max_h = fb_height;
    return 0;
}

static int64_t drm_ioctl_mode_getcrtc(void* argp) {
    struct crtc {
        uint64_t set_connectors_ptr;
        uint32_t count_connectors, crtc_id, fb_id;
        uint32_t x, y, gamma_size, mode_valid;
        uint8_t mode[68]; /* drm_mode_modeinfo */
    };
    auto* c = (crtc*)argp;
    if (!c) return -14;
    c->crtc_id = DRM_AMS_CRTC_ID;
    c->fb_id = g_active_fb;
    c->x = 0; c->y = 0;
    c->gamma_size = 0;
    c->mode_valid = 1;
    fill_current_mode(c->mode);
    return 0;
}

static int64_t drm_ioctl_mode_setcrtc(void* argp) {
    struct crtc {
        uint64_t set_connectors_ptr;
        uint32_t count_connectors, crtc_id, fb_id;
        uint32_t x, y, gamma_size, mode_valid;
        uint8_t mode[68];
    };
    auto* c = (crtc*)argp;
    if (!c) return -14;
    g_active_fb = c->fb_id;
    return 0;
}

static int64_t drm_ioctl_mode_getencoder(void* argp) {
    struct enc {
        uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones;
    };
    auto* e = (enc*)argp;
    if (!e) return -14;
    e->encoder_id = DRM_AMS_ENCODER_ID;
    e->encoder_type = 5; /* VIRTUAL */
    e->crtc_id = DRM_AMS_CRTC_ID;
    e->possible_crtcs = 1;
    e->possible_clones = 0;
    return 0;
}

static int64_t drm_ioctl_mode_getconnector(void* argp) {
    struct conn {
        uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
        uint32_t count_modes, count_props, count_encoders;
        uint32_t encoder_id, connector_id, connector_type, connector_type_id;
        uint32_t connection, mm_width, mm_height, subpixel, pad;
    };
    auto* c = (conn*)argp;
    if (!c) return -14;
    if (c->count_modes >= 1 && c->modes_ptr) {
        fill_current_mode((void*)c->modes_ptr);
    }
    c->count_modes = 1;
    if (c->count_encoders >= 1 && c->encoders_ptr) {
        uint32_t eid = DRM_AMS_ENCODER_ID;
        k_memcpy((void*)c->encoders_ptr, &eid, 4);
    }
    c->count_encoders = 1;
    c->count_props = 0;
    c->encoder_id = DRM_AMS_ENCODER_ID;
    c->connector_id = DRM_AMS_CONNECTOR_ID;
    c->connector_type = 15; /* Virtual */
    c->connector_type_id = 1;
    c->connection = 1; /* Connected */
    c->mm_width = 300; c->mm_height = 170;
    c->subpixel = 0;
    return 0;
}

static int64_t drm_ioctl_mode_create_dumb(void* argp) {
    struct create_dumb {
        uint32_t height, width, bpp, flags, handle, pitch;
        uint64_t size;
    };
    auto* cd = (create_dumb*)argp;
    if (!cd) return -14;

    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < DRM_AMS_MAX_DUMB; i++) {
        if (!g_dumb[i].in_use) { slot = i; break; }
    }
    if (slot == UINT32_MAX) return -28; /* ENOSPC */

    uint32_t pitch = cd->width * (cd->bpp / 8);
    pitch = (pitch + 63) & ~63u; /* align to 64 bytes */
    uint64_t size = (uint64_t)pitch * cd->height;

    uint8_t* data = (uint8_t*)kmalloc((size_t)size);
    if (!data) return -12; /* ENOMEM */
    k_memset(data, 0, (size_t)size);

    g_dumb[slot].in_use = true;
    g_dumb[slot].handle = g_next_handle++;
    g_dumb[slot].width  = cd->width;
    g_dumb[slot].height = cd->height;
    g_dumb[slot].bpp    = cd->bpp;
    g_dumb[slot].pitch  = pitch;
    g_dumb[slot].size   = size;
    g_dumb[slot].data   = data;
    g_dumb[slot].map_offset = (uint64_t)(uintptr_t)data;

    cd->handle = g_dumb[slot].handle;
    cd->pitch  = pitch;
    cd->size   = size;
    return 0;
}

static int64_t drm_ioctl_mode_map_dumb(void* argp) {
    struct map_dumb { uint32_t handle, pad; uint64_t offset; };
    auto* md = (map_dumb*)argp;
    if (!md) return -14;
    drm_dumb_bo* bo = find_bo(md->handle);
    if (!bo) return -22;
    md->offset = bo->map_offset;
    return 0;
}

static int64_t drm_ioctl_mode_destroy_dumb(void* argp) {
    struct destroy_dumb { uint32_t handle; };
    auto* dd = (destroy_dumb*)argp;
    if (!dd) return -14;
    drm_dumb_bo* bo = find_bo(dd->handle);
    if (!bo) return -22;
    if (bo->data) kfree(bo->data);
    bo->in_use = false;
    return 0;
}

static int64_t drm_ioctl_mode_addfb(void* argp) {
    struct fb_cmd {
        uint32_t fb_id, width, height, pitch, bpp, depth, handle;
    };
    auto* fc = (fb_cmd*)argp;
    if (!fc) return -14;

    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < DRM_AMS_MAX_FB; i++) {
        if (!g_fbs[i].in_use) { slot = i; break; }
    }
    if (slot == UINT32_MAX) return -28;

    g_fbs[slot].in_use = true;
    g_fbs[slot].fb_id  = g_next_fb_id++;
    g_fbs[slot].width  = fc->width;
    g_fbs[slot].height = fc->height;
    g_fbs[slot].pitch  = fc->pitch;
    g_fbs[slot].bpp    = fc->bpp;
    g_fbs[slot].depth  = fc->depth;
    g_fbs[slot].handle = fc->handle;
    fc->fb_id = g_fbs[slot].fb_id;
    return 0;
}

static int64_t drm_ioctl_mode_rmfb(void* argp) {
    uint32_t fb_id = *(uint32_t*)argp;
    drm_fb_obj* f = find_fb(fb_id);
    if (!f) return -22;
    f->in_use = false;
    return 0;
}

static int64_t drm_ioctl_mode_page_flip(void* argp) {
    struct page_flip { uint32_t crtc_id, fb_id, flags, reserved; uint64_t user_data; };
    auto* pf = (page_flip*)argp;
    if (!pf) return -14;

    drm_fb_obj* f = find_fb(pf->fb_id);
    if (!f) return -22;
    drm_dumb_bo* bo = find_bo(f->handle);
    if (!bo || !bo->data) return -22;

    g_active_fb = pf->fb_id;

    uint32_t cw = (bo->width < fb_width)  ? bo->width  : fb_width;
    uint32_t ch = (bo->height < fb_height) ? bo->height : fb_height;
    uint32_t off_x = (fb_width  - cw) / 2;
    uint32_t off_y = (fb_height - ch) / 2;

    for (uint32_t y = 0; y < ch; y++) {
        uint32_t* dst_row = backbuffer + (off_y + y) * fb_width + off_x;
        uint8_t*  src_row = bo->data   + y * bo->pitch;
        k_memcpy(dst_row, src_row, cw * 4);
    }
    graphics_flip();
    return 0;
}

/**
 * Main DRM ioctl dispatcher — called from sys_ioctl when the FD
 * is tagged as a DRM device.  Returns 0 on success, negative errno.
 */
extern "C" int64_t drm_ams_ioctl(uint64_t request, void* argp) {
    uint32_t nr = (uint32_t)(request & 0xFFu);

    write_serial_string("[DRM] ioctl nr=0x");
    write_serial_hex(nr);
    write_serial_string("\n");

    switch (nr) {
        case 0x00: return drm_ioctl_version(argp);
        case 0x0C: return drm_ioctl_get_cap(argp);
        case 0x0D: return drm_ioctl_set_client_cap(argp);
        case 0xA0: return drm_ioctl_mode_getresources(argp);
        case 0xA1: return drm_ioctl_mode_getcrtc(argp);
        case 0xA2: return drm_ioctl_mode_setcrtc(argp);
        case 0xA6: return drm_ioctl_mode_getencoder(argp);
        case 0xA7: return drm_ioctl_mode_getconnector(argp);
        case 0xAE: return drm_ioctl_mode_addfb(argp);
        case 0xAF: return drm_ioctl_mode_rmfb(argp);
        case 0xB0: return drm_ioctl_mode_page_flip(argp);
        case 0xB2: return drm_ioctl_mode_create_dumb(argp);
        case 0xB3: return drm_ioctl_mode_map_dumb(argp);
        case 0xB4: return drm_ioctl_mode_destroy_dumb(argp);
        default:   return -25; /* ENOTTY */
    }
}

extern "C" void drm_ams_init() {
    k_memset(g_dumb, 0, sizeof(g_dumb));
    k_memset(g_fbs, 0, sizeof(g_fbs));
    g_next_handle = 1;
    g_next_fb_id = 1;
    g_active_fb = 0;
    write_serial_string("[DRM] AMS DRM subsystem initialized\n");
}
