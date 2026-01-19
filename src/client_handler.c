#include "client_handler.h"
#include "cache.h"
#include "config.h"
#include "http_parser.h"
#include "server_fetch.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>


void send_to_client(int client_socket, cache_entry_t *entry) {
    pthread_mutex_lock(&entry->lock);
    
    while (!entry->ready) {
        pthread_cond_wait(&entry->ready_cond, &entry->lock);
    }
    
    if (entry->error || entry->data_size == 0) {
        pthread_mutex_unlock(&entry->lock);
        const char *error_response = 
            "HTTP/1.0 502 Bad Gateway\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n";
        send(client_socket, error_response, strlen(error_response), 0);
        LOG_WARN("Sent error response to client for %s", entry->url);
        return;
    }
    
    size_t total_sent = 0;
    while (total_sent < entry->data_size) {
        ssize_t bytes_sent = send(client_socket, 
                                  entry->data + total_sent,
                                  entry->data_size - total_sent, 
                                  0);
        if (bytes_sent < 0) {
            LOG_ERROR("Send to client failed: %s", strerror(errno));
            break;
        }
        total_sent += bytes_sent;
    }
    
    pthread_mutex_unlock(&entry->lock);
    LOG_INFO("Sent %zu bytes to client for %s", total_sent, entry->url);
}


void handle_client_direct(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    
    LOG_INFO("New connection from %s", inet_ntoa(client_info->client_addr.sin_addr));
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            LOG_ERROR("Recv failed: %s", strerror(errno));
        } else {
            LOG_DEBUG("Client closed connection immediately");
        }
        close(client_socket);
        free(client_info);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    char method[HTTP_METHOD_MAX_LEN], url[HTTP_URL_MAX_LEN], host[HTTP_HOST_MAX_LEN], path[HTTP_PATH_MAX_LEN];
    int port;
    
    int parse_result = parse_http_request(buffer, bytes_read,
                                          method, url, host, &port, path);
    
    if (parse_result < 0) {
        LOG_WARN("Failed to parse HTTP request from %s", 
                 inet_ntoa(client_info->client_addr.sin_addr));
        const char *error_response = "HTTP/1.0 400 Bad Request\r\n\r\n";
        send(client_socket, error_response, strlen(error_response), 0);
        close(client_socket);
        free(client_info);
        return;
    }
    
    LOG_INFO("Request: %s %s (Host: %s:%d, Path: %s)", method, url, host, port, path);
    
    cache_entry_t *entry = find_cache_entry(url);
    
    if (entry != NULL) {
        send_to_client(client_socket, entry);
        cache_entry_release(entry);
    } else {
        entry = create_cache_entry(url);
        if (entry == NULL) {
            LOG_ERROR("Failed to create cache entry for %s", url);
            const char *error_response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
            send(client_socket, error_response, strlen(error_response), 0);
            close(client_socket);
            free(client_info);
            return;
        }
        
        cache_entry_addref(entry);
        
        fetch_info_t *fetch_info = malloc(sizeof(fetch_info_t));
        if (!fetch_info) {
            LOG_ERROR("Failed to allocate fetch_info: %s", strerror(errno));
            cache_entry_release(entry);
            cache_entry_release(entry);
            const char *error_response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
            send(client_socket, error_response, strlen(error_response), 0);
            close(client_socket);
            free(client_info);
            return;
        }
        
        fetch_info->entry = entry;
        snprintf(fetch_info->url, sizeof(fetch_info->url), "%s", url);
        snprintf(fetch_info->host, sizeof(fetch_info->host), "%s", host);
        snprintf(fetch_info->path, sizeof(fetch_info->path), "%s", path);
        fetch_info->port = port;
        
        pthread_t fetch_thread;
        if (pthread_create(&fetch_thread, NULL, fetch_from_server, fetch_info) != 0) {
            LOG_ERROR("Failed to create fetch thread: %s", strerror(errno));
            cache_entry_release(entry);
            cache_entry_release(entry);
            free(fetch_info);
            const char *error_response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
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
    LOG_DEBUG("Connection closed for %s", inet_ntoa(client_info->client_addr.sin_addr));
}


void handle_client_wrapper(void *arg) {
    client_info_t *client_info = arg;
    handle_client_direct(client_info);
    free(client_info);  
}
