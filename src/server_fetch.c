#include "server_fetch.h"
#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

void *fetch_from_server(void *arg) {
    fetch_info_t *fetch_info = (fetch_info_t *)arg;
    cache_entry_t *entry = fetch_info->entry;
    int client_socket = fetch_info->client_socket;
    int server_socket = -1;
    int success = 0;
    size_t total_received = 0;
    int headers_sent_to_client = 0;
    
    LOG_INFO("Starting fetch: %s from %s:%d%s", 
             fetch_info->url, fetch_info->host, fetch_info->port, fetch_info->path);
    
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
                LOG_DEBUG("Connected to %s:%d", fetch_info->host, fetch_info->port);
                
                char request[HTTP_REQUEST_MAX_LEN];
                snprintf(request, sizeof(request),
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "Connection: close\r\n"
                    "User-Agent: ProxyCache/1.0\r\n"
                    "Accept: */*\r\n"
                    "\r\n", fetch_info->path, fetch_info->host);
                
                if (send(server_socket, request, strlen(request), 0) >= 0) {
                    pthread_mutex_lock(&entry->lock);
                    entry->data = malloc(BUFFER_SIZE);
                    int data_allocated = (entry->data != NULL);
                    entry->capacity = BUFFER_SIZE;
                    pthread_mutex_unlock(&entry->lock);
                    
                    if (data_allocated) {
                        char buffer[BUFFER_SIZE];
                        ssize_t bytes_received;
                        
                        while ((bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
                            pthread_mutex_lock(&entry->lock);
                            
                            if (entry->should_cache && total_received + bytes_received > MAX_OBJECT_SIZE) {
                                LOG_WARN("Object too large (%zu MB > %d MB): %s - switching to streaming", 
                                         (total_received + bytes_received) / 1024 / 1024,
                                         MAX_OBJECT_SIZE / 1024 / 1024, fetch_info->url);
                                
                                entry->should_cache = 0;
                                
                                if (!headers_sent_to_client && entry->data_size > 0) {
                                    send(client_socket, entry->data, entry->data_size, 0);
                                    headers_sent_to_client = 1;
                                }
                                
                                free(entry->data);
                                entry->data = NULL;
                                entry->capacity = 0;
                                entry->data_size = 0;
                                pthread_mutex_lock(&cache.cache_lock);
                                ht_remove(&cache.table, entry);
                                list_remove(&cache.list, entry);
                                pthread_mutex_unlock(&cache.cache_lock);
                            }
                            
                            if (!entry->should_cache) {
                                pthread_mutex_unlock(&entry->lock);
                                send(client_socket, buffer, bytes_received, 0);
                                total_received += bytes_received;
                                continue;  
                            }
                            
                            if (total_received + bytes_received > entry->capacity) {
                                size_t new_capacity = entry->capacity * 2;
                                if (new_capacity > MAX_OBJECT_SIZE) {
                                    new_capacity = MAX_OBJECT_SIZE;
                                }
                                
                                char *new_data = realloc(entry->data, new_capacity);
                                if (new_data != NULL) {
                                    entry->data = new_data;
                                    entry->capacity = new_capacity;
                                    LOG_DEBUG("Expanded buffer to %zu bytes", new_capacity);
                                } else {
                                    LOG_ERROR("Failed to realloc buffer: %s", strerror(errno));
                                    pthread_mutex_unlock(&entry->lock);
                                    break;
                                }
                            }

                            memcpy(entry->data + total_received, buffer, bytes_received);
                            total_received += bytes_received;
                            entry->data_size = total_received;
                            
                            pthread_mutex_unlock(&entry->lock);
                        }
                        
                        if (bytes_received < 0) {
                            LOG_ERROR("Recv failed: %s", strerror(errno));
                        }
                        
                        if (total_received > 0) {
                            success = 1;
                        }
                    } else {
                        LOG_ERROR("Failed to allocate initial buffer: %s", strerror(errno));
                    }
                } else {
                    LOG_ERROR("Send failed: %s", strerror(errno));
                }
            } else {
                LOG_ERROR("Connection failed to %s:%d: %s", 
                          fetch_info->host, fetch_info->port, strerror(errno));
            }
            freeaddrinfo(result);
        } else {
            LOG_ERROR("Host not found: %s (%s)", fetch_info->host, gai_strerror(err));
        }
        
        close(server_socket);
    } else {
        LOG_ERROR("Socket creation failed: %s", strerror(errno));
    }
    
    LOG_INFO("Fetch complete: %s (%zu bytes, %s, cached: %s)", 
             fetch_info->url, total_received, 
             success ? "success" : "failed",
             entry->should_cache ? "yes" : "no"); 
    
    pthread_mutex_lock(&entry->lock);
    entry->in_progress = 0;
    entry->ready = 1;
    if (!success || total_received == 0) {
        entry->error = 1;  
    }
    pthread_cond_broadcast(&entry->ready_cond);
    pthread_mutex_unlock(&entry->lock);
    
    if (!entry->error && total_received > 0 && entry->should_cache) {
        finalize_cache_entry(entry);
    }
    
    cache_entry_release(entry);
    free(fetch_info);
    return NULL;
}
