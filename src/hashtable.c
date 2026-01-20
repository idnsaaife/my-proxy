#include "hashtable.h"
#include "cache.h"
#include <string.h>

#define HASH_CONST 5
#define HASH_INIT_VAL 5381

static unsigned int ht_hash_url(const char *url) {
    unsigned int hash = HASH_INIT_VAL;
    int c;
    while ((c = *url++))
        hash = ((hash << HASH_CONST) + hash) + c;
    return hash % HASHTABLE_SIZE;
}

void ht_init(hashtable_t *ht) {
    memset(ht->buckets, 0, sizeof(ht->buckets));
}

cache_entry_t *ht_find(hashtable_t *ht, const char *url) {
    unsigned int h = ht_hash_url(url);
    cache_entry_t *e = ht->buckets[h];
    while (e) {
        if (strcmp(e->url, url) == 0) return e;
        e = e->hash_next;
    }
    return NULL;
}

void ht_insert(hashtable_t *ht, cache_entry_t *entry) {
    unsigned int h = ht_hash_url(entry->url);
    entry->hash_next = ht->buckets[h];
    ht->buckets[h] = entry;
}

void ht_remove(hashtable_t *ht, cache_entry_t *entry) {
    unsigned int h = ht_hash_url(entry->url);
    cache_entry_t **ptr = &ht->buckets[h];
    while (*ptr != NULL) {
        if (*ptr == entry) {
            *ptr = entry->hash_next;
            return;
        }
        ptr = &(*ptr)->hash_next;
    }
}
