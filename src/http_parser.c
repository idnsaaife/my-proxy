#include "http_parser.h"
#include "../lib/picohttpparser.h"
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

int parse_http_request(const char *buf, size_t buf_len,
                       char *method_out, char *url_out,
                       char *host_out, int *port_out, char *path_out) {
    const char *method, *path;
    size_t method_len, path_len;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);
    
    int pret = phr_parse_request(buf, buf_len,
                                  &method, &method_len,
                                  &path, &path_len,
                                  &minor_version,
                                  headers, &num_headers,
                                  0);
    
    if (pret < 0) {
        return -1;
    }
    
    snprintf(method_out, 16, "%.*s", (int)method_len, method);
    
    if (method_len != 3 || memcmp(method, "GET", 3) != 0) {
        return -1;
    }
    
    snprintf(url_out, 2048, "%.*s", (int)path_len, path);
    
    host_out[0] = '\0';
    for (size_t i = 0; i < num_headers; i++) {
        if (headers[i].name_len == 4 && 
            strncasecmp(headers[i].name, "Host", 4) == 0) {
            snprintf(host_out, 256, "%.*s", 
                    (int)headers[i].value_len, headers[i].value);
            break;
        }
    }
    
    if (strncmp(path, "http://", 7) == 0) {
        const char *host_start = path + 7;
        const char *path_start = strchr(host_start, '/');
        const char *port_start = strchr(host_start, ':');
        
        if (path_start) {
            strncpy(path_out, path_start, 1024);
            path_out[1023] = '\0';
        } else {
            strcpy(path_out, "/");
        }
        
        if (port_start && (!path_start || port_start < path_start)) {
            size_t host_len = port_start - host_start;
            snprintf(host_out, 256, "%.*s", (int)host_len, host_start);
            *port_out = atoi(port_start + 1);
        } else {
            size_t host_len = path_start ? 
                             (path_start - host_start) : strlen(host_start);
            snprintf(host_out, 256, "%.*s", (int)host_len, host_start);
            *port_out = 80;
        }
    } else {
        strncpy(path_out, url_out, 1024);
        path_out[1023] = '\0';
        *port_out = 80;
    }
    
    return pret;
}
