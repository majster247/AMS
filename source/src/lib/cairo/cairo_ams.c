/*
 * AMS Cairo shim - sits on top of the AMS pixman shim.
 *
 * The intent is API-parity with cairo 1.18 for what wlroots/GTK-style
 * apps actually call. Bitmap font is the same 8x16 used by AMS GUI.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pixman/pixman.h>
#include <cairo/cairo.h>

extern unsigned char ams_font_8x16[];

struct cairo_surface {
    pixman_image_t *img;
    cairo_format_t  fmt;
    int             owns_img;
};

struct cairo {
    cairo_surface_t *target;
    cairo_status_t   status;
    double           r, g, b, a;
    double           cx, cy;
    /* current rectangle (single-rect path collapses to this) */
    double           rx, ry, rw, rh;
    int              has_rect;
};

static pixman_format_code_t map_format(cairo_format_t f) {
    switch (f) {
        case CAIRO_FORMAT_ARGB32:  return PIXMAN_a8r8g8b8;
        case CAIRO_FORMAT_RGB24:   return PIXMAN_x8r8g8b8;
        case CAIRO_FORMAT_A8:      return PIXMAN_a8;
        case CAIRO_FORMAT_RGB16_565: return PIXMAN_r5g6b5;
        default:                   return PIXMAN_a8r8g8b8;
    }
}

cairo_surface_t *cairo_image_surface_create(cairo_format_t fmt, int w, int h) {
    cairo_surface_t *s = (cairo_surface_t*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->fmt = fmt;
    s->img = pixman_image_create_bits(map_format(fmt), w, h, NULL, 0);
    s->owns_img = 1;
    if (!s->img) { free(s); return NULL; }
    return s;
}

cairo_surface_t *cairo_image_surface_create_for_data(unsigned char *data,
                                                     cairo_format_t fmt,
                                                     int w, int h, int stride) {
    cairo_surface_t *s = (cairo_surface_t*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->fmt = fmt;
    s->img = pixman_image_create_bits(map_format(fmt), w, h, (uint32_t*)data, stride);
    s->owns_img = 1;
    if (!s->img) { free(s); return NULL; }
    return s;
}

void cairo_surface_destroy(cairo_surface_t *s) {
    if (!s) return;
    if (s->owns_img && s->img) pixman_image_unref(s->img);
    free(s);
}

unsigned char *cairo_image_surface_get_data(cairo_surface_t *s)  { return s ? (unsigned char*)pixman_image_get_data(s->img) : NULL; }
int            cairo_image_surface_get_width(cairo_surface_t *s) { return s ? pixman_image_get_width(s->img) : 0; }
int            cairo_image_surface_get_height(cairo_surface_t *s){ return s ? pixman_image_get_height(s->img) : 0; }
int            cairo_image_surface_get_stride(cairo_surface_t *s){ return s ? pixman_image_get_stride(s->img) : 0; }
cairo_format_t cairo_image_surface_get_format(cairo_surface_t *s){ return s ? s->fmt : CAIRO_FORMAT_INVALID; }

cairo_t *cairo_create(cairo_surface_t *target) {
    cairo_t *cr = (cairo_t*)calloc(1, sizeof(*cr));
    if (!cr) return NULL;
    cr->target = target;
    cr->a = 1.0;
    cr->status = target ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_NULL_POINTER;
    return cr;
}

void cairo_destroy(cairo_t *cr) { free(cr); }

cairo_status_t cairo_status(cairo_t *cr) { return cr ? cr->status : CAIRO_STATUS_NULL_POINTER; }

void cairo_set_source_rgb(cairo_t *cr, double r, double g, double b) {
    cairo_set_source_rgba(cr, r, g, b, 1.0);
}
void cairo_set_source_rgba(cairo_t *cr, double r, double g, double b, double a) {
    if (!cr) return;
    cr->r = r; cr->g = g; cr->b = b; cr->a = a;
}

static uint32_t pack_argb(double r, double g, double b, double a) {
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    if (b < 0) b = 0;
    if (b > 1) b = 1;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    uint32_t A = (uint32_t)(a * 255 + 0.5);
    uint32_t R = (uint32_t)(r * a * 255 + 0.5);
    uint32_t G = (uint32_t)(g * a * 255 + 0.5);
    uint32_t B = (uint32_t)(b * a * 255 + 0.5);
    return (A << 24) | (R << 16) | (G << 8) | B;
}

void cairo_rectangle(cairo_t *cr, double x, double y, double w, double h) {
    if (!cr) return;
    cr->rx = x; cr->ry = y; cr->rw = w; cr->rh = h;
    cr->has_rect = 1;
}

void cairo_paint(cairo_t *cr) {
    if (!cr || !cr->target) return;
    pixman_box32_t box = {
        .x1 = 0, .y1 = 0,
        .x2 = pixman_image_get_width(cr->target->img),
        .y2 = pixman_image_get_height(cr->target->img)
    };
    pixman_image_fill_rectangles(PIXMAN_OP_OVER, cr->target->img,
                                 pack_argb(cr->r, cr->g, cr->b, cr->a),
                                 1, &box);
}

void cairo_fill(cairo_t *cr) {
    if (!cr || !cr->target || !cr->has_rect) return;
    pixman_box32_t box = {
        .x1 = (int32_t)cr->rx,
        .y1 = (int32_t)cr->ry,
        .x2 = (int32_t)(cr->rx + cr->rw),
        .y2 = (int32_t)(cr->ry + cr->rh)
    };
    pixman_image_fill_rectangles(PIXMAN_OP_OVER, cr->target->img,
                                 pack_argb(cr->r, cr->g, cr->b, cr->a),
                                 1, &box);
    cr->has_rect = 0;
}

void cairo_move_to(cairo_t *cr, double x, double y) {
    if (!cr) return;
    cr->cx = x; cr->cy = y;
}

void cairo_show_text(cairo_t *cr, const char *utf8) {
    if (!cr || !cr->target || !utf8) return;
    pixman_image_t *dst = cr->target->img;
    int dw = pixman_image_get_width(dst), dh = pixman_image_get_height(dst);
    uint32_t *bits = pixman_image_get_data(dst);
    int stride = pixman_image_get_stride(dst) / 4;
    uint32_t color = pack_argb(cr->r, cr->g, cr->b, cr->a);
    int x = (int)cr->cx, y = (int)cr->cy;

    for (const char *p = utf8; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 32 || ch >= 128) ch = '?';
        const unsigned char *glyph = ams_font_8x16 + (ch - 32) * 16;
        for (int gy = 0; gy < 16; ++gy) {
            int py = y - 16 + gy;
            if (py < 0 || py >= dh) continue;
            unsigned char row = glyph[gy];
            for (int gx = 0; gx < 8; ++gx) {
                int px = x + gx;
                if (px < 0 || px >= dw) continue;
                if (row & (0x80u >> gx)) bits[py * stride + px] = color;
            }
        }
        x += 8;
    }
    cr->cx = x;
}
