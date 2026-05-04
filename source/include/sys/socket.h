/**
 * @file sys/socket.h
 * @brief POSIX socket interface for AMS-OS
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t socklen_t;
typedef uint16_t sa_family_t;

#define AF_UNIX     1
#define AF_LOCAL    AF_UNIX
#define AF_INET     2

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC  0x80000

#define SOL_SOCKET  1
#define SO_REUSEADDR 2
#define SO_REUSEPORT 15

#define SCM_RIGHTS  1

#define MSG_DONTWAIT  0x40
#define MSG_NOSIGNAL  0x4000
#define MSG_CMSG_CLOEXEC 0x40000000

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct msghdr {
    void         *msg_name;
    socklen_t     msg_namelen;
    struct iovec *msg_iov;
    size_t        msg_iovlen;
    void         *msg_control;
    size_t        msg_controllen;
    int           msg_flags;
};

struct cmsghdr {
    size_t cmsg_len;
    int    cmsg_level;
    int    cmsg_type;
};

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_DATA(cmsg) ((unsigned char *)((cmsg) + 1))
#define CMSG_FIRSTHDR(mhdr) \
    ((mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? \
     (struct cmsghdr *)(mhdr)->msg_control : (struct cmsghdr *)0)
#define CMSG_NXTHDR(mhdr, cmsg) \
    (((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len) + \
      sizeof(struct cmsghdr)) > \
     (unsigned char *)(mhdr)->msg_control + (mhdr)->msg_controllen ? \
     (struct cmsghdr *)0 : \
     (struct cmsghdr *)((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len)))

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
int shutdown(int sockfd, int how);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H */
