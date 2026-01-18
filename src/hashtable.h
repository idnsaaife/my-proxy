#ifndef HASHTABLE_H
#define HASHTABLE_H

#define HASHTABLE_SIZE 1024

typedef struct cache_entry cache_entry_t;

typedef struct {
    cache_entry_t *buckets[HASHTABLE_SIZE];
} hashtable_t;

void ht_init(hashtable_t *ht);
cache_entry_t *ht_find(hashtable_t *ht, const char *url);
void ht_insert(hashtable_t *ht, cache_entry_t *entry);
void ht_remove(hashtable_t *ht, cache_entry_t *entry);

#endif // HASHTABLE_H
