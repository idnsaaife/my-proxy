#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_CACHE_SIZE 104857600  // 100 MB
#define MAX_OBJECT_SIZE (10485760 * 50)  // 500 MB
#define CACHE_HASH_SIZE 1024

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
    cache_entry_t *head;
    cache_entry_t *tail;
    cache_entry_t *hash_table[CACHE_HASH_SIZE];
    size_t total_size;
    pthread_mutex_t cache_lock;
} cache_t;

extern cache_t cache;

void init_cache(void);
void cleanup_cache(void);
unsigned int hash_url(const char *url);
void move_to_front(cache_entry_t *entry);
void evict_entries(void);
cache_entry_t *find_cache_entry(const char *url);
cache_entry_t *create_cache_entry(const char *url);
void cache_entry_addref(cache_entry_t *entry);
void cache_entry_release(cache_entry_t *entry);
void finalize_cache_entry(cache_entry_t *entry);

#endif // CACHE_H
