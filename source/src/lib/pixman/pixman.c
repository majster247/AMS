/* Minimal pixman implementation for AMS — scalar C only.
 * Supports PIXMAN_a8r8g8b8 / x8r8g8b8 / a8b8g8r8 and ops CLEAR / SRC / OVER / DST.
 */
#include "pixman.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- image structure ---- */
struct pixman_image {
    pixman_format_code_t format;
    int width, height;
    int stride;          /* bytes per row */
    uint32_t* data;      /* pixel data pointer */
    uint32_t  solid;     /* for solid-fill images */
    int       is_solid;
    int       refs;
    pixman_region32_t clip;
    int       has_clip;
};

/* ---- region implementation ---- */

static pixman_box32_t box32_union(pixman_box32_t a, pixman_box32_t b) {
    pixman_box32_t r;
    r.x1 = a.x1 < b.x1 ? a.x1 : b.x1;
    r.y1 = a.y1 < b.y1 ? a.y1 : b.y1;
    r.x2 = a.x2 > b.x2 ? a.x2 : b.x2;
    r.y2 = a.y2 > b.y2 ? a.y2 : b.y2;
    return r;
}

static pixman_box32_t box32_intersect(pixman_box32_t a, pixman_box32_t b) {
    pixman_box32_t r;
    r.x1 = a.x1 > b.x1 ? a.x1 : b.x1;
    r.y1 = a.y1 > b.y1 ? a.y1 : b.y1;
    r.x2 = a.x2 < b.x2 ? a.x2 : b.x2;
    r.y2 = a.y2 < b.y2 ? a.y2 : b.y2;
    if (r.x1 >= r.x2 || r.y1 >= r.y2) { r.x1=r.y1=r.x2=r.y2=0; }
    return r;
}

void pixman_region32_init(pixman_region32_t* r) {
    r->n_rects = 0;
    r->extents.x1 = r->extents.y1 = r->extents.x2 = r->extents.y2 = 0;
    r->rects = NULL;
}

void pixman_region32_init_rect(pixman_region32_t* r, int x, int y, unsigned w, unsigned h) {
    r->extents.x1 = x; r->extents.y1 = y;
    r->extents.x2 = x + (int)w; r->extents.y2 = y + (int)h;
    r->n_rects = 1;
    r->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (r->rects) r->rects[0] = r->extents;
}

void pixman_region32_fini(pixman_region32_t* r) {
    if (r->rects) { free(r->rects); r->rects = NULL; }
    r->n_rects = 0;
}

void pixman_region32_clear(pixman_region32_t* r) {
    pixman_region32_fini(r);
    pixman_region32_init(r);
}

int pixman_region32_not_empty(pixman_region32_t* r) {
    return r->n_rects > 0;
}

void pixman_region32_union(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b) {
    if (dst->rects) free(dst->rects);
    if (!pixman_region32_not_empty((pixman_region32_t*)a)) {
        dst->extents = b->extents; dst->n_rects = b->n_rects;
        dst->rects = b->n_rects ? (pixman_box32_t*)malloc(b->n_rects * sizeof(pixman_box32_t)) : NULL;
        if (dst->rects) memcpy(dst->rects, b->rects, b->n_rects * sizeof(pixman_box32_t));
        return;
    }
    if (!pixman_region32_not_empty((pixman_region32_t*)b)) {
        dst->extents = a->extents; dst->n_rects = a->n_rects;
        dst->rects = a->n_rects ? (pixman_box32_t*)malloc(a->n_rects * sizeof(pixman_box32_t)) : NULL;
        if (dst->rects) memcpy(dst->rects, a->rects, a->n_rects * sizeof(pixman_box32_t));
        return;
    }
    /* Simple approximation: bounding box union */
    dst->extents = box32_union(a->extents, b->extents);
    dst->n_rects = 1;
    dst->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (dst->rects) dst->rects[0] = dst->extents;
}

void pixman_region32_intersect(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b) {
    if (dst->rects) free(dst->rects);
    pixman_box32_t r = box32_intersect(a->extents, b->extents);
    if (r.x1 >= r.x2 || r.y1 >= r.y2) {
        dst->n_rects = 0; dst->extents = r; dst->rects = NULL; return;
    }
    dst->extents = r; dst->n_rects = 1;
    dst->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (dst->rects) dst->rects[0] = r;
}

void pixman_region32_subtract(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b) {
    /* Conservative: return a's extents (approximation) */
    if (dst->rects) free(dst->rects);
    dst->extents = a->extents; dst->n_rects = a->n_rects;
    dst->rects = a->n_rects ? (pixman_box32_t*)malloc(a->n_rects * sizeof(pixman_box32_t)) : NULL;
    if (dst->rects) memcpy(dst->rects, a->rects, a->n_rects * sizeof(pixman_box32_t));
    (void)b;
}

void pixman_region32_union_rect(pixman_region32_t* dst, const pixman_region32_t* src,
                                int x, int y, unsigned w, unsigned h) {
    pixman_region32_t tmp;
    pixman_region32_init_rect(&tmp, x, y, w, h);
    pixman_region32_union(dst, src, &tmp);
    pixman_region32_fini(&tmp);
}

pixman_box32_t* pixman_region32_rectangles(pixman_region32_t* region, int* n_rects) {
    if (n_rects) *n_rects = region->n_rects;
    return region->rects;
}

pixman_box32_t pixman_region32_extents(pixman_region32_t* region) {
    return region->extents;
}

int pixman_region32_contains_point(pixman_region32_t* region, int x, int y, pixman_box32_t* box) {
    if (region->n_rects == 0) return 0;
    if (x < region->extents.x1 || x >= region->extents.x2 ||
        y < region->extents.y1 || y >= region->extents.y2) return 0;
    if (box) *box = region->extents;
    return 1;
}

void pixman_region32_copy(pixman_region32_t* dst, const pixman_region32_t* src) {
    if (dst->rects) free(dst->rects);
    dst->extents = src->extents;
    dst->n_rects = src->n_rects;
    dst->rects = src->n_rects ? (pixman_box32_t*)malloc(src->n_rects * sizeof(pixman_box32_t)) : NULL;
    if (dst->rects) memcpy(dst->rects, src->rects, src->n_rects * sizeof(pixman_box32_t));
}

void pixman_region32_translate(pixman_region32_t* region, int dx, int dy) {
    region->extents.x1 += dx; region->extents.x2 += dx;
    region->extents.y1 += dy; region->extents.y2 += dy;
    for (int i = 0; i < region->n_rects; i++) {
        region->rects[i].x1 += dx; region->rects[i].x2 += dx;
        region->rects[i].y1 += dy; region->rects[i].y2 += dy;
    }
}

/* ---- image ---- */

pixman_image_t* pixman_image_create_bits(pixman_format_code_t format,
                                         int width, int height,
                                         uint32_t* bits, int rowstride_bytes) {
    pixman_image_t* img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(*img));
    img->format = format;
    img->width  = width;
    img->height = height;
    img->stride = rowstride_bytes ? rowstride_bytes : width * 4;
    img->refs   = 1;
    if (bits) {
        img->data = bits;
    } else {
        img->data = (uint32_t*)malloc((size_t)img->stride * (size_t)height);
        if (img->data) memset(img->data, 0, (size_t)img->stride * (size_t)height);
    }
    pixman_region32_init(&img->clip);
    return img;
}

pixman_image_t* pixman_image_create_bits_no_clear(pixman_format_code_t format,
                                                  int width, int height,
                                                  uint32_t* bits, int rowstride_bytes) {
    return pixman_image_create_bits(format, width, height, bits, rowstride_bytes);
}

pixman_image_t* pixman_image_create_solid_fill(const pixman_color_t* color) {
    pixman_image_t* img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(*img));
    img->format    = PIXMAN_a8r8g8b8;
    img->is_solid  = 1;
    img->solid     = color ? *color : 0;
    img->refs      = 1;
    pixman_region32_init(&img->clip);
    return img;
}

pixman_image_t* pixman_image_ref(pixman_image_t* image) {
    if (image) image->refs++;
    return image;
}

int pixman_image_unref(pixman_image_t* image) {
    if (!image) return 0;
    if (--image->refs <= 0) {
        pixman_region32_fini(&image->clip);
        /* Don't free external data buffers (caller owns them) */
        free(image);
        return 1;
    }
    return 0;
}

int  pixman_image_get_width(pixman_image_t* i)  { return i ? i->width  : 0; }
int  pixman_image_get_height(pixman_image_t* i) { return i ? i->height : 0; }
int  pixman_image_get_stride(pixman_image_t* i) { return i ? i->stride : 0; }
pixman_format_code_t pixman_image_get_format(pixman_image_t* i) { return i ? i->format : 0; }
uint32_t* pixman_image_get_data(pixman_image_t* i) { return i ? i->data : NULL; }

void pixman_image_set_clip_region32(pixman_image_t* image, pixman_region32_t* region) {
    if (!image) return;
    pixman_region32_fini(&image->clip);
    if (region) {
        pixman_region32_copy(&image->clip, region);
        image->has_clip = 1;
    } else {
        pixman_region32_init(&image->clip);
        image->has_clip = 0;
    }
}

/* ---- pixel read helpers ---- */

static inline uint32_t read_pixel(const pixman_image_t* img, int x, int y) {
    if (img->is_solid) return img->solid;
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return 0;
    const uint8_t* row = (const uint8_t*)img->data + (size_t)y * (size_t)img->stride;
    return *(const uint32_t*)(row + x * 4);
}

/* Convert any supported format pixel to ARGB32 */
static inline uint32_t to_argb32(uint32_t px, pixman_format_code_t fmt) {
    switch (fmt) {
    case PIXMAN_a8r8g8b8: return px;
    case PIXMAN_x8r8g8b8: return px | 0xFF000000u;
    case PIXMAN_a8b8g8r8: /* ABGR → ARGB */
        return (px & 0xFF00FF00u) |
               ((px & 0x00FF0000u) >> 16) |
               ((px & 0x000000FFu) << 16);
    case PIXMAN_x8b8g8r8:
        return ((px & 0x00FF0000u) >> 16) |
               (px & 0x0000FF00u) |
               ((px & 0x000000FFu) << 16) | 0xFF000000u;
    default: return px;
    }
}

static inline uint32_t from_argb32(uint32_t px, pixman_format_code_t fmt) {
    switch (fmt) {
    case PIXMAN_a8r8g8b8: return px;
    case PIXMAN_x8r8g8b8: return px & 0x00FFFFFFu;
    case PIXMAN_a8b8g8r8:
        return (px & 0xFF00FF00u) |
               ((px & 0x00FF0000u) >> 16) |
               ((px & 0x000000FFu) << 16);
    case PIXMAN_x8b8g8r8:
        return ((px & 0x00FF0000u) >> 16) |
               (px & 0x0000FF00u) |
               ((px & 0x000000FFu) << 16);
    default: return px;
    }
}

/* Alpha-blend: OVER operator  dst = src + dst*(1-alpha)/255 */
static inline uint32_t blend_over(uint32_t src_argb, uint32_t dst_argb) {
    uint32_t sa = src_argb >> 24;
    if (sa == 255) return src_argb;
    if (sa == 0)   return dst_argb;
    uint32_t inv = 255 - sa;
    uint32_t r = ((src_argb >> 16 & 0xFF) * sa + (dst_argb >> 16 & 0xFF) * inv) / 255;
    uint32_t g = ((src_argb >>  8 & 0xFF) * sa + (dst_argb >>  8 & 0xFF) * inv) / 255;
    uint32_t b = ((src_argb       & 0xFF) * sa + (dst_argb       & 0xFF) * inv) / 255;
    uint32_t a = sa + (dst_argb >> 24) * inv / 255;
    if (a > 255) a = 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* ---- compositing ---- */

void pixman_image_composite32(pixman_op_t op,
                              pixman_image_t* src,
                              pixman_image_t* mask,
                              pixman_image_t* dst,
                              int16_t src_x, int16_t src_y,
                              int16_t mask_x, int16_t mask_y,
                              int16_t dst_x, int16_t dst_y,
                              uint16_t width, uint16_t height) {
    (void)mask; (void)mask_x; (void)mask_y;
    if (!src || !dst || width == 0 || height == 0) return;
    if (dst->is_solid || !dst->data) return;

    for (int16_t y = 0; y < (int16_t)height; y++) {
        int dy = dst_y + y;
        int sy = src_y + y;
        if (dy < 0 || dy >= dst->height) continue;

        uint32_t* dst_row = (uint32_t*)((uint8_t*)dst->data + (size_t)dy * (size_t)dst->stride);

        for (int16_t x = 0; x < (int16_t)width; x++) {
            int dx = dst_x + x;
            int sx = src_x + x;
            if (dx < 0 || dx >= dst->width) continue;

            uint32_t src_px = to_argb32(read_pixel(src, sx, sy), src->format);
            uint32_t dst_px = to_argb32(dst_row[dx], dst->format);
            uint32_t result;

            switch (op) {
            case PIXMAN_OP_CLEAR: result = 0;                          break;
            case PIXMAN_OP_SRC:   result = src_px;                     break;
            case PIXMAN_OP_DST:   result = dst_px;                     break;
            case PIXMAN_OP_OVER:  result = blend_over(src_px, dst_px); break;
            default:              result = src_px;                     break;
            }
            dst_row[dx] = from_argb32(result, dst->format);
        }
    }
}
