/*
 * Minimal libinput ABI shim for AMS.
 * Subset compatible with libinput 1.24 used by wlroots' libinput backend.
 */

#ifndef AMS_LIBINPUT_H
#define AMS_LIBINPUT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libinput              libinput;
typedef struct libinput_device       libinput_device;
typedef struct libinput_event        libinput_event;
typedef struct libinput_event_pointer  libinput_event_pointer;
typedef struct libinput_event_keyboard libinput_event_keyboard;

enum libinput_event_type {
    LIBINPUT_EVENT_NONE                 = 0,
    LIBINPUT_EVENT_DEVICE_ADDED         = 1,
    LIBINPUT_EVENT_DEVICE_REMOVED       = 2,
    LIBINPUT_EVENT_KEYBOARD_KEY         = 300,
    LIBINPUT_EVENT_POINTER_MOTION       = 400,
    LIBINPUT_EVENT_POINTER_BUTTON       = 402,
    LIBINPUT_EVENT_POINTER_AXIS         = 403
};

enum libinput_key_state {
    LIBINPUT_KEY_STATE_RELEASED = 0,
    LIBINPUT_KEY_STATE_PRESSED  = 1
};

enum libinput_button_state {
    LIBINPUT_BUTTON_STATE_RELEASED = 0,
    LIBINPUT_BUTTON_STATE_PRESSED  = 1
};

struct libinput_interface {
    int  (*open_restricted)(const char *path, int flags, void *user_data);
    void (*close_restricted)(int fd, void *user_data);
};

libinput      *libinput_path_create_context(const struct libinput_interface *iface, void *user_data);
void           libinput_unref(libinput *li);
int            libinput_get_fd(libinput *li);
int            libinput_dispatch(libinput *li);
libinput_event *libinput_get_event(libinput *li);
void           libinput_event_destroy(libinput_event *ev);

enum libinput_event_type libinput_event_get_type(libinput_event *ev);

libinput_event_pointer  *libinput_event_get_pointer_event(libinput_event *ev);
libinput_event_keyboard *libinput_event_get_keyboard_event(libinput_event *ev);

double  libinput_event_pointer_get_dx(libinput_event_pointer *ev);
double  libinput_event_pointer_get_dy(libinput_event_pointer *ev);
uint32_t libinput_event_pointer_get_button(libinput_event_pointer *ev);
enum libinput_button_state libinput_event_pointer_get_button_state(libinput_event_pointer *ev);

uint32_t libinput_event_keyboard_get_key(libinput_event_keyboard *ev);
enum libinput_key_state libinput_event_keyboard_get_key_state(libinput_event_keyboard *ev);

libinput_device *libinput_path_add_device(libinput *li, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* AMS_LIBINPUT_H */
