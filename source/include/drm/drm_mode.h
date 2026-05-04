/**
 * @file drm/drm_mode.h
 * @brief Additional DRM mode-setting constants re-exported for libdrm compat.
 */
#ifndef _AMS_DRM_MODE_H
#define _AMS_DRM_MODE_H

#include "drm.h"

#define DRM_MODE_TYPE_PREFERRED  (1 << 3)
#define DRM_MODE_TYPE_DRIVER     (1 << 6)
#define DRM_MODE_FLAG_PHSYNC     (1 << 0)
#define DRM_MODE_FLAG_NHSYNC     (1 << 1)
#define DRM_MODE_FLAG_PVSYNC     (1 << 2)
#define DRM_MODE_FLAG_NVSYNC     (1 << 3)

#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#define DRM_MODE_PAGE_FLIP_ASYNC 0x02

#define DRM_MODE_PROP_RANGE      (1 << 1)
#define DRM_MODE_PROP_ENUM       (1 << 3)
#define DRM_MODE_PROP_BLOB       (1 << 4)
#define DRM_MODE_PROP_BITMASK    (1 << 5)
#define DRM_MODE_PROP_IMMUTABLE  (1 << 6)
#define DRM_MODE_PROP_ATOMIC     (1 << 31)

#endif /* _AMS_DRM_MODE_H */
