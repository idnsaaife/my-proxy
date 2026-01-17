#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "cache.h"
#include "thread_pool.h"
#include "client_handler.h"

#define PORT 1234
#define MAX_CLIENTS 100

int main(void) {
    int server_socket;
    struct sockaddr_in server_addr;

    printf("HTTP Proxy Server with cache\n");

    init_cache();
    
    thread_pool_t *pool = thread_pool_init();
    if (!pool) {
        fprintf(stderr, "Failed to initialize thread pool\n");
        exit(1);
    }
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }
    
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(1);
    }
    
    printf("Server listening on port %d\n", PORT);
    printf("Waiting for connections\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        client_info_t *client_info = malloc(sizeof(client_info_t));
        if (!client_info) {
            close(client_socket);
            continue;
        }
        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;
        
        if (thread_pool_add_task(pool, handle_client_wrapper, client_info) != 0) {
            fprintf(stderr, "Failed to add task to thread pool\n");
            close(client_socket);
            free(client_info);
        }
    }
    
    close(server_socket);
    thread_pool_destroy(pool);
    cleanup_cache();
    return 0;
}
