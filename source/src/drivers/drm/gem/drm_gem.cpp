/*
 * AMS DRM GEM skeleton.
 *
 * GEM objects are kernel-pinned, kmalloc()-backed buffers (mlibc/Mesa
 * swrast doesn't need DMA). Each handle is a small index into a global
 * table; they can be mmap'd into userspace via our regular sys_mmap by
 * way of an internal "gem fd" pseudo-descriptor.
 */

#include <stdint.h>
#include <stddef.h>

extern "C" void* kmalloc(size_t size);
extern "C" void  kfree(void* ptr);
extern "C" void  k_memset(void* dst, int v, size_t n);
extern "C" void  write_serial_string(const char* s);

namespace ams_drm {

#define GEM_MAX 64

struct gem_object {
    int       in_use;
    size_t    size;
    void     *cpu;
};

static gem_object g_pool[GEM_MAX];

extern "C" int ams_gem_create(size_t size, uint32_t *handle_out) {
    if (!handle_out || size == 0) return -1;
    for (int i = 1; i < GEM_MAX; ++i) {
        if (!g_pool[i].in_use) {
            g_pool[i].cpu = kmalloc(size);
            if (!g_pool[i].cpu) return -2;
            k_memset(g_pool[i].cpu, 0, size);
            g_pool[i].size = size;
            g_pool[i].in_use = 1;
            *handle_out = (uint32_t)i;
            return 0;
        }
    }
    return -3;
}

extern "C" int ams_gem_close(uint32_t handle) {
    if (handle == 0 || handle >= GEM_MAX) return -1;
    if (!g_pool[handle].in_use) return -1;
    if (g_pool[handle].cpu) kfree(g_pool[handle].cpu);
    g_pool[handle].cpu = 0;
    g_pool[handle].size = 0;
    g_pool[handle].in_use = 0;
    return 0;
}

extern "C" void *ams_gem_cpu_ptr(uint32_t handle, size_t *size_out) {
    if (handle == 0 || handle >= GEM_MAX || !g_pool[handle].in_use) return 0;
    if (size_out) *size_out = g_pool[handle].size;
    return g_pool[handle].cpu;
}

}
