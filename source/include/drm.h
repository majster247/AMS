#ifndef AMS_DRM_H
#define AMS_DRM_H

#include <stdint.h>
#include <stddef.h>

enum ams_drm_buffer_kind {
    AMS_DRM_BUFFER_SCANOUT = 1,
    AMS_DRM_BUFFER_CURSOR = 2,
    AMS_DRM_BUFFER_GENERIC = 3,
};

struct ams_gem_buffer {
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t kind;
    uint32_t reserved;
    uint64_t phys;
    void* cpu_map;
};

struct ams_kms_mode {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_hz;
};

struct ams_kms_crtc {
    uint32_t id;
    ams_kms_mode mode;
    ams_gem_buffer* primary;
};

struct ams_kms_connector {
    uint32_t id;
    uint32_t connected;
    ams_kms_mode preferred_mode;
};

struct ams_drm_device {
    uint32_t initialized;
    uint32_t gem_next_handle;
    uint32_t ttm_pool_enabled;
    ams_kms_crtc crtc;
    ams_kms_connector connector;
    ams_gem_buffer scanout;
};

#ifdef __cplusplus
extern "C" {
#endif

void drm_init();
ams_drm_device* drm_get_device();
ams_gem_buffer* drm_gem_create_dumb(uint32_t width, uint32_t height, uint32_t kind);
void drm_present_backbuffer(const uint32_t* src, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif
