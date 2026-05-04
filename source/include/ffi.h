/**
 * @file ffi.h
 * @brief Minimal libffi API stubs for AMS.
 *
 * Provides the type definitions and function signatures needed by
 * wayland-server's closure/dispatch mechanism. The AMS implementation
 * supports only the x86_64 calling convention with up to 8 integer/pointer
 * arguments (which is what Wayland actually uses).
 */
#ifndef _AMS_FFI_H
#define _AMS_FFI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI,
} ffi_status;

typedef enum {
    FFI_DEFAULT_ABI = 0,
    FFI_SYSV = 1,
    FFI_UNIX64 = 2,
} ffi_abi;

typedef enum {
    FFI_TYPE_VOID    = 0,
    FFI_TYPE_INT     = 1,
    FFI_TYPE_FLOAT   = 2,
    FFI_TYPE_DOUBLE  = 3,
    FFI_TYPE_UINT8   = 5,
    FFI_TYPE_SINT8   = 6,
    FFI_TYPE_UINT16  = 7,
    FFI_TYPE_SINT16  = 8,
    FFI_TYPE_UINT32  = 9,
    FFI_TYPE_SINT32  = 10,
    FFI_TYPE_UINT64  = 11,
    FFI_TYPE_SINT64  = 12,
    FFI_TYPE_STRUCT  = 13,
    FFI_TYPE_POINTER = 14,
} ffi_type_enum;

typedef struct _ffi_type {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct _ffi_type **elements;
} ffi_type;

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

typedef struct {
    ffi_abi     abi;
    unsigned    nargs;
    ffi_type  **arg_types;
    ffi_type   *rtype;
    unsigned    bytes;
    unsigned    flags;
} ffi_cif;

typedef void (*ffi_closure_fun)(ffi_cif*, void*, void**, void*);

typedef struct {
    char tramp[24];
    ffi_cif *cif;
    ffi_closure_fun fun;
    void *user_data;
} ffi_closure;

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi,
                         unsigned int nargs,
                         ffi_type *rtype,
                         ffi_type **atypes);

void ffi_call(ffi_cif *cif, void (*fn)(void),
              void *rvalue, void **avalue);

ffi_closure *ffi_closure_alloc(size_t size, void **code);
void         ffi_closure_free(ffi_closure *closure);

ffi_status ffi_prep_closure_loc(ffi_closure *closure,
                                 ffi_cif *cif,
                                 ffi_closure_fun fun,
                                 void *user_data,
                                 void *codeloc);

#ifdef __cplusplus
}
#endif

#endif /* _AMS_FFI_H */
