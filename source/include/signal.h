#ifndef _SIGNAL_H
#define _SIGNAL_H
typedef int sig_atomic_t;
#define SIGINT  2
#define SIGILL  4
#define SIGABRT 6
#define SIGFPE  8
#define SIGSEGV 11
#define SIGTERM 15

#define REG_RIP 0
#define REG_RBP 1

#endif