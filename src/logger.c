#include "logger.h"
#include <stdarg.h>
#include <string.h>
#include <errno.h>

log_level_t global_log_level = LOG_INFO;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file = NULL;

static const char* level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

static const char* level_colors[] = {
    "\033[36m",  // DEBUG 
    "\033[32m",  // INFO 
    "\033[33m",  // WARN 
    "\033[31m"   // ERROR 
};

#define COLOR_RESET "\033[0m"

void log_init(log_level_t level, const char *log_filename) {
    global_log_level = level;
    
    if (log_filename != NULL) {
        log_file = fopen(log_filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", log_filename);
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
}

void log_close(void) {
    if (log_file && log_file != stderr && log_file != stdout) {
        fclose(log_file);
        log_file = NULL;
    }
}

void log_message(log_level_t level, const char *file, int line,
                 const char *func, const char *fmt, ...) {
    if (level < global_log_level) {
        return;
    }
    
    if (!log_file) {
        log_file = stderr;
    }
    
    pthread_mutex_lock(&log_mutex);
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    int use_color = (log_file == stderr || log_file == stdout);
    
    if (use_color) {
        fprintf(log_file, "%s[%02d:%02d:%02d] [%-5s]%s ",
                level_colors[level],
                t->tm_hour, t->tm_min, t->tm_sec,
                level_strings[level],
                COLOR_RESET);
    } else {
        fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] [%-5s] ",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                level_strings[level]);
    }
    
    if (level == LOG_DEBUG) {
        fprintf(log_file, "[%s:%d %s()] ", file, line, func);
    }
    
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
    
    pthread_mutex_unlock(&log_mutex);
}
