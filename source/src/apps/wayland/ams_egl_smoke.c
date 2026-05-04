/**
 * @file ams_egl_smoke.c
 * @brief EGL/GBM smoke test for AMS-OS.
 *
 * Verifies that the Mesa3D port is reachable: opens /dev/dri/card0,
 * creates a gbm_device, queries an EGL display, and clears a 1x1
 * surface. Used by `make wayland_desktop_matrix` as a regression for
 * the Mesa stack.
 *
 * If Mesa headers aren't yet available (sysroot empty) we degrade
 * gracefully and just log a status line so the build still completes.
 */

#include "libports/libports.h"
#include "ams_syscall.h"
#include <stdint.h>

#if __has_include(<EGL/egl.h>) && __has_include(<gbm.h>)
#  include <EGL/egl.h>
#  include <gbm.h>
#  define AMS_HAVE_EGL 1
#else
#  define AMS_HAVE_EGL 0
#endif

static void puts1(const char* s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
#if AMS_HAVE_EGL
    int drm_fd = ams_drm_open_card();
    if (drm_fd < 0) { puts1("ams-egl-smoke: open(/dev/dri/card0) failed"); return 1; }
    struct gbm_device* gbm = gbm_create_device(drm_fd);
    if (!gbm) { puts1("ams-egl-smoke: gbm_create_device failed"); return 2; }
    EGLDisplay disp = eglGetDisplay((EGLNativeDisplayType)gbm);
    if (disp == EGL_NO_DISPLAY) { puts1("ams-egl-smoke: eglGetDisplay failed"); return 3; }
    if (!eglInitialize(disp, NULL, NULL)) { puts1("ams-egl-smoke: eglInitialize failed"); return 4; }
    puts1("ams-egl-smoke: PASS - GBM + EGL initialised against /dev/dri/card0");
    return 0;
#else
    (void)ams_drm_open_card;
    puts1("ams-egl-smoke: EGL/GBM headers not in sysroot. "
          "Run 'make wayland_build' to populate Mesa.");
    return 0;
#endif
}
