/*
 * AMS DRM KMS skeleton - single CRTC, single connector, single mode.
 *
 * The "mode" reported is whatever the AMS framebuffer says it is. Page
 * flip ioctls are no-ops (we always present immediately via the
 * compositor blit path), but they return success so libdrm-using
 * clients don't trip up.
 */

#include <stdint.h>
#include <stddef.h>
#include "drm/drm.h"
#include "drm/drm_mode.h"
#include "drm/drm_fourcc.h"

extern "C" int  ams_drm_get_card_size(int *w, int *h);
extern "C" void write_serial_string(const char* s);

namespace ams_drm {

static int sw_memcpy(void *dst, const void *src, size_t n) {
    auto d = (uint8_t*)dst;
    auto s = (const uint8_t*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return 0;
}

extern "C" int ams_drm_kms_get_resources(struct drm_mode_card_res *res) {
    if (!res) return -1;
    res->count_fbs        = 1;
    res->count_crtcs      = 1;
    res->count_connectors = 1;
    res->count_encoders   = 1;
    int w = 1280, h = 720;
    ams_drm_get_card_size(&w, &h);
    res->min_width  = (uint32_t)w; res->max_width  = (uint32_t)w;
    res->min_height = (uint32_t)h; res->max_height = (uint32_t)h;
    return 0;
}

extern "C" int ams_drm_kms_get_connector(struct drm_mode_get_connector *c) {
    if (!c) return -1;
    int w = 1280, h = 720;
    ams_drm_get_card_size(&w, &h);
    c->encoder_id = 1;
    c->connector_id = 1;
    c->connector_type = 1; /* HDMI-A in Linux UAPI is 11; 1 = unknown/virtual */
    c->connector_type_id = 0;
    c->connection = 1; /* connected */
    c->mm_width  = (uint32_t)(w * 254 / 96 / 10);
    c->mm_height = (uint32_t)(h * 254 / 96 / 10);
    c->subpixel = 0;
    c->count_modes = 1;
    if (c->modes_ptr) {
        struct drm_mode_modeinfo m;
        for (size_t i = 0; i < sizeof(m); ++i) ((uint8_t*)&m)[i] = 0;
        m.clock = 60000;
        m.hdisplay = (uint16_t)w;
        m.vdisplay = (uint16_t)h;
        m.htotal = (uint16_t)w;
        m.vtotal = (uint16_t)h;
        m.vrefresh = 60;
        m.type = 0x40 /* preferred */;
        const char nm[] = "ams-fb";
        for (size_t i = 0; i < sizeof(nm); ++i) m.name[i] = nm[i];
        sw_memcpy((void*)(uintptr_t)c->modes_ptr, &m, sizeof(m));
    }
    return 0;
}

extern "C" int ams_drm_kms_set_crtc(struct drm_mode_crtc *crtc) {
    if (!crtc) return -1;
    write_serial_string("[DRM] kms_set_crtc (no-op, software FB)\n");
    return 0;
}

extern "C" int ams_drm_kms_page_flip(uint32_t crtc_id, uint32_t fb_id, uint32_t flags) {
    (void)crtc_id; (void)fb_id; (void)flags;
    return 0;
}

}
