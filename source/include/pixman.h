#ifndef _PIXMAN_H
#define _PIXMAN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int pixman_bool_t;
typedef int32_t pixman_fixed_t;

#define pixman_fixed_1       (1 << 16)
#define pixman_int_to_fixed(i) ((pixman_fixed_t)((i) << 16))
#define pixman_fixed_to_int(f) ((int)((f) >> 16))

typedef enum {
    PIXMAN_a8r8g8b8    = 0x34325241,
    PIXMAN_x8r8g8b8    = 0x34325258,
    PIXMAN_a8b8g8r8    = 0x34324241,
    PIXMAN_x8b8g8r8    = 0x34324258,
    PIXMAN_r8g8b8      = 0x34324752,
    PIXMAN_a8          = 0x38000041,
    PIXMAN_r5g6b5      = 0x36314752,
} pixman_format_code_t;

typedef enum {
    PIXMAN_OP_CLEAR = 0x00,
    PIXMAN_OP_SRC   = 0x01,
    PIXMAN_OP_DST   = 0x02,
    PIXMAN_OP_OVER  = 0x03,
    PIXMAN_OP_IN    = 0x05,
    PIXMAN_OP_OUT   = 0x06,
    PIXMAN_OP_ADD   = 0x0C,
} pixman_op_t;

typedef enum {
    PIXMAN_REPEAT_NONE   = 0,
    PIXMAN_REPEAT_NORMAL = 1,
    PIXMAN_REPEAT_PAD    = 2,
    PIXMAN_REPEAT_REFLECT = 3,
} pixman_repeat_t;

typedef enum {
    PIXMAN_FILTER_FAST    = 0,
    PIXMAN_FILTER_GOOD    = 1,
    PIXMAN_FILTER_BEST    = 2,
    PIXMAN_FILTER_NEAREST = 3,
    PIXMAN_FILTER_BILINEAR = 4,
} pixman_filter_t;

typedef struct {
    pixman_fixed_t matrix[3][3];
} pixman_transform_t;

typedef struct {
    int32_t x1, y1, x2, y2;
} pixman_box32_t;

typedef struct {
    int16_t x1, y1, x2, y2;
} pixman_box16_t;

typedef struct pixman_region32_data {
    long size;
    long numRects;
} pixman_region32_data_t;

typedef struct pixman_region32 {
    pixman_box32_t extents;
    pixman_region32_data_t* data;
} pixman_region32_t;

typedef struct pixman_region16_data {
    long size;
    long numRects;
} pixman_region16_data_t;

typedef struct pixman_region16 {
    pixman_box16_t extents;
    pixman_region16_data_t* data;
} pixman_region16_t;

typedef struct pixman_color {
    uint16_t red, green, blue, alpha;
} pixman_color_t;

typedef struct pixman_image pixman_image_t;

/* Image creation */
pixman_image_t* pixman_image_create_bits(pixman_format_code_t format,
    int width, int height, uint32_t* bits, int rowstride_bytes);
pixman_image_t* pixman_image_create_solid_fill(const pixman_color_t* color);
pixman_image_t* pixman_image_ref(pixman_image_t* image);
pixman_bool_t   pixman_image_unref(pixman_image_t* image);

/* Image properties */
int      pixman_image_get_width(pixman_image_t* image);
int      pixman_image_get_height(pixman_image_t* image);
int      pixman_image_get_stride(pixman_image_t* image);
uint32_t* pixman_image_get_data(pixman_image_t* image);
pixman_format_code_t pixman_image_get_format(pixman_image_t* image);

/* Image manipulation */
pixman_bool_t pixman_image_set_clip_region32(pixman_image_t* image,
    const pixman_region32_t* region);
void pixman_image_set_transform(pixman_image_t* image,
    const pixman_transform_t* transform);
void pixman_image_set_filter(pixman_image_t* image,
    pixman_filter_t filter, const pixman_fixed_t* params, int n_params);
void pixman_image_set_repeat(pixman_image_t* image, pixman_repeat_t repeat);

/* Compositing */
void pixman_image_composite32(pixman_op_t op,
    pixman_image_t* src, pixman_image_t* mask, pixman_image_t* dest,
    int32_t src_x, int32_t src_y,
    int32_t mask_x, int32_t mask_y,
    int32_t dest_x, int32_t dest_y,
    int32_t width, int32_t height);

pixman_bool_t pixman_image_fill_rectangles(pixman_op_t op,
    pixman_image_t* dest, const pixman_color_t* color,
    int n_rects, const pixman_box32_t* rects);

/* Region32 operations */
void pixman_region32_init(pixman_region32_t* region);
void pixman_region32_init_rect(pixman_region32_t* region,
    int x, int y, unsigned int width, unsigned int height);
void pixman_region32_init_with_extents(pixman_region32_t* region,
    const pixman_box32_t* extents);
void pixman_region32_fini(pixman_region32_t* region);
pixman_bool_t pixman_region32_copy(pixman_region32_t* dest,
    const pixman_region32_t* src);
pixman_bool_t pixman_region32_union(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2);
pixman_bool_t pixman_region32_union_rect(pixman_region32_t* dest,
    const pixman_region32_t* src, int x, int y,
    unsigned int width, unsigned int height);
pixman_bool_t pixman_region32_intersect(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2);
pixman_bool_t pixman_region32_intersect_rect(pixman_region32_t* dest,
    const pixman_region32_t* src, int x, int y,
    unsigned int width, unsigned int height);
pixman_bool_t pixman_region32_subtract(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2);
pixman_bool_t pixman_region32_not_empty(const pixman_region32_t* region);
pixman_box32_t* pixman_region32_extents(const pixman_region32_t* region);
pixman_box32_t* pixman_region32_rectangles(const pixman_region32_t* region,
    int* n_rects);
void pixman_region32_translate(pixman_region32_t* region, int x, int y);
pixman_bool_t pixman_region32_contains_point(const pixman_region32_t* region,
    int x, int y, pixman_box32_t* box);
pixman_bool_t pixman_region32_equal(const pixman_region32_t* r1,
    const pixman_region32_t* r2);
void pixman_region32_clear(pixman_region32_t* region);

/* Utility */
int pixman_format_supported_destination(pixman_format_code_t format);
int pixman_format_supported_source(pixman_format_code_t format);

#ifdef __cplusplus
}
#endif

#endif
