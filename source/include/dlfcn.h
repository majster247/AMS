#ifndef _DLFCN_H
#define _DLFCN_H

#define RTLD_LAZY   0x00001
#define RTLD_NOW    0x00002
#define RTLD_LOCAL  0x00000
#define RTLD_GLOBAL 0x00100

#ifdef __cplusplus
extern "C" {
#endif

void* dlopen(const char* filename, int flags);
int   dlclose(void* handle);
void* dlsym(void* handle, const char* symbol);
char* dlerror(void);

#ifdef __cplusplus
}
#endif

#endif
