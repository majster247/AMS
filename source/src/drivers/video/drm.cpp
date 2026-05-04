#include "drm.h"
#include "graphics.h"
#include "kernel.h"

extern "C" void* k_memcpy(void* dest, const void* src, size_t count);

static ams_drm_device g_drm_device;

void drm_init() {
    g_drm_device.initialized = 1;
    g_drm_device.gem_next_handle = 1;
    g_drm_device.ttm_pool_enabled = 1;

    g_drm_device.crtc.id = 1;
    g_drm_device.crtc.mode.width = fb.width;
    g_drm_device.crtc.mode.height = fb.height;
    g_drm_device.crtc.mode.refresh_hz = 60;

    g_drm_device.connector.id = 1;
    g_drm_device.connector.connected = 1;
    g_drm_device.connector.preferred_mode = g_drm_device.crtc.mode;

    g_drm_device.scanout.handle = g_drm_device.gem_next_handle++;
    g_drm_device.scanout.width = fb.width;
    g_drm_device.scanout.height = fb.height;
    g_drm_device.scanout.stride = fb.pitch;
    g_drm_device.scanout.size = fb.pitch * fb.height;
    g_drm_device.scanout.kind = AMS_DRM_BUFFER_SCANOUT;
    g_drm_device.scanout.phys = fb.address;
    g_drm_device.scanout.cpu_map = backbuffer;
    g_drm_device.crtc.primary = &g_drm_device.scanout;

    write_serial_string("[DRM] bootstrap device initialized\n");
    write_serial_string("[DRM] KMS/GEM/TTM skeleton enabled on top of framebuffer path\n");
}

ams_drm_device* drm_get_device() {
    return &g_drm_device;
}

ams_gem_buffer* drm_gem_create_dumb(uint32_t width, uint32_t height, uint32_t kind) {
    static ams_gem_buffer dumb_buffer;

    dumb_buffer.handle = g_drm_device.gem_next_handle++;
    dumb_buffer.width = width;
    dumb_buffer.height = height;
    dumb_buffer.stride = width * 4;
    dumb_buffer.size = dumb_buffer.stride * height;
    dumb_buffer.kind = kind;
    dumb_buffer.phys = 0;
    dumb_buffer.cpu_map = backbuffer;
    return &dumb_buffer;
}

void drm_present_backbuffer(const uint32_t* src, uint32_t width, uint32_t height) {
    if (!src || !backbuffer) return;
    if (width > fb.width || height > fb.height) return;

    for (uint32_t y = 0; y < height; ++y) {
        k_memcpy(backbuffer + y * fb.width, src + y * width, width * sizeof(uint32_t));
    }
    graphics_flip();
}
