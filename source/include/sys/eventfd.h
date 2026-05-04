#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H

#ifdef __cplusplus
extern "C" {
#endif

#define EFD_SEMAPHORE 00000001
#define EFD_CLOEXEC   02000000
#define EFD_NONBLOCK  00004000

int eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}
#endif

#endif
