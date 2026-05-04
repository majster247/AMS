#ifndef _EGL_H
#define _EGL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;
typedef unsigned int EGLenum;
typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef void* EGLClientBuffer;

#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_NO_DISPLAY      ((EGLDisplay)0)
#define EGL_NO_CONTEXT      ((EGLContext)0)
#define EGL_NO_SURFACE      ((EGLSurface)0)

#define EGL_FALSE 0
#define EGL_TRUE  1

#define EGL_SUCCESS             0x3000
#define EGL_NOT_INITIALIZED     0x3001
#define EGL_BAD_DISPLAY         0x3008
#define EGL_BAD_CONFIG          0x3005
#define EGL_BAD_CONTEXT         0x3006

#define EGL_NONE                0x3038
#define EGL_OPENGL_ES_API       0x30A0
#define EGL_OPENGL_ES2_BIT      0x0004
#define EGL_OPENGL_ES3_BIT      0x0040
#define EGL_RENDERABLE_TYPE     0x3040
#define EGL_RED_SIZE            0x3024
#define EGL_GREEN_SIZE          0x3023
#define EGL_BLUE_SIZE           0x3022
#define EGL_ALPHA_SIZE          0x3021
#define EGL_SURFACE_TYPE        0x3033
#define EGL_WINDOW_BIT          0x0004
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB

#define EGL_PLATFORM_GBM_KHR   0x31D7

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id);
EGLDisplay eglGetPlatformDisplay(EGLenum platform,
    void* native_display, const EGLint* attrib_list);
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor);
EGLBoolean eglTerminate(EGLDisplay dpy);
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
    EGLConfig* configs, EGLint config_size, EGLint* num_config);
EGLContext  eglCreateContext(EGLDisplay dpy, EGLConfig config,
    EGLContext share_context, const EGLint* attrib_list);
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
    EGLNativeWindowType win, const EGLint* attrib_list);
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
    EGLSurface read, EGLContext ctx);
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglBindAPI(EGLenum api);
EGLint     eglGetError(void);
const char* eglQueryString(EGLDisplay dpy, EGLint name);
void* eglGetProcAddress(const char* procname);
EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval);

#ifdef __cplusplus
}
#endif

#endif
