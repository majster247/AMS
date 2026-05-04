#ifndef _AMS_SYSCALL_H
#define _AMS_SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>

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

// Dodajmy też mmap, bo TCC go szuka w syscallach
void* mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void* addr, size_t length);
int sys_exec(const char* path, int argc, char** argv);
int poll(struct pollfd* fds, unsigned long nfds, int timeout);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
int shutdown(int sockfd, int how);
int shm_open(const char* name, int oflag, unsigned int mode);
int shm_unlink(const char* name);
int eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}
#endif

#endif