#ifndef _SYS_UN_H
#define _SYS_UN_H

#include <stdint.h>

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

#endif
