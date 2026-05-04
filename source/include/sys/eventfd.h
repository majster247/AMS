#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t eventfd_t;

int eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}
#endif

#endif
