#include "ams_syscall.h"
#include <ffi/ffi.h>
#include <stdint.h>

static void puts1(const char *s) {
    int n = 0; while (s[n]) ++n;
    ams_syscall(1, 1, (uint64_t)s, (uint64_t)n, 0, 0);
    ams_syscall(1, 1, (uint64_t)"\n", 1, 0, 0);
}

static int my_add(int a, int b) { return a + b; }

int main(void) {
    ffi_cif cif;
    ffi_type *args[2] = { &ffi_type_sint32, &ffi_type_sint32 };
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint32, args) != FFI_OK) {
        puts1("ffi_smoke: prep_cif fail");
        return 1;
    }
    int a = 7, b = 35, r = 0;
    void *vals[2] = { &a, &b };
    ffi_call(&cif, (void(*)(void))my_add, &r, vals);
    if (r != 42) { puts1("ffi_smoke: call mismatch"); return 2; }
    puts1("ffi_smoke: PASS");
    return 0;
}
