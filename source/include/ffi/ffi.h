/*
 * Minimal libffi ABI shim for AMS (x86_64 SysV only).
 * Subset compatible with libffi 3.4 needed by glib/gobject and
 * higher-level Wayland clients that want callbacks.
 */

#ifndef AMS_FFI_H
#define AMS_FFI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI
} ffi_status;

typedef enum {
    FFI_DEFAULT_ABI = 1,
    FFI_SYSV        = 1,
    FFI_UNIX64      = 1
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
    FFI_TYPE_POINTER = 14
} ffi_type_id;

typedef struct _ffi_type {
    size_t            size;
    unsigned short    alignment;
    unsigned short    type;
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
    ffi_abi    abi;
    unsigned   nargs;
    ffi_type **arg_types;
    ffi_type  *rtype;
    unsigned   bytes;
    unsigned   flags;
} ffi_cif;

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned int nargs,
                        ffi_type *rtype, ffi_type **atypes);

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue);

typedef struct ffi_closure {
    char     tramp[32];
    ffi_cif *cif;
    void   (*fun)(ffi_cif *cif, void *ret, void **args, void *user_data);
    void    *user_data;
} ffi_closure;

ffi_closure *ffi_closure_alloc(size_t size, void **code);
void         ffi_closure_free(ffi_closure *closure);
ffi_status   ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                  void (*fun)(ffi_cif*,void*,void**,void*),
                                  void *user_data, void *codeloc);

#ifdef __cplusplus
}
#endif

#endif /* AMS_FFI_H */
