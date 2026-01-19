#ifndef SERVER_FETCH_H
#define SERVER_FETCH_H

#include "cache.h"
#include "config.h"

typedef struct {
    cache_entry_t *entry;
    char url[HTTP_URL_MAX_LEN];
    char host[HTTP_HOST_MAX_LEN];
    int port;
    char path[HTTP_PATH_MAX_LEN];
} fetch_info_t;

void *fetch_from_server(void *arg);

#endif // SERVER_FETCH_H
