/**
 * @file EGL/egl.h
 * @brief EGL interface definitions for AMS-OS Mesa3D port
 *
 * Minimal EGL header providing the types and constants needed
 * by wlroots and Wayland EGL clients. Full implementation
 * is provided by the Mesa3D port.
 */

#ifndef __egl_h_
#define __egl_h_

#include <stdint.h>

typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef int32_t EGLint;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLClientBuffer;
typedef void *EGLImage;
typedef void *EGLSync;
typedef uint64_t EGLTime;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);

#define EGL_FALSE                   0
#define EGL_TRUE                    1

#define EGL_DEFAULT_DISPLAY         ((EGLDisplay)0)
#define EGL_NO_DISPLAY              ((EGLDisplay)0)
#define EGL_NO_CONTEXT              ((EGLContext)0)
#define EGL_NO_SURFACE              ((EGLSurface)0)
#define EGL_NO_IMAGE                ((EGLImage)0)

#define EGL_SUCCESS                 0x3000
#define EGL_NOT_INITIALIZED         0x3001
#define EGL_BAD_ACCESS              0x3002
#define EGL_BAD_ALLOC               0x3003
#define EGL_BAD_ATTRIBUTE           0x3004
#define EGL_BAD_CONFIG              0x3005
#define EGL_BAD_CONTEXT             0x3006
#define EGL_BAD_CURRENT_SURFACE     0x3007
#define EGL_BAD_DISPLAY             0x3008
#define EGL_BAD_MATCH               0x3009
#define EGL_BAD_NATIVE_PIXMAP       0x300A
#define EGL_BAD_NATIVE_WINDOW       0x300B
#define EGL_BAD_PARAMETER           0x300C
#define EGL_BAD_SURFACE             0x300D
#define EGL_CONTEXT_LOST            0x300E

#define EGL_BUFFER_SIZE             0x3020
#define EGL_ALPHA_SIZE              0x3021
#define EGL_BLUE_SIZE               0x3022
#define EGL_GREEN_SIZE              0x3023
#define EGL_RED_SIZE                0x3024
#define EGL_DEPTH_SIZE              0x3025
#define EGL_STENCIL_SIZE            0x3026
#define EGL_CONFIG_CAVEAT           0x3027
#define EGL_CONFIG_ID               0x3028
#define EGL_RENDERABLE_TYPE         0x3040
#define EGL_SURFACE_TYPE            0x3033
#define EGL_NONE                    0x3038

#define EGL_OPENGL_ES2_BIT          0x0004
#define EGL_OPENGL_BIT              0x0008
#define EGL_WINDOW_BIT              0x0004
#define EGL_PBUFFER_BIT             0x0001
#define EGL_PIXMAP_BIT              0x0002

#define EGL_OPENGL_ES_API           0x30A0
#define EGL_OPENGL_API              0x30A2
#define EGL_CONTEXT_CLIENT_VERSION  0x3098

#define EGL_PLATFORM_GBM_KHR       0x31D7
#define EGL_PLATFORM_WAYLAND_KHR   0x31D8

/* Core EGL functions (implemented by Mesa) */
EGLDisplay eglGetDisplay(void *native_display);
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean eglTerminate(EGLDisplay dpy);
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size,
                            EGLint *num_config);
EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                             EGLContext share_context,
                             const EGLint *attrib_list);
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                   void *native_window,
                                   const EGLint *attrib_list);
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                           EGLSurface read, EGLContext ctx);
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLint eglGetError(void);
const char *eglQueryString(EGLDisplay dpy, EGLint name);
EGLBoolean eglBindAPI(EGLenum api);

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname);

/* EGL_KHR_platform_base */
EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native_display,
                                  const EGLint *attrib_list);

#endif /* __egl_h_ */
