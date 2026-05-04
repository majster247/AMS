#ifndef _AMS_SYSCALL_H
#define _AMS_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);

void print_char(char c);
void print_string(const char* str);
void exit_program(int code);
int get_key();
void draw_rect(int x, int y, int w, int h, int color);
void refresh_screen();

#define SYS_AMS_FB_BLIT 450
#define SYS_AMS_GET_KEY 451
#define SYS_AMS_GET_FB_INFO 452
#define SYS_AMS_GET_MOUSE_EVENT 453
#define SYS_AMS_DRM_IOCTL 460
#define SYS_AMS_DRM_OPEN 461
#define SYS_AMS_SHM_OPEN 462
#define SYS_AMS_SHM_UNLINK 463

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void* addr, size_t length);
int sys_exec(const char* path, int argc, char** argv);

/* POSIX shared memory wrappers */
int shm_open(const char* name, int oflag, int mode);
int shm_unlink(const char* name);

/* DRM wrappers */
int drm_open(void);
long drm_ioctl(int fd, unsigned long request, void* arg);

#ifdef __cplusplus
}
#endif

#endif