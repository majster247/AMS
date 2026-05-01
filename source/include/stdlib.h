#ifndef _STDLIB_H
#define _STDLIB_H

#include "ams_syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);
void exit(int status);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
int abs(int j);
void* sbrk(intptr_t increment);

int atoi(const char *nptr);
char *getenv(const char *name);
int system(const char *command);
double atof(const char *nptr);

extern char **environ;

#ifdef __cplusplus
}
#endif



#endif