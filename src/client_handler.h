#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <netinet/in.h>
#include "cache.h"

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

void handle_client_direct(void *arg);
void handle_client_wrapper(void *arg);
void send_to_client(int client_socket, struct cache_entry *entry);

#endif // CLIENT_HANDLER_H
