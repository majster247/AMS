#ifndef _ERRNO_H
#define _ERRNO_H
extern "C" int* __errno_location();
#define errno (*__errno_location())
#define EINTR  4
#define EAGAIN 11
#define ENOMEM 12
#define EINVAL 22
#endif