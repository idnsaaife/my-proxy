#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stddef.h>
#include "list.h"
#include "hashtable.h"

#define MAX_CACHE_SIZE  (600 * 1024 * 1024)  // 600mb
#define MAX_OBJECT_SIZE (300 * 1024 * 1024) // 300mb

typedef struct cache_entry {
    char *url;
    char *data;
    size_t data_size;
    size_t capacity;
    int ready;
    int in_progress;
    int error;
    pthread_mutex_t lock;
    pthread_cond_t ready_cond;
    int ref_count;
    
    struct cache_entry *prev;
    struct cache_entry *next;
    struct cache_entry *hash_next;
} cache_entry_t;

typedef struct {
    list_t list;
    hashtable_t table;
    size_t total_size;
    pthread_mutex_t cache_lock;
} cache_t;

extern cache_t cache;

void init_cache(void);
void cleanup_cache(void);
cache_entry_t *find_cache_entry(const char *url);
cache_entry_t *create_cache_entry(const char *url);
void cache_entry_addref(cache_entry_t *entry);
void cache_entry_release(cache_entry_t *entry);
void finalize_cache_entry(cache_entry_t *entry);

#endif // CACHE_H
