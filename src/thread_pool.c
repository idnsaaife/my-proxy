#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void *worker_thread(void *arg);

void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->queue_lock);
        
        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_lock);
            pthread_exit(NULL);
        }
        
        task_t *task = pool->task_queue_head;
        if (task != NULL) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->task_count--;
            pthread_cond_signal(&pool->queue_not_full);
        }
        
        pthread_mutex_unlock(&pool->queue_lock);
        
        if (task) {
            (task->function)(task->arg);
            free(task);
        }
    }
    
    return NULL;
}

thread_pool_t *thread_pool_init(void) {
    thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (!pool) {
        perror("Failed to allocate thread pool");
        return NULL;
    }
    
    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->task_count = 0;
    pool->shutdown = 0;
    
    pthread_mutex_init(&pool->queue_lock, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL);
    pthread_cond_init(&pool->queue_not_full, NULL);
    
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            perror("Failed to create worker thread");
            thread_pool_destroy(pool);
            return NULL;
        }
        pthread_detach(pool->threads[i]);
    }
    
    printf("Thread pool initialized with %d workers\n", THREAD_POOL_SIZE);
    return pool;
}

int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    if (!pool || !function) {
        return -1;
    }
    
    task_t *task = (task_t *)malloc(sizeof(task_t));
    if (!task) {
        perror("Failed to allocate task");
        return -1;
    }
    
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->queue_lock);
    
    while (pool->task_count >= TASK_QUEUE_SIZE && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock);
    }
    
    if (pool->shutdown) {
        free(task);
        pthread_mutex_unlock(&pool->queue_lock);
        return -1;
    }
    
    if (pool->task_queue_tail == NULL) {
        pool->task_queue_head = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }
    
    pool->task_count++;
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    return 0;
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue_lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    usleep(100000);
    
    pthread_mutex_lock(&pool->queue_lock);
    task_t *task = pool->task_queue_head;
    while (task) {
        task_t *next = task->next;
        free(task);
        task = next;
    }
    pthread_mutex_unlock(&pool->queue_lock);
    
    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_not_full);
    
    free(pool);
    printf("Thread pool destroyed\n");
}
