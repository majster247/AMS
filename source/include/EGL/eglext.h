/**
 * @file EGL/eglext.h
 * @brief EGL extension definitions for AMS-OS Mesa3D port
 */

#ifndef __eglext_h_
#define __eglext_h_

#include <EGL/egl.h>

/* EGL_KHR_image_base */
#define EGL_KHR_image_base 1
#define EGL_IMAGE_PRESERVED_KHR     0x30D2
typedef EGLImage (EGLImageKHR);

/* EGL_EXT_image_dma_buf_import */
#define EGL_EXT_image_dma_buf_import 1
#define EGL_LINUX_DMA_BUF_EXT       0x3270
#define EGL_LINUX_DRM_FOURCC_EXT    0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT  0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT 0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT 0x3274

/* EGL_KHR_no_config_context */
#define EGL_KHR_no_config_context 1

/* EGL_KHR_surfaceless_context */
#define EGL_KHR_surfaceless_context 1

/* EGL_MESA_platform_gbm */
#define EGL_MESA_platform_gbm 1
#define EGL_PLATFORM_GBM_MESA       0x31D7

/* EGL_WL_bind_wayland_display */
#define EGL_WL_bind_wayland_display 1
#define EGL_WAYLAND_BUFFER_WL       0x31D5
#define EGL_WAYLAND_PLANE_WL        0x31D6
#define EGL_WAYLAND_Y_INVERTED_WL   0x31DB

#endif /* __eglext_h_ */
