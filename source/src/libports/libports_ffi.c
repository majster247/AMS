/**
 * @file libports_ffi.c
 * @brief libffi adaptation glue for AMS-OS.
 *
 * libffi compiles cleanly on a freestanding x86_64 toolchain provided
 * three things:
 *   - getauxval(AT_PAGESZ) returning a sane page size.
 *   - mmap(PROT_EXEC|PROT_WRITE)-capable memory for closures.
 *   - a sysconf-equivalent that returns the page size.
 *
 * We supply these here so that the upstream libffi (built unmodified
 * from external/wayland-stack/libffi) can be linked. libwayland in turn
 * uses libffi to dispatch interface methods.
 *
 * Note: AMS-OS' mmap currently always grants RWX (see sys_mmap) so the
 * closures can be written once and immediately invoked. mprotect is
 * accepted as a no-op by the kernel.
 */
#include "libports/libports.h"
#include "ams_syscall.h"
#include "linux_syscalls.h"

#define AMS_AT_PAGESZ 6

unsigned long ams_ffi_getauxval(unsigned long type) {
    if (type == AMS_AT_PAGESZ) return 4096UL;
    return 0UL;
}

void* ams_ffi_get_auxval(unsigned long type) {
    return (void*)ams_ffi_getauxval(type);
}

unsigned long getauxval(unsigned long type)
    __attribute__((weak, alias("ams_ffi_getauxval")));

long sysconf(int name) {
    /* _SC_PAGESIZE = 30. */
    if (name == 30) return 4096;
    return -1;
}
