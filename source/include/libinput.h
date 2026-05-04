#ifndef _LIBINPUT_H
#define _LIBINPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum libinput_event_type {
    LIBINPUT_EVENT_NONE = 0,
    LIBINPUT_EVENT_DEVICE_ADDED = 1,
    LIBINPUT_EVENT_DEVICE_REMOVED = 2,
    LIBINPUT_EVENT_KEYBOARD_KEY = 300,
    LIBINPUT_EVENT_POINTER_MOTION = 400,
    LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE = 401,
    LIBINPUT_EVENT_POINTER_BUTTON = 402,
    LIBINPUT_EVENT_POINTER_AXIS = 403,
};

enum libinput_key_state {
    LIBINPUT_KEY_STATE_RELEASED = 0,
    LIBINPUT_KEY_STATE_PRESSED = 1,
};

enum libinput_button_state {
    LIBINPUT_BUTTON_STATE_RELEASED = 0,
    LIBINPUT_BUTTON_STATE_PRESSED = 1,
};

enum libinput_device_capability {
    LIBINPUT_DEVICE_CAP_KEYBOARD = 0,
    LIBINPUT_DEVICE_CAP_POINTER = 1,
    LIBINPUT_DEVICE_CAP_TOUCH = 2,
    LIBINPUT_DEVICE_CAP_TABLET_TOOL = 3,
    LIBINPUT_DEVICE_CAP_TABLET_PAD = 4,
    LIBINPUT_DEVICE_CAP_GESTURE = 5,
    LIBINPUT_DEVICE_CAP_SWITCH = 6,
};

struct libinput;
struct libinput_device;
struct libinput_event;
struct libinput_event_keyboard;
struct libinput_event_pointer;

struct libinput_interface {
    int (*open_restricted)(const char* path, int flags, void* user_data);
    void (*close_restricted)(int fd, void* user_data);
};

/* Context */
struct libinput* libinput_path_create_context(
    const struct libinput_interface* interface, void* user_data);
struct libinput_device* libinput_path_add_device(
    struct libinput* li, const char* path);
void libinput_path_remove_device(struct libinput_device* device);
struct libinput* libinput_ref(struct libinput* li);
struct libinput* libinput_unref(struct libinput* li);

/* Event loop */
int libinput_get_fd(struct libinput* li);
int libinput_dispatch(struct libinput* li);
struct libinput_event* libinput_get_event(struct libinput* li);

/* Event accessors */
enum libinput_event_type libinput_event_get_type(struct libinput_event* event);
struct libinput_device* libinput_event_get_device(struct libinput_event* event);
void libinput_event_destroy(struct libinput_event* event);

/* Device */
const char* libinput_device_get_name(struct libinput_device* device);
int libinput_device_has_capability(struct libinput_device* device,
    enum libinput_device_capability cap);
unsigned int libinput_device_get_id_vendor(struct libinput_device* device);
unsigned int libinput_device_get_id_product(struct libinput_device* device);

/* Keyboard events */
struct libinput_event_keyboard* libinput_event_get_keyboard_event(
    struct libinput_event* event);
uint32_t libinput_event_keyboard_get_key(struct libinput_event_keyboard* event);
enum libinput_key_state libinput_event_keyboard_get_key_state(
    struct libinput_event_keyboard* event);
uint32_t libinput_event_keyboard_get_time(struct libinput_event_keyboard* event);

/* Pointer events */
struct libinput_event_pointer* libinput_event_get_pointer_event(
    struct libinput_event* event);
double libinput_event_pointer_get_dx(struct libinput_event_pointer* event);
double libinput_event_pointer_get_dy(struct libinput_event_pointer* event);
uint32_t libinput_event_pointer_get_button(struct libinput_event_pointer* event);
enum libinput_button_state libinput_event_pointer_get_button_state(
    struct libinput_event_pointer* event);
uint32_t libinput_event_pointer_get_time(struct libinput_event_pointer* event);

#ifdef __cplusplus
}
#endif

#endif
