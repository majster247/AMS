#ifndef _GBM_H
#define _GBM_H

#include <stdint.h>

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

#define GBM_BO_USE_SCANOUT   (1U << 0)
#define GBM_BO_USE_RENDERING (1U << 2)
#define GBM_FORMAT_XRGB8888  0x34325258U
#define GBM_FORMAT_ARGB8888  0x34325241U

#endif
