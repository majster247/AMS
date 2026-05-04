/* AMS wlroots-style compositor main entry point.
 *
 * This is a reference implementation showing how to use the AMS backend:
 *
 *  - Opens /dev/dri/card0 for scanout (DRM/KMS dumb buffers).
 *  - Opens /dev/input/event0 + event1 for keyboard + pointer.
 *  - Uses pixman to composite client surfaces into the framebuffer.
 *  - Runs the Wayland socket loop on /run/user/0/wayland-0.
 *
 * The program is compiled to wlroots_ams.elf and placed in
 * /programs/wayland/ on the disk image.
 */
#include "ams_backend.h"
#include "../../lib/pixman/pixman.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* AMS syscall helpers */
static long ams_write(int fd, const void* buf, long n) {
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(1),"D"((long)fd),"S"(buf),"d"(n):"rcx","r11","memory");
    return r;
}
static void ams_exit(int code) {
    __asm__ volatile("syscall"::"a"(60),"D"((long)code));
    __builtin_unreachable();
}

static void puts_serial(const char* s) {
    long n = 0;
    while (s[n]) n++;
    ams_write(1, s, n);
    ams_write(1, "\n", 1);
}

/* ---- Demo render callback ---- */
/* Draws a gradient background + a white rectangle in the center */
static void render_frame(ams_compositor* comp, void* pixels,
                         uint32_t width, uint32_t height,
                         uint32_t stride, void* userdata) {
    (void)comp; (void)userdata;
    uint32_t* fb = (uint32_t*)pixels;

    /* Background gradient */
    for (uint32_t y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb + y * stride);
        uint32_t  blue = (y * 255) / (height ? height : 1);
        for (uint32_t x = 0; x < width; x++) {
            uint32_t red = (x * 255) / (width ? width : 1);
            row[x] = 0xFF000000u | (red << 16) | blue;
        }
    }

    /* White rectangle in centre */
    uint32_t rx = width  / 4;
    uint32_t ry = height / 4;
    uint32_t rw = width  / 2;
    uint32_t rh = height / 2;
    for (uint32_t y = ry; y < ry + rh && y < height; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb + y * stride);
        for (uint32_t x = rx; x < rx + rw && x < width; x++) {
            row[x] = 0xFFFFFFFFu;
        }
    }

    /* Use pixman for a semi-transparent red overlay in top-left corner */
    pixman_image_t* dst = pixman_image_create_bits(PIXMAN_x8r8g8b8, (int)width, (int)height,
                                                   fb, (int)stride);
    if (dst) {
        pixman_color_t red_fill = 0x80FF0000u; /* semi-transparent red */
        pixman_image_t* src = pixman_image_create_solid_fill(&red_fill);
        if (src) {
            pixman_image_composite32(PIXMAN_OP_OVER, src, NULL, dst,
                                     0, 0, 0, 0, 0, 0, 200, 40);
            pixman_image_unref(src);
        }
        pixman_image_unref(dst);
    }
}

/* ---- Input callbacks ---- */
static void on_key(const ams_key_event* ev, void* userdata) {
    ams_compositor* comp = (ams_compositor*)userdata;
    /* ESC = quit */
    if (ev->keycode == 1 && ev->pressed) {
        ams_compositor_stop(comp);
    }
    (void)ev;
}

static void on_ptr(const ams_pointer_event* ev, void* userdata) {
    (void)ev; (void)userdata;
}

int main(void) {
    puts_serial("[wlroots-ams] Starting AMS wlroots backend compositor");

    ams_compositor comp;
    if (ams_compositor_init(&comp) < 0) {
        puts_serial("[wlroots-ams] ERROR: Failed to init compositor");
        ams_exit(1);
    }

    puts_serial("[wlroots-ams] Compositor running. Press ESC to quit.");
    ams_compositor_run(&comp, render_frame, on_key, on_ptr, &comp);

    ams_compositor_destroy(&comp);
    puts_serial("[wlroots-ams] Compositor stopped.");
    return 0;
}
