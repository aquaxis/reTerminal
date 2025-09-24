#include "logger.h"
#include <stdarg.h>
#include <string.h>

void logger_init(void) {
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("LOGGER", "Logger initialized");
}

void logger_set_level(const char *tag, log_level_t level) {
    esp_log_level_t esp_level = ESP_LOG_INFO;

    switch (level) {
        case LOG_LEVEL_ERROR:
            esp_level = ESP_LOG_ERROR;
            break;
        case LOG_LEVEL_WARN:
            esp_level = ESP_LOG_WARN;
            break;
        case LOG_LEVEL_INFO:
            esp_level = ESP_LOG_INFO;
            break;
        case LOG_LEVEL_DEBUG:
            esp_level = ESP_LOG_DEBUG;
            break;
        case LOG_LEVEL_VERBOSE:
            esp_level = ESP_LOG_VERBOSE;
            break;
        default:
            esp_level = ESP_LOG_INFO;
            break;
    }

    esp_log_level_set(tag, esp_level);
}

void log_debug(const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_DEBUG, tag, format, args);
    va_end(args);
}

void log_info(const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_INFO, tag, format, args);
    va_end(args);
}

void log_error(const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_ERROR, tag, format, args);
    va_end(args);
}