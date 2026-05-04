/*
 * Minimal DRM UAPI shim for AMS.
 *
 * Only the structures and ioctl numbers used by libdrm/Mesa swrast on
 * a software-rendered, single-output system are declared here. Layout
 * matches Linux's <drm/drm.h> bytewise so that libdrm built from
 * upstream sees the same ABI.
 */

#ifndef AMS_DRM_H
#define AMS_DRM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_NAME            "ams_drm"
#define DRM_DIR_NAME        "/dev/dri"
#define DRM_PRIMARY_MINOR   0

struct drm_version {
    int       version_major;
    int       version_minor;
    int       version_patchlevel;
    size_t    name_len;
    char     *name;
    size_t    date_len;
    char     *date;
    size_t    desc_len;
    char     *desc;
};

struct drm_unique {
    size_t  unique_len;
    char   *unique;
};

#define DRM_IOCTL_BASE          'd'
#define DRM_IO(nr)              ((DRM_IOCTL_BASE << 8) | (nr))

#define DRM_IOCTL_VERSION       DRM_IO(0x00)
#define DRM_IOCTL_GET_UNIQUE    DRM_IO(0x01)
#define DRM_IOCTL_GET_MAGIC     DRM_IO(0x02)
#define DRM_IOCTL_AUTH_MAGIC    DRM_IO(0x11)
#define DRM_IOCTL_GEM_CLOSE     DRM_IO(0x09)
#define DRM_IOCTL_GEM_FLINK     DRM_IO(0x0a)
#define DRM_IOCTL_GEM_OPEN      DRM_IO(0x0b)

#ifdef __cplusplus
}
#endif

#endif
