#ifndef _AMS_WLR_OUTPUT_H
#define _AMS_WLR_OUTPUT_H

#include <stdint.h>
#include "wayland/wayland-server.h"

struct wlr_output_mode {
    int32_t width, height;
    int32_t refresh;
    int preferred;
    struct wl_list link;
};

struct wlr_output {
    char name[24];
    int32_t width, height;
    int32_t phys_width, phys_height;
    struct wl_list modes;
    struct wlr_output_mode* current_mode;
    struct wl_signal frame;
    struct wl_signal destroy;
};

struct wlr_output_layout;

struct wlr_output_layout* wlr_output_layout_create(void);
void wlr_output_layout_destroy(struct wlr_output_layout* layout);
void wlr_output_layout_add_auto(struct wlr_output_layout* layout, struct wlr_output* output);

void wlr_output_create_global(struct wlr_output* output);
int wlr_output_commit(struct wlr_output* output);
void wlr_output_set_mode(struct wlr_output* output, struct wlr_output_mode* mode);

#endif
