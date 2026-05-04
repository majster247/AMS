/**
 * @file sys/un.h
 * @brief POSIX Unix domain socket address for AMS-OS
 */

#ifndef _SYS_UN_H
#define _SYS_UN_H

#include <sys/socket.h>

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

#endif /* _SYS_UN_H */
