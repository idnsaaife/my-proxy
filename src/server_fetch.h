#ifndef SERVER_FETCH_H
#define SERVER_FETCH_H

#include "cache.h"

#define BUFFER_SIZE 8192

typedef struct {
    cache_entry_t *entry;
    char url[2048];
    char host[256];
    int port;
    char path[1024];
} fetch_info_t;

void *fetch_from_server(void *arg);

#endif // SERVER_FETCH_H
