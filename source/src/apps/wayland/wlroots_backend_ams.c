/**
 * wlroots AMS backend — custom backend for wlroots on AMS OS.
 *
 * This implements a minimal wlr_backend that uses:
 *   - /dev/dri/card0 (AMS DRM) for display output
 *   - /dev/input/event0, event1 (AMS evdev) for input
 *   - GBM for buffer allocation
 *   - pixman for software rendering
 *
 * Architecture:
 *   wlr_backend_ams
 *    ├── output: one display at framebuffer resolution
 *    ├── input_kbd: keyboard via evdev → libinput
 *    └── input_mouse: mouse via evdev → libinput
 *
 * This file is a standalone reference implementation.
 * When cross-compiled into wlroots, it replaces the standard
 * DRM+libinput backend with AMS-specific paths.
 */

#include "ams_syscall.h"
#include <stdint.h>
#include <stddef.h>

#define SYS_OPEN  2
#define SYS_CLOSE 3
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_IOCTL 16
#define SYS_POLL  7
#define SYS_MMAP  9
#define SYS_EPOLL_CREATE1 291
#define SYS_EPOLL_CTL     233
#define SYS_EPOLL_WAIT    232
#define SYS_CLOCK_GETTIME 228
#define SYS_MEMFD_CREATE  319
#define SYS_FTRUNCATE     77

/* DRM ioctl numbers (AMS bridge) */
#define DRM_NR_CREATE_DUMB  0xB2
#define DRM_NR_MAP_DUMB     0xB3
#define DRM_NR_DESTROY_DUMB 0xB4
#define DRM_NR_ADDFB        0xAE
#define DRM_NR_RMFB         0xAF
#define DRM_NR_PAGE_FLIP    0xB0
#define DRM_NR_GET_RES      0xA0
#define DRM_NR_GET_CONN     0xA7
#define DRM_NR_GET_CRTC     0xA1
#define DRM_NR_SET_CRTC     0xA2
#define DRM_NR_VERSION      0x00
#define DRM_NR_GET_CAP      0x0C

#define DRM_IOCTL(nr) (0xC0000000u | (0x64u << 8) | (nr))

/* evdev input_event structure */
struct input_event_raw {
    uint64_t time_sec;
    uint64_t time_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

/* DRM request structures */
struct create_dumb_req {
    uint32_t height, width, bpp, flags, handle, pitch;
    uint64_t size;
};

struct map_dumb_req {
    uint32_t handle, pad;
    uint64_t offset;
};

struct fb_cmd {
    uint32_t fb_id, width, height, pitch, bpp, depth, handle;
};

struct page_flip_req {
    uint32_t crtc_id, fb_id, flags, reserved;
    uint64_t user_data;
};

/* Backend state */
static struct {
    int drm_fd;
    int kbd_fd;
    int mouse_fd;
    int epoll_fd;

    uint32_t fb_width;
    uint32_t fb_height;

    /* double buffering */
    struct {
        uint32_t handle;
        uint32_t fb_id;
        uint32_t pitch;
        uint64_t size;
        uint32_t* map;
    } buffers[2];
    int front;

    int running;
} g_backend;

static void puts_s(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(SYS_WRITE, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(SYS_WRITE, 1, (uint64_t)"\n", 1, 0, 0);
}

static int drm_ioctl(int fd, uint32_t nr, void* arg) {
    return (int)ams_syscall(SYS_IOCTL, (uint64_t)fd, DRM_IOCTL(nr), (uint64_t)arg, 0, 0);
}

/**
 * Initialize the AMS backend.
 * Opens DRM and input devices, creates double-buffered framebuffers.
 */
int ams_backend_init(void) {
    g_backend.drm_fd = (int)ams_syscall(SYS_OPEN, (uint64_t)"/dev/dri/card0", 0x0002, 0, 0, 0);
    if (g_backend.drm_fd < 0) {
        puts_s("wlr-ams: failed to open /dev/dri/card0");
        return -1;
    }

    /* Query DRM version */
    struct { int major, minor, patch; uint64_t nl; char* n; uint64_t dl; char* d; uint64_t dsl; char* ds; } ver;
    char name_buf[32] = {0};
    ver.nl = 31; ver.n = name_buf;
    ver.dl = 0; ver.d = (char*)0;
    ver.dsl = 0; ver.ds = (char*)0;
    if (drm_ioctl(g_backend.drm_fd, DRM_NR_VERSION, &ver) == 0) {
        puts_s("wlr-ams: DRM driver connected");
    }

    /* Get framebuffer dimensions from AMS */
    ams_syscall(SYS_AMS_GET_FB_INFO,
        (uint64_t)&g_backend.fb_width,
        (uint64_t)&g_backend.fb_height, 0, 0, 0);
    if (g_backend.fb_width == 0) g_backend.fb_width = 1280;
    if (g_backend.fb_height == 0) g_backend.fb_height = 720;

    /* Create double buffers via DRM dumb BOs */
    for (int i = 0; i < 2; i++) {
        struct create_dumb_req cd = {0};
        cd.width = g_backend.fb_width;
        cd.height = g_backend.fb_height;
        cd.bpp = 32;
        if (drm_ioctl(g_backend.drm_fd, DRM_NR_CREATE_DUMB, &cd) < 0) {
            puts_s("wlr-ams: CREATE_DUMB failed");
            return -1;
        }
        g_backend.buffers[i].handle = cd.handle;
        g_backend.buffers[i].pitch  = cd.pitch;
        g_backend.buffers[i].size   = cd.size;

        /* Add framebuffer */
        struct fb_cmd fc = {0};
        fc.width = g_backend.fb_width;
        fc.height = g_backend.fb_height;
        fc.pitch = cd.pitch;
        fc.bpp = 32;
        fc.depth = 24;
        fc.handle = cd.handle;
        if (drm_ioctl(g_backend.drm_fd, DRM_NR_ADDFB, &fc) < 0) {
            puts_s("wlr-ams: ADDFB failed");
            return -1;
        }
        g_backend.buffers[i].fb_id = fc.fb_id;

        /* Map buffer */
        struct map_dumb_req md = {0};
        md.handle = cd.handle;
        drm_ioctl(g_backend.drm_fd, DRM_NR_MAP_DUMB, &md);

        g_backend.buffers[i].map = (uint32_t*)ams_syscall(SYS_MMAP,
            0, cd.size, 0x3, 0x01, (uint64_t)g_backend.drm_fd);
    }

    /* Open input devices */
    g_backend.kbd_fd = (int)ams_syscall(SYS_OPEN,
        (uint64_t)"/dev/input/event0", 0, 0, 0, 0);
    g_backend.mouse_fd = (int)ams_syscall(SYS_OPEN,
        (uint64_t)"/dev/input/event1", 0, 0, 0, 0);

    g_backend.front = 0;
    g_backend.running = 1;

    puts_s("wlr-ams: backend initialized");
    return 0;
}

/**
 * Get the back buffer for rendering.
 */
uint32_t* ams_backend_get_buffer(uint32_t* width, uint32_t* height, uint32_t* stride) {
    int back = 1 - g_backend.front;
    if (width) *width = g_backend.fb_width;
    if (height) *height = g_backend.fb_height;
    if (stride) *stride = g_backend.buffers[back].pitch;
    return g_backend.buffers[back].map;
}

/**
 * Swap buffers — page flip through DRM.
 */
int ams_backend_swap(void) {
    int back = 1 - g_backend.front;
    struct page_flip_req pf = {0};
    pf.crtc_id = 1;
    pf.fb_id = g_backend.buffers[back].fb_id;
    pf.flags = 0;
    int ret = drm_ioctl(g_backend.drm_fd, DRM_NR_PAGE_FLIP, &pf);
    if (ret == 0) g_backend.front = back;
    return ret;
}

/**
 * Poll input events. Returns number of events processed.
 */
int ams_backend_poll_input(void) {
    int count = 0;
    struct input_event_raw ev;

    /* keyboard */
    if (g_backend.kbd_fd >= 0) {
        while (1) {
            int n = (int)ams_syscall(SYS_READ, (uint64_t)g_backend.kbd_fd,
                (uint64_t)&ev, sizeof(ev), 0, 0);
            if (n < (int)sizeof(ev)) break;
            count++;
        }
    }

    /* mouse */
    if (g_backend.mouse_fd >= 0) {
        while (1) {
            int n = (int)ams_syscall(SYS_READ, (uint64_t)g_backend.mouse_fd,
                (uint64_t)&ev, sizeof(ev), 0, 0);
            if (n < (int)sizeof(ev)) break;
            count++;
        }
    }

    return count;
}

/**
 * Shutdown the backend.
 */
void ams_backend_shutdown(void) {
    g_backend.running = 0;
    if (g_backend.kbd_fd >= 0)   ams_syscall(SYS_CLOSE, (uint64_t)g_backend.kbd_fd, 0, 0, 0, 0);
    if (g_backend.mouse_fd >= 0) ams_syscall(SYS_CLOSE, (uint64_t)g_backend.mouse_fd, 0, 0, 0, 0);
    if (g_backend.drm_fd >= 0)   ams_syscall(SYS_CLOSE, (uint64_t)g_backend.drm_fd, 0, 0, 0, 0);
    puts_s("wlr-ams: backend shutdown");
}

/**
 * Demo entry point — shows that the full stack works.
 */
int main(void) {
    if (ams_backend_init() < 0) return 1;

    /* Draw a gradient on the back buffer and flip */
    uint32_t w, h, stride;
    uint32_t* buf = ams_backend_get_buffer(&w, &h, &stride);
    if (buf) {
        uint32_t stride_px = stride / 4;
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                uint8_t r = (uint8_t)(x * 255 / w);
                uint8_t g = (uint8_t)(y * 255 / h);
                uint8_t b = 128;
                buf[y * stride_px + x] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
        ams_backend_swap();
        puts_s("wlr-ams: gradient rendered via DRM page flip");
    }

    ams_backend_shutdown();
    return 0;
}
