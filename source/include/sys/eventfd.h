#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EFD_NONBLOCK 0x800
#define EFD_CLOEXEC  0x80000

int eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}
#endif

#endif
