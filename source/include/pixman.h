/**
 * @file pixman.h
 * @brief Minimal pixman API for AMS (software rendering).
 *
 * Provides the subset of pixman used by wlroots for software compositing:
 * image creation/destruction, fill, blit, format conversion.
 * Backed by plain memcpy/loop rendering against CPU buffers.
 */
#ifndef _AMS_PIXMAN_H
#define _AMS_PIXMAN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int pixman_bool_t;
typedef int32_t pixman_fixed_t;

/* Pixel formats (subset matching wlroots usage) */
typedef enum {
    PIXMAN_a8r8g8b8 = 0x34325241,
    PIXMAN_x8r8g8b8 = 0x34325258,
    PIXMAN_r5g6b5   = 0x36314752,
    PIXMAN_a8       = 0x38000008,
    PIXMAN_r8g8b8   = 0x20202052,
} pixman_format_code_t;

typedef enum {
    PIXMAN_OP_SRC      = 0x01,
    PIXMAN_OP_OVER     = 0x03,
    PIXMAN_OP_CLEAR    = 0x00,
} pixman_op_t;

typedef enum {
    PIXMAN_REPEAT_NONE   = 0,
    PIXMAN_REPEAT_NORMAL = 1,
    PIXMAN_REPEAT_PAD    = 2,
    PIXMAN_REPEAT_REFLECT= 3,
} pixman_repeat_t;

typedef enum {
    PIXMAN_FILTER_FAST    = 0,
    PIXMAN_FILTER_GOOD    = 1,
    PIXMAN_FILTER_BEST    = 2,
    PIXMAN_FILTER_NEAREST = 3,
    PIXMAN_FILTER_BILINEAR= 4,
} pixman_filter_t;

typedef struct pixman_image pixman_image_t;

typedef struct {
    int32_t x, y;
    int32_t width, height;
} pixman_box32_t;

typedef struct {
    int32_t x1, y1, x2, y2;
} pixman_rectangle32_t;

typedef struct {
    int n_rects;
    pixman_box32_t *rects;
} pixman_region32_t;

typedef struct {
    pixman_fixed_t matrix[3][3];
} pixman_transform_t;

typedef union {
    struct { uint16_t blue, green, red, alpha; };
    uint64_t pad;
} pixman_color_t;

/* Region operations */
void pixman_region32_init(pixman_region32_t *region);
void pixman_region32_init_rect(pixman_region32_t *region,
                                int x, int y,
                                unsigned int width, unsigned int height);
void pixman_region32_fini(pixman_region32_t *region);
pixman_bool_t pixman_region32_union(pixman_region32_t *new_reg,
                                     pixman_region32_t *reg1,
                                     pixman_region32_t *reg2);
pixman_bool_t pixman_region32_intersect(pixman_region32_t *new_reg,
                                         pixman_region32_t *reg1,
                                         pixman_region32_t *reg2);
pixman_bool_t pixman_region32_subtract(pixman_region32_t *new_reg,
                                        pixman_region32_t *reg1,
                                        pixman_region32_t *reg2);
pixman_bool_t pixman_region32_not_empty(pixman_region32_t *region);
pixman_box32_t *pixman_region32_rectangles(pixman_region32_t *region, int *n_rects);
pixman_box32_t *pixman_region32_extents(pixman_region32_t *region);
void pixman_region32_translate(pixman_region32_t *region, int x, int y);
pixman_bool_t pixman_region32_copy(pixman_region32_t *dest, pixman_region32_t *source);
pixman_bool_t pixman_region32_contains_point(pixman_region32_t *region,
                                              int x, int y,
                                              pixman_box32_t *box);

/* Image operations */
pixman_image_t *pixman_image_create_bits(pixman_format_code_t format,
                                          int width, int height,
                                          uint32_t *bits, int rowstride_bytes);
pixman_image_t *pixman_image_create_bits_no_clear(pixman_format_code_t format,
                                                    int width, int height,
                                                    uint32_t *bits, int rowstride_bytes);
pixman_image_t *pixman_image_create_solid_fill(const pixman_color_t *color);
pixman_image_t *pixman_image_ref(pixman_image_t *image);
pixman_bool_t   pixman_image_unref(pixman_image_t *image);

int       pixman_image_get_width(pixman_image_t *image);
int       pixman_image_get_height(pixman_image_t *image);
int       pixman_image_get_stride(pixman_image_t *image);
uint32_t *pixman_image_get_data(pixman_image_t *image);
pixman_format_code_t pixman_image_get_format(pixman_image_t *image);

void pixman_image_set_clip_region32(pixman_image_t *image,
                                     pixman_region32_t *region);
void pixman_image_set_transform(pixman_image_t *image,
                                 const pixman_transform_t *transform);
void pixman_image_set_filter(pixman_image_t *image,
                              pixman_filter_t filter,
                              const pixman_fixed_t *params,
                              int n_params);
void pixman_image_set_repeat(pixman_image_t *image,
                              pixman_repeat_t repeat);

/* Composite */
void pixman_image_composite32(pixman_op_t op,
                               pixman_image_t *src,
                               pixman_image_t *mask,
                               pixman_image_t *dest,
                               int32_t src_x, int32_t src_y,
                               int32_t mask_x, int32_t mask_y,
                               int32_t dest_x, int32_t dest_y,
                               int32_t width, int32_t height);

/* Fill */
pixman_bool_t pixman_image_fill_rectangles(pixman_op_t op,
                                            pixman_image_t *dest,
                                            const pixman_color_t *color,
                                            int n_rects,
                                            const pixman_rectangle32_t *rects);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_PIXMAN_H */
