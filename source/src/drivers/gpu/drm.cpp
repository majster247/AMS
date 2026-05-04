/**
 * @file drm.cpp
 * @brief AMS-OS DRM/KMS subsystem with GEM/TTM buffer management
 *
 * Provides a Linux DRM-compatible kernel interface for:
 *   - KMS mode setting on the Multiboot2 linear framebuffer
 *   - GEM buffer object allocation/mapping
 *   - TTM-style buffer placement tracking
 *   - Dumb buffer support for software compositors (wlroots, etc.)
 *   - Framebuffer object management with page-flip
 */

#include "drm.h"
#include "graphics.h"
#include "kernel.h"
#include "vmm.h"
#include <stdint.h>

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" int k_strlen(const char* str);
extern "C" char* k_strcpy(char* dest, const char* src);

extern "C" void write_serial_string(const char* str);
extern "C" void write_serial_hex(uint64_t val);
extern "C" void write_serial_dec(uint64_t val);

extern Framebuffer fb;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t* backbuffer;

extern "C" void graphics_flip();
extern "C" void* pmm_alloc_frame();
extern "C" void vmm_map_page_ex(uint64_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#define PHYS_OFFSET 0xFFFF800000000000ULL

struct drm_device g_drm_dev;

void drm_init(uint64_t fb_addr, uint32_t width, uint32_t height,
              uint32_t pitch, uint8_t bpp) {
    k_memset(&g_drm_dev, 0, sizeof(g_drm_dev));

    g_drm_dev.fb_width = width;
    g_drm_dev.fb_height = height;
    g_drm_dev.fb_pitch = pitch;
    g_drm_dev.fb_bpp = bpp;
    g_drm_dev.fb_base = (void*)(fb_addr + PHYS_OFFSET);
    g_drm_dev.next_gem_handle = 1;
    g_drm_dev.next_gem_name = 1;
    g_drm_dev.next_fb_id = 1;
    g_drm_dev.master_fd = -1;

    /* Initialize default CRTC */
    g_drm_dev.crtc_count = 1;
    g_drm_dev.crtcs[0].crtc_id = 1;
    g_drm_dev.crtcs[0].mode_valid = 1;
    g_drm_dev.crtcs[0].mode.clock = (width * height * 60) / 1000;
    g_drm_dev.crtcs[0].mode.hdisplay = (uint16_t)width;
    g_drm_dev.crtcs[0].mode.hsync_start = (uint16_t)(width + 48);
    g_drm_dev.crtcs[0].mode.hsync_end = (uint16_t)(width + 48 + 32);
    g_drm_dev.crtcs[0].mode.htotal = (uint16_t)(width + 48 + 32 + 80);
    g_drm_dev.crtcs[0].mode.vdisplay = (uint16_t)height;
    g_drm_dev.crtcs[0].mode.vsync_start = (uint16_t)(height + 3);
    g_drm_dev.crtcs[0].mode.vsync_end = (uint16_t)(height + 3 + 5);
    g_drm_dev.crtcs[0].mode.vtotal = (uint16_t)(height + 3 + 5 + 33);
    g_drm_dev.crtcs[0].mode.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
    g_drm_dev.crtcs[0].mode.type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;

    /* Build mode name string */
    char* nm = g_drm_dev.crtcs[0].mode.name;
    int pos = 0;
    uint32_t w = width, h = height;
    char tmp[16];
    int ti = 0;
    do { tmp[ti++] = '0' + (w % 10); w /= 10; } while (w);
    while (ti > 0) nm[pos++] = tmp[--ti];
    nm[pos++] = 'x';
    ti = 0;
    do { tmp[ti++] = '0' + (h % 10); h /= 10; } while (h);
    while (ti > 0) nm[pos++] = tmp[--ti];
    nm[pos] = '\0';

    g_drm_dev.initialized = 1;

    write_serial_string("[DRM] Initialized: ");
    write_serial_dec(width);
    write_serial_string("x");
    write_serial_dec(height);
    write_serial_string(" bpp=");
    write_serial_dec(bpp);
    write_serial_string("\n");
}

/* --- GEM Operations --- */

uint32_t drm_gem_create(uint64_t size) {
    if (size == 0) return 0;

    uint64_t aligned_size = (size + 0xFFF) & ~0xFFFULL;
    uint32_t num_pages = (uint32_t)(aligned_size >> 12);

    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (!g_drm_dev.gems[i].in_use) {
            void* mem = kmalloc(aligned_size);
            if (!mem) return 0;
            k_memset(mem, 0, aligned_size);

            g_drm_dev.gems[i].handle = g_drm_dev.next_gem_handle++;
            g_drm_dev.gems[i].size = aligned_size;
            g_drm_dev.gems[i].vaddr = mem;
            g_drm_dev.gems[i].ref_count = 1;
            g_drm_dev.gems[i].in_use = 1;
            g_drm_dev.gems[i].name = 0;

            /* TTM tracking */
            g_drm_dev.ttm_bufs[i].placement = TTM_PL_SYSTEM;
            g_drm_dev.ttm_bufs[i].gem_handle = g_drm_dev.gems[i].handle;
            g_drm_dev.ttm_bufs[i].size = aligned_size;
            g_drm_dev.ttm_bufs[i].pages = mem;
            g_drm_dev.ttm_bufs[i].num_pages = num_pages;
            g_drm_dev.ttm_bufs[i].in_use = 1;

            write_serial_string("[DRM/GEM] Created handle=");
            write_serial_dec(g_drm_dev.gems[i].handle);
            write_serial_string(" size=");
            write_serial_dec(aligned_size);
            write_serial_string("\n");

            return g_drm_dev.gems[i].handle;
        }
    }
    return 0;
}

void drm_gem_destroy(uint32_t handle) {
    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (g_drm_dev.gems[i].in_use && g_drm_dev.gems[i].handle == handle) {
            if (--g_drm_dev.gems[i].ref_count == 0) {
                if (g_drm_dev.gems[i].vaddr)
                    kfree(g_drm_dev.gems[i].vaddr);
                g_drm_dev.gems[i].in_use = 0;
                g_drm_dev.ttm_bufs[i].in_use = 0;
            }
            return;
        }
    }
}

struct drm_gem_object* drm_gem_lookup(uint32_t handle) {
    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (g_drm_dev.gems[i].in_use && g_drm_dev.gems[i].handle == handle)
            return &g_drm_dev.gems[i];
    }
    return nullptr;
}

/* --- TTM Operations --- */

int drm_ttm_alloc(uint32_t gem_handle, uint32_t placement) {
    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (g_drm_dev.ttm_bufs[i].in_use &&
            g_drm_dev.ttm_bufs[i].gem_handle == gem_handle) {
            g_drm_dev.ttm_bufs[i].placement = placement;
            return 0;
        }
    }
    return -1;
}

void drm_ttm_free(uint32_t gem_handle) {
    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (g_drm_dev.ttm_bufs[i].in_use &&
            g_drm_dev.ttm_bufs[i].gem_handle == gem_handle) {
            g_drm_dev.ttm_bufs[i].in_use = 0;
            return;
        }
    }
}

/* --- DRM ioctl handler --- */

static int64_t drm_ioctl_version(uint64_t arg) {
    struct drm_version* ver = (struct drm_version*)arg;
    if (!ver) return -14; /* EFAULT */

    ver->version_major = 1;
    ver->version_minor = 0;
    ver->version_patchlevel = 0;
    k_strcpy(ver->name, "ams-drm");
    ver->name_len = 7;
    k_strcpy(ver->date, "20260504");
    ver->date_len = 8;
    k_strcpy(ver->desc, "AMS-OS DRM/KMS with GEM/TTM");
    ver->desc_len = 27;
    return 0;
}

static int64_t drm_ioctl_get_resources(uint64_t arg) {
    struct drm_mode_card_res* res = (struct drm_mode_card_res*)arg;
    if (!res) return -14;

    res->count_crtcs = g_drm_dev.crtc_count;
    res->count_connectors = 1;
    res->count_encoders = 1;
    res->count_fbs = g_drm_dev.fb_count;

    res->crtc_ids[0] = 1;
    res->connector_ids[0] = 1;
    res->encoder_ids[0] = 1;

    return 0;
}

static int64_t drm_ioctl_get_connector(uint64_t arg) {
    struct drm_mode_get_connector* conn = (struct drm_mode_get_connector*)arg;
    if (!conn) return -14;

    conn->connector_id = 1;
    conn->encoder_id = 1;
    conn->connector_type = DRM_MODE_CONNECTOR_VIRTUAL;
    conn->connection = DRM_MODE_CONNECTED;
    conn->count_modes = 1;
    conn->mm_width = 300;
    conn->mm_height = 170;

    k_memcpy(&conn->modes[0], &g_drm_dev.crtcs[0].mode,
             sizeof(struct drm_mode_modeinfo));

    return 0;
}

static int64_t drm_ioctl_get_encoder(uint64_t arg) {
    struct drm_mode_get_encoder* enc = (struct drm_mode_get_encoder*)arg;
    if (!enc) return -14;

    enc->encoder_id = 1;
    enc->encoder_type = 1;
    enc->crtc_id = 1;
    enc->possible_crtcs = 1;
    enc->possible_clones = 0;
    return 0;
}

static int64_t drm_ioctl_get_crtc(uint64_t arg) {
    struct drm_mode_crtc* crtc = (struct drm_mode_crtc*)arg;
    if (!crtc) return -14;

    uint32_t id = crtc->crtc_id;
    if (id == 0) id = 1;

    for (uint32_t i = 0; i < g_drm_dev.crtc_count; ++i) {
        if (g_drm_dev.crtcs[i].crtc_id == id) {
            k_memcpy(crtc, &g_drm_dev.crtcs[i], sizeof(struct drm_mode_crtc));
            return 0;
        }
    }
    return -22; /* EINVAL */
}

static int64_t drm_ioctl_set_crtc(uint64_t arg) {
    struct drm_mode_crtc* crtc = (struct drm_mode_crtc*)arg;
    if (!crtc) return -14;

    for (uint32_t i = 0; i < g_drm_dev.crtc_count; ++i) {
        if (g_drm_dev.crtcs[i].crtc_id == crtc->crtc_id) {
            g_drm_dev.crtcs[i].fb_id = crtc->fb_id;
            g_drm_dev.crtcs[i].x = crtc->x;
            g_drm_dev.crtcs[i].y = crtc->y;
            if (crtc->mode_valid) {
                k_memcpy(&g_drm_dev.crtcs[i].mode, &crtc->mode,
                         sizeof(struct drm_mode_modeinfo));
                g_drm_dev.crtcs[i].mode_valid = 1;
            }
            g_drm_dev.active_fb_id = crtc->fb_id;
            return 0;
        }
    }
    return -22;
}

static int64_t drm_ioctl_create_dumb(uint64_t arg) {
    struct drm_mode_create_dumb* req = (struct drm_mode_create_dumb*)arg;
    if (!req || req->width == 0 || req->height == 0 || req->bpp == 0) return -22;

    req->pitch = req->width * (req->bpp / 8);
    if (req->pitch & 63) req->pitch = (req->pitch + 63) & ~63U;
    req->size = (uint64_t)req->pitch * req->height;

    uint32_t handle = drm_gem_create(req->size);
    if (!handle) return -12; /* ENOMEM */

    req->handle = handle;

    write_serial_string("[DRM] create_dumb: ");
    write_serial_dec(req->width);
    write_serial_string("x");
    write_serial_dec(req->height);
    write_serial_string(" handle=");
    write_serial_dec(handle);
    write_serial_string("\n");

    return 0;
}

static int64_t drm_ioctl_map_dumb(uint64_t arg) {
    struct drm_mode_map_dumb* req = (struct drm_mode_map_dumb*)arg;
    if (!req) return -14;

    struct drm_gem_object* gem = drm_gem_lookup(req->handle);
    if (!gem) return -22;

    req->offset = (uint64_t)gem->vaddr;
    return 0;
}

static int64_t drm_ioctl_destroy_dumb(uint64_t arg) {
    struct drm_mode_destroy_dumb* req = (struct drm_mode_destroy_dumb*)arg;
    if (!req) return -14;
    drm_gem_destroy(req->handle);
    return 0;
}

static int64_t drm_ioctl_add_fb(uint64_t arg) {
    struct drm_mode_fb_cmd* req = (struct drm_mode_fb_cmd*)arg;
    if (!req) return -14;

    struct drm_gem_object* gem = drm_gem_lookup(req->handle);
    if (!gem) return -22;

    if (g_drm_dev.fb_count >= 16) return -28; /* ENOSPC */

    uint32_t fb_id = g_drm_dev.next_fb_id++;
    uint32_t idx = g_drm_dev.fb_count++;

    g_drm_dev.fbs[idx].fb_id = fb_id;
    g_drm_dev.fbs[idx].width = req->width;
    g_drm_dev.fbs[idx].height = req->height;
    g_drm_dev.fbs[idx].pitch = req->pitch;
    g_drm_dev.fbs[idx].bpp = req->bpp;
    g_drm_dev.fbs[idx].depth = req->depth;
    g_drm_dev.fbs[idx].handle = req->handle;

    req->fb_id = fb_id;

    write_serial_string("[DRM] add_fb: id=");
    write_serial_dec(fb_id);
    write_serial_string(" ");
    write_serial_dec(req->width);
    write_serial_string("x");
    write_serial_dec(req->height);
    write_serial_string("\n");

    return 0;
}

static int64_t drm_ioctl_rm_fb(uint64_t arg) {
    uint32_t fb_id = *(uint32_t*)arg;
    for (uint32_t i = 0; i < g_drm_dev.fb_count; ++i) {
        if (g_drm_dev.fbs[i].fb_id == fb_id) {
            for (uint32_t j = i; j + 1 < g_drm_dev.fb_count; ++j)
                g_drm_dev.fbs[j] = g_drm_dev.fbs[j + 1];
            g_drm_dev.fb_count--;
            return 0;
        }
    }
    return -22;
}

static int64_t drm_ioctl_page_flip(uint64_t arg) {
    struct drm_mode_page_flip* req = (struct drm_mode_page_flip*)arg;
    if (!req) return -14;

    /* Find the framebuffer */
    struct drm_mode_fb_cmd* fb_cmd = nullptr;
    for (uint32_t i = 0; i < g_drm_dev.fb_count; ++i) {
        if (g_drm_dev.fbs[i].fb_id == req->fb_id) {
            fb_cmd = &g_drm_dev.fbs[i];
            break;
        }
    }
    if (!fb_cmd) return -22;

    struct drm_gem_object* gem = drm_gem_lookup(fb_cmd->handle);
    if (!gem || !gem->vaddr) return -22;

    /* Copy GEM buffer to hardware backbuffer and flip */
    if (backbuffer && gem->vaddr) {
        uint32_t copy_h = (fb_cmd->height < fb_height) ? fb_cmd->height : fb_height;
        uint32_t copy_w = (fb_cmd->width < fb_width) ? fb_cmd->width : fb_width;
        uint32_t src_pitch = fb_cmd->pitch;
        uint32_t dst_pitch = fb_width * 4;

        for (uint32_t y = 0; y < copy_h; ++y) {
            k_memcpy((uint8_t*)backbuffer + y * dst_pitch,
                     (uint8_t*)gem->vaddr + y * src_pitch,
                     copy_w * 4);
        }
        graphics_flip();
    }

    g_drm_dev.active_fb_id = req->fb_id;
    return 0;
}

static int64_t drm_ioctl_gem_close(uint64_t arg) {
    struct drm_gem_close* req = (struct drm_gem_close*)arg;
    if (!req) return -14;
    drm_gem_destroy(req->handle);
    return 0;
}

static int64_t drm_ioctl_gem_open(uint64_t arg) {
    struct drm_gem_open* req = (struct drm_gem_open*)arg;
    if (!req) return -14;

    for (uint32_t i = 0; i < DRM_GEM_MAX_OBJECTS; ++i) {
        if (g_drm_dev.gems[i].in_use && g_drm_dev.gems[i].name == req->name) {
            g_drm_dev.gems[i].ref_count++;
            req->handle = g_drm_dev.gems[i].handle;
            req->size = g_drm_dev.gems[i].size;
            return 0;
        }
    }
    return -2; /* ENOENT */
}

static int64_t drm_ioctl_gem_flink(uint64_t arg) {
    struct drm_gem_flink* req = (struct drm_gem_flink*)arg;
    if (!req) return -14;

    struct drm_gem_object* gem = drm_gem_lookup(req->handle);
    if (!gem) return -22;

    if (gem->name == 0)
        gem->name = g_drm_dev.next_gem_name++;
    req->name = gem->name;
    return 0;
}

static int64_t drm_ioctl_set_master(int fd) {
    g_drm_dev.master_fd = fd;
    return 0;
}

static int64_t drm_ioctl_drop_master(int fd) {
    if (g_drm_dev.master_fd == fd)
        g_drm_dev.master_fd = -1;
    return 0;
}

int64_t drm_ioctl(int fd, uint64_t request, uint64_t arg) {
    if (!g_drm_dev.initialized) return -19; /* ENODEV */

    switch (request) {
        case DRM_IOCTL_VERSION:           return drm_ioctl_version(arg);
        case DRM_IOCTL_MODE_GETRESOURCES: return drm_ioctl_get_resources(arg);
        case DRM_IOCTL_MODE_GETCRTC:      return drm_ioctl_get_crtc(arg);
        case DRM_IOCTL_MODE_SETCRTC:      return drm_ioctl_set_crtc(arg);
        case DRM_IOCTL_MODE_GETCONNECTOR: return drm_ioctl_get_connector(arg);
        case DRM_IOCTL_MODE_GETENCODER:   return drm_ioctl_get_encoder(arg);
        case DRM_IOCTL_MODE_CREATE_DUMB:  return drm_ioctl_create_dumb(arg);
        case DRM_IOCTL_MODE_MAP_DUMB:     return drm_ioctl_map_dumb(arg);
        case DRM_IOCTL_MODE_DESTROY_DUMB: return drm_ioctl_destroy_dumb(arg);
        case DRM_IOCTL_MODE_ADDFB:        return drm_ioctl_add_fb(arg);
        case DRM_IOCTL_MODE_RMFB:         return drm_ioctl_rm_fb(arg);
        case DRM_IOCTL_MODE_PAGE_FLIP:    return drm_ioctl_page_flip(arg);
        case DRM_IOCTL_GEM_CLOSE:         return drm_ioctl_gem_close(arg);
        case DRM_IOCTL_GEM_OPEN:          return drm_ioctl_gem_open(arg);
        case DRM_IOCTL_GEM_FLINK:         return drm_ioctl_gem_flink(arg);
        case DRM_IOCTL_SET_MASTER:        return drm_ioctl_set_master(fd);
        case DRM_IOCTL_DROP_MASTER:       return drm_ioctl_drop_master(fd);
        default:
            write_serial_string("[DRM] Unknown ioctl: ");
            write_serial_hex(request);
            write_serial_string("\n");
            return -25; /* ENOTTY */
    }
}
