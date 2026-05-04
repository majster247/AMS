/* AMS userspace libdrm header — wraps /dev/dri/card0 ioctls. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "drm_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int drm_magic_t;

/* ---- open/close ---- */
int drmOpen(const char* name, const char* busid);
int drmClose(int fd);

/* ---- auth ---- */
int drmGetMagic(int fd, drm_magic_t* magic);
int drmAuthMagic(int fd, drm_magic_t magic);

/* ---- capabilities ---- */
int drmGetCap(int fd, uint64_t capability, uint64_t* value);
int drmSetClientCap(int fd, uint64_t capability, uint64_t value);

/* ---- mode resources ---- */
drmModeResPtr        drmModeGetResources(int fd);
void                 drmModeFreeResources(drmModeResPtr ptr);

drmModeConnectorPtr  drmModeGetConnector(int fd, uint32_t connector_id);
void                 drmModeFreeConnector(drmModeConnectorPtr ptr);

drmModeEncoderPtr    drmModeGetEncoder(int fd, uint32_t encoder_id);
void                 drmModeFreeEncoder(drmModeEncoderPtr ptr);

drmModeCrtcPtr       drmModeGetCrtc(int fd, uint32_t crtc_id);
int                  drmModeSetCrtc(int fd, uint32_t crtc_id, uint32_t fb_id,
                                    uint32_t x, uint32_t y,
                                    uint32_t* connectors, int count,
                                    drmModeModeInfoPtr mode);
void                 drmModeFreeCrtc(drmModeCrtcPtr ptr);

/* ---- framebuffers ---- */
int  drmModeAddFB(int fd, uint32_t width, uint32_t height,
                  uint8_t depth, uint8_t bpp, uint32_t pitch,
                  uint32_t bo_handle, uint32_t* buf_id);
int  drmModeRmFB(int fd, uint32_t fb_id);

/* ---- page flip ---- */
int  drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
                     uint32_t flags, void* user_data);

/* ---- dumb buffers ---- */
int  drmModeCreateDumbBuffer(int fd, uint32_t width, uint32_t height,
                             uint32_t bpp, uint32_t flags,
                             uint32_t* handle, uint32_t* pitch, uint64_t* size);
int  drmModeDestroyDumbBuffer(int fd, uint32_t handle);
int  drmModeMapDumbBuffer(int fd, uint32_t handle, uint64_t* offset);

/* ---- raw ioctl ---- */
int  drmIoctl(int fd, unsigned long request, void* arg);

/* ---- PRIME ---- */
int  drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int* prime_fd);
int  drmPrimeFDToHandle(int fd, int prime_fd, uint32_t* handle);

#ifdef __cplusplus
}
#endif
