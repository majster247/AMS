#ifndef _FCNTL_H
#define _FCNTL_H
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   64
#define O_TRUNC   512
#define O_CLOEXEC 02000000
#define O_NONBLOCK 04000
#define O_BINARY  0 
#endif
#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  64
#define O_TRUNC  512
#define O_CLOEXEC 02000000
#define O_NONBLOCK 04000
#define O_BINARY 0
#endif
