#ifndef _AMS_PIXMAN_H
#define _AMS_PIXMAN_H

#include <stdint.h>

typedef int32_t pixman_fixed_t;
typedef int pixman_bool_t;

typedef enum {
    PIXMAN_a8r8g8b8 = 0x34325241,
    PIXMAN_x8r8g8b8 = 0x34325258,
    PIXMAN_r8g8b8   = 0x20424752,
    PIXMAN_a8       = 0x20203841,
} pixman_format_code_t;

typedef enum {
    PIXMAN_OP_SRC  = 0x01,
    PIXMAN_OP_OVER = 0x03,
    PIXMAN_OP_CLEAR = 0x00,
} pixman_op_t;

typedef enum {
    PIXMAN_FILTER_FAST,
    PIXMAN_FILTER_GOOD,
    PIXMAN_FILTER_BEST,
    PIXMAN_FILTER_NEAREST,
    PIXMAN_FILTER_BILINEAR,
} pixman_filter_t;

typedef enum {
    PIXMAN_REPEAT_NONE,
    PIXMAN_REPEAT_NORMAL,
    PIXMAN_REPEAT_PAD,
    PIXMAN_REPEAT_REFLECT,
} pixman_repeat_t;

typedef struct {
    int32_t x1, y1, x2, y2;
} pixman_box32_t;

typedef struct {
    int32_t x, y;
    int32_t width, height;
} pixman_rectangle32_t;

typedef struct {
    int n;
    pixman_box32_t extents;
    pixman_box32_t* rects;
} pixman_region32_t;

typedef struct pixman_image pixman_image_t;
typedef struct { pixman_fixed_t matrix[3][3]; } pixman_transform_t;

pixman_image_t* pixman_image_create_bits(pixman_format_code_t format,
                                          int width, int height,
                                          uint32_t* bits, int rowstride_bytes);
pixman_image_t* pixman_image_create_solid_fill(uint32_t color);
void pixman_image_unref(pixman_image_t* image);

pixman_bool_t pixman_image_composite32(pixman_op_t op,
    pixman_image_t* src, pixman_image_t* mask, pixman_image_t* dst,
    int32_t src_x, int32_t src_y, int32_t mask_x, int32_t mask_y,
    int32_t dest_x, int32_t dest_y, int32_t width, int32_t height);

void pixman_region32_init(pixman_region32_t* region);
void pixman_region32_init_rect(pixman_region32_t* region, int x, int y, unsigned int width, unsigned int height);
void pixman_region32_fini(pixman_region32_t* region);
pixman_bool_t pixman_region32_union_rect(pixman_region32_t* dest, pixman_region32_t* source, int x, int y, unsigned int width, unsigned int height);
pixman_bool_t pixman_region32_not_empty(pixman_region32_t* region);
pixman_box32_t* pixman_region32_rectangles(pixman_region32_t* region, int* n_rects);

#endif
