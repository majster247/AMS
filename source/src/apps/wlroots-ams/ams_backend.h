/* AMS wlroots backend public header.
 *
 * This backend implements the wlroots output/input/renderer interface for the
 * AMS OS.  It uses:
 *   - /dev/dri/card0   for KMS-style dumb-buffer scanout
 *   - /dev/input/event0 + event1  for keyboard + pointer (libinput stub)
 *   - pixman            for software compositing
 *   - epoll             for event dispatch
 *
 * Compile with USER_CFLAGS and link against:
 *   libdrm.a  libinput.a  pixman.a  libffi.a  libc.a
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Output (display) ---- */
typedef struct ams_output {
    int      drm_fd;
    uint32_t crtc_id;
    uint32_t connector_id;
    uint32_t mode_width;
    uint32_t mode_height;
    uint32_t refresh_hz;
    /* dumb framebuffers (double-buffered) */
    uint32_t fb_handle[2];
    uint32_t fb_id[2];
    uint32_t fb_pitch;
    uint64_t fb_size;
    void*    fb_map[2];   /* mmap'd pixel data */
    int      front;       /* index of currently displayed buffer */
} ams_output;

int  ams_output_open(ams_output* out);
void ams_output_close(ams_output* out);
int  ams_output_begin_frame(ams_output* out);
int  ams_output_commit_frame(ams_output* out);
void* ams_output_get_pixbuf(ams_output* out); /* pointer to back buffer pixels */

/* ---- Input ---- */
typedef struct ams_input {
    int kbd_fd;   /* /dev/input/event0 */
    int ptr_fd;   /* /dev/input/event1 */
    int epoll_fd;
} ams_input;

typedef struct ams_key_event {
    uint32_t keycode;  /* Linux KEY_* */
    int      pressed;  /* 1=press, 0=release */
    uint64_t time_usec;
} ams_key_event;

typedef struct ams_pointer_event {
    double   dx, dy;   /* relative motion */
    uint32_t button;   /* BTN_* code or 0 */
    int      btn_pressed; /* 1=press, 0=release, -1=not a button event */
    uint64_t time_usec;
} ams_pointer_event;

typedef void (*ams_key_cb)(const ams_key_event* ev, void* userdata);
typedef void (*ams_ptr_cb)(const ams_pointer_event* ev, void* userdata);

int  ams_input_open(ams_input* inp);
void ams_input_close(ams_input* inp);
int  ams_input_dispatch(ams_input* inp, ams_key_cb on_key, ams_ptr_cb on_ptr, void* userdata);

/* ---- Compositor loop ---- */
typedef struct ams_compositor {
    ams_output output;
    ams_input  input;
    int        running;
} ams_compositor;

int  ams_compositor_init(ams_compositor* comp);
void ams_compositor_run(ams_compositor* comp,
                        /* called each frame before commit */
                        void (*render_cb)(ams_compositor* comp, void* pixels,
                                         uint32_t width, uint32_t height,
                                         uint32_t stride, void* userdata),
                        ams_key_cb on_key, ams_ptr_cb on_ptr,
                        void* userdata);
void ams_compositor_stop(ams_compositor* comp);
void ams_compositor_destroy(ams_compositor* comp);

#ifdef __cplusplus
}
#endif
