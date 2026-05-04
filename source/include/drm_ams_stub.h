/*
 * DRM ioctl numbers match Linux x86_64 (verified via host preprocessor).
 * Struct layouts match Linux drm.h / drm_mode.h for x86_64.
 */
#define AMS_DRM_IOCTL_VERSION         0xc0406400ul
#define AMS_DRM_IOCTL_GET_CAP         0xc010640cul
#define AMS_DRM_IOCTL_MODE_CREATE_DUMB 0xc02064b2ul
#define AMS_DRM_IOCTL_MODE_MAP_DUMB    0xc01064b3ul
#define AMS_DRM_IOCTL_MODE_DESTROY_DUMB 0xc00464b4ul

#define AMS_DRM_CAP_DUMB_BUFFER 0x1ull

#define AMS_DRM_MMAP_BASE 0x300000000ull

struct ams_drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    unsigned long name_len;
    char* name;
    unsigned long date_len;
    char* date;
    unsigned long desc_len;
    char* desc;
};

struct ams_drm_get_cap {
    unsigned long long capability;
    unsigned long long value;
};

struct ams_drm_mode_create_dumb {
    unsigned int height;
    unsigned int width;
    unsigned int bpp;
    unsigned int flags;
    unsigned int handle;
    unsigned int pitch;
    unsigned long long size;
};

struct ams_drm_mode_map_dumb {
    unsigned int handle;
    unsigned int pad;
    unsigned long long offset;
};

struct ams_drm_mode_destroy_dumb {
    unsigned int handle;
};
