#ifndef SERVER_FETCH_H
#define SERVER_FETCH_H

#include "cache.h"

#define BUFFER_SIZE 8192
#define MAX_URL_LEN    2048
#define MAX_HOST_LEN   256
#define MAX_PATH_LEN   1024

typedef struct {
    cache_entry_t *entry;
    char url[MAX_URL_LEN];
    char host[MAX_HOST_LEN];
    int port;
    char path[MAX_PATH_LEN];
} fetch_info_t;

void *fetch_from_server(void *arg);

#endif // SERVER_FETCH_H
