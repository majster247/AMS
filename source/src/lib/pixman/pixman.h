/* Minimal pixman-1 port for AMS.
 * Implements pixel-format compositing needed by wlroots software renderer.
 * Only scalar C; no SIMD.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- pixel formats ---- */
typedef uint32_t pixman_format_code_t;

#define PIXMAN_FORMAT(bpp,type,a,r,g,b) \
    (((bpp) << 24) | ((type) << 16) | ((a) << 12) | ((r) << 8) | ((g) << 4) | (b))

#define PIXMAN_TYPE_OTHER  0
#define PIXMAN_TYPE_ARGB   2
#define PIXMAN_TYPE_ABGR   3
#define PIXMAN_TYPE_BGRA   6
#define PIXMAN_TYPE_RGBA   7

#define PIXMAN_a8r8g8b8  PIXMAN_FORMAT(32, PIXMAN_TYPE_ARGB, 8, 8, 8, 8)
#define PIXMAN_x8r8g8b8  PIXMAN_FORMAT(32, PIXMAN_TYPE_ARGB, 0, 8, 8, 8)
#define PIXMAN_a8b8g8r8  PIXMAN_FORMAT(32, PIXMAN_TYPE_ABGR, 8, 8, 8, 8)
#define PIXMAN_x8b8g8r8  PIXMAN_FORMAT(32, PIXMAN_TYPE_ABGR, 0, 8, 8, 8)
#define PIXMAN_b8g8r8a8  PIXMAN_FORMAT(32, PIXMAN_TYPE_BGRA, 8, 8, 8, 8)
#define PIXMAN_r8g8b8a8  PIXMAN_FORMAT(32, PIXMAN_TYPE_RGBA, 8, 8, 8, 8)
#define PIXMAN_r5g6b5    PIXMAN_FORMAT(16, PIXMAN_TYPE_ARGB, 0, 5, 6, 5)

/* ---- compositing operators ---- */
typedef enum {
    PIXMAN_OP_CLEAR         = 0x00,
    PIXMAN_OP_SRC           = 0x01,
    PIXMAN_OP_DST           = 0x02,
    PIXMAN_OP_OVER          = 0x03,
    PIXMAN_OP_OVER_REVERSE  = 0x04,
    PIXMAN_OP_IN            = 0x05,
    PIXMAN_OP_IN_REVERSE    = 0x06,
    PIXMAN_OP_OUT           = 0x07,
    PIXMAN_OP_OUT_REVERSE   = 0x08,
    PIXMAN_OP_ATOP          = 0x09,
    PIXMAN_OP_ATOP_REVERSE  = 0x0A,
    PIXMAN_OP_XOR           = 0x0B,
    PIXMAN_OP_ADD           = 0x0C,
    PIXMAN_OP_SATURATE      = 0x0D,
    PIXMAN_OP_CONJOINT_CLEAR    = 0x20,
    PIXMAN_OP_DISJOINT_CLEAR    = 0x10,
    PIXMAN_OP_MULTIPLY      = 0x30,
    PIXMAN_OP_SCREEN        = 0x31,
    PIXMAN_OP_OVERLAY       = 0x32,
} pixman_op_t;

/* ---- region ---- */
typedef struct { int32_t x, y, width, height; } pixman_rectangle16_t;
typedef struct { int32_t x1, y1, x2, y2; } pixman_box32_t;
typedef struct { int n_rects; pixman_box32_t extents; pixman_box32_t* rects; } pixman_region32_t;

void pixman_region32_init(pixman_region32_t* region);
void pixman_region32_init_rect(pixman_region32_t* region, int x, int y, unsigned w, unsigned h);
void pixman_region32_fini(pixman_region32_t* region);
int  pixman_region32_not_empty(pixman_region32_t* region);
void pixman_region32_union(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b);
void pixman_region32_intersect(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b);
void pixman_region32_subtract(pixman_region32_t* dst, const pixman_region32_t* a, const pixman_region32_t* b);
void pixman_region32_union_rect(pixman_region32_t* dst, const pixman_region32_t* src,
                                int x, int y, unsigned w, unsigned h);
pixman_box32_t* pixman_region32_rectangles(pixman_region32_t* region, int* n_rects);
pixman_box32_t  pixman_region32_extents(pixman_region32_t* region);
int pixman_region32_contains_point(pixman_region32_t* region, int x, int y, pixman_box32_t* box);
void pixman_region32_copy(pixman_region32_t* dst, const pixman_region32_t* src);
void pixman_region32_translate(pixman_region32_t* region, int x, int y);
void pixman_region32_clear(pixman_region32_t* region);

/* ---- image ---- */
typedef struct pixman_image pixman_image_t;

typedef uint32_t pixman_color_t; /* packed ARGB32 */

pixman_image_t* pixman_image_create_bits(pixman_format_code_t format,
                                         int width, int height,
                                         uint32_t* bits, int rowstride_bytes);
pixman_image_t* pixman_image_create_bits_no_clear(pixman_format_code_t format,
                                                  int width, int height,
                                                  uint32_t* bits, int rowstride_bytes);
pixman_image_t* pixman_image_create_solid_fill(const pixman_color_t* color);
pixman_image_t* pixman_image_ref(pixman_image_t* image);
int             pixman_image_unref(pixman_image_t* image);
int             pixman_image_get_width(pixman_image_t* image);
int             pixman_image_get_height(pixman_image_t* image);
int             pixman_image_get_stride(pixman_image_t* image);
pixman_format_code_t pixman_image_get_format(pixman_image_t* image);
uint32_t*       pixman_image_get_data(pixman_image_t* image);
void            pixman_image_set_clip_region32(pixman_image_t* image, pixman_region32_t* region);

/* ---- compositing ---- */
void pixman_image_composite32(pixman_op_t op,
                              pixman_image_t* src,
                              pixman_image_t* mask,
                              pixman_image_t* dst,
                              int16_t src_x, int16_t src_y,
                              int16_t mask_x, int16_t mask_y,
                              int16_t dst_x, int16_t dst_y,
                              uint16_t width, uint16_t height);

/* ---- format helpers ---- */
static inline int pixman_format_bpp(pixman_format_code_t f) { return (int)((f) >> 24); }

#ifdef __cplusplus
}
#endif
