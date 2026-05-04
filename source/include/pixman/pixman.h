/*
 * Minimal pixman ABI shim for AMS.
 *
 * This is a subset compatible with pixman-1 1.40.x for what wlroots'
 * software renderer needs:
 *  - image creation (bits backed),
 *  - format codes (a8r8g8b8 / x8r8g8b8 / r5g6b5 / a8),
 *  - solid fill + composite32 (over only),
 *  - pixman_region32_* (rect-only, single-rect optimization).
 */

#ifndef AMS_PIXMAN_H
#define AMS_PIXMAN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PIXMAN_FORMAT_BITS_PER_PIXEL_SHIFT 24
#define PIXMAN_FORMAT(bpp, type, a, r, g, b) \
    (((bpp) << 24) | ((type) << 16) | ((a) << 12) | ((r) << 8) | ((g) << 4) | (b))

typedef enum {
    PIXMAN_TYPE_OTHER  = 0,
    PIXMAN_TYPE_A      = 1,
    PIXMAN_TYPE_ARGB   = 2,
    PIXMAN_TYPE_RGBA   = 3,
    PIXMAN_TYPE_ABGR   = 4
} pixman_format_type_t;

typedef enum {
    PIXMAN_a8        = PIXMAN_FORMAT(8,  PIXMAN_TYPE_A,    8, 0, 0, 0),
    PIXMAN_r5g6b5    = PIXMAN_FORMAT(16, PIXMAN_TYPE_ARGB, 0, 5, 6, 5),
    PIXMAN_x8r8g8b8  = PIXMAN_FORMAT(32, PIXMAN_TYPE_ARGB, 0, 8, 8, 8),
    PIXMAN_a8r8g8b8  = PIXMAN_FORMAT(32, PIXMAN_TYPE_ARGB, 8, 8, 8, 8),
    PIXMAN_a8b8g8r8  = PIXMAN_FORMAT(32, PIXMAN_TYPE_ABGR, 8, 8, 8, 8)
} pixman_format_code_t;

typedef enum {
    PIXMAN_OP_CLEAR = 0,
    PIXMAN_OP_SRC   = 1,
    PIXMAN_OP_OVER  = 3
} pixman_op_t;

typedef struct pixman_image pixman_image_t;

typedef struct {
    int32_t x1, y1, x2, y2;
} pixman_box32_t;

typedef struct {
    pixman_box32_t extents;
    int           n_rects;
    pixman_box32_t *rects;
} pixman_region32_t;

typedef struct {
    int32_t x, y;
} pixman_point_t;

pixman_image_t *pixman_image_create_bits(pixman_format_code_t fmt,
                                         int width, int height,
                                         uint32_t *bits, int rowstride_bytes);
void            pixman_image_unref(pixman_image_t *img);
int             pixman_image_ref(pixman_image_t *img); /* returns refcount */

uint32_t       *pixman_image_get_data(pixman_image_t *img);
int             pixman_image_get_width(pixman_image_t *img);
int             pixman_image_get_height(pixman_image_t *img);
int             pixman_image_get_stride(pixman_image_t *img);
pixman_format_code_t pixman_image_get_format(pixman_image_t *img);

void pixman_image_composite32(pixman_op_t op,
                              pixman_image_t *src, pixman_image_t *mask,
                              pixman_image_t *dst,
                              int32_t src_x, int32_t src_y,
                              int32_t mask_x, int32_t mask_y,
                              int32_t dst_x, int32_t dst_y,
                              int32_t width, int32_t height);

void pixman_image_fill_rectangles(pixman_op_t op, pixman_image_t *dst,
                                  uint32_t color, int n_rects,
                                  const pixman_box32_t *rects);

/* Region (rectangle list, no real Y-X-banding for the shim). */
void pixman_region32_init(pixman_region32_t *r);
void pixman_region32_init_rect(pixman_region32_t *r,
                               int32_t x, int32_t y,
                               uint32_t w, uint32_t h);
void pixman_region32_fini(pixman_region32_t *r);
int  pixman_region32_union_rect(pixman_region32_t *dst,
                                pixman_region32_t *src,
                                int32_t x, int32_t y,
                                uint32_t w, uint32_t h);
int  pixman_region32_subtract(pixman_region32_t *dst,
                              pixman_region32_t *m, pixman_region32_t *s);
const pixman_box32_t *pixman_region32_extents(pixman_region32_t *r);
int  pixman_region32_n_rects(pixman_region32_t *r);
const pixman_box32_t *pixman_region32_rectangles(pixman_region32_t *r,
                                                 int *n_rects);

#ifdef __cplusplus
}
#endif

#endif /* AMS_PIXMAN_H */
