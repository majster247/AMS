#include "ams_syscall.h"
#include <pixman/pixman.h>
#include <stdint.h>

static void puts1(const char *s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, 64, 64, 0, 0);
    pixman_image_t *dst = pixman_image_create_bits(PIXMAN_a8r8g8b8, 64, 64, 0, 0);
    if (!src || !dst) { puts1("pixman_smoke: alloc fail"); return 1; }
    pixman_box32_t box = { 0, 0, 64, 64 };
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, src, 0xFF223344u, 1, &box);
    pixman_image_composite32(PIXMAN_OP_SRC, src, 0, dst, 0, 0, 0, 0, 0, 0, 64, 64);
    uint32_t *bits = pixman_image_get_data(dst);
    if (!bits || bits[0] != 0xFF223344u) {
        puts1("pixman_smoke: pixel mismatch");
        return 2;
    }
    pixman_image_unref(src);
    pixman_image_unref(dst);
    puts1("pixman_smoke: PASS");
    return 0;
}
