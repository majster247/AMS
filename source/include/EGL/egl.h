/*
 * Minimal EGL 1.5 ABI shim for AMS - software-only.
 *
 * Implementation in src/lib/wayland/egl_softpipe.c uploads frames into
 * a wl_buffer via SHM.
 */

#ifndef AMS_EGL_H
#define AMS_EGL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int          EGLBoolean;
typedef int          EGLenum;
typedef void        *EGLDisplay;
typedef void        *EGLContext;
typedef void        *EGLSurface;
typedef void        *EGLConfig;
typedef void        *EGLNativeDisplayType;
typedef void        *EGLNativeWindowType;
typedef void        *EGLClientBuffer;
typedef int32_t      EGLint;

#define EGL_TRUE  1
#define EGL_FALSE 0

#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)

#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)

#define EGL_VERSION  0x3054
#define EGL_VENDOR   0x3053
#define EGL_EXTENSIONS 0x3055

EGLDisplay  eglGetDisplay(EGLNativeDisplayType native);
EGLBoolean  eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean  eglTerminate(EGLDisplay dpy);
const char *eglQueryString(EGLDisplay dpy, EGLint name);
EGLBoolean  eglChooseConfig(EGLDisplay dpy, const EGLint *attrib, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLContext  eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share, const EGLint *attrib);
EGLBoolean  eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLSurface  eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib);
EGLBoolean  eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLBoolean  eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
EGLBoolean  eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

#ifdef __cplusplus
}
#endif

#endif
