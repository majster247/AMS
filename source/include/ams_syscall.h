#ifndef _AMS_SYSCALL_H
#define _AMS_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

inline uint64_t ams_syscall(uint64_t sys_num, uint64_t p1, uint64_t p2, uint64_t p3);

void print_char(char c);
void print_string(const char* str);
void exit_program(int code);
int get_key();
void draw_rect(int x, int y, int w, int h, int color);
void refresh_screen();

// Dodajmy też mmap, bo TCC go szuka w syscallach
void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void* addr, size_t length);
int sys_exec(const char* path, int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif