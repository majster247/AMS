/* AMS virtual DRM/KMS kernel driver.
 *
 * Exposes /dev/dri/card0 to userspace.  The "GPU" is the multiboot2
 * linear framebuffer — dumb buffers are kernel-allocated anonymous memory
 * and scanout is performed by copying pixels into the kernel backbuffer
 * followed by graphics_flip().
 *
 * All DRM ioctls arrive via sys_ioctl when fd refers to the card0 VFS node.
 */
#include "drm_virt.h"
#include "kernel.h"
#include "vfs.h"
#include "vmm.h"
#include "task.h"
#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

extern "C" void*    kmalloc(size_t);
extern "C" void     kfree(void*);
extern "C" void*    k_memset(void*, int, size_t);
extern "C" void*    k_memcpy(void*, const void*, size_t);
extern "C" char*    k_strcpy(char*, const char*);
extern "C" uint64_t vmm_get_phys_ex(uint64_t pml4, uint64_t virt);
extern "C" void     vmm_map_page_ex(uint64_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);
extern void*        pmm_alloc_frame();
extern void         graphics_flip();
extern uint32_t*    backbuffer;
extern uint32_t     fb_width;
extern uint32_t     fb_height;
extern vfs_node*    vfs_root;
extern task*        current_task;

/* ---- static DRM state ---- */

static ams_dumb_buf dumb_bufs[DRM_MAX_DUMB_BUFS];
static uint32_t     next_handle = 1;
static uint32_t     active_fb   = 0; /* handle of the currently scanned-out dumb buf */

/* mmap offset range: each dumb buffer gets  handle << 12  as its token.
 * sys_mmap() checks if the requested offset falls in [DUMB_MMAP_BASE, ...) */
#define DUMB_MMAP_BASE  0x100000000ULL   /* 4 GiB token space */

/* VFS node for /dev/dri/card0 */
static vfs_node card0_node;

/* ---- helpers ---- */

static bool is_user_ptr(uint64_t a) {
    return (a >= 0x04000000ULL && a < 0x0000800000000000ULL);
}

/* Copy n bytes from user virtual address src into kernel buffer dst.
 * Uses the current task's page tables to walk pages. */
static bool copy_from_user(void* dst, uint64_t src_va, uint64_t n) {
    if (!is_user_ptr(src_va)) return false;
    uint8_t* d = (uint8_t*)dst;
    uint64_t done = 0;
    while (done < n) {
        uint64_t page_off = (src_va + done) & 0xFFF;
        uint64_t chunk    = 0x1000 - page_off;
        if (chunk > n - done) chunk = n - done;
        uint64_t phys = vmm_get_phys_ex(current_task->cr3, src_va + done);
        if (!phys) return false;
        k_memcpy(d + done, (void*)(phys + 0xFFFF800000000000ULL + page_off), chunk);
        done += chunk;
    }
    return true;
}

static bool copy_to_user(uint64_t dst_va, const void* src, uint64_t n) {
    if (!is_user_ptr(dst_va)) return false;
    const uint8_t* s = (const uint8_t*)src;
    uint64_t done = 0;
    while (done < n) {
        uint64_t page_off = (dst_va + done) & 0xFFF;
        uint64_t chunk    = 0x1000 - page_off;
        if (chunk > n - done) chunk = n - done;
        uint64_t phys = vmm_get_phys_ex(current_task->cr3, dst_va + done);
        if (!phys) return false;
        k_memcpy((void*)(phys + 0xFFFF800000000000ULL + page_off), s + done, chunk);
        done += chunk;
    }
    return true;
}

/* Build a drm_mode_modeinfo for the current framebuffer dimensions */
static void fill_mode(drm_mode_modeinfo* m) {
    uint32_t w = fb_width  ? fb_width  : 1920;
    uint32_t h = fb_height ? fb_height : 1080;
    k_memset(m, 0, sizeof(*m));
    m->clock       = (uint32_t)(w * h * 60 / 1000);
    m->hdisplay    = (uint16_t)w;
    m->hsync_start = (uint16_t)(w + 88);
    m->hsync_end   = (uint16_t)(w + 88 + 44);
    m->htotal      = (uint16_t)(w + 88 + 44 + 148);
    m->vdisplay    = (uint16_t)h;
    m->vsync_start = (uint16_t)(h + 4);
    m->vsync_end   = (uint16_t)(h + 4 + 5);
    m->vtotal      = (uint16_t)(h + 4 + 5 + 36);
    m->vrefresh    = 60;
    m->flags       = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;
    m->type        = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;
    /* Build name string manually without sprintf */
    char* name = m->name;
    /* Print width */
    uint32_t tmp = w;
    int pos = 0;
    char tmp_s[16];
    int tpos = 0;
    do { tmp_s[tpos++] = '0' + (tmp % 10); tmp /= 10; } while (tmp);
    while (tpos > 0) name[pos++] = tmp_s[--tpos];
    name[pos++] = 'x';
    tmp = h; tpos = 0;
    do { tmp_s[tpos++] = '0' + (tmp % 10); tmp /= 10; } while (tmp);
    while (tpos > 0) name[pos++] = tmp_s[--tpos];
    name[pos] = '\0';
}

/* ---- DRM ioctl handlers ---- */

static int handle_version(uint64_t arg) {
    drm_version v;
    if (!copy_from_user(&v, arg, sizeof(v))) return -14;
    /* Fill in version numbers; ignore name/date/desc pointers for now */
    v.version_major      = 1;
    v.version_minor      = 0;
    v.version_patchlevel = 0;
    /* If caller provided name buffer, write into it */
    if (v.name && v.name_len >= 7 && is_user_ptr(v.name)) {
        const char* drv = "ams-drm";
        copy_to_user(v.name, drv, 8);
        v.name_len = 7;
    }
    if (v.date && v.date_len >= 4 && is_user_ptr(v.date)) {
        copy_to_user(v.date, "2026", 5);
        v.date_len = 4;
    }
    if (v.desc && v.desc_len >= 20 && is_user_ptr(v.desc)) {
        const char* d = "AMS virtual DRM 0.1";
        copy_to_user(v.desc, d, 20);
        v.desc_len = 19;
    }
    copy_to_user(arg, &v, sizeof(v));
    return 0;
}

static int handle_get_cap(uint64_t arg) {
    drm_get_cap gc;
    if (!copy_from_user(&gc, arg, sizeof(gc))) return -14;
    switch (gc.capability) {
    case DRM_CAP_DUMB_BUFFER:           gc.value = 1; break;
    case DRM_CAP_DUMB_PREFERRED_DEPTH:  gc.value = 32; break;
    case DRM_CAP_DUMB_PREFER_SHADOW:    gc.value = 0; break;
    case DRM_CAP_PRIME:                 gc.value = 0; break;
    case DRM_CAP_TIMESTAMP_MONOTONIC:   gc.value = 1; break;
    case DRM_CAP_ASYNC_PAGE_FLIP:       gc.value = 0; break;
    case DRM_CAP_CURSOR_WIDTH:          gc.value = 64; break;
    case DRM_CAP_CURSOR_HEIGHT:         gc.value = 64; break;
    case DRM_CAP_ADDFB2_MODIFIERS:      gc.value = 0; break;
    default:                            gc.value = 0; break;
    }
    copy_to_user(arg, &gc, sizeof(gc));
    return 0;
}

static int handle_set_client_cap(uint64_t arg) {
    (void)arg;
    return 0; /* accept all client caps */
}

static int handle_auth_magic(uint64_t arg) {
    (void)arg;
    return 0; /* single-seat, no auth needed */
}

static int handle_get_resources(uint64_t arg) {
    drm_mode_card_res res;
    if (!copy_from_user(&res, arg, sizeof(res))) return -14;

    static const uint32_t crtc_ids[1]      = { 1 };
    static const uint32_t encoder_ids[1]   = { 1 };
    static const uint32_t connector_ids[1] = { 1 };

    /* Write IDs into caller's arrays if they provided them */
    if (res.count_crtcs >= 1 && res.crtc_id_ptr && is_user_ptr(res.crtc_id_ptr))
        copy_to_user(res.crtc_id_ptr, crtc_ids, sizeof(crtc_ids));
    if (res.count_encoders >= 1 && res.encoder_id_ptr && is_user_ptr(res.encoder_id_ptr))
        copy_to_user(res.encoder_id_ptr, encoder_ids, sizeof(encoder_ids));
    if (res.count_connectors >= 1 && res.connector_id_ptr && is_user_ptr(res.connector_id_ptr))
        copy_to_user(res.connector_id_ptr, connector_ids, sizeof(connector_ids));

    res.count_fbs        = 0;
    res.count_crtcs      = 1;
    res.count_connectors = 1;
    res.count_encoders   = 1;
    res.min_width        = 1;
    res.max_width        = fb_width ? fb_width : 7680;
    res.min_height       = 1;
    res.max_height       = fb_height ? fb_height : 4320;
    copy_to_user(arg, &res, sizeof(res));
    return 0;
}

static int handle_get_crtc(uint64_t arg) {
    drm_mode_crtc crtc;
    if (!copy_from_user(&crtc, arg, sizeof(crtc))) return -14;
    if (crtc.crtc_id != 1) return -22;

    crtc.fb_id      = active_fb;
    crtc.x          = 0;
    crtc.y          = 0;
    crtc.gamma_size = 256;
    crtc.mode_valid = 1;
    fill_mode(&crtc.mode);
    copy_to_user(arg, &crtc, sizeof(crtc));
    return 0;
}

static int handle_set_crtc(uint64_t arg) {
    drm_mode_crtc crtc;
    if (!copy_from_user(&crtc, arg, sizeof(crtc))) return -14;
    if (crtc.crtc_id != 1) return -22;

    /* Find the dumb buffer associated with fb_id and blit it */
    uint32_t fb_handle = 0;
    for (int i = 0; i < DRM_MAX_DUMB_BUFS; i++) {
        if (dumb_bufs[i].handle && dumb_bufs[i].fb_id == crtc.fb_id) {
            fb_handle = dumb_bufs[i].handle;
            break;
        }
    }
    active_fb = crtc.fb_id;

    if (fb_handle) {
        ams_dumb_buf* db = &dumb_bufs[fb_handle - 1];
        if (db->phys_pages && backbuffer) {
            /* Blit from dumb buffer pages into kernel backbuffer */
            uint32_t blit_w = db->width  < fb_width  ? db->width  : fb_width;
            uint32_t blit_h = db->height < fb_height ? db->height : fb_height;
            uint32_t row_bytes = blit_w * 4;
            for (uint32_t y = 0; y < blit_h; y++) {
                uint64_t src_off = (uint64_t)y * db->pitch;
                uint32_t pg   = (uint32_t)(src_off >> 12);
                uint32_t poff = (uint32_t)(src_off & 0xFFF);
                if (pg < db->n_pages && db->phys_pages[pg]) {
                    uint8_t* src = (uint8_t*)(db->phys_pages[pg] + 0xFFFF800000000000ULL + poff);
                    k_memcpy(backbuffer + y * fb_width, src, row_bytes);
                }
            }
            graphics_flip();
        }
    }
    return 0;
}

static int handle_get_encoder(uint64_t arg) {
    drm_mode_get_encoder enc;
    if (!copy_from_user(&enc, arg, sizeof(enc))) return -14;
    if (enc.encoder_id != 1) return -22;
    enc.encoder_type    = DRM_MODE_ENCODER_TMDS;
    enc.crtc_id         = 1;
    enc.possible_crtcs  = 0x1;
    enc.possible_clones = 0;
    copy_to_user(arg, &enc, sizeof(enc));
    return 0;
}

static int handle_get_connector(uint64_t arg) {
    drm_mode_get_connector conn;
    if (!copy_from_user(&conn, arg, sizeof(conn))) return -14;
    if (conn.connector_id != 1) return -22;

    uint32_t want_modes    = conn.count_modes;
    uint32_t want_encoders = conn.count_encoders;

    conn.encoder_id       = 1;
    conn.connector_type   = DRM_MODE_CONNECTOR_HDMIA;
    conn.connector_type_id= 1;
    conn.connection       = DRM_MODE_CONNECTED;
    conn.mm_width         = 527; /* ~27" monitor */
    conn.mm_height        = 296;
    conn.subpixel         = 0;
    conn.count_modes      = 1;
    conn.count_props      = 0;
    conn.count_encoders   = 1;

    /* Write mode */
    if (want_modes >= 1 && conn.modes_ptr && is_user_ptr(conn.modes_ptr)) {
        drm_mode_modeinfo mi;
        fill_mode(&mi);
        copy_to_user(conn.modes_ptr, &mi, sizeof(mi));
    }
    /* Write encoder IDs */
    static const uint32_t enc_id = 1;
    if (want_encoders >= 1 && conn.encoders_ptr && is_user_ptr(conn.encoders_ptr)) {
        copy_to_user(conn.encoders_ptr, &enc_id, sizeof(enc_id));
    }
    copy_to_user(arg, &conn, sizeof(conn));
    return 0;
}

static int handle_add_fb(uint64_t arg) {
    drm_mode_fb_cmd fb;
    if (!copy_from_user(&fb, arg, sizeof(fb))) return -14;

    /* Associate the dumb buffer handle with a synthetic FB id */
    for (int i = 0; i < DRM_MAX_DUMB_BUFS; i++) {
        if (dumb_bufs[i].handle == fb.handle) {
            dumb_bufs[i].fb_id = fb.handle; /* use handle as fb_id for simplicity */
            fb.fb_id = fb.handle;
            copy_to_user(arg, &fb, sizeof(fb));
            return 0;
        }
    }
    return -22; /* invalid handle */
}

static int handle_rm_fb(uint64_t arg) {
    uint32_t fb_id = 0;
    copy_from_user(&fb_id, arg, sizeof(fb_id));
    for (int i = 0; i < DRM_MAX_DUMB_BUFS; i++) {
        if (dumb_bufs[i].fb_id == fb_id) dumb_bufs[i].fb_id = 0;
    }
    return 0;
}

static int handle_page_flip(uint64_t arg) {
    drm_mode_crtc_page_flip pf;
    if (!copy_from_user(&pf, arg, sizeof(pf))) return -14;
    /* Re-use set_crtc blit logic */
    drm_mode_crtc fake;
    k_memset(&fake, 0, sizeof(fake));
    fake.crtc_id = pf.crtc_id;
    fake.fb_id   = pf.fb_id;
    /* Build a temporary crtc struct on the stack and write it to a scratch
     * kernel address (we call the handler with a kernel pointer — skip
     * copy_from_user validation by calling the blit inline). */
    uint32_t fb_handle = 0;
    for (int i = 0; i < DRM_MAX_DUMB_BUFS; i++) {
        if (dumb_bufs[i].handle && dumb_bufs[i].fb_id == pf.fb_id) {
            fb_handle = dumb_bufs[i].handle;
            break;
        }
    }
    active_fb = pf.fb_id;
    if (fb_handle) {
        ams_dumb_buf* db = &dumb_bufs[fb_handle - 1];
        if (db->phys_pages && backbuffer) {
            uint32_t blit_w = db->width  < fb_width  ? db->width  : fb_width;
            uint32_t blit_h = db->height < fb_height ? db->height : fb_height;
            uint32_t row_bytes = blit_w * 4;
            for (uint32_t y = 0; y < blit_h; y++) {
                uint64_t src_off = (uint64_t)y * db->pitch;
                uint32_t pg   = (uint32_t)(src_off >> 12);
                uint32_t poff = (uint32_t)(src_off & 0xFFF);
                if (pg < db->n_pages && db->phys_pages[pg]) {
                    uint8_t* src = (uint8_t*)(db->phys_pages[pg] + 0xFFFF800000000000ULL + poff);
                    k_memcpy(backbuffer + y * fb_width, src, row_bytes);
                }
            }
            graphics_flip();
        }
    }
    return 0;
}

static int handle_create_dumb(uint64_t arg) {
    drm_mode_create_dumb cd;
    if (!copy_from_user(&cd, arg, sizeof(cd))) return -14;
    if (cd.width == 0 || cd.height == 0) return -22;
    if (next_handle > DRM_MAX_DUMB_BUFS) return -12; /* ENOMEM */

    uint32_t bpp   = cd.bpp ? cd.bpp : 32;
    uint32_t pitch = ((cd.width * bpp + 7) / 8 + 63) & ~63u;
    uint64_t size  = (uint64_t)pitch * cd.height;
    size = (size + 4095) & ~4095ULL;
    uint32_t n_pages = (uint32_t)(size >> 12);

    ams_dumb_buf* db = &dumb_bufs[next_handle - 1];
    k_memset(db, 0, sizeof(*db));

    db->phys_pages = (uint64_t*)kmalloc(n_pages * sizeof(uint64_t));
    if (!db->phys_pages) return -12;

    for (uint32_t i = 0; i < n_pages; i++) {
        void* pg = pmm_alloc_frame();
        if (!pg) { kfree(db->phys_pages); db->phys_pages = nullptr; return -12; }
        db->phys_pages[i] = (uint64_t)pg;
        k_memset((void*)((uint64_t)pg + 0xFFFF800000000000ULL), 0, 4096);
    }

    db->handle     = next_handle;
    db->width      = cd.width;
    db->height     = cd.height;
    db->pitch      = pitch;
    db->size       = size;
    db->n_pages    = n_pages;
    db->map_offset = DUMB_MMAP_BASE + ((uint64_t)next_handle << 12);

    cd.handle = next_handle;
    cd.pitch  = pitch;
    cd.size   = size;
    next_handle++;

    copy_to_user(arg, &cd, sizeof(cd));
    return 0;
}

static int handle_map_dumb(uint64_t arg) {
    drm_mode_map_dumb md;
    if (!copy_from_user(&md, arg, sizeof(md))) return -14;
    if (md.handle == 0 || md.handle > DRM_MAX_DUMB_BUFS) return -22;

    ams_dumb_buf* db = &dumb_bufs[md.handle - 1];
    if (!db->phys_pages) return -22;
    md.offset = db->map_offset;
    copy_to_user(arg, &md, sizeof(md));
    return 0;
}

static int handle_destroy_dumb(uint64_t arg) {
    drm_mode_destroy_dumb dd;
    if (!copy_from_user(&dd, arg, sizeof(dd))) return -14;
    if (dd.handle == 0 || dd.handle > DRM_MAX_DUMB_BUFS) return -22;

    ams_dumb_buf* db = &dumb_bufs[dd.handle - 1];
    if (db->phys_pages) {
        kfree(db->phys_pages);
        db->phys_pages = nullptr;
    }
    k_memset(db, 0, sizeof(*db));
    return 0;
}

/* ---- Public kernel API ---- */

void drm_virt_init(void) {
    k_memset(dumb_bufs, 0, sizeof(dumb_bufs));
    next_handle = 1;
    active_fb   = 0;

    k_memset(&card0_node, 0, sizeof(card0_node));
    k_strcpy(card0_node.name, "card0");
    card0_node.type   = FS_FILE;
    card0_node.source = FS_TAR;
    /* No read/write callbacks — access is via ioctl only */
    card0_node.next = vfs_root;
    vfs_root = &card0_node;
}

uint64_t drm_virt_ioctl(int /*fd*/, uint64_t request, uint64_t arg) {
    /* Extract ioctl nr from request (bits 8–15) */
    uint8_t nr = (uint8_t)((request >> 8) & 0xFF);
    int ret = -22; /* EINVAL default */
    switch (nr) {
    case 0x00: ret = handle_version(arg);         break;
    case 0x0C: ret = handle_get_cap(arg);         break;
    case 0x0D: ret = handle_set_client_cap(arg);  break;
    case 0x11: ret = handle_auth_magic(arg);      break;
    case 0xA0: ret = handle_get_resources(arg);   break;
    case 0xA1: ret = handle_get_crtc(arg);        break;
    case 0xA2: ret = handle_set_crtc(arg);        break;
    case 0xA3: ret = 0;                           break; /* cursor — stub */
    case 0xA4: ret = 0;                           break; /* get gamma — stub */
    case 0xA5: ret = 0;                           break; /* set gamma — stub */
    case 0xA6: ret = handle_get_encoder(arg);     break;
    case 0xA7: ret = handle_get_connector(arg);   break;
    case 0xAE: ret = handle_add_fb(arg);          break;
    case 0xAF: ret = handle_rm_fb(arg);           break;
    case 0xB0: ret = handle_page_flip(arg);       break;
    case 0xB1: ret = 0;                           break; /* dirty fb — stub */
    case 0xB2: ret = handle_create_dumb(arg);     break;
    case 0xB3: ret = handle_map_dumb(arg);        break;
    case 0xB4: ret = handle_destroy_dumb(arg);    break;
    default:   ret = -25;                         break; /* ENOTTY */
    }
    return (uint64_t)(int64_t)ret;
}

uint64_t drm_virt_mmap_dumb(uint64_t map_offset, uint64_t length) {
    /* Find which dumb buffer owns this offset */
    for (int i = 0; i < DRM_MAX_DUMB_BUFS; i++) {
        ams_dumb_buf* db = &dumb_bufs[i];
        if (!db->phys_pages) continue;
        if (db->map_offset != map_offset) continue;

        /* Map the dumb buffer's physical pages into current task's address space */
        static uint64_t dumb_mmap_va = 0x600000000ULL;
        uint64_t base = dumb_mmap_va;
        uint32_t np = db->n_pages;
        if ((uint64_t)np * 4096 < length) np = (uint32_t)((length + 4095) >> 12);

        for (uint32_t j = 0; j < np; j++) {
            uint64_t phys = (j < db->n_pages) ? db->phys_pages[j] : 0;
            if (!phys) {
                void* pg = pmm_alloc_frame();
                phys = (uint64_t)pg;
                if (j < db->n_pages) db->phys_pages[j] = phys;
                k_memset((void*)(phys + 0xFFFF800000000000ULL), 0, 4096);
            }
            vmm_map_page_ex(current_task->cr3, base + (uint64_t)j * 4096, phys, 0x7);
        }
        dumb_mmap_va += ((uint64_t)np << 12);
        return base;
    }
    return (uint64_t)-1;
}

ams_dumb_buf* drm_virt_get_buf(uint32_t handle) {
    if (handle == 0 || handle > DRM_MAX_DUMB_BUFS) return nullptr;
    ams_dumb_buf* db = &dumb_bufs[handle - 1];
    return db->phys_pages ? db : nullptr;
}
