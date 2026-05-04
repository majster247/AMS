/* AMS wlroots-compatible backend implementation.
 *
 * Output path:  open /dev/dri/card0 → create dumb buffers → mmap →
 *               render into back-buffer → drmModePageFlip → visible.
 *
 * Input path:   open /dev/input/event{0,1} via epoll → parse input_event →
 *               dispatch as ams_key_event / ams_pointer_event.
 */
#include "ams_backend.h"
#include "../../lib/drm/drm.h"
#include "../../lib/drm/drm_mode.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Linux evdev input_event */
struct ev_raw {
    int64_t  tv_sec;
    int64_t  tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};
#define EV_SYN  0
#define EV_KEY  1
#define EV_REL  2
#define REL_X   0
#define REL_Y   1
#define BTN_BASE 0x110

/* ---- AMS syscall wrappers ---- */
static long ams_open(const char* path, int flags) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(2),"D"(path),"S"(flags):"rcx","r11","memory");
    return r;
}
static long ams_close(int fd) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(3),"D"((long)fd):"rcx","r11");
    return r;
}
static long ams_read(int fd, void* buf, long n) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(0),"D"((long)fd),"S"(buf),"d"(n):"rcx","r11","memory");
    return r;
}
static long ams_mmap(void* addr, long len, int prot, int flags, int fd, long off) {
    long r;
    register long r10 __asm__("r10") = (long)flags;
    register long r8  __asm__("r8")  = (long)fd;
    register long r9  __asm__("r9")  = off;
    __asm__ volatile("syscall":"=a"(r):"0"(9),"D"((long)addr),"S"(len),"d"(prot),
                     "r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory");
    return r;
}
static long ams_epoll_create1(int flags) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(291),"D"((long)flags):"rcx","r11");
    return r;
}
static long ams_epoll_ctl(int epfd, int op, int fd, void* ev) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(233),"D"((long)epfd),"S"((long)op),"d"((long)fd),
                     "r"((long)ev):"rcx","r11","memory");
    return r;
}
typedef struct { uint32_t events; uint32_t pad; uint64_t data; } epoll_ev_t;
static long ams_epoll_wait(int epfd, epoll_ev_t* evs, int maxev, int timeout) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(232),"D"((long)epfd),"S"(evs),"d"((long)maxev),
                     "r"((long)timeout):"rcx","r11","memory");
    return r;
}
#define EPOLLIN 0x001
#define EPOLL_CTL_ADD 1

/* ---- Output ---- */

int ams_output_open(ams_output* out) {
    memset(out, 0, sizeof(*out));
    out->drm_fd = drmOpen("ams", NULL);
    if (out->drm_fd < 0) return -1;

    /* Enable dumb buffer cap */
    uint64_t val = 0;
    drmGetCap(out->drm_fd, DRM_CAP_DUMB_BUFFER, &val);
    if (!val) { drmClose(out->drm_fd); return -2; }

    /* Get resources */
    drmModeResPtr res = drmModeGetResources(out->drm_fd);
    if (!res || res->count_connectors == 0 || res->count_crtcs == 0) {
        if (res) drmModeFreeResources(res);
        drmClose(out->drm_fd);
        return -3;
    }

    /* Find connected connector */
    drmModeConnectorPtr conn = NULL;
    for (int i = 0; i < res->count_connectors && !conn; i++) {
        drmModeConnectorPtr c = drmModeGetConnector(out->drm_fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
            conn = c;
            out->connector_id = c->connector_id;
        } else if (c) {
            drmModeFreeConnector(c);
        }
    }
    if (!conn) { drmModeFreeResources(res); drmClose(out->drm_fd); return -4; }

    /* Use preferred mode */
    drmModeModeInfo* mode = &conn->modes[0];
    for (int i = 0; i < conn->count_modes; i++) {
        if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) { mode = &conn->modes[i]; break; }
    }
    out->mode_width  = mode->hdisplay;
    out->mode_height = mode->vdisplay;
    out->refresh_hz  = mode->vrefresh ? mode->vrefresh : 60;

    /* Use first CRTC */
    out->crtc_id = res->crtcs[0];

    /* Create two dumb buffers */
    for (int i = 0; i < 2; i++) {
        uint32_t pitch = 0;
        uint64_t size  = 0;
        if (drmModeCreateDumbBuffer(out->drm_fd, out->mode_width, out->mode_height, 32, 0,
                                    &out->fb_handle[i], &pitch, &size) < 0) {
            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            drmClose(out->drm_fd);
            return -5;
        }
        out->fb_pitch = pitch;
        out->fb_size  = size;

        /* Add framebuffer */
        drmModeAddFB(out->drm_fd, out->mode_width, out->mode_height, 24, 32, pitch,
                     out->fb_handle[i], &out->fb_id[i]);

        /* Map into process address space */
        uint64_t map_offset = 0;
        drmModeMapDumbBuffer(out->drm_fd, out->fb_handle[i], &map_offset);
        long ptr = ams_mmap(NULL, (long)size, 3 /*PROT_READ|WRITE*/, 1 /*MAP_SHARED*/,
                            out->drm_fd, (long)map_offset);
        out->fb_map[i] = (ptr > 0) ? (void*)ptr : NULL;
    }
    out->front = 0;

    /* Set CRTC to first buffer */
    drmModeSetCrtc(out->drm_fd, out->crtc_id, out->fb_id[0], 0, 0,
                   &out->connector_id, 1, mode);

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    return 0;
}

void ams_output_close(ams_output* out) {
    for (int i = 0; i < 2; i++) {
        if (out->fb_map[i]) {
            /* munmap */
            long r;
            __asm__ volatile("syscall":"=a"(r):"0"(11),"D"(out->fb_map[i]),"S"(out->fb_size):"rcx","r11");
        }
        if (out->fb_id[i])     drmModeRmFB(out->drm_fd, out->fb_id[i]);
        if (out->fb_handle[i]) drmModeDestroyDumbBuffer(out->drm_fd, out->fb_handle[i]);
    }
    if (out->drm_fd >= 0) drmClose(out->drm_fd);
    memset(out, 0, sizeof(*out));
    out->drm_fd = -1;
}

int ams_output_begin_frame(ams_output* out) {
    (void)out;
    return 0;
}

/* Swap front/back, page flip */
int ams_output_commit_frame(ams_output* out) {
    int back = out->front ^ 1;
    drmModePageFlip(out->drm_fd, out->crtc_id, out->fb_id[back], DRM_MODE_PAGE_FLIP_EVENT, NULL);
    out->front = back;
    return 0;
}

void* ams_output_get_pixbuf(ams_output* out) {
    return out->fb_map[out->front ^ 1]; /* back buffer */
}

/* ---- Input ---- */

int ams_input_open(ams_input* inp) {
    memset(inp, 0, sizeof(*inp));
    inp->kbd_fd = (int)ams_open("/dev/input/event0", 0);
    inp->ptr_fd = (int)ams_open("/dev/input/event1", 0);
    inp->epoll_fd = (int)ams_epoll_create1(0);
    if (inp->epoll_fd < 0) return -1;

    epoll_ev_t ev = { EPOLLIN, 0, 0 };
    if (inp->kbd_fd >= 0) {
        ev.data = (uint64_t)inp->kbd_fd;
        ams_epoll_ctl(inp->epoll_fd, EPOLL_CTL_ADD, inp->kbd_fd, &ev);
    }
    if (inp->ptr_fd >= 0) {
        ev.data = (uint64_t)inp->ptr_fd;
        ams_epoll_ctl(inp->epoll_fd, EPOLL_CTL_ADD, inp->ptr_fd, &ev);
    }
    return 0;
}

void ams_input_close(ams_input* inp) {
    if (inp->epoll_fd >= 0) ams_close(inp->epoll_fd);
    if (inp->kbd_fd >= 0)   ams_close(inp->kbd_fd);
    if (inp->ptr_fd >= 0)   ams_close(inp->ptr_fd);
    memset(inp, 0, sizeof(*inp));
    inp->epoll_fd = inp->kbd_fd = inp->ptr_fd = -1;
}

int ams_input_dispatch(ams_input* inp, ams_key_cb on_key, ams_ptr_cb on_ptr, void* userdata) {
    epoll_ev_t events[4];
    long nev = ams_epoll_wait(inp->epoll_fd, events, 4, 0 /* non-blocking */);
    if (nev <= 0) return 0;

    struct ev_raw raw[32];

    for (long i = 0; i < nev; i++) {
        int fd = (int)events[i].data;
        long n = ams_read(fd, raw, sizeof(raw));
        if (n <= 0) continue;
        int count = (int)n / (int)sizeof(struct ev_raw);

        double pending_dx = 0, pending_dy = 0;
        int has_motion = 0;

        for (int j = 0; j < count; j++) {
            struct ev_raw* re = &raw[j];
            uint64_t ts = (uint64_t)re->tv_sec * 1000000ULL + (uint64_t)(uint64_t)re->tv_usec;

            if (re->type == EV_SYN) {
                if (has_motion && on_ptr) {
                    ams_pointer_event pev = { pending_dx, pending_dy, 0, -1, ts };
                    on_ptr(&pev, userdata);
                    pending_dx = pending_dy = 0;
                    has_motion = 0;
                }
            } else if (re->type == EV_REL) {
                if (re->code == REL_X) { pending_dx += (double)re->value; has_motion = 1; }
                if (re->code == REL_Y) { pending_dy += (double)re->value; has_motion = 1; }
            } else if (re->type == EV_KEY) {
                if (re->code >= BTN_BASE && re->code <= (BTN_BASE + 6)) {
                    if (on_ptr) {
                        ams_pointer_event pev = { 0, 0, re->code, re->value ? 1 : 0, ts };
                        on_ptr(&pev, userdata);
                    }
                } else {
                    if (on_key) {
                        ams_key_event kev = { re->code, re->value ? 1 : 0, ts };
                        on_key(&kev, userdata);
                    }
                }
            }
        }
    }
    return 0;
}

/* ---- Compositor loop ---- */

int ams_compositor_init(ams_compositor* comp) {
    memset(comp, 0, sizeof(*comp));
    if (ams_output_open(&comp->output) < 0) return -1;
    if (ams_input_open(&comp->input) < 0) {
        ams_output_close(&comp->output);
        return -2;
    }
    comp->running = 1;
    return 0;
}

void ams_compositor_run(ams_compositor* comp,
                        void (*render_cb)(ams_compositor* comp, void* pixels,
                                         uint32_t width, uint32_t height,
                                         uint32_t stride, void* userdata),
                        ams_key_cb on_key, ams_ptr_cb on_ptr,
                        void* userdata) {
    while (comp->running) {
        /* Gather input */
        ams_input_dispatch(&comp->input, on_key, on_ptr, userdata);

        /* Render */
        void* pixels = ams_output_get_pixbuf(&comp->output);
        if (pixels && render_cb) {
            render_cb(comp, pixels,
                      comp->output.mode_width, comp->output.mode_height,
                      comp->output.fb_pitch, userdata);
        }

        /* Flip */
        ams_output_commit_frame(&comp->output);
    }
}

void ams_compositor_stop(ams_compositor* comp) {
    comp->running = 0;
}

void ams_compositor_destroy(ams_compositor* comp) {
    ams_input_close(&comp->input);
    ams_output_close(&comp->output);
}
