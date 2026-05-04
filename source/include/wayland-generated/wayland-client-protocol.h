/* Pre-generated Wayland client protocol header (vendored snapshot).
 * Regenerate with: bash source/tools/wayland_scan.sh
 * Generated from wayland.xml (Wayland 1.22)
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- wire message header ---- */
struct wl_message {
    const char* name;
    const char* signature;
    const struct wl_interface** types;
};

struct wl_interface {
    const char* name;
    int version;
    int method_count;
    const struct wl_message* methods;
    int event_count;
    const struct wl_message* events;
};

/* ---- object ---- */
struct wl_object {
    const struct wl_interface* interface;
    const void* implementation;
    uint32_t id;
};

/* ---- proxy ---- */
struct wl_proxy;
struct wl_display;
struct wl_event_queue;

/* ---- listener ---- */
struct wl_listener {
    void (*notify)(struct wl_listener* listener, void* data);
    struct { struct wl_listener *prev, *next; } link;
};

struct wl_signal {
    struct { struct wl_listener *prev, *next; } listener_list;
};

/* ---- wl_array ---- */
struct wl_array {
    size_t  size;
    size_t  alloc;
    void*   data;
};

/* ---- fixed-point ---- */
typedef int32_t wl_fixed_t;
static inline double wl_fixed_to_double(wl_fixed_t f) { return (double)f / 256.0; }
static inline wl_fixed_t wl_fixed_from_double(double d) { return (wl_fixed_t)(d * 256.0); }
static inline int wl_fixed_to_int(wl_fixed_t f) { return f >> 8; }
static inline wl_fixed_t wl_fixed_from_int(int i) { return i << 8; }

/* ---- display ---- */
struct wl_display* wl_display_connect(const char* name);
struct wl_display* wl_display_connect_to_fd(int fd);
void               wl_display_disconnect(struct wl_display* display);
int                wl_display_get_fd(struct wl_display* display);
int                wl_display_dispatch(struct wl_display* display);
int                wl_display_dispatch_pending(struct wl_display* display);
int                wl_display_roundtrip(struct wl_display* display);
int                wl_display_flush(struct wl_display* display);
uint32_t           wl_display_get_error(struct wl_display* display);

/* ---- proxy ---- */
struct wl_proxy* wl_proxy_create(struct wl_proxy* factory, const struct wl_interface* interface);
void             wl_proxy_destroy(struct wl_proxy* proxy);
int              wl_proxy_add_listener(struct wl_proxy* proxy, void (**implementation)(void), void* data);
void             wl_proxy_set_user_data(struct wl_proxy* proxy, void* user_data);
void*            wl_proxy_get_user_data(struct wl_proxy* proxy);
uint32_t         wl_proxy_get_version(struct wl_proxy* proxy);
uint32_t         wl_proxy_get_id(struct wl_proxy* proxy);
const char*      wl_proxy_get_class(struct wl_proxy* proxy);
void             wl_proxy_marshal(struct wl_proxy* proxy, uint32_t opcode, ...);
struct wl_proxy* wl_proxy_marshal_constructor(struct wl_proxy* proxy, uint32_t opcode,
                                              const struct wl_interface* interface, ...);
struct wl_proxy* wl_proxy_marshal_constructor_versioned(struct wl_proxy* proxy, uint32_t opcode,
                                                        const struct wl_interface* interface,
                                                        uint32_t version, ...);

/* ---- registry ---- */
extern const struct wl_interface wl_registry_interface;
struct wl_registry;

typedef struct wl_registry_listener {
    void (*global)(void* data, struct wl_registry* registry, uint32_t name,
                   const char* interface, uint32_t version);
    void (*global_remove)(void* data, struct wl_registry* registry, uint32_t name);
} wl_registry_listener;

int wl_registry_add_listener(struct wl_registry* r, const wl_registry_listener* l, void* data);
void* wl_registry_bind(struct wl_registry* registry, uint32_t name,
                       const struct wl_interface* interface, uint32_t version);
void wl_registry_destroy(struct wl_registry* registry);
struct wl_registry* wl_display_get_registry(struct wl_display* display);

/* ---- wl_compositor ---- */
extern const struct wl_interface wl_compositor_interface;
struct wl_compositor;
struct wl_surface* wl_compositor_create_surface(struct wl_compositor* compositor);
struct wl_region*  wl_compositor_create_region(struct wl_compositor* compositor);
void wl_compositor_destroy(struct wl_compositor* compositor);

/* ---- wl_surface ---- */
extern const struct wl_interface wl_surface_interface;
struct wl_surface;
struct wl_buffer;
struct wl_callback;

typedef struct wl_surface_listener {
    void (*enter)(void* data, struct wl_surface* surface, struct wl_output* output);
    void (*leave)(void* data, struct wl_surface* surface, struct wl_output* output);
} wl_surface_listener;

int  wl_surface_add_listener(struct wl_surface* s, const wl_surface_listener* l, void* data);
void wl_surface_attach(struct wl_surface* surface, struct wl_buffer* buffer, int32_t x, int32_t y);
void wl_surface_damage(struct wl_surface* surface, int32_t x, int32_t y, int32_t w, int32_t h);
void wl_surface_damage_buffer(struct wl_surface* surface, int32_t x, int32_t y, int32_t w, int32_t h);
void wl_surface_commit(struct wl_surface* surface);
void wl_surface_destroy(struct wl_surface* surface);
void wl_surface_set_opaque_region(struct wl_surface* surface, struct wl_region* region);
void wl_surface_set_input_region(struct wl_surface* surface, struct wl_region* region);
struct wl_callback* wl_surface_frame(struct wl_surface* surface);

/* ---- wl_shm ---- */
extern const struct wl_interface wl_shm_interface;
struct wl_shm;
struct wl_shm_pool;

#define WL_SHM_FORMAT_ARGB8888 0
#define WL_SHM_FORMAT_XRGB8888 1
#define WL_SHM_FORMAT_ABGR8888 0x34324241
#define WL_SHM_FORMAT_XBGR8888 0x34324258

typedef struct wl_shm_listener {
    void (*format)(void* data, struct wl_shm* shm, uint32_t format);
} wl_shm_listener;

int  wl_shm_add_listener(struct wl_shm* shm, const wl_shm_listener* l, void* data);
struct wl_shm_pool* wl_shm_create_pool(struct wl_shm* shm, int fd, int32_t size);
void wl_shm_destroy(struct wl_shm* shm);

struct wl_buffer* wl_shm_pool_create_buffer(struct wl_shm_pool* pool,
                                             int32_t offset, int32_t width, int32_t height,
                                             int32_t stride, uint32_t format);
void wl_shm_pool_resize(struct wl_shm_pool* pool, int32_t size);
void wl_shm_pool_destroy(struct wl_shm_pool* pool);

/* ---- wl_buffer ---- */
extern const struct wl_interface wl_buffer_interface;

typedef struct wl_buffer_listener {
    void (*release)(void* data, struct wl_buffer* buffer);
} wl_buffer_listener;

int  wl_buffer_add_listener(struct wl_buffer* b, const wl_buffer_listener* l, void* data);
void wl_buffer_destroy(struct wl_buffer* buffer);

/* ---- wl_callback ---- */
extern const struct wl_interface wl_callback_interface;

typedef struct wl_callback_listener {
    void (*done)(void* data, struct wl_callback* callback, uint32_t serial);
} wl_callback_listener;

int  wl_callback_add_listener(struct wl_callback* c, const wl_callback_listener* l, void* data);
void wl_callback_destroy(struct wl_callback* callback);

/* ---- wl_seat ---- */
extern const struct wl_interface wl_seat_interface;
struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
struct wl_touch;

#define WL_SEAT_CAPABILITY_POINTER  1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_SEAT_CAPABILITY_TOUCH    4

typedef struct wl_seat_listener {
    void (*capabilities)(void* data, struct wl_seat* seat, uint32_t caps);
    void (*name)(void* data, struct wl_seat* seat, const char* name);
} wl_seat_listener;

int  wl_seat_add_listener(struct wl_seat* seat, const wl_seat_listener* l, void* data);
struct wl_keyboard* wl_seat_get_keyboard(struct wl_seat* seat);
struct wl_pointer*  wl_seat_get_pointer(struct wl_seat* seat);
void wl_seat_destroy(struct wl_seat* seat);

/* ---- wl_output ---- */
extern const struct wl_interface wl_output_interface;
struct wl_output;

typedef struct wl_output_listener {
    void (*geometry)(void* data, struct wl_output*, int32_t x, int32_t y,
                     int32_t pw, int32_t ph, int32_t subpixel, const char* make,
                     const char* model, int32_t transform);
    void (*mode)(void* data, struct wl_output*, uint32_t flags, int32_t width,
                 int32_t height, int32_t refresh);
    void (*done)(void* data, struct wl_output*);
    void (*scale)(void* data, struct wl_output*, int32_t factor);
} wl_output_listener;

int  wl_output_add_listener(struct wl_output* o, const wl_output_listener* l, void* data);
void wl_output_destroy(struct wl_output* output);

/* ---- wl_region ---- */
struct wl_region;
void wl_region_add(struct wl_region* region, int32_t x, int32_t y, int32_t w, int32_t h);
void wl_region_subtract(struct wl_region* region, int32_t x, int32_t y, int32_t w, int32_t h);
void wl_region_destroy(struct wl_region* region);

/* ---- wl_keyboard ---- */
extern const struct wl_interface wl_keyboard_interface;

#define WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP 0
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1    1
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEY_STATE_PRESSED  1

typedef struct wl_keyboard_listener {
    void (*keymap)(void* data, struct wl_keyboard*, uint32_t format, int fd, uint32_t size);
    void (*enter)(void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface*, struct wl_array* keys);
    void (*leave)(void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface*);
    void (*key)(void* data, struct wl_keyboard*, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    void (*modifiers)(void* data, struct wl_keyboard*, uint32_t serial,
                      uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
    void (*repeat_info)(void* data, struct wl_keyboard*, int32_t rate, int32_t delay);
} wl_keyboard_listener;

int  wl_keyboard_add_listener(struct wl_keyboard* k, const wl_keyboard_listener* l, void* data);
void wl_keyboard_destroy(struct wl_keyboard* keyboard);

/* ---- wl_pointer ---- */
extern const struct wl_interface wl_pointer_interface;

#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_BUTTON_STATE_PRESSED  1
#define WL_POINTER_AXIS_VERTICAL_SCROLL   0
#define WL_POINTER_AXIS_HORIZONTAL_SCROLL 1

typedef struct wl_pointer_listener {
    void (*enter)(void* data, struct wl_pointer*, uint32_t serial, struct wl_surface*,
                  wl_fixed_t sx, wl_fixed_t sy);
    void (*leave)(void* data, struct wl_pointer*, uint32_t serial, struct wl_surface*);
    void (*motion)(void* data, struct wl_pointer*, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    void (*button)(void* data, struct wl_pointer*, uint32_t serial, uint32_t time,
                   uint32_t button, uint32_t state);
    void (*axis)(void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value);
    void (*frame)(void* data, struct wl_pointer*);
    void (*axis_source)(void* data, struct wl_pointer*, uint32_t axis_source);
    void (*axis_stop)(void* data, struct wl_pointer*, uint32_t time, uint32_t axis);
    void (*axis_discrete)(void* data, struct wl_pointer*, uint32_t axis, int32_t discrete);
} wl_pointer_listener;

int  wl_pointer_add_listener(struct wl_pointer* p, const wl_pointer_listener* l, void* data);
void wl_pointer_set_cursor(struct wl_pointer* pointer, uint32_t serial,
                           struct wl_surface* surface, int32_t hotspot_x, int32_t hotspot_y);
void wl_pointer_destroy(struct wl_pointer* pointer);

#ifdef __cplusplus
}
#endif
