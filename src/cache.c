#include "cache.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cache_t cache;

void init_cache(void) {
    list_init(&cache.list);
    ht_init(&cache.table);
    cache.total_size = 0;
    pthread_mutex_init(&cache.cache_lock, NULL);
    
    LOG_INFO("Cache initialized (max size: %d MB)", MAX_CACHE_SIZE / 1024 / 1024);
}

static void evict_entries(void) {
    cache_entry_t *victim = list_get_tail(&cache.list);
    int evicted_count = 0;
    size_t freed_bytes = 0;

    while (cache.total_size > MAX_CACHE_SIZE && victim != NULL) {
        cache_entry_t *prev_victim = victim->prev;
        
        pthread_mutex_lock(&victim->lock);
        int ref_count = atomic_load(&victim->ref_count); 
        int can_evict = (ref_count == 0 && !victim->in_progress);
        pthread_mutex_unlock(&victim->lock);

        if (can_evict) {
            LOG_DEBUG("Evicting entry: %s (size: %zu bytes, ref_count: %d)",
                      victim->url, victim->data_size, victim->ref_count);
            
            freed_bytes += victim->data_size;
            
            list_remove(&cache.list, victim);
            ht_remove(&cache.table, victim);
            
            cache.total_size -= victim->data_size;
            
            free(victim->url);
            if (victim->data) free(victim->data);
            pthread_mutex_destroy(&victim->lock);
            pthread_cond_destroy(&victim->ready_cond);
            free(victim);
            
            evicted_count++;
        }
        
        victim = prev_victim;
    }
    
    if (evicted_count > 0) {
        LOG_INFO("Eviction complete: removed %d entries, freed %zu KB (cache size: %zu MB)",
                 evicted_count, freed_bytes / 1024, cache.total_size / 1024 / 1024);
    }
}

cache_entry_t *find_cache_entry(const char *url) {
    pthread_mutex_lock(&cache.cache_lock);
    
    cache_entry_t *entry = ht_find(&cache.table, url);
    if (entry != NULL) {
        list_move_to_front(&cache.list, entry);
        int new_ref = atomic_fetch_add(&entry->ref_count, 1) + 1;
        
        LOG_DEBUG("Cache HIT: %s (ref_count: %d)", url, new_ref);
    } else {
        LOG_DEBUG("Cache MISS: %s", url);
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
    return entry;
}

cache_entry_t *create_cache_entry(const char *url) {
    cache_entry_t *entry = (cache_entry_t *)malloc(sizeof(cache_entry_t));
    if (!entry) {
        LOG_ERROR("Failed to allocate memory for cache entry: %s", url);
        return NULL;
    }
    
    entry->url = strdup(url);
    if (!entry->url) {
        LOG_ERROR("Failed to duplicate URL string: %s", url);
        free(entry);
        return NULL;
    }
    
    entry->data = NULL;
    entry->data_size = 0;
    entry->capacity = 0;
    entry->ready = 0;
    entry->in_progress = 1;
    entry->error = 0;
    atomic_init(&entry->ref_count, 1); 
    entry->prev = NULL;
    entry->next = NULL;
    entry->hash_next = NULL;
    
    pthread_mutex_init(&entry->lock, NULL);
    pthread_cond_init(&entry->ready_cond, NULL);
    
    pthread_mutex_lock(&cache.cache_lock);
    
    cache_entry_t *existing = ht_find(&cache.table, url);
    if (existing != NULL) {
        pthread_mutex_unlock(&cache.cache_lock);
        
        LOG_DEBUG("Entry already exists (created by another thread): %s", url);
        
        free(entry->url);
        pthread_mutex_destroy(&entry->lock);
        pthread_cond_destroy(&entry->ready_cond);
        free(entry);
        atomic_fetch_add(&existing->ref_count, 1); 
        return existing;
    }
    
    ht_insert(&cache.table, entry);
    list_add_front(&cache.list, entry);
    
    pthread_mutex_unlock(&cache.cache_lock);
    
    LOG_DEBUG("Created new cache entry: %s (ref_count: 1)", url);
    
    return entry;
}

void cache_entry_addref(cache_entry_t *entry) {
    atomic_fetch_add(&entry->ref_count, 1);
}

void cache_entry_release(cache_entry_t *entry) {
    int old_ref_count = atomic_fetch_sub(&entry->ref_count, 1);
    int new_ref_count = old_ref_count - 1;  
    if (new_ref_count < 0) {
        LOG_ERROR("CRITICAL: Negative ref_count=%d", new_ref_count);
    }
}

void finalize_cache_entry(cache_entry_t *entry) {
    pthread_mutex_lock(&cache.cache_lock);
    
    cache.total_size += entry->data_size;
    
    LOG_INFO("Finalized cache entry: %s (size: %zu KB, cache total: %zu MB)",
             entry->url, entry->data_size / 1024, cache.total_size / 1024 / 1024);
    
    if (cache.total_size > MAX_CACHE_SIZE) {
        LOG_WARN("Cache size exceeded (%zu MB > %d MB), starting eviction",
                 cache.total_size / 1024 / 1024, MAX_CACHE_SIZE / 1024 / 1024);
        evict_entries();
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
}

void cleanup_cache(void) {
    LOG_INFO("Starting cache cleanup...");
    
    pthread_mutex_lock(&cache.cache_lock);
    
    int entries_freed = 0;
    size_t total_freed = 0;
    
    cache_entry_t *entry = cache.list.head;
    while (entry != NULL) {
        cache_entry_t *next = entry->next;
        
        int ref_count = atomic_load(&entry->ref_count);
        if (ref_count > 0) {
            LOG_WARN("Entry still referenced during cleanup: %s (ref_count: %d)",
                    entry->url, ref_count);
        }
        
        total_freed += entry->data_size;
        
        free(entry->url);
        if (entry->data) free(entry->data);
        pthread_mutex_destroy(&entry->lock);
        pthread_cond_destroy(&entry->ready_cond);
        free(entry);
        
        entries_freed++;
        entry = next;
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
    pthread_mutex_destroy(&cache.cache_lock);
    
    LOG_INFO("Cache cleanup complete: freed %d entries (%zu KB)",
             entries_freed, total_freed / 1024);
}
