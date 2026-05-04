/**
 * @file libffi/ffi.h
 * @brief Minimal libffi API for AMS-OS x86-64 (System V AMD64 ABI).
 *
 * Supports:
 *  - Scalar integer types (void, int8, int16, int32, int64, uint variants)
 *  - Floating-point types (float, double)
 *  - Pointer type
 *  - Structs (passed by value, up to 128 bytes)
 *  - ffi_call() with up to 16 arguments
 *  - ffi_closure_alloc() / ffi_prep_closure_loc() for libwayland dispatch
 */

#ifndef _AMS_FFI_H
#define _AMS_FFI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Type codes ---- */
#define FFI_TYPE_VOID       0
#define FFI_TYPE_INT        1
#define FFI_TYPE_FLOAT      2
#define FFI_TYPE_DOUBLE     3
#define FFI_TYPE_LONGDOUBLE 4
#define FFI_TYPE_UINT8      5
#define FFI_TYPE_SINT8      6
#define FFI_TYPE_UINT16     7
#define FFI_TYPE_SINT16     8
#define FFI_TYPE_UINT32     9
#define FFI_TYPE_SINT32    10
#define FFI_TYPE_UINT64    11
#define FFI_TYPE_SINT64    12
#define FFI_TYPE_STRUCT    13
#define FFI_TYPE_POINTER   14
#define FFI_TYPE_COMPLEX   15
#define FFI_TYPE_LAST      FFI_TYPE_COMPLEX

typedef struct ffi_type {
    size_t         size;
    unsigned short alignment;
    unsigned short type;
    struct ffi_type** elements; /* for structs: NULL-terminated array */
} ffi_type;

/* Built-in types */
extern ffi_type ffi_type_void;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;
extern ffi_type ffi_type_pointer;
extern ffi_type ffi_type_ulong;
extern ffi_type ffi_type_slong;

/* Calling convention */
#define FFI_FIRST_ABI  0
#define FFI_SYSV       1
#define FFI_DEFAULT_ABI FFI_SYSV
#define FFI_LAST_ABI   2

typedef unsigned int ffi_abi;

/* Return codes */
typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI
} ffi_status;

/* Call Interface structure */
typedef struct {
    ffi_abi    abi;
    unsigned   nargs;
    ffi_type **arg_types;
    ffi_type  *rtype;
    unsigned   bytes;   /* stack bytes required */
    unsigned   flags;
} ffi_cif;

/* Closure */
typedef struct ffi_closure {
    char         trampoline[40]; /* x86-64 trampoline code */
    ffi_cif     *cif;
    void        (*fun)(ffi_cif*, void*, void**, void*);
    void        *user_data;
} ffi_closure;

/* Allocate an executable+writable page for a closure */
void* ffi_closure_alloc(size_t size, void **code);
void  ffi_closure_free(void *writable);

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned int nargs,
                         ffi_type *rtype, ffi_type **argtypes);

ffi_status ffi_prep_cif_var(ffi_cif *cif, ffi_abi abi,
                             unsigned int nfixedargs, unsigned int ntotalargs,
                             ffi_type *rtype, ffi_type **argtypes);

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue);

ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                 void (*fun)(ffi_cif*, void*, void**, void*),
                                 void *user_data, void *codeloc);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_FFI_H */
