/**
 * @file drm/gem_ttm.cpp
 * @brief GEM + TTM implementation for AMS-OS.
 *
 * Architecture:
 *  - GEM BOs are backed by contiguous physical pages allocated through pmm_alloc_frame().
 *  - The TTM placement flags select VRAM (framebuffer region) vs. system RAM.
 *  - KMS uses the multiboot2 linear framebuffer as the sole output.
 *  - A "dumb buffer" (DRM_IOCTL_MODE_CREATE_DUMB) creates a GEM BO that can be
 *    scanned out by the CRTC after a DRM_IOCTL_MODE_PAGE_FLIP.
 *  - GBM / EGL will use DMA-BUF prime export to share BOs with Mesa.
 */

#include "drm/gem_ttm.h"
#include "kernel.h"
#include "vmm.h"
#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

extern "C" {
    void* kmalloc(size_t);
    void  kfree(void*);
    void* k_memset(void*, int, size_t);
    void* k_memcpy(void*, const void*, size_t);
    void  write_serial_string(const char*);
    void  write_serial_hex(uint64_t);
    void  write_serial_dec(uint64_t);
    void  graphics_flip(void);

    /* PMM interface */
    void* pmm_alloc_frame(void);
    void  pmm_free_frame(void* phys);
}

extern uint32_t* backbuffer;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern task* current_task;

/* ---- GEM BO table ---- */
static gem_bo g_bos[GEM_MAX_BOS];
static uint32_t g_next_handle = 1;

/* ---- KMS state ---- */
static kms_connector g_connector;
static kms_crtc      g_crtc;

/* ---- DRM fd tracking ---- */
#define DRM_MAX_FDS 16
static struct {
    bool     open;
    uint32_t handles[GEM_MAX_HANDLES];  /* BO handles owned by this fd */
    uint32_t n_handles;
} g_drm_fds[DRM_MAX_FDS];

/* ---- mmap arena cursor ---- */
static uint64_t g_gem_mmap_ptr = GEM_MMAP_BASE;

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */
static void dbg(const char* msg) {
    write_serial_string("[GEM] ");
    write_serial_string(msg);
    write_serial_string("\n");
}

static gem_bo* alloc_bo_slot(void) {
    for (int i = 0; i < GEM_MAX_BOS; ++i) {
        if (!g_bos[i].in_use) return &g_bos[i];
    }
    return nullptr;
}

/* -----------------------------------------------------------------------
 * Init
 * --------------------------------------------------------------------- */
void gem_ttm_init(void) {
    k_memset(g_bos, 0, sizeof(g_bos));
    k_memset(g_drm_fds, 0, sizeof(g_drm_fds));

    for (int i = 0; i < DRM_MAX_FDS; ++i)
        g_drm_fds[i].open = false;

    /* Initialise singleton KMS connector from the live framebuffer */
    g_connector.id        = 1;
    g_connector.width_mm  = 530;
    g_connector.height_mm = 300;
    g_connector.hdisplay  = fb_width  ? fb_width  : 1280;
    g_connector.vdisplay  = fb_height ? fb_height : 720;
    g_connector.vrefresh  = 60000; /* mHz */
    g_connector.connected = 1;

    g_crtc.id              = 1;
    g_crtc.active          = 1;
    g_crtc.scanout_bo_handle = 0;
    g_crtc.x = g_crtc.y   = 0;
    g_crtc.mode_width      = g_connector.hdisplay;
    g_crtc.mode_height     = g_connector.vdisplay;

    dbg("subsystem initialised");
}

/* -----------------------------------------------------------------------
 * GEM BO lifecycle
 * --------------------------------------------------------------------- */
gem_bo* gem_create(uint64_t size, uint32_t placement) {
    if (!size) return nullptr;
    size = (size + 4095) & ~4095ULL;

    gem_bo* bo = alloc_bo_slot();
    if (!bo) { dbg("gem_create: table full"); return nullptr; }

    uint64_t n_pages = size / 4096;
    /* Allocate physically-contiguous pages via PMM.
     * For simplicity we chain pmm_alloc_frame calls and record only the base.
     * A real TTM would use scatter-gather; this is sufficient for software
     * rendering (swrast/llvmpipe) and the CPU-blit scanout path. */
    void* first = pmm_alloc_frame();
    if (!first) { dbg("gem_create: OOM"); return nullptr; }
    uint64_t phys = (uint64_t)first;

    /* Zero all pages through the HHDM mirror */
    for (uint64_t p = 0; p < n_pages; ++p) {
        void* page = (p == 0) ? first : pmm_alloc_frame();
        if (!page) { dbg("gem_create: OOM mid-alloc"); break; }
        k_memset((void*)((uint64_t)page + PHYS_OFFSET), 0, 4096);
    }

    k_memset(bo, 0, sizeof(*bo));
    bo->in_use    = 1;
    bo->handle    = g_next_handle++;
    bo->placement = placement;
    bo->size      = size;
    bo->phys_base = phys;
    bo->user_vaddr = 0;
    bo->refcount  = 1;
    bo->dma_buf_fd = -1;

    return bo;
}

void gem_get(gem_bo* bo) {
    if (bo) bo->refcount++;
}

void gem_put(gem_bo* bo) {
    if (!bo) return;
    if (bo->refcount == 0) return;
    bo->refcount--;
    if (bo->refcount == 0) {
        /* Free physical pages */
        uint64_t n_pages = bo->size / 4096;
        for (uint64_t p = 0; p < n_pages; ++p) {
            pmm_free_frame((void*)(bo->phys_base + p * 4096));
        }
        k_memset(bo, 0, sizeof(*bo));
    }
}

gem_bo* gem_lookup(uint32_t handle) {
    if (!handle) return nullptr;
    for (int i = 0; i < GEM_MAX_BOS; ++i) {
        if (g_bos[i].in_use && g_bos[i].handle == handle)
            return &g_bos[i];
    }
    return nullptr;
}

/* -----------------------------------------------------------------------
 * GEM mmap
 * Map BO physical pages into the current user task with write-combine attrs.
 * --------------------------------------------------------------------- */
uint64_t gem_mmap_bo(gem_bo* bo) {
    if (!bo || !bo->in_use) return 0;
    if (bo->user_vaddr) return bo->user_vaddr; /* already mapped */

    if (!current_task) return 0;

    uint64_t vaddr = g_gem_mmap_ptr;
    g_gem_mmap_ptr += bo->size + 4096; /* guard page gap */

    uint64_t n_pages = bo->size / 4096;
    for (uint64_t p = 0; p < n_pages; ++p) {
        uint64_t phys = bo->phys_base + p * 4096;
        /* flags: USER(2) | WRITABLE(1) | PRESENT(0) – write-combine at PTE level
         * requires PAT support; we set PWT+PCD bits (flags |= 0x18) for WC.
         * On QEMU/simulated hw this maps to write-combine if PAT entry 1 is WC. */
        uint64_t flags = 0x7 | 0x18; /* USER | WRITABLE | PRESENT | PWT | PCD */
        vmm_map_page_ex(current_task->cr3, vaddr + p * 4096, phys, flags);
    }

    bo->user_vaddr = vaddr;
    return vaddr;
}

/* -----------------------------------------------------------------------
 * KMS scanout / page flip
 * --------------------------------------------------------------------- */
int kms_page_flip(uint32_t crtc_id, gem_bo* bo) {
    if (crtc_id != 1 || !bo || !bo->in_use) return -22; /* EINVAL */
    if (!bo->phys_base) return -22;

    uint32_t w = bo->width  ? bo->width  : g_crtc.mode_width;
    uint32_t h = bo->height ? bo->height : g_crtc.mode_height;
    uint32_t pitch = bo->pitch ? bo->pitch : w * 4;

    /* Blit BO to the kernel backbuffer line-by-line via HHDM mirror */
    for (uint32_t y = 0; y < h && y < fb_height; ++y) {
        uint64_t src_phys = bo->phys_base + (uint64_t)y * pitch;
        const uint32_t* src = (const uint32_t*)(src_phys + PHYS_OFFSET);
        uint32_t* dst = backbuffer + y * fb_width;
        uint32_t copy_px = w < fb_width ? w : fb_width;
        k_memcpy(dst, src, copy_px * 4);
    }

    /* Push backbuffer to the physical LFB */
    graphics_flip();

    g_crtc.scanout_bo_handle = bo->handle;
    return 0;
}

/* -----------------------------------------------------------------------
 * DRM fd management
 * --------------------------------------------------------------------- */
int drm_open(void) {
    for (int i = 0; i < DRM_MAX_FDS; ++i) {
        if (!g_drm_fds[i].open) {
            g_drm_fds[i].open = true;
            g_drm_fds[i].n_handles = 0;
            return i; /* index into g_drm_fds; caller maps to real fd */
        }
    }
    return -1;
}

void drm_close(int drm_idx) {
    if (drm_idx < 0 || drm_idx >= DRM_MAX_FDS) return;
    auto& d = g_drm_fds[drm_idx];
    /* Release all BO handles owned by this fd */
    for (uint32_t i = 0; i < d.n_handles; ++i) {
        gem_bo* bo = gem_lookup(d.handles[i]);
        if (bo) gem_put(bo);
    }
    k_memset(&d, 0, sizeof(d));
}

kms_connector* kms_get_connector(void) { return &g_connector; }
kms_crtc*      kms_get_crtc(void)      { return &g_crtc; }

/* -----------------------------------------------------------------------
 * DRM ioctl dispatcher
 * --------------------------------------------------------------------- */
static void fill_modeinfo(drm_mode_modeinfo* m, uint32_t w, uint32_t h, uint32_t vrefresh_hz) {
    k_memset(m, 0, sizeof(*m));
    m->hdisplay = (uint16_t)w;
    m->vdisplay = (uint16_t)h;
    m->vrefresh = vrefresh_hz;
    m->type = 8; /* DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER */
    /* Build a name string "WxH" */
    char* n = m->name;
    uint32_t v = w;
    char tmp[16]; int tl = 0;
    do { tmp[tl++] = '0' + (v % 10); v /= 10; } while (v);
    for (int i = tl - 1; i >= 0; --i) *n++ = tmp[i];
    *n++ = 'x';
    v = h; tl = 0;
    do { tmp[tl++] = '0' + (v % 10); v /= 10; } while (v);
    for (int i = tl - 1; i >= 0; --i) *n++ = tmp[i];
    *n = '\0';
}

uint64_t drm_ioctl(int drm_idx, uint64_t request, uint64_t arg_ptr) {
    if (drm_idx < 0 || drm_idx >= DRM_MAX_FDS) return (uint64_t)-9;

    uint32_t cmd = (uint32_t)(request & 0xFFFF);

    switch (cmd) {

    /* DRM_IOCTL_GEM_CLOSE */
    case 0x0964 & 0xFFFF:
    case 0x09: {
        drm_gem_close* a = (drm_gem_close*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup(a->handle);
        if (bo) gem_put(bo);
        return 0;
    }

    /* DRM_IOCTL_GET_CAP */
    case 0x0c: {
        drm_get_cap* a = (drm_get_cap*)arg_ptr;
        if (!a) return (uint64_t)-14;
        /* capability 1 = DUMB_BUFFER, 2 = VBLANK_HIGH_CRTC, 3 = DUMB_PREFERRED_DEPTH */
        switch (a->capability) {
            case 1: a->value = 1; break; /* DUMB_BUFFER supported */
            case 2: a->value = 0; break;
            case 3: a->value = 32; break;
            case 4: a->value = 0; break; /* DUMB_PREFER_SHADOW */
            default: a->value = 0; break;
        }
        return 0;
    }

    /* DRM_IOCTL_MODE_GETRESOURCES */
    case 0xa0: {
        drm_mode_resources* a = (drm_mode_resources*)arg_ptr;
        if (!a) return (uint64_t)-14;
        a->count_fbs        = 0;
        a->count_crtcs      = 1;
        a->count_connectors = 1;
        a->count_encoders   = 1;
        a->min_width  = 64;  a->max_width  = 8192;
        a->min_height = 64;  a->max_height = 8192;
        if (a->crtc_id_ptr) {
            uint32_t* p = (uint32_t*)a->crtc_id_ptr;
            *p = 1;
        }
        if (a->connector_id_ptr) {
            uint32_t* p = (uint32_t*)a->connector_id_ptr;
            *p = 1;
        }
        if (a->encoder_id_ptr) {
            uint32_t* p = (uint32_t*)a->encoder_id_ptr;
            *p = 1;
        }
        return 0;
    }

    /* DRM_IOCTL_MODE_GETCRTC */
    case 0xa1: {
        drm_mode_crtc* a = (drm_mode_crtc*)arg_ptr;
        if (!a) return (uint64_t)-14;
        a->crtc_id   = g_crtc.id;
        a->fb_id     = g_crtc.scanout_bo_handle;
        a->x = g_crtc.x; a->y = g_crtc.y;
        a->mode_valid = 1;
        fill_modeinfo(&a->mode, g_crtc.mode_width, g_crtc.mode_height, 60);
        return 0;
    }

    /* DRM_IOCTL_MODE_SETCRTC */
    case 0xa2: {
        drm_mode_crtc* a = (drm_mode_crtc*)arg_ptr;
        if (!a) return (uint64_t)-14;
        if (a->fb_id) {
            gem_bo* bo = gem_lookup(a->fb_id);
            if (bo) kms_page_flip(1, bo);
        }
        g_crtc.x = a->x; g_crtc.y = a->y;
        return 0;
    }

    /* DRM_IOCTL_MODE_CREATE_DUMB */
    case 0xb2: {
        drm_mode_create_dumb* a = (drm_mode_create_dumb*)arg_ptr;
        if (!a) return (uint64_t)-14;
        uint32_t bpp = a->bpp ? a->bpp : 32;
        uint32_t pitch = ((a->width * bpp + 7) / 8 + 63) & ~63u;
        uint64_t size  = (uint64_t)pitch * a->height;

        gem_bo* bo = gem_create(size, TTM_PL_FLAG_TT | TTM_PL_FLAG_WC);
        if (!bo) return (uint64_t)-12; /* ENOMEM */

        bo->width  = a->width;
        bo->height = a->height;
        bo->pitch  = pitch;
        bo->format = 0x34325241; /* DRM_FORMAT_ARGB8888 */

        a->handle = bo->handle;
        a->pitch  = pitch;
        a->size   = size;

        /* Track handle in drm fd */
        auto& d = g_drm_fds[drm_idx];
        if (d.n_handles < GEM_MAX_HANDLES)
            d.handles[d.n_handles++] = bo->handle;

        return 0;
    }

    /* DRM_IOCTL_MODE_MAP_DUMB */
    case 0xb3: {
        drm_mode_map_dumb* a = (drm_mode_map_dumb*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup(a->handle);
        if (!bo) return (uint64_t)-9;
        /* Return the phys_base encoded as mmap offset (fake offset = phys_base).
         * The subsequent mmap() with this offset will be intercepted in sys_mmap
         * and routed to gem_mmap_bo(). */
        a->offset = bo->phys_base;
        return 0;
    }

    /* DRM_IOCTL_MODE_DESTROY_DUMB */
    case 0xb4: {
        drm_mode_destroy_dumb* a = (drm_mode_destroy_dumb*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup(a->handle);
        if (bo) gem_put(bo);
        return 0;
    }

    /* DRM_IOCTL_MODE_PAGE_FLIP */
    case 0xb0: {
        drm_mode_page_flip* a = (drm_mode_page_flip*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup(a->fb_id);
        if (!bo) return (uint64_t)-9;
        return (uint64_t)kms_page_flip(a->crtc_id, bo);
    }

    /* DRM_IOCTL_PRIME_HANDLE_TO_FD */
    case 0x2d: {
        drm_prime_handle* a = (drm_prime_handle*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup(a->handle);
        if (!bo) return (uint64_t)-9;
        /* In a full implementation we'd create a dma-buf fd via sys_memfd_create.
         * Here we use the BO handle as a synthetic dma-buf fd number. */
        a->fd = (int32_t)bo->handle;
        bo->dma_buf_fd = a->fd;
        return 0;
    }

    /* DRM_IOCTL_PRIME_FD_TO_HANDLE */
    case 0x2e: {
        drm_prime_handle* a = (drm_prime_handle*)arg_ptr;
        if (!a) return (uint64_t)-14;
        gem_bo* bo = gem_lookup((uint32_t)a->fd);
        if (!bo) return (uint64_t)-9;
        a->handle = bo->handle;
        return 0;
    }

    default:
        write_serial_string("[DRM] unknown ioctl cmd=");
        write_serial_hex(cmd);
        write_serial_string("\n");
        return (uint64_t)-25; /* ENOTTY */
    }
}
