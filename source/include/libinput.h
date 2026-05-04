#ifndef _AMS_LIBINPUT_H
#define _AMS_LIBINPUT_H

#include <stdint.h>

struct libinput;
struct libinput_device;
struct libinput_event;
struct libinput_event_pointer;
struct libinput_event_keyboard;

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

struct libinput_interface {
    int (*open_restricted)(const char* path, int flags, void* user_data);
    void (*close_restricted)(int fd, void* user_data);
};

struct libinput* libinput_udev_create_context(const struct libinput_interface* interface,
                                               void* user_data, void* udev);
int libinput_udev_assign_seat(struct libinput* li, const char* seat_id);
void libinput_unref(struct libinput* li);
int libinput_get_fd(struct libinput* li);
int libinput_dispatch(struct libinput* li);
struct libinput_event* libinput_get_event(struct libinput* li);
enum libinput_event_type libinput_event_get_type(struct libinput_event* event);
void libinput_event_destroy(struct libinput_event* event);

struct libinput_event_pointer* libinput_event_get_pointer_event(struct libinput_event* event);
struct libinput_event_keyboard* libinput_event_get_keyboard_event(struct libinput_event* event);

double libinput_event_pointer_get_dx(struct libinput_event_pointer* event);
double libinput_event_pointer_get_dy(struct libinput_event_pointer* event);
uint32_t libinput_event_pointer_get_button(struct libinput_event_pointer* event);
enum libinput_button_state libinput_event_pointer_get_button_state(struct libinput_event_pointer* event);

uint32_t libinput_event_keyboard_get_key(struct libinput_event_keyboard* event);
enum libinput_key_state libinput_event_keyboard_get_key_state(struct libinput_event_keyboard* event);

#endif
