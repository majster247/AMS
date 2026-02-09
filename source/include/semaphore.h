#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

typedef struct {
    int val;
} sem_t;

#ifdef __cplusplus
extern "C" {
#endif

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);

#ifdef __cplusplus
}
#endif

#endif
