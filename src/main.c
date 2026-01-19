#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "cache.h"
#include "thread_pool.h"
#include "client_handler.h"
#include "logger.h"
#include "config.h"


int main(void) {
    log_init(LOG_DEBUG, "proxy.log");
    
    int server_socket;
    struct sockaddr_in server_addr;

    LOG_INFO("HTTP Proxy Server starting on port %d", PORT);
    
    init_cache();
    
    thread_pool_t *pool = thread_pool_init();
    if (!pool) {
        LOG_ERROR("Failed to initialize thread pool");
        log_close();
        exit(1);
    }
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        LOG_ERROR("Socket creation failed: %s", strerror(errno));
        log_close();
        exit(1);
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Bind failed: %s", strerror(errno));
        close(server_socket);
        log_close();
        exit(1);
    }
    
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        LOG_ERROR("Listen failed: %s", strerror(errno));
        close(server_socket);
        log_close();
        exit(1);
    }
    
    LOG_INFO("Server listening on port %d", PORT);
    LOG_INFO("Waiting for connections...");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &client_len);
        if (client_socket < 0) {
            LOG_ERROR("Accept failed: %s", strerror(errno));
            continue;
        }
        
        client_info_t *client_info = malloc(sizeof(client_info_t));
        if (!client_info) {
            LOG_ERROR("Failed to allocate client_info: %s", strerror(errno));
            close(client_socket);
            continue;
        }
        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;
        
        if (thread_pool_add_task(pool, handle_client_wrapper, client_info) != 0) {
            LOG_ERROR("Failed to add task to thread pool");
            close(client_socket);
            free(client_info);
        }
    }
    
    close(server_socket);
    thread_pool_destroy(pool);
    cleanup_cache();
    log_close();
    return 0;
}
