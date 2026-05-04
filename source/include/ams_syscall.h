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

/* AMS custom syscalls */
#define SYS_AMS_FB_BLIT          450
#define SYS_AMS_GET_KEY          451
#define SYS_AMS_GET_FB_INFO      452
#define SYS_AMS_GET_MOUSE_EVENT  453
#define SYS_AMS_DRM_OPEN         460
#define SYS_AMS_DRM_IOCTL        461
#define SYS_AMS_SHM_OPEN         462
#define SYS_AMS_SHM_UNLINK       463
#define SYS_AMS_GBM_ALLOC        464

/* mmap / munmap */
void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int   munmap(void* addr, size_t length);

/* POSIX shared memory */
int   shm_open(const char* name, int oflag, unsigned int mode);
int   shm_unlink(const char* name);

/* poll */
struct pollfd_ams {
    int     fd;
    short   events;
    short   revents;
};
int   poll(struct pollfd_ams* fds, unsigned long nfds, int timeout);

/* exec */
int sys_exec(const char* path, int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif