#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cache_t cache;

void init_cache(void) {
    cache.head = NULL;
    cache.tail = NULL;
    memset(cache.hash_table, 0, sizeof(cache.hash_table));
    cache.total_size = 0;
    pthread_mutex_init(&cache.cache_lock, NULL);
    printf("Cache initialized (max size: %d MB)\n", MAX_CACHE_SIZE / 1024 / 1024);
}

unsigned int hash_url(const char *url) {
    unsigned int hash = 5381;
    int c;
    while ((c = *url++))
        hash = ((hash << 5) + hash) + c;
    return hash % CACHE_HASH_SIZE;
}

void move_to_front(cache_entry_t *entry) {
    if (entry == cache.head) {
        return;
    }
    
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cache.tail) {
        cache.tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = cache.head;
    if (cache.head) {
        cache.head->prev = entry;
    }
    cache.head = entry;
    
    if (cache.tail == NULL) {
        cache.tail = entry;
    }
}

void evict_entries(void) {
    while (cache.total_size > MAX_CACHE_SIZE && cache.tail != NULL) {
        cache_entry_t *victim = cache.tail;
        
        if (victim->ref_count > 0 || victim->in_progress) {
            victim = victim->prev;
            if (victim == NULL) break;
            continue;
        }
        
        printf("[CACHE] Evicting: %s (size: %zu bytes)\n", victim->url, victim->data_size);
        
        if (victim->prev) {
            victim->prev->next = NULL;
        }
        cache.tail = victim->prev;
        if (cache.tail == NULL) {
            cache.head = NULL;
        }
        
        unsigned int hash = hash_url(victim->url);
        cache_entry_t **ptr = &cache.hash_table[hash];
        while (*ptr != NULL) {
            if (*ptr == victim) {
                *ptr = victim->hash_next;
                break;
            }
            ptr = &(*ptr)->hash_next;
        }
        
        cache.total_size -= victim->data_size;
        
        free(victim->url);
        if (victim->data) free(victim->data);
        pthread_mutex_destroy(&victim->lock);
        pthread_cond_destroy(&victim->ready_cond);
        free(victim);
    }
}

cache_entry_t *find_cache_entry(const char *url) {
    unsigned int hash = hash_url(url);
    pthread_mutex_lock(&cache.cache_lock);
    
    cache_entry_t *entry = cache.hash_table[hash];
    while (entry != NULL) {
        if (strcmp(entry->url, url) == 0) {
            move_to_front(entry);
            cache_entry_addref(entry);
            pthread_mutex_unlock(&cache.cache_lock);
            return entry;
        }
        entry = entry->hash_next;
    }
    
    pthread_mutex_unlock(&cache.cache_lock);
    return NULL;
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
    
    unsigned int hash = hash_url(url);
    entry->hash_next = cache.hash_table[hash];
    cache.hash_table[hash] = entry;
    
    entry->next = cache.head;
    if (cache.head != NULL) {
        cache.head->prev = entry;
    }
    cache.head = entry;
    if (cache.tail == NULL) {
        cache.tail = entry;
    }
    
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
    
    cache_entry_t *entry = cache.head;
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
