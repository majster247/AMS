#ifndef _AMS_CAIRO_H
#define _AMS_CAIRO_H

#include <stdint.h>

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_pattern cairo_pattern_t;

typedef enum {
    CAIRO_FORMAT_INVALID  = -1,
    CAIRO_FORMAT_ARGB32   = 0,
    CAIRO_FORMAT_RGB24    = 1,
    CAIRO_FORMAT_A8       = 2,
    CAIRO_FORMAT_A1       = 3,
    CAIRO_FORMAT_RGB16_565 = 4,
} cairo_format_t;

typedef enum {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_NULL_POINTER,
} cairo_status_t;

typedef enum {
    CAIRO_OPERATOR_CLEAR,
    CAIRO_OPERATOR_SOURCE,
    CAIRO_OPERATOR_OVER,
} cairo_operator_t;

cairo_surface_t* cairo_image_surface_create(cairo_format_t format, int width, int height);
cairo_surface_t* cairo_image_surface_create_for_data(unsigned char* data, cairo_format_t format, int width, int height, int stride);
void cairo_surface_destroy(cairo_surface_t* surface);
unsigned char* cairo_image_surface_get_data(cairo_surface_t* surface);
int cairo_image_surface_get_width(cairo_surface_t* surface);
int cairo_image_surface_get_height(cairo_surface_t* surface);
int cairo_image_surface_get_stride(cairo_surface_t* surface);
int cairo_format_stride_for_width(cairo_format_t format, int width);
void cairo_surface_flush(cairo_surface_t* surface);
void cairo_surface_mark_dirty(cairo_surface_t* surface);

cairo_t* cairo_create(cairo_surface_t* target);
void cairo_destroy(cairo_t* cr);

void cairo_set_source_rgb(cairo_t* cr, double red, double green, double blue);
void cairo_set_source_rgba(cairo_t* cr, double red, double green, double blue, double alpha);
void cairo_set_operator(cairo_t* cr, cairo_operator_t op);
void cairo_paint(cairo_t* cr);
void cairo_fill(cairo_t* cr);
void cairo_stroke(cairo_t* cr);

void cairo_rectangle(cairo_t* cr, double x, double y, double width, double height);
void cairo_move_to(cairo_t* cr, double x, double y);
void cairo_line_to(cairo_t* cr, double x, double y);
void cairo_set_line_width(cairo_t* cr, double width);

void cairo_save(cairo_t* cr);
void cairo_restore(cairo_t* cr);

#endif
