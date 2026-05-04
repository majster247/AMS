#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/types.h>
#include <stddef.h>

#define AF_UNIX 1

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

#define SOL_SOCKET 1
#define SCM_RIGHTS 1

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};

struct iovec {
    void* iov_base;
    size_t iov_len;
};

struct msghdr {
    void* msg_name;
    unsigned int msg_namelen;
    struct iovec* msg_iov;
    size_t msg_iovlen;
    void* msg_control;
    size_t msg_controllen;
    int msg_flags;
};

struct cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};

#ifdef __cplusplus
extern "C" {
#endif

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr* addr, unsigned int* addrlen);
int connect(int sockfd, const struct sockaddr* addr, unsigned int addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);

#ifdef __cplusplus
}
#endif

#endif
