#include "client_handler.h"
#include "cache.h"
#include "http_parser.h"
#include "server_fetch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

void send_to_client(int client_socket, cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    
    while (!entry->ready) {
        pthread_cond_wait(&entry->ready_cond, &entry->lock);
    }
    
    if (entry->error || entry->data_size == 0) {
        pthread_mutex_unlock(&entry->lock);
        const char *error_response = 
            "HTTP Bad Gateway\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        send(client_socket, error_response, strlen(error_response), 0);
        printf("[SEND] Error response sent\n");
        return;
    }
    
    size_t total_sent = 0;
    while (total_sent < entry->data_size) {
        ssize_t bytes_sent = send(client_socket, 
                                  entry->data + total_sent,
                                  entry->data_size - total_sent, 
                                  0);
        if (bytes_sent < 0) {
            perror("Send to client failed");
            break;
        }
        total_sent += bytes_sent;
    }
    
    pthread_mutex_unlock(&entry->lock);
    printf("[SEND] Sent %zu bytes to client\n", total_sent);
}

void handle_client_direct(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    
    printf("\n[CLIENT] New connection from %s\n", 
           inet_ntoa(client_info->client_addr.sin_addr));
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_socket);
        free(client_info);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    char method[16], url[2048], host[256], path[1024];
    int port;
    
    int parse_result = parse_http_request(buffer, bytes_read,
                                          method, url, host, &port, path);
    
    if (parse_result < 0) {
        printf("[CLIENT] Failed to parse request\n");
        const char *error_response = "HTTP Bad Request\r\n\r\n";
        send(client_socket, error_response, strlen(error_response), 0);
        close(client_socket);
        free(client_info);
        return;
    }
    
    printf("[CLIENT] Request: %s %s (Host: %s:%d, Path: %s)\n", 
           method, url, host, port, path);
    
    cache_entry_t *entry = find_cache_entry(url);
    
    if (entry != NULL) {
        printf("[CACHE] HIT for %s\n", url);
        send_to_client(client_socket, entry);
        cache_entry_release(entry);
    } else {
        printf("[CACHE] MISS for %s\n", url);
        entry = create_cache_entry(url);
        if (entry == NULL) {
            const char *error_response = "HTTP Internal Server Error\r\n\r\n";
            send(client_socket, error_response, strlen(error_response), 0);
            close(client_socket);
            free(client_info);
            return;
        }
        
        cache_entry_addref(entry);
        
        fetch_info_t *fetch_info = malloc(sizeof(fetch_info_t));
        fetch_info->entry = entry;
        strncpy(fetch_info->url, url, sizeof(fetch_info->url) - 1);
        strncpy(fetch_info->host, host, sizeof(fetch_info->host) - 1);
        strncpy(fetch_info->path, path, sizeof(fetch_info->path) - 1);
        fetch_info->port = port;
        
        pthread_t fetch_thread;
        if (pthread_create(&fetch_thread, NULL, fetch_from_server, fetch_info) != 0) {
            perror("Failed to create fetch thread");
            cache_entry_release(entry);
            cache_entry_release(entry);
            free(fetch_info);
            const char *error_response = "HTTP Internal Server Error\r\n\r\n";
            send(client_socket, error_response, strlen(error_response), 0);
            close(client_socket);
            free(client_info);
            return;
        }
        pthread_detach(fetch_thread);
        
        send_to_client(client_socket, entry);
        cache_entry_release(entry);
    }
    
    close(client_socket);
    free(client_info);
    printf("[CLIENT] Connection closed\n");
}

void handle_client_wrapper(void *arg) {
    handle_client_direct(arg);
}
