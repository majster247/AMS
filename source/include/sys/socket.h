#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AF_UNIX 1

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 04000
#define SOCK_CLOEXEC 02000000

#define SOL_SOCKET 1
#define SCM_RIGHTS 1

struct iovec {
    void* iov_base;
    size_t iov_len;
};

struct msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    struct iovec* msg_iov;
    size_t msg_iovlen;
    void* msg_control;
    size_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int sockfd, const void* addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, void* addr, unsigned int* addrlen);
int accept4(int sockfd, void* addr, unsigned int* addrlen, int flags);
int connect(int sockfd, const void* addr, unsigned int addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);
int shutdown(int sockfd, int how);
int getsockname(int sockfd, void* addr, unsigned int* addrlen);
int getpeername(int sockfd, void* addr, unsigned int* addrlen);

#ifdef __cplusplus
}
#endif

#endif
