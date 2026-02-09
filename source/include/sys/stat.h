#ifndef _SYS_STAT_H
#define _SYS_STAT_H
struct stat {
    uint32_t st_mode;
    uint64_t st_size;
};
#endif