#include "ams_syscall.h"
#include <cairo/cairo.h>
#include <stdint.h>

static void puts1(const char *s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
    cairo_t *cr = cairo_create(s);
    if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        puts1("cairo_smoke: create fail");
        return 1;
    }
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.5);
    cairo_paint(cr);
    uint32_t *bits = (uint32_t*)cairo_image_surface_get_data(s);
    uint32_t pixel = bits[0];
    if (((pixel >> 24) & 0xFFu) != 0xFFu) {
        puts1("cairo_smoke: alpha mismatch");
        return 2;
    }
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    puts1("cairo_smoke: PASS");
    return 0;
}
