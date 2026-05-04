#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stdint.h>

typedef uint64_t pthread_t;
typedef struct { int __data; } pthread_attr_t;
typedef struct { int __lock; } pthread_mutex_t;
typedef struct { int __data; } pthread_mutexattr_t;
typedef struct { int __data; } pthread_cond_t;
typedef struct { int __data; } pthread_condattr_t;
typedef int pthread_once_t;

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER  {0}
#define PTHREAD_ONCE_INIT 0

#ifdef __cplusplus
extern "C" {
#endif

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
int pthread_detach(pthread_t thread);
pthread_t pthread_self(void);

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

#ifdef __cplusplus
}
#endif

#endif
