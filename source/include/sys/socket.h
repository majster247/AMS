#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

#define AF_UNIX 1

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define SOL_SOCKET 1
#define SCM_RIGHTS 1

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct iovec {
    void* iov_base;
    size_t iov_len;
};

struct msghdr {
    void* msg_name;
    uint32_t msg_namelen;
    uint32_t __pad0;
    struct iovec* msg_iov;
    uint64_t msg_iovlen;
    void* msg_control;
    uint64_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct cmsghdr {
    uint64_t cmsg_len;
    int32_t cmsg_level;
    int32_t cmsg_type;
};

#ifdef __cplusplus
extern "C" {
#endif

int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
int sendmsg(int sockfd, const struct msghdr* msg, int flags);
int recvmsg(int sockfd, struct msghdr* msg, int flags);
int shutdown(int sockfd, int how);

#ifdef __cplusplus
}
#endif

#endif
