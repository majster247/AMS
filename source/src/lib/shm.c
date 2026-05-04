/* shm_open / shm_unlink wrappers for AMS.
 * Maps to /dev/shm/<name> which the kernel's sys_open creates as a memfd node.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* AMS syscall helpers */
static long ams_open(const char* path, int flags, int mode) {
    long r;
    __asm__ volatile (
        "syscall"
        : "=a"(r)
        : "0"(2), "D"(path), "S"(flags), "d"(mode)
        : "rcx", "r11", "memory"
    );
    return r;
}

static long ams_unlink(const char* path) {
    /* SYS_UNLINK = 87 */
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(87),"D"(path):"rcx","r11","memory");
    return r;
}

int shm_open(const char* name, int oflag, unsigned int mode) {
    /* Construct /dev/shm/<name> */
    char path[128];
    const char* prefix = "/dev/shm/";
    int i = 0;
    while (prefix[i]) { path[i] = prefix[i]; i++; }
    /* skip leading '/' in name */
    const char* n = name;
    if (*n == '/') n++;
    int j = 0;
    while (n[j] && i + j < 126) { path[i + j] = n[j]; j++; }
    path[i + j] = '\0';
    return (int)ams_open(path, oflag, (int)mode);
}

int shm_unlink(const char* name) {
    char path[128];
    const char* prefix = "/dev/shm/";
    int i = 0;
    while (prefix[i]) { path[i] = prefix[i]; i++; }
    const char* n = name;
    if (*n == '/') n++;
    int j = 0;
    while (n[j] && i + j < 126) { path[i + j] = n[j]; j++; }
    path[i + j] = '\0';
    return (int)ams_unlink(path);
}
