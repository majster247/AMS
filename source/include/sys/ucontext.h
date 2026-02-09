#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H

#include <stdint.h>

typedef uint64_t greg_t;
typedef greg_t gregset_t[23];

typedef struct {
    gregset_t gregs;
} mcontext_t;

typedef struct ucontext_t {
    struct ucontext_t *uc_link;
    mcontext_t uc_mcontext;
} ucontext_t;

#endif
