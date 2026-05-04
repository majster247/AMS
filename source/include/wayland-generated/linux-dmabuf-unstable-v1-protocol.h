/* Pre-generated linux-dmabuf-unstable-v1 client protocol header (vendored).
 * Stub only — AMS does not implement dma-buf; wlroots falls back to shared-memory.
 */
#pragma once
#include "wayland-client-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct wl_interface zwp_linux_dmabuf_v1_interface;
extern const struct wl_interface zwp_linux_buffer_params_v1_interface;

struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

#define ZWP_LINUX_DMABUF_V1_FLAGS_Y_INVERT 1

typedef struct zwp_linux_dmabuf_v1_listener {
    void (*format)(void* data, struct zwp_linux_dmabuf_v1* dmabuf, uint32_t format);
    void (*modifier)(void* data, struct zwp_linux_dmabuf_v1* dmabuf,
                     uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo);
} zwp_linux_dmabuf_v1_listener;

int  zwp_linux_dmabuf_v1_add_listener(struct zwp_linux_dmabuf_v1* dmabuf,
                                       const zwp_linux_dmabuf_v1_listener* l, void* data);
void zwp_linux_dmabuf_v1_destroy(struct zwp_linux_dmabuf_v1* dmabuf);
struct zwp_linux_buffer_params_v1* zwp_linux_dmabuf_v1_create_params(struct zwp_linux_dmabuf_v1* dmabuf);

typedef struct zwp_linux_buffer_params_v1_listener {
    void (*created)(void* data, struct zwp_linux_buffer_params_v1* params, struct wl_buffer* buffer);
    void (*failed)(void* data, struct zwp_linux_buffer_params_v1* params);
} zwp_linux_buffer_params_v1_listener;

int  zwp_linux_buffer_params_v1_add_listener(struct zwp_linux_buffer_params_v1* params,
                                              const zwp_linux_buffer_params_v1_listener* l, void* data);
void zwp_linux_buffer_params_v1_destroy(struct zwp_linux_buffer_params_v1* params);
void zwp_linux_buffer_params_v1_add(struct zwp_linux_buffer_params_v1* params, int fd,
                                     uint32_t plane_idx, uint32_t offset, uint32_t stride,
                                     uint32_t modifier_hi, uint32_t modifier_lo);
void zwp_linux_buffer_params_v1_create(struct zwp_linux_buffer_params_v1* params,
                                        int32_t width, int32_t height, uint32_t format, uint32_t flags);
struct wl_buffer* zwp_linux_buffer_params_v1_create_immed(struct zwp_linux_buffer_params_v1* params,
                                                           int32_t width, int32_t height,
                                                           uint32_t format, uint32_t flags);

#ifdef __cplusplus
}
#endif
