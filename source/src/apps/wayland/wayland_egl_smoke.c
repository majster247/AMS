#include "ams_syscall.h"
#include "drm_ams_stub.h"
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>

#define SYS_MUNMAP 11
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define O_RDWR 0x2

static void puts1(const char* s) {
    int n = 0;
    while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

int main(void) {
    puts1("wayland-egl-smoke: start (DRM dumb path)");
    int fd = (int)ams_syscall(SYS_OPEN, (uint64_t)"/dev/dri/card0", (uint64_t)O_RDWR, 0, 0, 0);
    if (fd < 0) {
        puts1("wayland-egl-smoke: open /dev/dri/card0 failed");
        return 1;
    }
    ams_drm_version ver = {0};
    int r = ioctl(fd, AMS_DRM_IOCTL_VERSION, &ver);
    if (r != 0) {
        puts1("wayland-egl-smoke: DRM_IOCTL_VERSION failed");
        return 2;
    }
    ams_drm_get_cap cap = {AMS_DRM_CAP_DUMB_BUFFER, 0};
    r = ioctl(fd, AMS_DRM_IOCTL_GET_CAP, &cap);
    if (r != 0 || cap.value != 1) {
        puts1("wayland-egl-smoke: DRM_CAP_DUMB_BUFFER missing");
        return 3;
    }
    ams_drm_mode_create_dumb create = {64, 64, 32, 0, 0, 0, 0};
    r = ioctl(fd, AMS_DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (r != 0 || create.handle == 0) {
        puts1("wayland-egl-smoke: CREATE_DUMB failed");
        return 4;
    }
    ams_drm_mode_map_dumb map = {create.handle, 0, 0};
    r = ioctl(fd, AMS_DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (r != 0 || map.offset == 0) {
        puts1("wayland-egl-smoke: MAP_DUMB failed");
        return 5;
    }
    void* p = mmap(0, (size_t)create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (long)map.offset);
    if ((intptr_t)(uintptr_t)p < 0) {
        puts1("wayland-egl-smoke: mmap GEM failed");
        return 6;
    }
    volatile uint32_t* px = (volatile uint32_t*)p;
    px[0] = 0xFF00FF00u;
    ams_syscall(SYS_MUNMAP, (uint64_t)p, (uint64_t)create.size, 0, 0, 0);
    ams_drm_mode_destroy_dumb destroy = {create.handle};
    (void)ioctl(fd, AMS_DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    ams_syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
    puts1("wayland-egl-smoke: PASS (DRM facade)");
    return 0;
}
