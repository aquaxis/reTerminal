#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include "esp_log.h"

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

void logger_init(void);
void logger_set_level(const char *tag, log_level_t level);
void log_debug(const char *tag, const char *format, ...);
void log_info(const char *tag, const char *format, ...);
void log_error(const char *tag, const char *format, ...);

#endif