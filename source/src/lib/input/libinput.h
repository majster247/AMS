/* AMS libinput stub — reads /dev/input/event{0,1} and presents
 * the libinput API surface needed by wlroots.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types */
struct libinput;
struct libinput_device;
struct libinput_event;

/* Event types */
typedef enum {
    LIBINPUT_EVENT_NONE = 0,
    LIBINPUT_EVENT_DEVICE_ADDED,
    LIBINPUT_EVENT_DEVICE_REMOVED,
    LIBINPUT_EVENT_KEYBOARD_KEY,
    LIBINPUT_EVENT_POINTER_MOTION,
    LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE,
    LIBINPUT_EVENT_POINTER_BUTTON,
    LIBINPUT_EVENT_POINTER_AXIS,
    LIBINPUT_EVENT_TOUCH_DOWN,
    LIBINPUT_EVENT_TOUCH_UP,
    LIBINPUT_EVENT_TOUCH_MOTION,
    LIBINPUT_EVENT_TOUCH_CANCEL,
    LIBINPUT_EVENT_TOUCH_FRAME,
} libinput_event_type;

typedef enum {
    LIBINPUT_KEY_STATE_RELEASED = 0,
    LIBINPUT_KEY_STATE_PRESSED  = 1,
} libinput_key_state;

typedef enum {
    LIBINPUT_BUTTON_STATE_RELEASED = 0,
    LIBINPUT_BUTTON_STATE_PRESSED  = 1,
} libinput_button_state;

/* Interface passed to libinput_create_path_context */
struct libinput_interface {
    int  (*open_restricted)(const char* path, int flags, void* user_data);
    void (*close_restricted)(int fd, void* user_data);
};

/* ---- context ---- */
struct libinput* libinput_path_create_context(const struct libinput_interface* iface, void* user_data);
struct libinput* libinput_ref(struct libinput* li);
struct libinput* libinput_unref(struct libinput* li);
int              libinput_get_fd(struct libinput* li);
int              libinput_dispatch(struct libinput* li);

/* ---- device ---- */
struct libinput_device* libinput_path_add_device(struct libinput* li, const char* path);
struct libinput_device* libinput_device_ref(struct libinput_device* device);
struct libinput_device* libinput_device_unref(struct libinput_device* device);
const char*             libinput_device_get_name(struct libinput_device* device);

/* ---- event queue ---- */
struct libinput_event*   libinput_get_event(struct libinput* li);
libinput_event_type      libinput_event_get_type(struct libinput_event* event);
struct libinput_device*  libinput_event_get_device(struct libinput_event* event);
void                     libinput_event_destroy(struct libinput_event* event);

/* ---- keyboard events ---- */
struct libinput_event_keyboard* libinput_event_get_keyboard_event(struct libinput_event* event);
uint32_t         libinput_event_keyboard_get_key(struct libinput_event_keyboard* event);
libinput_key_state libinput_event_keyboard_get_key_state(struct libinput_event_keyboard* event);
uint64_t         libinput_event_keyboard_get_time_usec(struct libinput_event_keyboard* event);

/* ---- pointer motion events ---- */
struct libinput_event_pointer* libinput_event_get_pointer_event(struct libinput_event* event);
double           libinput_event_pointer_get_dx(struct libinput_event_pointer* event);
double           libinput_event_pointer_get_dy(struct libinput_event_pointer* event);
uint32_t         libinput_event_pointer_get_button(struct libinput_event_pointer* event);
libinput_button_state libinput_event_pointer_get_button_state(struct libinput_event_pointer* event);
uint64_t         libinput_event_pointer_get_time_usec(struct libinput_event_pointer* event);

#ifdef __cplusplus
}
#endif
