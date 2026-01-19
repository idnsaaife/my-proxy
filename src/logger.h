#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <pthread.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} log_level_t;

extern log_level_t global_log_level;
extern pthread_mutex_t log_mutex;
extern FILE *log_file;

void log_init(log_level_t level, const char *log_filename);
void log_close(void);
void log_message(log_level_t level, const char *file, int line, 
                 const char *func, const char *fmt, ...);

#define LOG_DEBUG(...) \
    log_message(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_INFO(...) \
    log_message(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_WARN(...) \
    log_message(LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_ERROR(...) \
    log_message(LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif // LOGGER_H
