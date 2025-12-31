#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#define THREAD_POOL_SIZE 20
#define TASK_QUEUE_SIZE 256

typedef struct task {
    void (*function)(void *);
    void *arg;
    struct task *next;
} task_t;

typedef struct {
    pthread_t threads[THREAD_POOL_SIZE];
    task_t *task_queue_head;
    task_t *task_queue_tail;
    int task_count;
    int shutdown;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
} thread_pool_t;

thread_pool_t *thread_pool_init(void);
int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg);
void thread_pool_destroy(thread_pool_t *pool);

#endif // THREAD_POOL_H
