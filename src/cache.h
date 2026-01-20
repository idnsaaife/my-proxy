#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>
#include "list.h"
#include "hashtable.h"

typedef enum {
    CACHE_ENTRY_FETCHING,
    CACHE_ENTRY_READY,
    CACHE_ENTRY_ERROR,
    CACHE_ENTRY_TOO_LARGE
} cache_entry_state_t;

typedef struct cache_entry {
    char *url;
    char *data;
    size_t data_size;
    size_t capacity;
    cache_entry_state_t state;
    int should_cache;
    pthread_mutex_t lock;
    pthread_cond_t ready_cond;
    atomic_int ref_count;
    
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
