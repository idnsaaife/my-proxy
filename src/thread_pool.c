#include "thread_pool.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


typedef struct task {
    void (*function)(void *);
    void *arg;
    struct task *next;
} task_t;


struct thread_pool {
    pthread_t threads[THREAD_POOL_SIZE];
    task_t *task_queue_head;
    task_t *task_queue_tail;
    int task_count;
    int shutdown;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
};


void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    
    LOG_DEBUG("Worker thread started (tid: %lu)", pthread_self());
    
    while (1) {
        pthread_mutex_lock(&pool->queue_lock);
        
        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
        }
        
        if (pool->shutdown) {
            LOG_DEBUG("Worker thread shutting down (tid: %lu)", pthread_self());
            pthread_mutex_unlock(&pool->queue_lock);
            pthread_exit(NULL);
        }
        
        task_t *task = pool->task_queue_head;
        int queue_size = 0; 
        
        if (task != NULL) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->task_count--;
            queue_size = pool->task_count;  
            pthread_cond_signal(&pool->queue_not_full);
        }
        
        pthread_mutex_unlock(&pool->queue_lock);
        
        if (task) {
            LOG_DEBUG("Worker executing task (tid: %lu, queue size: %d)", 
                      pthread_self(), queue_size); 
            (task->function)(task->arg);
            free(task);
        }
    }
    
    return NULL;
}



thread_pool_t *thread_pool_init(void) {
    thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate thread pool: %s", strerror(errno));
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
            LOG_ERROR("Failed to create worker thread %d: %s", i, strerror(errno));
            thread_pool_destroy(pool);
            return NULL;
        }
        pthread_detach(pool->threads[i]);
    }
    
    LOG_INFO("Thread pool initialized with %d workers", THREAD_POOL_SIZE);
    return pool;
}


int thread_pool_add_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    if (!pool || !function) {
        LOG_ERROR("Invalid parameters to thread_pool_add_task");
        return -1;
    }
    
    task_t *task = (task_t *)malloc(sizeof(task_t));
    if (!task) {
        LOG_ERROR("Failed to allocate task: %s", strerror(errno));
        return -1;
    }
    
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->queue_lock);
    
    while (pool->task_count >= TASK_QUEUE_SIZE && !pool->shutdown) {
        int count = pool->task_count;  
        pthread_mutex_unlock(&pool->queue_lock);
        
        LOG_WARN("Task queue full (%d tasks), waiting...", count);  
        
        pthread_mutex_lock(&pool->queue_lock);
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock);
    }
    
    if (pool->shutdown) {
        LOG_WARN("Cannot add task: thread pool is shutting down");
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
    int queue_size = pool->task_count;  
    
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    LOG_DEBUG("Task added to queue (queue size: %d)", queue_size); 
    
    return 0;
}


void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) return;
    
    LOG_INFO("Shutting down thread pool...");
    
    pthread_mutex_lock(&pool->queue_lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    pthread_mutex_lock(&pool->queue_lock);
    task_t *task = pool->task_queue_head;
    int dropped_tasks = 0;
    while (task) {
        task_t *next = task->next;
        free(task);
        task = next;
        dropped_tasks++;
    }
    if (dropped_tasks > 0) {
        LOG_WARN("Dropped %d pending tasks during shutdown", dropped_tasks);
    }
    pthread_mutex_unlock(&pool->queue_lock);
    
    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_not_full);
    
    free(pool);
    LOG_INFO("Thread pool destroyed");
}
