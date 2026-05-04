/*
 * Minimal Cairo ABI shim for AMS, sitting on top of the AMS pixman shim.
 * Subset implemented:
 *  - cairo_image_surface_create / _data / _stride / _width / _height,
 *  - cairo_create / cairo_destroy,
 *  - cairo_set_source_rgba, cairo_set_source_rgb,
 *  - cairo_paint, cairo_fill, cairo_rectangle,
 *  - cairo_show_text (8x16 bitmap font),
 *  - cairo_status_t for ABI parity.
 */

#ifndef AMS_CAIRO_H
#define AMS_CAIRO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAIRO_STATUS_SUCCESS         = 0,
    CAIRO_STATUS_NO_MEMORY       = 1,
    CAIRO_STATUS_INVALID_FORMAT  = 8,
    CAIRO_STATUS_NULL_POINTER    = 9
} cairo_status_t;

typedef enum {
    CAIRO_FORMAT_INVALID = -1,
    CAIRO_FORMAT_ARGB32  = 0,
    CAIRO_FORMAT_RGB24   = 1,
    CAIRO_FORMAT_A8      = 2,
    CAIRO_FORMAT_RGB16_565 = 4
} cairo_format_t;

typedef struct cairo_surface cairo_surface_t;
typedef struct cairo cairo_t;

cairo_surface_t *cairo_image_surface_create(cairo_format_t fmt, int w, int h);
cairo_surface_t *cairo_image_surface_create_for_data(unsigned char *data,
                                                     cairo_format_t fmt,
                                                     int w, int h, int stride);
void             cairo_surface_destroy(cairo_surface_t *s);
unsigned char   *cairo_image_surface_get_data(cairo_surface_t *s);
int              cairo_image_surface_get_width(cairo_surface_t *s);
int              cairo_image_surface_get_height(cairo_surface_t *s);
int              cairo_image_surface_get_stride(cairo_surface_t *s);
cairo_format_t   cairo_image_surface_get_format(cairo_surface_t *s);

cairo_t *cairo_create(cairo_surface_t *target);
void     cairo_destroy(cairo_t *cr);
cairo_status_t cairo_status(cairo_t *cr);

void cairo_set_source_rgb(cairo_t *cr, double r, double g, double b);
void cairo_set_source_rgba(cairo_t *cr, double r, double g, double b, double a);

void cairo_rectangle(cairo_t *cr, double x, double y, double w, double h);
void cairo_paint(cairo_t *cr);
void cairo_fill(cairo_t *cr);
void cairo_move_to(cairo_t *cr, double x, double y);
void cairo_show_text(cairo_t *cr, const char *utf8);

#ifdef __cplusplus
}
#endif

#endif /* AMS_CAIRO_H */
