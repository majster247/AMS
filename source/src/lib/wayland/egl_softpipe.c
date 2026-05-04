/*
 * AMS software-EGL backend.
 *
 * Provides eglInitialize/eglCreateContext/eglMakeCurrent/eglSwapBuffers
 * as a thin wrapper that draws into an in-memory ARGB buffer and pushes
 * that buffer to the AMS framebuffer via SYS_AMS_FB_BLIT. There is no
 * real GL pipeline yet - clients are expected to use the surface bits
 * pointer directly (à la wl_shm), which is identical to what they do
 * today with our pixman shim. This unblocks porting EGL-using upstream
 * code with zero GPU.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include "ams_syscall.h"

struct egl_display {
    int      initialized;
    int      width, height;
    uint32_t *bits;
};

struct egl_context { int dummy; };
struct egl_surface { struct egl_display *dpy; };

static struct egl_display g_dpy;
static struct egl_context g_ctx;
static struct egl_surface g_surf;

EGLDisplay eglGetDisplay(EGLNativeDisplayType native) { (void)native; return (EGLDisplay)&g_dpy; }

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    struct egl_display *d = (struct egl_display*)dpy;
    if (!d) return EGL_FALSE;
    uint32_t w = 1280, h = 720;
    ams_syscall(452 /*SYS_AMS_GET_FB_INFO*/, (uint64_t)&w, (uint64_t)&h, 0, 0, 0);
    d->width = (int)w; d->height = (int)h;
    d->bits = (uint32_t*)calloc(1, (size_t)w * (size_t)h * 4u);
    d->initialized = 1;
    if (major) *major = 1;
    if (minor) *minor = 5;
    return EGL_TRUE;
}

EGLBoolean eglTerminate(EGLDisplay dpy) {
    struct egl_display *d = (struct egl_display*)dpy;
    if (!d) return EGL_FALSE;
    if (d->bits) free(d->bits);
    d->bits = NULL;
    d->initialized = 0;
    return EGL_TRUE;
}

const char *eglQueryString(EGLDisplay dpy, EGLint name) {
    (void)dpy;
    switch (name) {
        case EGL_VERSION:    return "1.5 AMS soft";
        case EGL_VENDOR:     return "AMS-OS";
        case EGL_EXTENSIONS: return "";
        default:             return "";
    }
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib, EGLConfig *configs, EGLint cap, EGLint *num) {
    (void)dpy; (void)attrib;
    if (configs && cap > 0) configs[0] = (EGLConfig)1;
    if (num) *num = (cap > 0) ? 1 : 0;
    return EGL_TRUE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig cfg, EGLContext share, const EGLint *attrib) {
    (void)dpy; (void)cfg; (void)share; (void)attrib;
    return (EGLContext)&g_ctx;
}
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext c) { (void)dpy; (void)c; return EGL_TRUE; }

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig cfg, EGLNativeWindowType win, const EGLint *attrib) {
    (void)cfg; (void)win; (void)attrib;
    g_surf.dpy = (struct egl_display*)dpy;
    return (EGLSurface)&g_surf;
}
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface s) { (void)dpy; (void)s; return EGL_TRUE; }

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface d, EGLSurface r, EGLContext c) {
    (void)dpy; (void)d; (void)r; (void)c;
    return EGL_TRUE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    struct egl_display *d = (struct egl_display*)dpy;
    if (!d || !d->bits) return EGL_FALSE;
    ams_syscall(450 /*SYS_AMS_FB_BLIT*/, (uint64_t)d->bits, (uint64_t)d->width, (uint64_t)d->height, 0, 0);
    (void)surface;
    return EGL_TRUE;
}
