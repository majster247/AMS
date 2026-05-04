#ifndef _SYS_MEMFD_H
#define _SYS_MEMFD_H

#ifdef __cplusplus
extern "C" {
#endif

#define MFD_CLOEXEC  0x0001U
#define MFD_ALLOW_SEALING 0x0002U

int memfd_create(const char* name, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif
