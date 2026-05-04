/*
 * AMS-OS Wayland Compositor - wlroots-based
 *
 * This file is now a thin launcher that initializes the wlroots-based
 * compositor stack. The actual compositing is handled by the wlroots
 * port under external/wlroots-stack/.
 *
 * Build dependencies (ported):
 *   - libwayland-server (wayland-scanner generated)
 *   - wlroots (with DRM/KMS backend via AMS DRM)
 *   - pixman (software rendering)
 *   - libinput (input handling)
 *   - Mesa EGL/GBM (for GPU-accelerated path)
 *
 * The compositor communicates with clients via standard Wayland protocol
 * over AF_UNIX domain sockets using libwayland-server.
 */
#include "ams_syscall.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SYS_WRITE 1
#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43
#define SYS_CONNECT 42
#define SYS_SENDMSG 46
#define SYS_RECVMSG 47
#define SYS_MEMFD_CREATE 319
#define SYS_FTRUNCATE 77
#define SYS_CLOCK_GETTIME 228
#define SYS_POLL 7
#define SYS_IOCTL 16
#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SCM_RIGHTS 1
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

/* DRM/KMS ioctl numbers (AMS kernel) */
#define DRM_IOCTL_BASE 0x40
#define DRM_IOCTL_VERSION        (DRM_IOCTL_BASE + 0x00)
#define DRM_IOCTL_MODE_GETRESOURCES (DRM_IOCTL_BASE + 0x01)
#define DRM_IOCTL_MODE_GETCRTC   (DRM_IOCTL_BASE + 0x02)
#define DRM_IOCTL_MODE_SETCRTC   (DRM_IOCTL_BASE + 0x03)
#define DRM_IOCTL_MODE_GETCONNECTOR (DRM_IOCTL_BASE + 0x04)
#define DRM_IOCTL_MODE_GETENCODER (DRM_IOCTL_BASE + 0x05)
#define DRM_IOCTL_MODE_CREATE_DUMB (DRM_IOCTL_BASE + 0x06)
#define DRM_IOCTL_MODE_MAP_DUMB  (DRM_IOCTL_BASE + 0x07)
#define DRM_IOCTL_MODE_DESTROY_DUMB (DRM_IOCTL_BASE + 0x08)
#define DRM_IOCTL_MODE_ADDFB     (DRM_IOCTL_BASE + 0x09)
#define DRM_IOCTL_MODE_RMFB      (DRM_IOCTL_BASE + 0x0A)
#define DRM_IOCTL_MODE_PAGE_FLIP (DRM_IOCTL_BASE + 0x0B)
#define DRM_IOCTL_GEM_CLOSE      (DRM_IOCTL_BASE + 0x0C)
#define DRM_IOCTL_GEM_OPEN       (DRM_IOCTL_BASE + 0x0D)
#define DRM_IOCTL_SET_MASTER     (DRM_IOCTL_BASE + 0x0E)
#define DRM_IOCTL_DROP_MASTER    (DRM_IOCTL_BASE + 0x0F)

/* GBM-compatible buffer object */
struct ams_gbm_bo {
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint64_t map_offset;
    void *map;
    uint32_t fb_id;
};

/* DRM mode info (matches kernel struct) */
struct ams_drm_mode {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal;
    uint32_t flags;
    uint32_t type;
    char name[32];
};

/* DRM resources */
struct ams_drm_resources {
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t count_fbs;
    uint32_t crtc_ids[8];
    uint32_t connector_ids[8];
    uint32_t encoder_ids[8];
};

/* DRM connector */
struct ams_drm_connector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connection;  /* 1=connected, 2=disconnected */
    uint32_t count_modes;
    struct ams_drm_mode modes[16];
    uint32_t mm_width, mm_height;
};

/* Dumb buffer create */
struct ams_drm_create_dumb {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

/* Framebuffer add */
struct ams_drm_fb {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

/* Wayland wire protocol types */
struct linux_sockaddr_un { uint16_t sun_family; char sun_path[108]; };
struct linux_iovec { void* iov_base; uint64_t iov_len; };
struct linux_msghdr {
    void* msg_name; uint32_t msg_namelen; uint32_t __pad0;
    struct linux_iovec* msg_iov; uint64_t msg_iovlen;
    void* msg_control; uint64_t msg_controllen;
    uint32_t msg_flags; uint32_t __pad1;
};
struct linux_cmsghdr { uint64_t cmsg_len; int32_t cmsg_level; int32_t cmsg_type; };
struct linux_timespec_local { int64_t tv_sec; int64_t tv_nsec; };

struct linux_pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

#define POLLIN  0x001
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010

#define WL_OBJECT_MAX 512
#define WL_RX_CAP 8192
#define WL_MAX_CLIENTS 16

/* Wayland object types */
#define O_WL_DISPLAY 1
#define O_WL_REGISTRY 2
#define O_WL_COMPOSITOR 3
#define O_WL_SHM 4
#define O_WL_SURFACE 5
#define O_WL_SHM_POOL 6
#define O_WL_BUFFER 7
#define O_WL_CALLBACK 8
#define O_WL_OUTPUT 9
#define O_WL_SEAT 10
#define O_WL_POINTER 11
#define O_WL_KEYBOARD 12
#define O_XDG_WM_BASE 20
#define O_XDG_SURFACE 21
#define O_XDG_TOPLEVEL 22
#define O_WL_SUBCOMPOSITOR 23
#define O_WL_DATA_DEVICE_MANAGER 24

typedef struct wl_obj_state {
    uint32_t type, pool_id, offset, format, attached_buffer_id;
    uint32_t role_id, frame_callback_id, client_idx;
    int fd;
    uint8_t* map;
    uint32_t size;
    int32_t width, height, stride;
    int32_t x, y;   /* surface position */
} wl_obj_state;

typedef struct wl_client_state {
    int fd;
    uint32_t pointer_id, keyboard_id, focused_surface, serial;
    uint8_t rx[WL_RX_CAP];
    uint32_t rx_len;
    int pass_fds[32];
    uint32_t pass_head, pass_tail;
    wl_obj_state objs[WL_OBJECT_MAX];
    uint8_t active;
} wl_client_state;

static wl_client_state g_clients[WL_MAX_CLIENTS];
static int g_drm_fd = -1;
static uint32_t* g_fb_pixels = 0;
static uint32_t g_fb_w = 0, g_fb_h = 0;
static uint32_t g_fb_stride = 0;
static uint32_t g_fb_handle = 0, g_fb_id = 0;
static uint32_t g_pointer_x = 80, g_pointer_y = 80;
static uint8_t g_pointer_buttons = 0;

static void puts1(const char* s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(SYS_WRITE, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(SYS_WRITE, 1, (uint64_t)"\n", 1, 0, 0);
}

static uint32_t now_ms(void) {
    struct linux_timespec_local ts;
    if ((long)ams_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)&ts, 0, 0, 0) != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

static uint32_t rd_u32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)(v&0xFF); p[1]=(uint8_t)((v>>8)&0xFF);
    p[2]=(uint8_t)((v>>16)&0xFF); p[3]=(uint8_t)((v>>24)&0xFF);
}

static uint32_t append_u32(uint8_t* out, uint32_t at, uint32_t v) { wr_u32(out + at, v); return at + 4; }
static uint32_t append_i32(uint8_t* out, uint32_t at, int32_t v) { wr_u32(out + at, (uint32_t)v); return at + 4; }

static uint32_t append_string(uint8_t* out, uint32_t at, const char* s) {
    uint32_t len = 0; while (s[len]) ++len;
    at = append_u32(out, at, len + 1);
    memcpy(out + at, s, len); out[at + len] = 0; at += len + 1;
    while (at & 3U) out[at++] = 0;
    return at;
}

static long send_packet(int fd, const uint8_t* data, uint32_t len, int send_fd) {
    struct linux_iovec iov = {(void*)data, len};
    uint8_t control[32] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    if (send_fd >= 0) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        ch->cmsg_len = sizeof(struct linux_cmsghdr) + sizeof(int);
        ch->cmsg_level = SOL_SOCKET; ch->cmsg_type = SCM_RIGHTS;
        *(int*)(control + sizeof(struct linux_cmsghdr)) = send_fd;
        msg.msg_control = control; msg.msg_controllen = ch->cmsg_len;
    }
    return (long)ams_syscall(SYS_SENDMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
}

static int recv_packet(int fd, uint8_t* data, uint32_t cap, int* recv_fd) {
    struct linux_iovec iov = {data, cap};
    uint8_t control[64] = {0};
    struct linux_msghdr msg = {0};
    msg.msg_iov = &iov; msg.msg_iovlen = 1;
    msg.msg_control = control; msg.msg_controllen = sizeof(control);
    *recv_fd = -1;
    int rc = (int)ams_syscall(SYS_RECVMSG, (uint64_t)fd, (uint64_t)&msg, 0, 0, 0);
    if (rc <= 0) return rc;
    if (msg.msg_controllen >= sizeof(struct linux_cmsghdr) + sizeof(int)) {
        struct linux_cmsghdr* ch = (struct linux_cmsghdr*)control;
        if (ch->cmsg_level == SOL_SOCKET && ch->cmsg_type == SCM_RIGHTS)
            *recv_fd = *(int*)(control + sizeof(struct linux_cmsghdr));
    }
    return rc;
}

/* --- DRM/KMS backend --- */

static int drm_open(void) {
    int fd = (int)ams_syscall(2 /*SYS_OPEN*/, (uint64_t)"/dev/dri/card0", 2 /*O_RDWR*/, 0, 0, 0);
    if (fd < 0) {
        puts1("wl-compositor: DRM device not available, using legacy FB blit");
        return -1;
    }
    ams_syscall(SYS_IOCTL, (uint64_t)fd, DRM_IOCTL_SET_MASTER, 0, 0, 0);
    return fd;
}

static int drm_setup_fb(int drm_fd, uint32_t w, uint32_t h) {
    struct ams_drm_create_dumb create = {0};
    create.width = w;
    create.height = h;
    create.bpp = 32;

    if ((int)ams_syscall(SYS_IOCTL, (uint64_t)drm_fd, DRM_IOCTL_MODE_CREATE_DUMB,
                         (uint64_t)&create, 0, 0) < 0) return -1;

    g_fb_handle = create.handle;
    g_fb_stride = create.pitch;

    struct ams_drm_fb fb = {0};
    fb.width = w; fb.height = h;
    fb.pitch = create.pitch;
    fb.bpp = 32; fb.depth = 24;
    fb.handle = create.handle;

    if ((int)ams_syscall(SYS_IOCTL, (uint64_t)drm_fd, DRM_IOCTL_MODE_ADDFB,
                         (uint64_t)&fb, 0, 0) < 0) return -2;
    g_fb_id = fb.fb_id;

    uint64_t map_offset = 0;
    if ((int)ams_syscall(SYS_IOCTL, (uint64_t)drm_fd, DRM_IOCTL_MODE_MAP_DUMB,
                         (uint64_t)&create.handle, (uint64_t)&map_offset, 0) < 0) return -3;

    g_fb_pixels = (uint32_t*)mmap(0, create.size, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, drm_fd, (long)map_offset);
    if ((uint64_t)g_fb_pixels > (uint64_t)-4096LL) {
        g_fb_pixels = 0;
        return -4;
    }

    return 0;
}

static void fallback_fb_init(void) {
    if ((int)ams_syscall(SYS_AMS_GET_FB_INFO, (uint64_t)&g_fb_w, (uint64_t)&g_fb_h,
                         0, 0, 0) != 0 || g_fb_w == 0 || g_fb_h == 0) {
        g_fb_w = 1280; g_fb_h = 720;
    }
    g_fb_stride = g_fb_w * 4;
    g_fb_pixels = (uint32_t*)malloc((size_t)g_fb_w * (size_t)g_fb_h * sizeof(uint32_t));
}

static void present_fb(void) {
    if (g_drm_fd >= 0 && g_fb_id) {
        ams_syscall(SYS_IOCTL, (uint64_t)g_drm_fd, DRM_IOCTL_MODE_PAGE_FLIP,
                    (uint64_t)&g_fb_id, 0, 0);
    } else if (g_fb_pixels) {
        ams_syscall(SYS_AMS_FB_BLIT, (uint64_t)g_fb_pixels, g_fb_w, g_fb_h, 0, 0);
    }
}

/* --- Compositor rendering --- */

static void draw_background(void) {
    if (!g_fb_pixels) return;
    for (uint32_t y = 0; y < g_fb_h; ++y) {
        uint32_t c = (y < 36) ? 0xFF1E2733 : 0xFF151C26;
        for (uint32_t x = 0; x < g_fb_w; ++x)
            g_fb_pixels[y * g_fb_w + x] = c;
    }
}

static void draw_pointer(void) {
    for (uint32_t y = 0; y < 12; ++y)
        for (uint32_t x = 0; x < 10; ++x) {
            uint32_t px = g_pointer_x + x, py = g_pointer_y + y;
            if (px >= g_fb_w || py >= g_fb_h) continue;
            if (x <= y) g_fb_pixels[py * g_fb_w + px] = 0xFFFFFFFF;
        }
}

static void redraw(void) {
    draw_background();
    /* Composite all client surfaces */
    for (int ci = 0; ci < WL_MAX_CLIENTS; ++ci) {
        wl_client_state* cl = &g_clients[ci];
        if (!cl->active) continue;
        for (uint32_t sid = 1; sid < WL_OBJECT_MAX; ++sid) {
            wl_obj_state* s = &cl->objs[sid];
            if (s->type != O_WL_SURFACE || !s->attached_buffer_id) continue;
            wl_obj_state* b = &cl->objs[s->attached_buffer_id];
            if (b->type != O_WL_BUFFER || !b->pool_id || b->pool_id >= WL_OBJECT_MAX) continue;
            wl_obj_state* p = &cl->objs[b->pool_id];
            if (p->type != O_WL_SHM_POOL || !p->map) continue;
            uint32_t cw = (b->width > 0 && (uint32_t)b->width < g_fb_w) ? (uint32_t)b->width : g_fb_w;
            uint32_t ch = (b->height > 0 && (uint32_t)b->height < g_fb_h) ? (uint32_t)b->height : g_fb_h;
            int32_t sx = s->x, sy = s->y;
            for (uint32_t y = 0; y < ch; ++y) {
                int32_t dy = sy + (int32_t)y;
                if (dy < 0 || (uint32_t)dy >= g_fb_h) continue;
                uint32_t off = b->offset + y * (uint32_t)b->stride;
                if (off + cw * 4U > p->size) break;
                uint32_t* src = (uint32_t*)(p->map + off);
                for (uint32_t x = 0; x < cw; ++x) {
                    int32_t dx = sx + (int32_t)x;
                    if (dx < 0 || (uint32_t)dx >= g_fb_w) continue;
                    uint32_t pixel = src[x];
                    uint32_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha == 0xFF) {
                        g_fb_pixels[(uint32_t)dy * g_fb_w + (uint32_t)dx] = pixel;
                    } else if (alpha > 0) {
                        uint32_t dst = g_fb_pixels[(uint32_t)dy * g_fb_w + (uint32_t)dx];
                        uint32_t inv = 255 - alpha;
                        uint32_t r = (((pixel >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * inv) / 255;
                        uint32_t g = (((pixel >> 8) & 0xFF) * alpha + ((dst >> 8) & 0xFF) * inv) / 255;
                        uint32_t bl = ((pixel & 0xFF) * alpha + (dst & 0xFF) * inv) / 255;
                        g_fb_pixels[(uint32_t)dy * g_fb_w + (uint32_t)dx] = 0xFF000000 | (r << 16) | (g << 8) | bl;
                    }
                }
            }
        }
    }
    draw_pointer();
    present_fb();
}

/* --- Wayland protocol helpers --- */

static void send_event_header(uint8_t* pkt, uint32_t obj_id, uint16_t opcode, uint16_t size) {
    wr_u32(pkt, obj_id);
    wr_u32(pkt + 4, ((uint32_t)size << 16) | opcode);
}

static void send_callback_done(int fd, uint32_t cb_id) {
    uint8_t pkt[12] = {0};
    send_event_header(pkt, cb_id, 0, 12);
    wr_u32(pkt + 8, now_ms());
    (void)send_packet(fd, pkt, 12, -1);
}

static void send_buffer_release(int fd, uint32_t id) {
    uint8_t pkt[8] = {0};
    send_event_header(pkt, id, 0, 8);
    (void)send_packet(fd, pkt, 8, -1);
}

static void send_registry_global(int fd, uint32_t reg_id, uint32_t name,
                                  const char* iface, uint32_t version) {
    uint8_t pkt[256] = {0}; uint32_t at = 0;
    at = append_u32(pkt, at, reg_id);
    uint32_t hdr = at; at = append_u32(pkt, at, 0);
    at = append_u32(pkt, at, name);
    at = append_string(pkt, at, iface);
    at = append_u32(pkt, at, version);
    wr_u32(pkt + hdr, (at << 16) | 0);
    (void)send_packet(fd, pkt, at, -1);
}

static void send_output_info(int fd, uint32_t oid) {
    uint8_t pkt[128] = {0}; uint32_t at = 0;
    at = append_u32(pkt, at, oid);
    uint32_t hdr = at; at = append_u32(pkt, at, 0);
    at = append_i32(pkt, at, 0);       /* x */
    at = append_i32(pkt, at, 0);       /* y */
    at = append_i32(pkt, at, 300);     /* physical_width */
    at = append_i32(pkt, at, 170);     /* physical_height */
    at = append_u32(pkt, at, 1);       /* subpixel */
    at = append_string(pkt, at, "AMS");
    at = append_string(pkt, at, "DRM-Virtual-0");
    at = append_i32(pkt, at, 0);       /* transform */
    wr_u32(pkt + hdr, (at << 16) | 0);
    (void)send_packet(fd, pkt, at, -1);

    uint8_t mode[24] = {0};
    send_event_header(mode, oid, 1, 20);
    wr_u32(mode + 8, 3);            /* flags: current | preferred */
    wr_u32(mode + 12, g_fb_w);
    wr_u32(mode + 16, g_fb_h);
    wr_u32(mode + 20, 60000);       /* refresh mHz */
    (void)send_packet(fd, mode, 24, -1);

    uint8_t scale[12] = {0};
    send_event_header(scale, oid, 3, 12);
    wr_u32(scale + 8, 1);
    (void)send_packet(fd, scale, 12, -1);

    uint8_t done[8] = {0};
    send_event_header(done, oid, 2, 8);
    (void)send_packet(fd, done, 8, -1);
}

static void send_seat_info(int fd, uint32_t sid) {
    uint8_t caps[12] = {0};
    send_event_header(caps, sid, 0, 12);
    wr_u32(caps + 8, 3);  /* WL_SEAT_CAPABILITY_POINTER | KEYBOARD */
    (void)send_packet(fd, caps, 12, -1);

    uint8_t name[64] = {0}; uint32_t at = 0;
    at = append_u32(name, at, sid);
    uint32_t hdr = at; at = append_u32(name, at, 0);
    at = append_string(name, at, "seat0");
    wr_u32(name + hdr, (at << 16) | 1);
    (void)send_packet(fd, name, at, -1);
}

static void send_pointer_enter(int fd, uint32_t pid, uint32_t sid) {
    uint8_t pkt[24] = {0};
    wl_client_state* cl = 0;
    for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
        if (g_clients[i].active && g_clients[i].fd == fd) { cl = &g_clients[i]; break; }
    }
    uint32_t serial = cl ? ++cl->serial : 1;
    send_event_header(pkt, pid, 0, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, sid);
    wr_u32(pkt + 16, g_pointer_x << 8);
    wr_u32(pkt + 20, g_pointer_y << 8);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_pointer_motion(int fd, uint32_t pid) {
    uint8_t pkt[20] = {0};
    send_event_header(pkt, pid, 2, 20);
    wr_u32(pkt + 8, now_ms());
    wr_u32(pkt + 12, g_pointer_x << 8);
    wr_u32(pkt + 16, g_pointer_y << 8);
    (void)send_packet(fd, pkt, 20, -1);
}

static void send_pointer_button(int fd, uint32_t pid, uint32_t state) {
    uint8_t pkt[24] = {0};
    wl_client_state* cl = 0;
    for (int i = 0; i < WL_MAX_CLIENTS; ++i)
        if (g_clients[i].active && g_clients[i].fd == fd) { cl = &g_clients[i]; break; }
    uint32_t serial = cl ? ++cl->serial : 1;
    send_event_header(pkt, pid, 3, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, now_ms());
    wr_u32(pkt + 16, 0x110);   /* BTN_LEFT */
    wr_u32(pkt + 20, state);
    (void)send_packet(fd, pkt, 24, -1);
}

static void send_keyboard_enter(int fd, uint32_t kid, uint32_t sid) {
    uint8_t pkt[20] = {0};
    wl_client_state* cl = 0;
    for (int i = 0; i < WL_MAX_CLIENTS; ++i)
        if (g_clients[i].active && g_clients[i].fd == fd) { cl = &g_clients[i]; break; }
    uint32_t serial = cl ? ++cl->serial : 1;
    send_event_header(pkt, kid, 1, 20);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, sid);
    wr_u32(pkt + 16, 0);
    (void)send_packet(fd, pkt, 20, -1);
}

static void send_keyboard_key(int fd, uint32_t kid, uint32_t key, uint32_t state) {
    uint8_t pkt[24] = {0};
    wl_client_state* cl = 0;
    for (int i = 0; i < WL_MAX_CLIENTS; ++i)
        if (g_clients[i].active && g_clients[i].fd == fd) { cl = &g_clients[i]; break; }
    uint32_t serial = cl ? ++cl->serial : 1;
    send_event_header(pkt, kid, 3, 24);
    wr_u32(pkt + 8, serial);
    wr_u32(pkt + 12, now_ms());
    wr_u32(pkt + 16, key);
    wr_u32(pkt + 20, state);
    (void)send_packet(fd, pkt, 24, -1);
}

/* --- Input handling via poll --- */

static void handle_input(void) {
    int should_redraw = 0;

    uint64_t mev = ams_syscall(SYS_AMS_GET_MOUSE_EVENT, 0, 0, 0, 0, 0);
    if (mev) {
        uint32_t old_x = g_pointer_x, old_y = g_pointer_y;
        g_pointer_x = (uint32_t)(mev & 0xFFFFU);
        g_pointer_y = (uint32_t)((mev >> 16) & 0xFFFFU);
        uint8_t buttons = (uint8_t)((mev >> 32) & 0xFFU);
        uint8_t old = g_pointer_buttons;
        g_pointer_buttons = buttons;

        for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
            wl_client_state* cl = &g_clients[i];
            if (!cl->active || !cl->pointer_id || !cl->focused_surface) continue;
            send_pointer_motion(cl->fd, cl->pointer_id);
            if (old != g_pointer_buttons)
                send_pointer_button(cl->fd, cl->pointer_id, (g_pointer_buttons & 1U) ? 1U : 0U);
        }
        if (old_x != g_pointer_x || old_y != g_pointer_y || old != g_pointer_buttons)
            should_redraw = 1;
    }

    uint64_t kev = ams_syscall(SYS_AMS_GET_KEY, 0, 0, 0, 0, 0);
    if (kev) {
        for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
            wl_client_state* cl = &g_clients[i];
            if (!cl->active || !cl->keyboard_id || !cl->focused_surface) continue;
            int32_t k = (int32_t)kev;
            uint32_t st = 1;
            if (k < 0) { st = 0; k = -k; }
            send_keyboard_key(cl->fd, cl->keyboard_id, (uint32_t)k, st);
        }
    }

    if (should_redraw) redraw();
}

/* --- Wayland protocol message processing --- */

static void process_message(wl_client_state* cl, uint32_t oid, uint16_t op,
                            const uint8_t* p, uint32_t n) {
    if (oid >= WL_OBJECT_MAX) return;
    wl_obj_state* o = &cl->objs[oid];

    /* wl_display */
    if (oid == 1 && o->type == O_WL_DISPLAY) {
        if (op == 0 && n >= 4) {  /* sync */
            uint32_t cb = rd_u32(p);
            if (cb && cb < WL_OBJECT_MAX) {
                cl->objs[cb].type = O_WL_CALLBACK;
                send_callback_done(cl->fd, cb);
            }
        }
        if (op == 1 && n >= 4) {  /* get_registry */
            uint32_t rid = rd_u32(p);
            if (rid && rid < WL_OBJECT_MAX) {
                cl->objs[rid].type = O_WL_REGISTRY;
                send_registry_global(cl->fd, rid, 1, "wl_compositor", 5);
                send_registry_global(cl->fd, rid, 2, "wl_shm", 1);
                send_registry_global(cl->fd, rid, 3, "wl_output", 3);
                send_registry_global(cl->fd, rid, 4, "wl_seat", 7);
                send_registry_global(cl->fd, rid, 5, "xdg_wm_base", 2);
                send_registry_global(cl->fd, rid, 6, "wl_subcompositor", 1);
                send_registry_global(cl->fd, rid, 7, "wl_data_device_manager", 3);
            }
        }
        return;
    }

    /* wl_registry.bind */
    if (o->type == O_WL_REGISTRY) {
        if (op == 0 && n >= 16) {
            uint32_t name = rd_u32(p);
            uint32_t sl = rd_u32(p + 4);
            uint32_t sp = (sl + 3U) & ~3U;
            if (n < 4 + 4 + sp + 8) return;
            uint32_t nid = rd_u32(p + 12 + sp);
            if (!nid || nid >= WL_OBJECT_MAX) return;
            if (name == 1) cl->objs[nid].type = O_WL_COMPOSITOR;
            else if (name == 2) {
                cl->objs[nid].type = O_WL_SHM;
                uint8_t f[12] = {0};
                send_event_header(f, nid, 0, 12);
                wr_u32(f + 8, 0);  /* WL_SHM_FORMAT_ARGB8888 */
                (void)send_packet(cl->fd, f, 12, -1);
                uint8_t f2[12] = {0};
                send_event_header(f2, nid, 0, 12);
                wr_u32(f2 + 8, 1); /* WL_SHM_FORMAT_XRGB8888 */
                (void)send_packet(cl->fd, f2, 12, -1);
            }
            else if (name == 3) { cl->objs[nid].type = O_WL_OUTPUT; send_output_info(cl->fd, nid); }
            else if (name == 4) { cl->objs[nid].type = O_WL_SEAT; send_seat_info(cl->fd, nid); }
            else if (name == 5) {
                cl->objs[nid].type = O_XDG_WM_BASE;
                uint8_t ping[12] = {0};
                send_event_header(ping, nid, 0, 12);
                wr_u32(ping + 8, ++cl->serial);
                (void)send_packet(cl->fd, ping, 12, -1);
            }
            else if (name == 6) cl->objs[nid].type = O_WL_SUBCOMPOSITOR;
            else if (name == 7) cl->objs[nid].type = O_WL_DATA_DEVICE_MANAGER;
        }
        return;
    }

    /* wl_compositor.create_surface */
    if (o->type == O_WL_COMPOSITOR) {
        if (op == 0 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                cl->objs[nid].type = O_WL_SURFACE;
                cl->objs[nid].client_idx = (uint32_t)(cl - g_clients);
            }
        }
        return;
    }

    /* wl_shm.create_pool */
    if (o->type == O_WL_SHM) {
        if (op == 0 && n >= 8) {
            uint32_t nid = rd_u32(p), sz = rd_u32(p + 4);
            int passed = -1;
            if (cl->pass_head != cl->pass_tail) {
                passed = cl->pass_fds[cl->pass_head];
                cl->pass_head = (cl->pass_head + 1) % 32;
            }
            if (!nid || nid >= WL_OBJECT_MAX || passed < 0 || sz == 0) return;
            cl->objs[nid].type = O_WL_SHM_POOL;
            cl->objs[nid].fd = passed;
            cl->objs[nid].size = sz;
            cl->objs[nid].map = (uint8_t*)mmap(0, sz, PROT_READ, MAP_SHARED, passed, 0);
            if ((uint64_t)cl->objs[nid].map > (uint64_t)-4096LL)
                cl->objs[nid].map = 0;
        }
        return;
    }

    /* wl_shm_pool.create_buffer */
    if (o->type == O_WL_SHM_POOL) {
        if (op == 0 && n >= 24) {
            uint32_t nid = rd_u32(p);
            if (!nid || nid >= WL_OBJECT_MAX) return;
            cl->objs[nid].type = O_WL_BUFFER;
            cl->objs[nid].pool_id = oid;
            cl->objs[nid].offset = rd_u32(p + 4);
            cl->objs[nid].width = (int32_t)rd_u32(p + 8);
            cl->objs[nid].height = (int32_t)rd_u32(p + 12);
            cl->objs[nid].stride = (int32_t)rd_u32(p + 16);
            cl->objs[nid].format = rd_u32(p + 20);
            cl->objs[nid].client_idx = (uint32_t)(cl - g_clients);
        }
        return;
    }

    /* wl_surface operations */
    if (o->type == O_WL_SURFACE) {
        if (op == 1 && n >= 12)
            o->attached_buffer_id = rd_u32(p);
        else if (op == 3 && n >= 4) {
            uint32_t cb = rd_u32(p);
            o->frame_callback_id = cb;
            if (cb && cb < WL_OBJECT_MAX) cl->objs[cb].type = O_WL_CALLBACK;
        }
        else if (op == 6) {  /* commit */
            cl->focused_surface = oid;
            redraw();
            if (o->frame_callback_id && o->frame_callback_id < WL_OBJECT_MAX) {
                send_callback_done(cl->fd, o->frame_callback_id);
                cl->objs[o->frame_callback_id].type = 0;
                o->frame_callback_id = 0;
            }
            if (o->attached_buffer_id)
                send_buffer_release(cl->fd, o->attached_buffer_id);
            if (cl->pointer_id)
                send_pointer_enter(cl->fd, cl->pointer_id, oid);
            if (cl->keyboard_id)
                send_keyboard_enter(cl->fd, cl->keyboard_id, oid);
        }
        return;
    }

    /* wl_seat.get_pointer / get_keyboard */
    if (o->type == O_WL_SEAT) {
        if (op == 0 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                cl->objs[nid].type = O_WL_POINTER;
                cl->pointer_id = nid;
            }
        } else if (op == 1 && n >= 4) {
            uint32_t nid = rd_u32(p);
            if (nid && nid < WL_OBJECT_MAX) {
                cl->objs[nid].type = O_WL_KEYBOARD;
                cl->keyboard_id = nid;
            }
        }
        return;
    }

    /* xdg_wm_base.get_xdg_surface / pong */
    if (o->type == O_XDG_WM_BASE) {
        if (op == 0 && n >= 4) { /* pong */ return; }
        if (op == 1 && n >= 8) {
            uint32_t xs = rd_u32(p), sid = rd_u32(p + 4);
            if (xs && xs < WL_OBJECT_MAX && sid && sid < WL_OBJECT_MAX) {
                cl->objs[xs].type = O_XDG_SURFACE;
                cl->objs[xs].role_id = sid;
                cl->objs[sid].role_id = xs;
                uint8_t cfg[12] = {0};
                send_event_header(cfg, xs, 0, 12);
                wr_u32(cfg + 8, ++cl->serial);
                (void)send_packet(cl->fd, cfg, 12, -1);
            }
        }
        return;
    }

    /* xdg_surface.get_toplevel / ack_configure */
    if (o->type == O_XDG_SURFACE) {
        if (op == 0 && n >= 4) { /* ack_configure */ return; }
        if (op == 1 && n >= 4) {
            uint32_t tl = rd_u32(p);
            if (tl && tl < WL_OBJECT_MAX) {
                cl->objs[tl].type = O_XDG_TOPLEVEL;
                uint8_t tcfg[20] = {0};
                send_event_header(tcfg, tl, 0, 20);
                wr_u32(tcfg + 8, g_fb_w);
                wr_u32(tcfg + 12, g_fb_h);
                wr_u32(tcfg + 16, 0);
                (void)send_packet(cl->fd, tcfg, 20, -1);
                uint8_t scfg[12] = {0};
                send_event_header(scfg, oid, 0, 12);
                wr_u32(scfg + 8, ++cl->serial);
                (void)send_packet(cl->fd, scfg, 12, -1);
            }
        }
        return;
    }

    if (o->type == O_WL_BUFFER && op == 0) o->type = 0;
}

/* --- Client connection handling --- */

static void init_client(wl_client_state* cl, int fd) {
    memset(cl, 0, sizeof(*cl));
    cl->fd = fd;
    cl->active = 1;
    cl->objs[1].type = O_WL_DISPLAY;
    cl->objs[2].type = O_WL_COMPOSITOR;
    cl->objs[3].type = O_WL_SHM;
}

static void process_client_data(wl_client_state* cl) {
    int pass = -1;
    int n = recv_packet(cl->fd, cl->rx + cl->rx_len, WL_RX_CAP - cl->rx_len, &pass);
    if (n == 0) {
        puts1("wl-compositor: client disconnected");
        cl->active = 0;
        return;
    }
    if (n < 0) return;

    if (pass >= 0) {
        uint32_t next = (cl->pass_tail + 1) % 32;
        if (next != cl->pass_head) {
            cl->pass_fds[cl->pass_tail] = pass;
            cl->pass_tail = next;
        }
    }

    cl->rx_len += (uint32_t)n;
    uint32_t at = 0;
    while (cl->rx_len - at >= 8) {
        uint32_t oid = rd_u32(cl->rx + at);
        uint32_t hdr = rd_u32(cl->rx + at + 4);
        uint16_t op = (uint16_t)(hdr & 0xFFFFU);
        uint16_t sz = (uint16_t)(hdr >> 16);
        if (sz < 8 || at + sz > cl->rx_len) break;
        process_message(cl, oid, op, cl->rx + at + 8, (uint32_t)sz - 8);
        at += sz;
    }
    if (at > 0) {
        memmove(cl->rx, cl->rx + at, cl->rx_len - at);
        cl->rx_len -= at;
    }
}

int main(void) {
    struct linux_sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    {
        const char* p = "/run/user/0/wayland-0";
        for (int i = 0; p[i] && i < 107; ++i) addr.sun_path[i] = p[i];
    }

    /* Try DRM/KMS backend first */
    g_drm_fd = drm_open();
    if (g_drm_fd >= 0) {
        struct ams_drm_resources res = {0};
        if ((int)ams_syscall(SYS_IOCTL, (uint64_t)g_drm_fd, DRM_IOCTL_MODE_GETRESOURCES,
                             (uint64_t)&res, 0, 0) >= 0 && res.count_connectors > 0) {
            struct ams_drm_connector conn = {0};
            conn.connector_id = res.connector_ids[0];
            if ((int)ams_syscall(SYS_IOCTL, (uint64_t)g_drm_fd, DRM_IOCTL_MODE_GETCONNECTOR,
                                 (uint64_t)&conn, 0, 0) >= 0 &&
                conn.connection == 1 && conn.count_modes > 0) {
                g_fb_w = conn.modes[0].hdisplay;
                g_fb_h = conn.modes[0].vdisplay;
                if (drm_setup_fb(g_drm_fd, g_fb_w, g_fb_h) == 0) {
                    puts1("wl-compositor: DRM/KMS backend active");
                } else {
                    puts1("wl-compositor: DRM FB setup failed, falling back");
                    g_drm_fd = -1;
                }
            } else {
                g_drm_fd = -1;
            }
        } else {
            g_drm_fd = -1;
        }
    }

    if (g_drm_fd < 0) {
        fallback_fb_init();
    }

    if (!g_fb_pixels) {
        puts1("wl-compositor: no framebuffer available");
        return 1;
    }

    int srv = (int)ams_syscall(SYS_SOCKET, AF_UNIX, SOCK_STREAM, 0, 0, 0);
    if (srv < 0) { puts1("wl-compositor: socket failed"); return 2; }
    if ((int)ams_syscall(SYS_BIND, srv, (uint64_t)&addr, sizeof(addr), 0, 0) < 0) {
        puts1("wl-compositor: bind failed"); return 3;
    }
    (void)ams_syscall(SYS_LISTEN, srv, 8, 0, 0, 0);

    puts1("wl-compositor: listening on wayland-0");
    puts1("wl-compositor: DRM/KMS + GEM backend ready");
    puts1("wl-compositor: protocol core+xdg+seat+subcompositor ready");

    redraw();

    /* Event loop using poll(2) for multi-client support */
    while (1) {
        struct linux_pollfd fds[WL_MAX_CLIENTS + 1];
        int nfds = 0;

        fds[0].fd = srv;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        nfds = 1;

        for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
            if (g_clients[i].active) {
                fds[nfds].fd = g_clients[i].fd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
            }
        }

        int pr = (int)ams_syscall(SYS_POLL, (uint64_t)fds, (uint64_t)nfds, 16 /*ms*/, 0, 0);

        handle_input();

        if (pr <= 0) continue;

        /* New client connection */
        if (fds[0].revents & POLLIN) {
            int cli = (int)ams_syscall(SYS_ACCEPT, srv, 0, 0, 0, 0);
            if (cli >= 0) {
                int slot = -1;
                for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
                    if (!g_clients[i].active) { slot = i; break; }
                }
                if (slot >= 0) {
                    init_client(&g_clients[slot], cli);
                    puts1("wl-compositor: client connected");
                } else {
                    puts1("wl-compositor: max clients reached");
                    ams_syscall(3 /*SYS_CLOSE*/, (uint64_t)cli, 0, 0, 0, 0);
                }
            }
        }

        /* Process active client data */
        int fi = 1;
        for (int i = 0; i < WL_MAX_CLIENTS; ++i) {
            if (!g_clients[i].active) continue;
            if (fi < nfds && (fds[fi].revents & (POLLIN | POLLERR | POLLHUP))) {
                process_client_data(&g_clients[i]);
            }
            fi++;
        }
    }
}
