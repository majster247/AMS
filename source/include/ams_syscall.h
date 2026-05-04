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
// AMS custom: skopiuj bufor RGBA user-space do framebuffera
#define SYS_AMS_FB_BLIT 450
// AMS custom: pobierz jeden klawisz z kolejki (0 = brak)
#define SYS_AMS_GET_KEY 451
// AMS custom: pobierz aktualną rozdzielczość framebuffera (w,h)
#define SYS_AMS_GET_FB_INFO 452
// AMS custom: pobierz zdarzenie myszy (0 = brak)
#define SYS_AMS_GET_MOUSE_EVENT 453

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void* addr, size_t length);
int sys_exec(const char* path, int argc, char** argv);
int ioctl(int fd, unsigned long request, void* arg);
int shm_open(const char* name, int oflag, unsigned int mode);
int shm_unlink(const char* name);
int memfd_create(const char* name, unsigned int flags);
int ftruncate(int fd, long length);

#ifdef __cplusplus
}
#endif

#endif