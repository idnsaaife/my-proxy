#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cache_t cache;

void init_cache(void) {
    list_init(&cache.list);
    ht_init(&cache.table);
    cache.total_size = 0;
    pthread_mutex_init(&cache.cache_lock, NULL);
    printf("Cache initialized (max size: %d Mb)\n", MAX_CACHE_SIZE / 1024 / 1024);
}

static void evict_entries(void) {
    cache_entry_t *victim = list_get_tail(&cache.list);

    while (cache.total_size > MAX_CACHE_SIZE && victim != NULL) {
        cache_entry_t *prev_victim = victim->prev;
        
        
        pthread_mutex_lock(&victim->lock);
        int can_evict = (victim->ref_count == 0 && !victim->in_progress);
        pthread_mutex_unlock(&victim->lock);

        if (can_evict) {
            printf("[CACHE] Evicting: %s (size: %zu bytes)\n", 
                   victim->url, victim->data_size);
            
            list_remove(&cache.list, victim);
            ht_remove(&cache.table, victim);
            
            cache.total_size -= victim->data_size;
            
            free(victim->url);
            if (victim->data) free(victim->data);
            pthread_mutex_destroy(&victim->lock);
            pthread_cond_destroy(&victim->ready_cond);
            free(victim);
        }
        
        victim = prev_victim;  
    }
}

cache_entry_t *find_cache_entry(const char *url) {
    pthread_mutex_lock(&cache.cache_lock);
    
    cache_entry_t *entry = ht_find(&cache.table, url);
    if (entry != NULL) {
        list_move_to_front(&cache.list, entry);
        pthread_mutex_lock(&entry->lock);
        entry->ref_count++;  
        pthread_mutex_unlock(&entry->lock);
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
    return entry;
}

cache_entry_t *create_cache_entry(const char *url) {

    
    cache_entry_t *entry = (cache_entry_t *)malloc(sizeof(cache_entry_t));
    if (!entry) return NULL;
    
    entry->url = strdup(url);
    entry->data = NULL;
    entry->data_size = 0;
    entry->capacity = 0;
    entry->ready = 0;
    entry->in_progress = 1;
    entry->error = 0;
    entry->ref_count = 1;
    entry->prev = NULL;
    entry->next = NULL;
    entry->hash_next = NULL;
    
    pthread_mutex_init(&entry->lock, NULL);
    pthread_cond_init(&entry->ready_cond, NULL);
    
    pthread_mutex_lock(&cache.cache_lock);
    
    ht_insert(&cache.table, entry);
    list_add_front(&cache.list, entry);
    
    pthread_mutex_unlock(&cache.cache_lock);
    
    return entry;
}

void cache_entry_addref(cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    entry->ref_count++;
    pthread_mutex_unlock(&entry->lock);
}

void cache_entry_release(cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    entry->ref_count--;
    pthread_mutex_unlock(&entry->lock);
}

void finalize_cache_entry(cache_entry_t *entry) {
    pthread_mutex_lock(&cache.cache_lock);
    cache.total_size += entry->data_size;
    
    if (cache.total_size > MAX_CACHE_SIZE) {
        evict_entries();
    }
    pthread_mutex_unlock(&cache.cache_lock);
}

void cleanup_cache(void) {
    pthread_mutex_lock(&cache.cache_lock);
    
    cache_entry_t *entry = cache.list.head;
    while (entry != NULL) {
        cache_entry_t *next = entry->next;
        
        free(entry->url);
        if (entry->data) free(entry->data);
        pthread_mutex_destroy(&entry->lock);
        pthread_cond_destroy(&entry->ready_cond);
        free(entry);
        
        entry = next;
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
    pthread_mutex_destroy(&cache.cache_lock);
    printf("Cache cleaned up\n");
}
