#ifndef CONFIG_H
#define CONFIG_H

#define PORT 1234
#define MAX_CLIENTS 100
#define BUFFER_SIZE 8192

// Cache Configuration
#define MAX_CACHE_SIZE  (600 * 1024 * 1024)  // 600mb
#define MAX_OBJECT_SIZE (300 * 1024 * 1024) // 300mb
#define HASHTABLE_SIZE         1024

// Thread Pool Configuration
#define THREAD_POOL_SIZE        50
#define TASK_QUEUE_SIZE         512


// HTTP Protocol Limits
#define HTTP_METHOD_MAX_LEN     16
#define HTTP_URL_MAX_LEN        2048
#define HTTP_HOST_MAX_LEN       256
#define HTTP_PATH_MAX_LEN       1024
#define HTTP_HEADER_MAX_COUNT   100
#define HTTP_REQUEST_MAX_LEN    2048

#endif // CONFIG_H
