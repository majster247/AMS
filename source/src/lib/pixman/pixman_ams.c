/*
 * AMS pixman shim - software-only subset.
 *
 * The shim deliberately mirrors pixman-1 ABI surface used by wlroots'
 * pixman renderer and by cairo image surfaces. It is not bit-exact with
 * upstream pixman; format conversions are limited to ARGB32/XRGB32 and
 * fall back to memcpy when src and dst share the same format.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <pixman/pixman.h>

struct pixman_image {
    pixman_format_code_t fmt;
    int  width, height, stride;
    uint32_t *bits;
    int  owns_bits;
    int  refcount;
};

static inline int format_bpp(pixman_format_code_t fmt) {
    return (int)((((uint32_t)fmt) >> 24) & 0xFFu);
}

pixman_image_t *pixman_image_create_bits(pixman_format_code_t fmt,
                                         int w, int h, uint32_t *bits, int rowstride) {
    pixman_image_t *img = (pixman_image_t*)calloc(1, sizeof(*img));
    if (!img) return NULL;
    img->fmt = fmt;
    img->width = w;
    img->height = h;
    img->refcount = 1;

    int bpp = format_bpp(fmt);
    if (bpp <= 0) bpp = 32;
    int min_stride = (w * bpp + 7) / 8;

    if (bits) {
        img->bits = bits;
        img->stride = (rowstride > 0) ? rowstride : min_stride;
        img->owns_bits = 0;
    } else {
        img->stride = (rowstride > 0) ? rowstride : min_stride;
        img->bits = (uint32_t*)calloc(1, (size_t)img->stride * (size_t)h);
        if (!img->bits) { free(img); return NULL; }
        img->owns_bits = 1;
    }
    return img;
}

int pixman_image_ref(pixman_image_t *img) {
    if (!img) return 0;
    img->refcount++;
    return img->refcount;
}

void pixman_image_unref(pixman_image_t *img) {
    if (!img) return;
    if (--img->refcount > 0) return;
    if (img->owns_bits && img->bits) free(img->bits);
    free(img);
}

uint32_t *pixman_image_get_data(pixman_image_t *img)   { return img ? img->bits : NULL; }
int       pixman_image_get_width(pixman_image_t *img)  { return img ? img->width : 0; }
int       pixman_image_get_height(pixman_image_t *img) { return img ? img->height : 0; }
int       pixman_image_get_stride(pixman_image_t *img) { return img ? img->stride : 0; }
pixman_format_code_t pixman_image_get_format(pixman_image_t *img) { return img ? img->fmt : (pixman_format_code_t)0; }

static inline uint32_t blend_over(uint32_t s, uint32_t d) {
    uint32_t a = (s >> 24) & 0xFFu;
    if (a == 0) return d;
    if (a == 0xFFu) return s;
    uint32_t inv = 255u - a;
    uint32_t sr = (s >> 16) & 0xFFu;
    uint32_t sg = (s >> 8) & 0xFFu;
    uint32_t sb = (s)      & 0xFFu;
    uint32_t dr = (d >> 16) & 0xFFu;
    uint32_t dg = (d >> 8) & 0xFFu;
    uint32_t db = (d)      & 0xFFu;
    uint32_t r = sr + ((dr * inv + 127u) / 255u);
    uint32_t g = sg + ((dg * inv + 127u) / 255u);
    uint32_t b = sb + ((db * inv + 127u) / 255u);
    uint32_t da = a + ((((d >> 24) & 0xFFu) * inv + 127u) / 255u);
    return (da << 24) | (r << 16) | (g << 8) | b;
}

void pixman_image_composite32(pixman_op_t op, pixman_image_t *src, pixman_image_t *mask,
                              pixman_image_t *dst, int32_t sx, int32_t sy,
                              int32_t mx, int32_t my, int32_t dx, int32_t dy,
                              int32_t w, int32_t h) {
    (void)mask; (void)mx; (void)my;
    if (!src || !dst || w <= 0 || h <= 0) return;
    int dst_bpp = format_bpp(dst->fmt) / 8;
    int src_bpp = format_bpp(src->fmt) / 8;
    if (dst_bpp != 4 || src_bpp != 4) return; /* ARGB32-only fast path */

    for (int32_t row = 0; row < h; ++row) {
        int32_t syy = sy + row, dyy = dy + row;
        if (syy < 0 || syy >= src->height || dyy < 0 || dyy >= dst->height) continue;
        uint32_t *srow = (uint32_t*)((uint8_t*)src->bits + (size_t)syy * src->stride);
        uint32_t *drow = (uint32_t*)((uint8_t*)dst->bits + (size_t)dyy * dst->stride);
        for (int32_t col = 0; col < w; ++col) {
            int32_t sxx = sx + col, dxx = dx + col;
            if (sxx < 0 || sxx >= src->width || dxx < 0 || dxx >= dst->width) continue;
            uint32_t s = srow[sxx];
            switch (op) {
                case PIXMAN_OP_CLEAR: drow[dxx] = 0; break;
                case PIXMAN_OP_SRC:   drow[dxx] = s; break;
                case PIXMAN_OP_OVER:  drow[dxx] = blend_over(s, drow[dxx]); break;
                default: drow[dxx] = s; break;
            }
        }
    }
}

void pixman_image_fill_rectangles(pixman_op_t op, pixman_image_t *dst, uint32_t color,
                                  int n_rects, const pixman_box32_t *rects) {
    if (!dst || !rects || n_rects <= 0) return;
    int dst_bpp = format_bpp(dst->fmt) / 8;
    if (dst_bpp != 4) return;
    for (int i = 0; i < n_rects; ++i) {
        int32_t x1 = rects[i].x1, y1 = rects[i].y1;
        int32_t x2 = rects[i].x2, y2 = rects[i].y2;
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > dst->width)  x2 = dst->width;
        if (y2 > dst->height) y2 = dst->height;
        for (int32_t y = y1; y < y2; ++y) {
            uint32_t *row = (uint32_t*)((uint8_t*)dst->bits + (size_t)y * dst->stride);
            for (int32_t x = x1; x < x2; ++x) {
                if (op == PIXMAN_OP_OVER)
                    row[x] = blend_over(color, row[x]);
                else
                    row[x] = color;
            }
        }
    }
}

/* --- Regions --------------------------------------------------------- */

static pixman_box32_t *region_alloc(int n) {
    return (pixman_box32_t*)calloc((size_t)(n > 0 ? n : 1), sizeof(pixman_box32_t));
}

void pixman_region32_init(pixman_region32_t *r) {
    if (!r) return;
    r->extents.x1 = r->extents.y1 = r->extents.x2 = r->extents.y2 = 0;
    r->n_rects = 0;
    r->rects = NULL;
}

void pixman_region32_init_rect(pixman_region32_t *r, int32_t x, int32_t y,
                               uint32_t w, uint32_t h) {
    if (!r) return;
    r->n_rects = 1;
    r->rects = region_alloc(1);
    r->rects[0].x1 = x;
    r->rects[0].y1 = y;
    r->rects[0].x2 = x + (int32_t)w;
    r->rects[0].y2 = y + (int32_t)h;
    r->extents = r->rects[0];
}

void pixman_region32_fini(pixman_region32_t *r) {
    if (!r) return;
    if (r->rects) free(r->rects);
    r->rects = NULL;
    r->n_rects = 0;
}

static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }

int pixman_region32_union_rect(pixman_region32_t *dst, pixman_region32_t *src,
                               int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (!dst) return 0;
    int n = (src ? src->n_rects : 0) + 1;
    pixman_box32_t *out = region_alloc(n);
    int idx = 0;
    if (src) {
        for (int i = 0; i < src->n_rects; ++i) out[idx++] = src->rects[i];
    }
    out[idx].x1 = x;
    out[idx].y1 = y;
    out[idx].x2 = x + (int32_t)w;
    out[idx].y2 = y + (int32_t)h;
    idx++;

    if (dst == src) free(dst->rects);
    dst->rects = out;
    dst->n_rects = idx;
    if (idx > 0) {
        dst->extents = out[0];
        for (int i = 1; i < idx; ++i) {
            dst->extents.x1 = imin(dst->extents.x1, out[i].x1);
            dst->extents.y1 = imin(dst->extents.y1, out[i].y1);
            dst->extents.x2 = imax(dst->extents.x2, out[i].x2);
            dst->extents.y2 = imax(dst->extents.y2, out[i].y2);
        }
    }
    return 1;
}

int pixman_region32_subtract(pixman_region32_t *dst, pixman_region32_t *m, pixman_region32_t *s) {
    (void)s;
    if (!dst || !m) return 0;
    pixman_region32_fini(dst);
    dst->n_rects = m->n_rects;
    if (m->n_rects > 0) {
        dst->rects = region_alloc(m->n_rects);
        memcpy(dst->rects, m->rects, sizeof(pixman_box32_t) * m->n_rects);
        dst->extents = m->extents;
    }
    return 1;
}

const pixman_box32_t *pixman_region32_extents(pixman_region32_t *r) {
    return r ? &r->extents : NULL;
}
int pixman_region32_n_rects(pixman_region32_t *r) {
    return r ? r->n_rects : 0;
}
const pixman_box32_t *pixman_region32_rectangles(pixman_region32_t *r, int *n) {
    if (n) *n = r ? r->n_rects : 0;
    return r ? r->rects : NULL;
}
