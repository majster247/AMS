#ifndef AMS_DRM_FOURCC_H
#define AMS_DRM_FOURCC_H

#define fourcc_code(a, b, c, d) \
    ((unsigned int)(a) | ((unsigned int)(b) << 8) | \
     ((unsigned int)(c) << 16) | ((unsigned int)(d) << 24))

#define DRM_FORMAT_ARGB8888  fourcc_code('A', 'R', '2', '4')
#define DRM_FORMAT_XRGB8888  fourcc_code('X', 'R', '2', '4')
#define DRM_FORMAT_RGB565    fourcc_code('R', 'G', '1', '6')
#define DRM_FORMAT_ABGR8888  fourcc_code('A', 'B', '2', '4')
#define DRM_FORMAT_XBGR8888  fourcc_code('X', 'B', '2', '4')

#endif
