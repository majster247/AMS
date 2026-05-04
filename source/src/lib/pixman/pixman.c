/**
 * @file pixman.c
 * @brief Minimal pixman implementation for AMS.
 *
 * Software-only pixel manipulation sufficient for wlroots compositing.
 * Region operations use single-rectangle approximation.
 * Image compositing uses memcpy (SRC) or alpha-blend (OVER) loops.
 */

#include "pixman.h"
#include <string.h>
#include <stdlib.h>

struct pixman_image {
    pixman_format_code_t format;
    int      width;
    int      height;
    int      stride;
    uint32_t *data;
    int      ref_count;
    int      owns_data;
    uint32_t solid_color;
    int      is_solid;
};

static pixman_box32_t g_empty_box = {0, 0, 0, 0};

/* ---- Region32 ---- */

void pixman_region32_init(pixman_region32_t *r) {
    r->n_rects = 0;
    r->rects = NULL;
}

void pixman_region32_init_rect(pixman_region32_t *r,
                                int x, int y,
                                unsigned int w, unsigned int h) {
    r->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (r->rects) {
        r->n_rects = 1;
        r->rects[0].x = x;
        r->rects[0].y = y;
        r->rects[0].width = (int32_t)w;
        r->rects[0].height = (int32_t)h;
    } else {
        r->n_rects = 0;
    }
}

void pixman_region32_fini(pixman_region32_t *r) {
    if (r->rects) { free(r->rects); r->rects = NULL; }
    r->n_rects = 0;
}

pixman_bool_t pixman_region32_union(pixman_region32_t *out,
                                     pixman_region32_t *a,
                                     pixman_region32_t *b) {
    if (a->n_rects == 0 && b->n_rects == 0) {
        out->n_rects = 0; out->rects = NULL;
        return 1;
    }
    pixman_box32_t *ar = a->n_rects ? a->rects : NULL;
    pixman_box32_t *br = b->n_rects ? b->rects : NULL;
    int32_t x1, y1, x2, y2;
    if (ar && br) {
        x1 = ar->x < br->x ? ar->x : br->x;
        y1 = ar->y < br->y ? ar->y : br->y;
        int32_t ax2 = ar->x + ar->width, bx2 = br->x + br->width;
        int32_t ay2 = ar->y + ar->height, by2 = br->y + br->height;
        x2 = ax2 > bx2 ? ax2 : bx2;
        y2 = ay2 > by2 ? ay2 : by2;
    } else if (ar) {
        x1 = ar->x; y1 = ar->y; x2 = ar->x + ar->width; y2 = ar->y + ar->height;
    } else {
        x1 = br->x; y1 = br->y; x2 = br->x + br->width; y2 = br->y + br->height;
    }
    if (out->rects == NULL) out->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (!out->rects) return 0;
    out->n_rects = 1;
    out->rects[0].x = x1; out->rects[0].y = y1;
    out->rects[0].width = x2 - x1; out->rects[0].height = y2 - y1;
    return 1;
}

pixman_bool_t pixman_region32_intersect(pixman_region32_t *out,
                                         pixman_region32_t *a,
                                         pixman_region32_t *b) {
    if (a->n_rects == 0 || b->n_rects == 0) {
        out->n_rects = 0; out->rects = NULL; return 1;
    }
    int32_t x1 = a->rects->x > b->rects->x ? a->rects->x : b->rects->x;
    int32_t y1 = a->rects->y > b->rects->y ? a->rects->y : b->rects->y;
    int32_t ax2 = a->rects->x + a->rects->width, bx2 = b->rects->x + b->rects->width;
    int32_t ay2 = a->rects->y + a->rects->height, by2 = b->rects->y + b->rects->height;
    int32_t x2 = ax2 < bx2 ? ax2 : bx2;
    int32_t y2 = ay2 < by2 ? ay2 : by2;
    if (x2 <= x1 || y2 <= y1) { out->n_rects = 0; out->rects = NULL; return 1; }
    if (out->rects == NULL) out->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t));
    if (!out->rects) return 0;
    out->n_rects = 1;
    out->rects[0].x = x1; out->rects[0].y = y1;
    out->rects[0].width = x2 - x1; out->rects[0].height = y2 - y1;
    return 1;
}

pixman_bool_t pixman_region32_subtract(pixman_region32_t *out,
                                        pixman_region32_t *a,
                                        pixman_region32_t *b) {
    (void)b;
    if (a->n_rects == 0) { out->n_rects = 0; out->rects = NULL; return 1; }
    return pixman_region32_copy(out, a);
}

pixman_bool_t pixman_region32_not_empty(pixman_region32_t *r) {
    return r->n_rects > 0;
}

pixman_box32_t *pixman_region32_rectangles(pixman_region32_t *r, int *n) {
    if (n) *n = r->n_rects;
    return r->rects ? r->rects : &g_empty_box;
}

pixman_box32_t *pixman_region32_extents(pixman_region32_t *r) {
    return r->n_rects ? r->rects : &g_empty_box;
}

void pixman_region32_translate(pixman_region32_t *r, int x, int y) {
    for (int i = 0; i < r->n_rects; i++) {
        r->rects[i].x += x;
        r->rects[i].y += y;
    }
}

pixman_bool_t pixman_region32_copy(pixman_region32_t *dst, pixman_region32_t *src) {
    pixman_region32_fini(dst);
    if (src->n_rects == 0) { dst->n_rects = 0; dst->rects = NULL; return 1; }
    dst->rects = (pixman_box32_t*)malloc(sizeof(pixman_box32_t) * src->n_rects);
    if (!dst->rects) return 0;
    dst->n_rects = src->n_rects;
    memcpy(dst->rects, src->rects, sizeof(pixman_box32_t) * src->n_rects);
    return 1;
}

pixman_bool_t pixman_region32_contains_point(pixman_region32_t *r, int x, int y, pixman_box32_t *box) {
    for (int i = 0; i < r->n_rects; i++) {
        if (x >= r->rects[i].x && x < r->rects[i].x + r->rects[i].width &&
            y >= r->rects[i].y && y < r->rects[i].y + r->rects[i].height) {
            if (box) *box = r->rects[i];
            return 1;
        }
    }
    return 0;
}

/* ---- Image ---- */

static int format_bpp(pixman_format_code_t f) {
    switch (f) {
        case PIXMAN_a8r8g8b8: case PIXMAN_x8r8g8b8: return 32;
        case PIXMAN_r5g6b5: return 16;
        case PIXMAN_a8: return 8;
        case PIXMAN_r8g8b8: return 24;
        default: return 32;
    }
}

pixman_image_t *pixman_image_create_bits(pixman_format_code_t format,
                                          int w, int h,
                                          uint32_t *bits, int stride) {
    pixman_image_t *img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(*img));
    img->format = format;
    img->width = w;
    img->height = h;
    img->stride = stride ? stride : ((w * format_bpp(format) + 31) / 32) * 4;
    img->ref_count = 1;
    if (bits) {
        img->data = bits;
        img->owns_data = 0;
    } else {
        size_t sz = (size_t)img->stride * h;
        img->data = (uint32_t*)malloc(sz);
        if (img->data) memset(img->data, 0, sz);
        img->owns_data = 1;
    }
    return img;
}

pixman_image_t *pixman_image_create_bits_no_clear(pixman_format_code_t format,
                                                    int w, int h,
                                                    uint32_t *bits, int stride) {
    pixman_image_t *img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(*img));
    img->format = format;
    img->width = w;
    img->height = h;
    img->stride = stride ? stride : ((w * format_bpp(format) + 31) / 32) * 4;
    img->ref_count = 1;
    if (bits) {
        img->data = bits;
        img->owns_data = 0;
    } else {
        img->data = (uint32_t*)malloc((size_t)img->stride * h);
        img->owns_data = 1;
    }
    return img;
}

pixman_image_t *pixman_image_create_solid_fill(const pixman_color_t *color) {
    pixman_image_t *img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(*img));
    img->format = PIXMAN_a8r8g8b8;
    img->width = 1;
    img->height = 1;
    img->stride = 4;
    img->ref_count = 1;
    img->is_solid = 1;
    uint8_t a = color->alpha >> 8;
    uint8_t r = color->red >> 8;
    uint8_t g = color->green >> 8;
    uint8_t b = color->blue >> 8;
    img->solid_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    img->data = &img->solid_color;
    img->owns_data = 0;
    return img;
}

pixman_image_t *pixman_image_ref(pixman_image_t *img) {
    if (img) img->ref_count++;
    return img;
}

pixman_bool_t pixman_image_unref(pixman_image_t *img) {
    if (!img) return 0;
    if (--img->ref_count <= 0) {
        if (img->owns_data && img->data) free(img->data);
        free(img);
        return 1;
    }
    return 0;
}

int pixman_image_get_width(pixman_image_t *img) { return img ? img->width : 0; }
int pixman_image_get_height(pixman_image_t *img) { return img ? img->height : 0; }
int pixman_image_get_stride(pixman_image_t *img) { return img ? img->stride : 0; }
uint32_t *pixman_image_get_data(pixman_image_t *img) { return img ? img->data : NULL; }
pixman_format_code_t pixman_image_get_format(pixman_image_t *img) { return img ? img->format : PIXMAN_a8r8g8b8; }

void pixman_image_set_clip_region32(pixman_image_t *img, pixman_region32_t *r) { (void)img; (void)r; }
void pixman_image_set_transform(pixman_image_t *img, const pixman_transform_t *t) { (void)img; (void)t; }
void pixman_image_set_filter(pixman_image_t *img, pixman_filter_t f, const pixman_fixed_t *p, int n) {
    (void)img; (void)f; (void)p; (void)n;
}
void pixman_image_set_repeat(pixman_image_t *img, pixman_repeat_t r) { (void)img; (void)r; }

/* Simple SRC/OVER composite for XRGB/ARGB */
void pixman_image_composite32(pixman_op_t op,
                               pixman_image_t *src,
                               pixman_image_t *mask,
                               pixman_image_t *dst,
                               int32_t sx, int32_t sy,
                               int32_t mx, int32_t my,
                               int32_t dx, int32_t dy,
                               int32_t w, int32_t h) {
    (void)mask; (void)mx; (void)my;
    if (!src || !dst || !dst->data) return;

    for (int32_t y = 0; y < h; y++) {
        int32_t dst_y = dy + y;
        int32_t src_y = sy + y;
        if (dst_y < 0 || dst_y >= dst->height) continue;
        if (src_y < 0 || src_y >= src->height) continue;

        uint32_t *drow = (uint32_t*)((uint8_t*)dst->data + dst_y * dst->stride);
        uint32_t *srow;
        uint32_t solid;
        if (src->is_solid) {
            solid = src->solid_color;
            srow = &solid;
        } else {
            srow = (uint32_t*)((uint8_t*)src->data + src_y * src->stride);
        }

        for (int32_t x = 0; x < w; x++) {
            int32_t dst_x = dx + x;
            int32_t src_x = sx + x;
            if (dst_x < 0 || dst_x >= dst->width) continue;
            if (!src->is_solid && (src_x < 0 || src_x >= src->width)) continue;

            uint32_t spx = src->is_solid ? solid : srow[src_x];

            if (op == PIXMAN_OP_SRC || op == PIXMAN_OP_CLEAR) {
                drow[dst_x] = (op == PIXMAN_OP_CLEAR) ? 0 : spx;
            } else { /* OVER */
                uint32_t sa = (spx >> 24) & 0xFF;
                if (sa == 0xFF) {
                    drow[dst_x] = spx;
                } else if (sa > 0) {
                    uint32_t dpx = drow[dst_x];
                    uint32_t inv = 255 - sa;
                    uint32_t rb = ((spx & 0xFF00FF) * sa + (dpx & 0xFF00FF) * inv + 0x800080) >> 8;
                    uint32_t g  = ((spx & 0x00FF00) * sa + (dpx & 0x00FF00) * inv + 0x008000) >> 8;
                    drow[dst_x] = (rb & 0xFF00FF) | (g & 0x00FF00) | 0xFF000000;
                }
            }
        }
    }
}

pixman_bool_t pixman_image_fill_rectangles(pixman_op_t op,
                                            pixman_image_t *dst,
                                            const pixman_color_t *color,
                                            int n_rects,
                                            const pixman_rectangle32_t *rects) {
    (void)op;
    if (!dst || !dst->data || !color || !rects) return 0;
    uint8_t a = color->alpha >> 8;
    uint8_t r = color->red >> 8;
    uint8_t g = color->green >> 8;
    uint8_t b = color->blue >> 8;
    uint32_t px = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    for (int i = 0; i < n_rects; i++) {
        int32_t rx = rects[i].x1, ry = rects[i].y1;
        int32_t rw = rects[i].x2, rh = rects[i].y2;
        for (int32_t y = ry; y < ry + rh && y < dst->height; y++) {
            if (y < 0) continue;
            uint32_t *row = (uint32_t*)((uint8_t*)dst->data + y * dst->stride);
            for (int32_t x = rx; x < rx + rw && x < dst->width; x++) {
                if (x < 0) continue;
                row[x] = px;
            }
        }
    }
    return 1;
}
