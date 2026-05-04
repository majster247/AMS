/**
 * Software EGL stubs for AMS.
 *
 * Provides the minimum EGL API surface so wlroots GLES2 renderer
 * can probe and fall back to pixman gracefully.
 * For actual rendering, the pixman renderer path should be used.
 */

#include <stdint.h>
#include <stddef.h>

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;
typedef unsigned int EGLenum;
typedef int32_t EGLint;
typedef unsigned int EGLBoolean;

#define EGL_FALSE           0
#define EGL_TRUE            1
#define EGL_NO_DISPLAY      ((EGLDisplay)0)
#define EGL_NO_CONTEXT      ((EGLContext)0)
#define EGL_NO_SURFACE      ((EGLSurface)0)
#define EGL_SUCCESS         0x3000
#define EGL_NOT_INITIALIZED 0x3001
#define EGL_BAD_DISPLAY     0x3008

static EGLint g_egl_error = EGL_SUCCESS;

static struct {
    int initialized;
    void* native_display;
} g_egl_display;

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id) {
    g_egl_display.native_display = display_id;
    return (EGLDisplay)&g_egl_display;
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform,
    void* native_display, const EGLint* attrib_list)
{
    (void)platform; (void)attrib_list;
    g_egl_display.native_display = native_display;
    return (EGLDisplay)&g_egl_display;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
    (void)dpy;
    if (major) *major = 1;
    if (minor) *minor = 4;
    g_egl_display.initialized = 1;
    return EGL_TRUE;
}

EGLBoolean eglTerminate(EGLDisplay dpy) {
    (void)dpy;
    g_egl_display.initialized = 0;
    return EGL_TRUE;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
    EGLConfig* configs, EGLint config_size, EGLint* num_config)
{
    (void)dpy; (void)attrib_list;
    if (configs && config_size > 0) configs[0] = (EGLConfig)1;
    if (num_config) *num_config = 1;
    return EGL_TRUE;
}

static int g_egl_ctx_dummy = 1;

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
    EGLContext share_context, const EGLint* attrib_list)
{
    (void)dpy; (void)config; (void)share_context; (void)attrib_list;
    return (EGLContext)&g_egl_ctx_dummy;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    (void)dpy; (void)ctx;
    return EGL_TRUE;
}

static int g_egl_surf_dummy = 1;

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
    EGLNativeWindowType win, const EGLint* attrib_list)
{
    (void)dpy; (void)config; (void)win; (void)attrib_list;
    return (EGLSurface)&g_egl_surf_dummy;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    (void)dpy; (void)surface;
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
    EGLSurface read, EGLContext ctx)
{
    (void)dpy; (void)draw; (void)read; (void)ctx;
    return EGL_TRUE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    (void)dpy; (void)surface;
    return EGL_TRUE;
}

EGLBoolean eglBindAPI(EGLenum api) {
    (void)api;
    return EGL_TRUE;
}

EGLint eglGetError(void) {
    EGLint err = g_egl_error;
    g_egl_error = EGL_SUCCESS;
    return err;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name) {
    (void)dpy;
    switch (name) {
        case 0x3053: return ""; /* EGL_EXTENSIONS */
        case 0x3054: return "AMS Software"; /* EGL_VENDOR */
        case 0x3098: return "1.4"; /* EGL_VERSION */
        default: return "";
    }
}

void* eglGetProcAddress(const char* procname) {
    (void)procname;
    return (void*)0;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    (void)dpy; (void)interval;
    return EGL_TRUE;
}
