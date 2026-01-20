#ifndef LIST_H
#define LIST_H

typedef struct cache_entry cache_entry_t;

typedef struct {
    cache_entry_t *head;
    cache_entry_t *tail;
} list_t;

void list_init(list_t *list);
void list_move_to_front(list_t *list, cache_entry_t *entry);
cache_entry_t *list_get_tail(list_t *list);
void list_remove(list_t *list, cache_entry_t *entry);
void list_add_front(list_t *list, cache_entry_t *entry);

#endif // LIST_H
