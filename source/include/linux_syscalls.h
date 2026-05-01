#pragma once

#define SYS_READ         0
#define SYS_WRITE        1
#define SYS_OPEN         2
#define SYS_CLOSE        3
#define SYS_STAT         4
#define SYS_FSTAT        5
#define SYS_POLL         7
#define SYS_LSEEK        8
#define SYS_MMAP         9
#define SYS_MPROTECT     10
#define SYS_MUNMAP       11
#define SYS_BRK          12
#define SYS_RT_SIGACTION 13
#define SYS_RT_SIGPROCMASK 14
#define SYS_IOCTL        16
#define SYS_PREAD64      17
#define SYS_PWRITE64     18
#define SYS_GETPEERNAME  52
#define SYS_GETSOCKNAME  51
#define SYS_SHUTDOWN     48
#define SYS_FTRUNCATE    77
#define SYS_READV        19
#define SYS_WRITEV       20
#define SYS_ACCESS       21
#define SYS_MREMAP       25
#define SYS_MADVISE      28
#define SYS_GETTIMEOFDAY 96
#define SYS_CLONE        56
#define SYS_DUP          32
#define SYS_DUP2         33
#define SYS_NANOSLEEP    35
#define SYS_FCNTL        72
#define SYS_READLINK     89
#define SYS_GETCWD       79
#define SYS_GETPID       39
#define SYS_WAIT4        61
#define SYS_SOCKET       41
#define SYS_CONNECT      42
#define SYS_ACCEPT       43
#define SYS_SENDMSG      46
#define SYS_RECVMSG      47
#define SYS_BIND         49
#define SYS_LISTEN       50
#define SYS_UNAME        63
#define SYS_EXIT         60
#define SYS_GETDENTS64   217
#define SYS_CLOCK_GETTIME 228
#define SYS_ARCH_PRCTL   158 // Kluczowe dla mlibc (ustawianie FS/GS)
#define SYS_OPENAT       257
#define SYS_NEWFSTATAT   262
#define SYS_FACCESSAT    269
#define SYS_PPOLL        271
#define SYS_SET_ROBUST_LIST 273
#define SYS_GET_ROBUST_LIST 274
#define SYS_PIPE2        293
#define SYS_MEMFD_CREATE 319
#define SYS_PRLIMIT64    302
#define SYS_GETRANDOM    318
#define SYS_EXEC         10 // Nasz własny syscall do execve (nie mylić z SYS_MPROTECT)


/*
 //syscalle dla implementacji mlibc (TCC)
        syscall_table[SYS_EXECVE]   = sys_exec;
        syscall_table[SYS_FORK]     = [](registers* r) -> uint64_t { return -1; }; // Nie obsługujemy fork()
        //set_tid_address, gettid, getppid, getuid, getgid, geteuid, getegid, etc. mogą być dodane później w razie potrzeby
        syscall_table[SYS_SET_TID_ADDRESS] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETTID] = [](registers* r) -> uint64_t { return 1; };
        syscall_table[SYS_GETPPID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETUID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETGID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETEUID] = [](registers* r) -> uint64_t { return 0; };
        syscall_table[SYS_GETEGID] = [](registers* r) -> uint64_t { return 0; };

*/
#define SYS_EXECVE       59
#define SYS_FORK         57
#define SYS_SET_TID_ADDRESS 100
#define SYS_SIGALTSTACK  131
#define SYS_GETTID       186
#define SYS_FUTEX        202
#define SYS_GETPPID      110
#define SYS_GETUID       102
#define SYS_GETGID       104
#define SYS_GETEUID      107
#define SYS_GETEGID      108
#define SYS_EPOLL_WAIT   232
#define SYS_EPOLL_CTL    233
#define SYS_TGKILL       234
#define SYS_EXIT_GROUP   231
#define SYS_READLINKAT   267
#define SYS_EVENTFD2     290
#define SYS_ACCEPT4      288
#define SYS_EPOLL_CREATE1 291
#define SYS_STATX        332
#define SYS_AMS_GET_MOUSE_EVENT 453
