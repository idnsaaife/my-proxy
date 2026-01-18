#include "list.h"
#include "cache.h"


void list_init(list_t *list) {
    list->head = NULL;
    list->tail = NULL;
}

void list_move_to_front(list_t *list, cache_entry_t *entry) {
    if (entry == list->head) {
        return;
    }
    
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == list->tail) {
        list->tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = list->head;
    if (list->head) {
        list->head->prev = entry;
    }
    list->head = entry;
    
    if (list->tail == NULL) {
        list->tail = entry;
    }
}

cache_entry_t *list_get_tail(list_t *list) {
    return list->tail;
}

void list_remove(list_t *list, cache_entry_t *entry) {
    if (entry->prev) {
        entry->prev->next = NULL;
    }
    list->tail = entry->prev;
    if (list->tail == NULL) {
        list->head = NULL;
    }
    entry->prev = entry->next = NULL;
}

void list_add_front(list_t *list, cache_entry_t *entry) {
    entry->prev = NULL;
    entry->next = list->head;
    if (list->head != NULL) {
        list->head->prev = entry;
    }
    list->head = entry;
    if (list->tail == NULL) {
        list->tail = entry;
    }
}
