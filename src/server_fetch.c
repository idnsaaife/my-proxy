#include "server_fetch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void *fetch_from_server(void *arg) {
    fetch_info_t *fetch_info = (fetch_info_t *)arg;
    cache_entry_t *entry = fetch_info->entry;
    int server_socket = -1;
    int success = 0;
    size_t total_received = 0;
    
    printf("[FETCH] Starting: %s from %s:%d%s\n", 
           fetch_info->url, fetch_info->host, fetch_info->port, fetch_info->path);
    
    printf("[FETCH] Requesting path: %s\n", fetch_info->path);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket >= 0) {
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;        
        hints.ai_socktype = SOCK_STREAM; 

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", fetch_info->port);

        int err = getaddrinfo(fetch_info->host, port_str, &hints, &result);
        if (err == 0) {
            if (connect(server_socket, result->ai_addr, result->ai_addrlen) >= 0) {
                char request[2048];
                snprintf(request, sizeof(request),
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "Connection: close\r\n"
                    "User-Agent: ProxyCache/1.0\r\n"
                    "Accept: */*\r\n"
                    "\r\n", fetch_info->path, fetch_info->host);
                printf("---\n%s---\n", request);
                
                if (send(server_socket, request, strlen(request), 0) >= 0) {
                    pthread_mutex_lock(&entry->lock);
                    entry->data = malloc(BUFFER_SIZE);
                    entry->capacity = BUFFER_SIZE;
                    pthread_mutex_unlock(&entry->lock);
                    
                    if (entry->data != NULL) {
                        char buffer[BUFFER_SIZE];
                        ssize_t bytes_received;
                        
                        while ((bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
                            pthread_mutex_lock(&entry->lock);
                            
                            if (total_received + bytes_received > entry->capacity) {
                                size_t new_capacity = entry->capacity * 2;
                                if (new_capacity <= MAX_OBJECT_SIZE) {
                                    char *new_data = realloc(entry->data, new_capacity);
                                    if (new_data != NULL) {
                                        entry->data = new_data;
                                        entry->capacity = new_capacity;
                                    } else {
                                        pthread_mutex_unlock(&entry->lock);
                                        break;
                                    }
                                } else {
                                    printf("[FETCH] Object too large\n");
                                    pthread_mutex_unlock(&entry->lock);
                                    break;
                                }
                            }
                            
                            memcpy(entry->data + total_received, buffer, bytes_received);
                            total_received += bytes_received;
                            entry->data_size = total_received;
                            
                            pthread_mutex_unlock(&entry->lock);
                        }
                        
                        if (total_received > 0) {
                            success = 1;
                        }
                    }
                } else {
                    perror("Send failed");
                }
            } else {
                perror("Connection failed");
            }
            freeaddrinfo(result);
        } else {
            printf("Host not found: %s (%s)\n", fetch_info->host, gai_strerror(err));
        }
        
        close(server_socket);
    } else {
        perror("Socket creation failed");
    }
    
    printf("[FETCH] Received %zu bytes\n", total_received);
    
    pthread_mutex_lock(&entry->lock);
    entry->in_progress = 0;
    entry->ready = 1;
    if (!success || total_received == 0) {
        entry->error = 1;
    }
    pthread_cond_broadcast(&entry->ready_cond);
    pthread_mutex_unlock(&entry->lock);
    
    if (!entry->error && total_received > 0) {
        finalize_cache_entry(entry);
    }
    
    cache_entry_release(entry);
    free(fetch_info);
    return NULL;
}
