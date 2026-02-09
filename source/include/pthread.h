#ifndef _PTHREAD_H
#define _PTHREAD_H
#include <stddef.h>

typedef unsigned long pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER 0

#ifdef __cplusplus
extern "C" {
#endif

int pthread_create(pthread_t *thread, const void *attr, void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif
