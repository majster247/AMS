/**
 * Minimal pixman implementation for AMS.
 *
 * Provides enough functionality for wlroots' pixman renderer:
 * - pixman_image_t with ARGB32/XRGB32 buffers
 * - SRC and OVER compositing
 * - Region32 rectangle-set math
 */

#include "pixman.h"
#include <stdint.h>
#include <stddef.h>

extern void* malloc(size_t size);
extern void  free(void* ptr);
extern void* memcpy(void* dest, const void* src, size_t n);
extern void* memset(void* dest, int ch, size_t n);

/* ---------- internal image struct ---------- */

typedef enum {
    PIXMAN_IMAGE_BITS,
    PIXMAN_IMAGE_SOLID,
} pixman_image_type_t;

struct pixman_image {
    pixman_image_type_t type;
    int ref_count;
    pixman_format_code_t format;
    int width, height;
    int stride; /* bytes per row */
    uint32_t* bits;
    int bits_owned; /* 1 if we allocated the buffer */
    pixman_region32_t clip;
    int has_clip;
    pixman_transform_t* transform;
    pixman_filter_t filter;
    pixman_repeat_t repeat;
    /* for solid fill images */
    uint32_t solid_color;
};

/* ---------- image creation ---------- */

pixman_image_t* pixman_image_create_bits(pixman_format_code_t format,
    int width, int height, uint32_t* bits, int rowstride_bytes)
{
    pixman_image_t* img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return (pixman_image_t*)0;
    memset(img, 0, sizeof(pixman_image_t));

    img->type = PIXMAN_IMAGE_BITS;
    img->ref_count = 1;
    img->format = format;
    img->width = width;
    img->height = height;
    img->stride = rowstride_bytes ? rowstride_bytes : (width * 4);

    if (bits) {
        img->bits = bits;
        img->bits_owned = 0;
    } else {
        size_t sz = (size_t)img->stride * (size_t)height;
        img->bits = (uint32_t*)malloc(sz);
        if (img->bits) memset(img->bits, 0, sz);
        img->bits_owned = 1;
    }
    return img;
}

pixman_image_t* pixman_image_create_solid_fill(const pixman_color_t* color) {
    pixman_image_t* img = (pixman_image_t*)malloc(sizeof(pixman_image_t));
    if (!img) return (pixman_image_t*)0;
    memset(img, 0, sizeof(pixman_image_t));

    img->type = PIXMAN_IMAGE_SOLID;
    img->ref_count = 1;
    img->format = PIXMAN_a8r8g8b8;
    img->width = 1;
    img->height = 1;

    uint8_t a = (uint8_t)(color->alpha >> 8);
    uint8_t r = (uint8_t)(color->red >> 8);
    uint8_t g = (uint8_t)(color->green >> 8);
    uint8_t b = (uint8_t)(color->blue >> 8);
    img->solid_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                       ((uint32_t)g << 8) | b;
    return img;
}

pixman_image_t* pixman_image_ref(pixman_image_t* image) {
    if (image) image->ref_count++;
    return image;
}

pixman_bool_t pixman_image_unref(pixman_image_t* image) {
    if (!image) return 0;
    image->ref_count--;
    if (image->ref_count <= 0) {
        if (image->bits_owned && image->bits) free(image->bits);
        if (image->transform) free(image->transform);
        if (image->has_clip) pixman_region32_fini(&image->clip);
        free(image);
        return 1;
    }
    return 0;
}

/* ---------- image properties ---------- */

int pixman_image_get_width(pixman_image_t* image) { return image ? image->width : 0; }
int pixman_image_get_height(pixman_image_t* image) { return image ? image->height : 0; }
int pixman_image_get_stride(pixman_image_t* image) { return image ? image->stride : 0; }
uint32_t* pixman_image_get_data(pixman_image_t* image) { return image ? image->bits : 0; }
pixman_format_code_t pixman_image_get_format(pixman_image_t* image) {
    return image ? image->format : PIXMAN_a8r8g8b8;
}

pixman_bool_t pixman_image_set_clip_region32(pixman_image_t* image,
    const pixman_region32_t* region) {
    if (!image) return 0;
    if (region) {
        if (!image->has_clip) { pixman_region32_init(&image->clip); image->has_clip = 1; }
        pixman_region32_copy(&image->clip, region);
    } else {
        if (image->has_clip) { pixman_region32_fini(&image->clip); image->has_clip = 0; }
    }
    return 1;
}

void pixman_image_set_transform(pixman_image_t* image,
    const pixman_transform_t* transform) {
    if (!image) return;
    if (!transform) { if (image->transform) { free(image->transform); image->transform = 0; } return; }
    if (!image->transform) image->transform = (pixman_transform_t*)malloc(sizeof(pixman_transform_t));
    if (image->transform) memcpy(image->transform, transform, sizeof(pixman_transform_t));
}

void pixman_image_set_filter(pixman_image_t* image,
    pixman_filter_t filter, const pixman_fixed_t* params, int n_params) {
    (void)params; (void)n_params;
    if (image) image->filter = filter;
}

void pixman_image_set_repeat(pixman_image_t* image, pixman_repeat_t repeat) {
    if (image) image->repeat = repeat;
}

/* ---------- compositing ---------- */

static inline uint8_t alpha_blend(uint8_t src, uint8_t dst, uint8_t sa) {
    return (uint8_t)((uint32_t)src + (uint32_t)dst * (255u - sa) / 255u);
}

static uint32_t get_pixel(pixman_image_t* img, int x, int y) {
    if (img->type == PIXMAN_IMAGE_SOLID) return img->solid_color;
    if (!img->bits) return 0;
    if (x < 0 || x >= img->width || y < 0 || y >= img->height) return 0;
    uint32_t* row = (uint32_t*)((uint8_t*)img->bits + (size_t)y * (size_t)img->stride);
    return row[x];
}

void pixman_image_composite32(pixman_op_t op,
    pixman_image_t* src, pixman_image_t* mask, pixman_image_t* dest,
    int32_t src_x, int32_t src_y,
    int32_t mask_x, int32_t mask_y,
    int32_t dest_x, int32_t dest_y,
    int32_t width, int32_t height)
{
    if (!src || !dest || !dest->bits) return;

    for (int32_t y = 0; y < height; y++) {
        int dy = dest_y + y;
        if (dy < 0 || dy >= dest->height) continue;
        uint32_t* drow = (uint32_t*)((uint8_t*)dest->bits + (size_t)dy * (size_t)dest->stride);

        for (int32_t x = 0; x < width; x++) {
            int dx = dest_x + x;
            if (dx < 0 || dx >= dest->width) continue;

            uint32_t sp = get_pixel(src, src_x + x, src_y + y);

            /* apply mask */
            if (mask) {
                uint32_t mp = get_pixel(mask, mask_x + x, mask_y + y);
                uint8_t ma = (uint8_t)(mp >> 24);
                uint8_t sa = (uint8_t)(sp >> 24);
                sa = (uint8_t)((uint32_t)sa * ma / 255u);
                sp = (sp & 0x00FFFFFFu) | ((uint32_t)sa << 24);
            }

            switch (op) {
                case PIXMAN_OP_SRC:
                    drow[dx] = sp;
                    break;
                case PIXMAN_OP_OVER: {
                    uint8_t sa = (uint8_t)(sp >> 24);
                    if (sa == 255) { drow[dx] = sp; break; }
                    if (sa == 0) break;
                    uint32_t dp = drow[dx];
                    uint8_t sr = (uint8_t)(sp >> 16), sg = (uint8_t)(sp >> 8), sb = (uint8_t)sp;
                    uint8_t dr = (uint8_t)(dp >> 16), dg = (uint8_t)(dp >> 8), db = (uint8_t)dp;
                    uint8_t da = (uint8_t)(dp >> 24);
                    drow[dx] = ((uint32_t)alpha_blend(sa, da, sa) << 24) |
                               ((uint32_t)alpha_blend(sr, dr, sa) << 16) |
                               ((uint32_t)alpha_blend(sg, dg, sa) << 8) |
                               alpha_blend(sb, db, sa);
                    break;
                }
                case PIXMAN_OP_CLEAR:
                    drow[dx] = 0;
                    break;
                case PIXMAN_OP_ADD: {
                    uint32_t dp = drow[dx];
                    uint32_t ra = ((sp >> 24) + (dp >> 24)); if (ra > 255) ra = 255;
                    uint32_t rr = (((sp >> 16) & 0xFF) + ((dp >> 16) & 0xFF)); if (rr > 255) rr = 255;
                    uint32_t rg = (((sp >> 8) & 0xFF) + ((dp >> 8) & 0xFF)); if (rg > 255) rg = 255;
                    uint32_t rb = ((sp & 0xFF) + (dp & 0xFF)); if (rb > 255) rb = 255;
                    drow[dx] = (ra << 24) | (rr << 16) | (rg << 8) | rb;
                    break;
                }
                default:
                    drow[dx] = sp;
                    break;
            }
        }
    }
}

pixman_bool_t pixman_image_fill_rectangles(pixman_op_t op,
    pixman_image_t* dest, const pixman_color_t* color,
    int n_rects, const pixman_box32_t* rects)
{
    if (!dest || !dest->bits || !color) return 0;

    uint8_t a = (uint8_t)(color->alpha >> 8);
    uint8_t r = (uint8_t)(color->red >> 8);
    uint8_t g = (uint8_t)(color->green >> 8);
    uint8_t b = (uint8_t)(color->blue >> 8);
    uint32_t pixel = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

    for (int i = 0; i < n_rects; i++) {
        int x1 = rects[i].x1, y1 = rects[i].y1;
        int x2 = rects[i].x2, y2 = rects[i].y2;
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > dest->width)  x2 = dest->width;
        if (y2 > dest->height) y2 = dest->height;
        for (int y = y1; y < y2; y++) {
            uint32_t* row = (uint32_t*)((uint8_t*)dest->bits + (size_t)y * (size_t)dest->stride);
            if (op == PIXMAN_OP_SRC || a == 255) {
                for (int x = x1; x < x2; x++) row[x] = pixel;
            } else if (op == PIXMAN_OP_CLEAR) {
                for (int x = x1; x < x2; x++) row[x] = 0;
            }
        }
    }
    return 1;
}

/* ---------- region32 ---------- */

#define MAX_RECTS_INLINE 64

typedef struct {
    pixman_region32_data_t header;
    pixman_box32_t rects[MAX_RECTS_INLINE];
} region_data_storage;

static pixman_region32_data_t g_empty_data = {0, 0};

void pixman_region32_init(pixman_region32_t* region) {
    region->extents.x1 = 0; region->extents.y1 = 0;
    region->extents.x2 = 0; region->extents.y2 = 0;
    region->data = &g_empty_data;
}

void pixman_region32_init_rect(pixman_region32_t* region,
    int x, int y, unsigned int width, unsigned int height)
{
    region->extents.x1 = x; region->extents.y1 = y;
    region->extents.x2 = x + (int)width;
    region->extents.y2 = y + (int)height;
    region->data = (pixman_region32_data_t*)0;
}

void pixman_region32_init_with_extents(pixman_region32_t* region,
    const pixman_box32_t* extents)
{
    region->extents = *extents;
    region->data = (pixman_region32_data_t*)0;
}

void pixman_region32_fini(pixman_region32_t* region) {
    if (region->data && region->data != &g_empty_data) free(region->data);
    region->data = &g_empty_data;
    region->extents.x1 = region->extents.y1 = 0;
    region->extents.x2 = region->extents.y2 = 0;
}

void pixman_region32_clear(pixman_region32_t* region) {
    pixman_region32_fini(region);
    pixman_region32_init(region);
}

pixman_bool_t pixman_region32_copy(pixman_region32_t* dest,
    const pixman_region32_t* src) {
    if (dest == src) return 1;
    dest->extents = src->extents;
    if (!src->data || src->data == &g_empty_data) {
        if (dest->data && dest->data != &g_empty_data) free(dest->data);
        dest->data = src->data;
        return 1;
    }
    long n = src->data->numRects;
    size_t sz = sizeof(pixman_region32_data_t) + (size_t)n * sizeof(pixman_box32_t);
    if (dest->data && dest->data != &g_empty_data) free(dest->data);
    dest->data = (pixman_region32_data_t*)malloc(sz);
    if (!dest->data) { dest->data = &g_empty_data; return 0; }
    memcpy(dest->data, src->data, sz);
    return 1;
}

static pixman_box32_t* region_rects(const pixman_region32_t* r) {
    if (!r->data || r->data == &g_empty_data) return (pixman_box32_t*)0;
    return (pixman_box32_t*)((uint8_t*)r->data + sizeof(pixman_region32_data_t));
}

pixman_bool_t pixman_region32_not_empty(const pixman_region32_t* region) {
    return (region->extents.x1 < region->extents.x2 &&
            region->extents.y1 < region->extents.y2) ? 1 : 0;
}

pixman_box32_t* pixman_region32_extents(const pixman_region32_t* region) {
    return (pixman_box32_t*)&region->extents;
}

pixman_box32_t* pixman_region32_rectangles(const pixman_region32_t* region,
    int* n_rects)
{
    if (!region->data || region->data == &g_empty_data || region->data->numRects == 0) {
        if (pixman_region32_not_empty(region)) {
            if (n_rects) *n_rects = 1;
            return (pixman_box32_t*)&region->extents;
        }
        if (n_rects) *n_rects = 0;
        return (pixman_box32_t*)&region->extents;
    }
    if (n_rects) *n_rects = (int)region->data->numRects;
    return region_rects(region);
}

void pixman_region32_translate(pixman_region32_t* region, int x, int y) {
    region->extents.x1 += x; region->extents.x2 += x;
    region->extents.y1 += y; region->extents.y2 += y;
    if (region->data && region->data != &g_empty_data) {
        pixman_box32_t* rects = region_rects(region);
        for (long i = 0; i < region->data->numRects; i++) {
            rects[i].x1 += x; rects[i].x2 += x;
            rects[i].y1 += y; rects[i].y2 += y;
        }
    }
}

pixman_bool_t pixman_region32_union_rect(pixman_region32_t* dest,
    const pixman_region32_t* src, int x, int y,
    unsigned int width, unsigned int height)
{
    pixman_region32_t r;
    pixman_region32_init_rect(&r, x, y, width, height);
    pixman_bool_t ret = pixman_region32_union(dest, src, &r);
    pixman_region32_fini(&r);
    return ret;
}

pixman_bool_t pixman_region32_union(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2)
{
    if (!pixman_region32_not_empty(r1)) return pixman_region32_copy(dest, r2);
    if (!pixman_region32_not_empty(r2)) return pixman_region32_copy(dest, r1);

    pixman_box32_t ext;
    ext.x1 = (r1->extents.x1 < r2->extents.x1) ? r1->extents.x1 : r2->extents.x1;
    ext.y1 = (r1->extents.y1 < r2->extents.y1) ? r1->extents.y1 : r2->extents.y1;
    ext.x2 = (r1->extents.x2 > r2->extents.x2) ? r1->extents.x2 : r2->extents.x2;
    ext.y2 = (r1->extents.y2 > r2->extents.y2) ? r1->extents.y2 : r2->extents.y2;

    if (dest->data && dest->data != &g_empty_data) free(dest->data);
    dest->extents = ext;
    dest->data = (pixman_region32_data_t*)0;
    return 1;
}

static int box_intersect(const pixman_box32_t* a, const pixman_box32_t* b,
    pixman_box32_t* out)
{
    out->x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
    out->y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
    out->x2 = (a->x2 < b->x2) ? a->x2 : b->x2;
    out->y2 = (a->y2 < b->y2) ? a->y2 : b->y2;
    return (out->x1 < out->x2 && out->y1 < out->y2) ? 1 : 0;
}

pixman_bool_t pixman_region32_intersect(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2)
{
    pixman_box32_t out;
    if (!box_intersect(&r1->extents, &r2->extents, &out)) {
        pixman_region32_clear(dest);
        return 1;
    }
    if (dest->data && dest->data != &g_empty_data) free(dest->data);
    dest->extents = out;
    dest->data = (pixman_region32_data_t*)0;
    return 1;
}

pixman_bool_t pixman_region32_intersect_rect(pixman_region32_t* dest,
    const pixman_region32_t* src, int x, int y,
    unsigned int width, unsigned int height)
{
    pixman_region32_t r;
    pixman_region32_init_rect(&r, x, y, width, height);
    pixman_bool_t ret = pixman_region32_intersect(dest, src, &r);
    pixman_region32_fini(&r);
    return ret;
}

pixman_bool_t pixman_region32_subtract(pixman_region32_t* dest,
    const pixman_region32_t* r1, const pixman_region32_t* r2)
{
    (void)r2;
    return pixman_region32_copy(dest, r1);
}

pixman_bool_t pixman_region32_contains_point(const pixman_region32_t* region,
    int x, int y, pixman_box32_t* box)
{
    if (x >= region->extents.x1 && x < region->extents.x2 &&
        y >= region->extents.y1 && y < region->extents.y2) {
        if (box) *box = region->extents;
        return 1;
    }
    return 0;
}

pixman_bool_t pixman_region32_equal(const pixman_region32_t* r1,
    const pixman_region32_t* r2)
{
    return (r1->extents.x1 == r2->extents.x1 &&
            r1->extents.y1 == r2->extents.y1 &&
            r1->extents.x2 == r2->extents.x2 &&
            r1->extents.y2 == r2->extents.y2) ? 1 : 0;
}

/* ---------- format support ---------- */

int pixman_format_supported_destination(pixman_format_code_t format) {
    return (format == PIXMAN_a8r8g8b8 || format == PIXMAN_x8r8g8b8 ||
            format == PIXMAN_a8b8g8r8 || format == PIXMAN_x8b8g8r8) ? 1 : 0;
}

int pixman_format_supported_source(pixman_format_code_t format) {
    return pixman_format_supported_destination(format);
}
