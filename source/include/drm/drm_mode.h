#ifndef _AMS_DRM_MODE_H
#define _AMS_DRM_MODE_H

#include "drm.h"

#define DRM_MODE_TYPE_PREFERRED  (1 << 3)
#define DRM_MODE_TYPE_DRIVER    (1 << 6)

#define DRM_MODE_FLAG_PHSYNC    (1 << 0)
#define DRM_MODE_FLAG_NHSYNC    (1 << 1)
#define DRM_MODE_FLAG_PVSYNC    (1 << 2)
#define DRM_MODE_FLAG_NVSYNC    (1 << 3)
#define DRM_MODE_FLAG_INTERLACE (1 << 4)

#define DRM_MODE_SUBPIXEL_UNKNOWN       1
#define DRM_MODE_SUBPIXEL_HORIZONTAL_RGB 2

#endif
