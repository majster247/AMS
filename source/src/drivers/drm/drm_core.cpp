/*
 * AMS DRM core - kernel-side skeleton.
 *
 * Registers a single virtual DRM card (card0) backed by the AMS
 * framebuffer. Not yet wired into VFS as /dev/dri/card0 because the
 * AMS VFS is flat and read-only (ext2/initrd); this module exposes a
 * helper API for sys_ioctl in src/arch/x86_64/syscall.cpp to forward
 * DRM_IOCTL_* calls into.
 *
 * Subsystems:
 *   - drm_card (this file)        : registry, open/close, version.
 *   - drm_kms.cpp                  : connectors, CRTCs, mode set, page flip.
 *   - drm_gem.cpp                  : GEM objects (PMM-backed buffers).
 *
 * The split mirrors Linux's drivers/gpu/drm so a future port can copy
 * upstream files into src/drivers/drm/ with minimal glue.
 */

#include <stdint.h>
#include <stddef.h>
#include "drm/drm.h"

extern "C" void write_serial_string(const char* s);

namespace ams_drm {

struct drm_card {
    int      id;
    int      width;
    int      height;
    uint32_t format;
};

static drm_card g_card0 = { 0, 1280, 720, 0 };

extern "C" void ams_drm_init(int fb_w, int fb_h) {
    g_card0.id = 0;
    g_card0.width  = (fb_w  > 0) ? fb_w  : 1280;
    g_card0.height = (fb_h > 0) ? fb_h : 720;
    write_serial_string("[DRM] ams_drm card0 registered (software framebuffer)\n");
}

extern "C" int ams_drm_get_card_size(int *w, int *h) {
    if (w) *w = g_card0.width;
    if (h) *h = g_card0.height;
    return 0;
}

extern "C" int ams_drm_ioctl_version(struct drm_version *v) {
    if (!v) return -1;
    v->version_major = 1;
    v->version_minor = 4;
    v->version_patchlevel = 0;
    static const char dname[] = "ams_drm";
    static const char ddate[] = "AMS-1";
    static const char ddesc[] = "AMS software DRM (FB-backed)";
    if (v->name && v->name_len >= sizeof(dname)) {
        for (size_t i = 0; i < sizeof(dname); ++i) v->name[i] = dname[i];
        v->name_len = sizeof(dname) - 1;
    }
    if (v->date && v->date_len >= sizeof(ddate)) {
        for (size_t i = 0; i < sizeof(ddate); ++i) v->date[i] = ddate[i];
        v->date_len = sizeof(ddate) - 1;
    }
    if (v->desc && v->desc_len >= sizeof(ddesc)) {
        for (size_t i = 0; i < sizeof(ddesc); ++i) v->desc[i] = ddesc[i];
        v->desc_len = sizeof(ddesc) - 1;
    }
    return 0;
}

}
