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

int ams_drm_cap_dumb_buffer(struct ams_drm_get_cap cap) {
    return (int)cap.value == 1;
}




int main(void) {
    puts1("wayland-egl-smoke: start (DRM dumb path)");
    int fd = (int)ams_syscall(SYS_OPEN, (uint64_t)"/dev/dri/card0", (uint64_t)O_RDWR, 0, 0, 0);
    if (fd < 0) {
        puts1("wayland-egl-smoke: open /dev/dri/card0 failed");
        return 1;
    }
    struct ams_drm_version ver = {0};
    (void)ams_syscall(16, fd, (uint64_t)AMS_DRM_IOCTL_VERSION, (uint64_t)&ver, 0, 0);
    struct ams_drm_get_cap cap = {AMS_DRM_CAP_DUMB_BUFFER, 0};
    (void)ams_syscall(16, fd, (uint64_t)AMS_DRM_IOCTL_GET_CAP, (uint64_t)&cap, 0, 0);
    if (!ams_drm_cap_dumb_buffer(cap)) {
        puts1("wayland-egl-smoke: DRM_CAP_DUMB_BUFFER missing");
        return 2;
    }
    struct ams_drm_mode_create_dumb create = {64, 64, 32, 0, 0, 0, 0};
    (void)ams_syscall(16, fd, (uint64_t)AMS_DRM_IOCTL_MODE_CREATE_DUMB, (uint64_t)&create, 0, 0);
    if (create.handle == 0 || create.pitch == 0 || create.size == 0) {
        puts1("wayland-egl-smoke: MODE_CREATE_DUMB failed");
        return 3;
    }
    void* p = mmap(0, (size_t)create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (long)create.pitch);
    if (p == MAP_FAILED) {
        puts1("wayland-egl-smoke: mmap failed");
        return 4;
    }
    volatile uint32_t* px = (volatile uint32_t*)p;
    px[0] = 0xFF00FF00u;
    (void)ams_syscall(SYS_MUNMAP, (uint64_t)p, (uint64_t)create.size, 0, 0, 0);
    struct ams_drm_mode_destroy_dumb destroy = {create.handle};
    (void)ams_syscall(16, fd, (uint64_t)AMS_DRM_IOCTL_MODE_DESTROY_DUMB, (uint64_t)&destroy, 0, 0);
    (void)ams_syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
    puts1("wayland-egl-smoke: PASS (DRM dumb path)");
    return 0;
}