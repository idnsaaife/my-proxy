#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#define THREAD_POOL_SIZE 20 
#define TASK_QUEUE_SIZE 256

typedef struct thread_pool thread_pool_t;

thread_pool_t *thread_pool_init(void);
int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg);
void thread_pool_destroy(thread_pool_t *pool);

#endif // THREAD_POOL_H
