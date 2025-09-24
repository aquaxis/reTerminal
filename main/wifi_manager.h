#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "config_parser.h"

typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_ERROR
} wifi_status_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const wifi_config_data_t *config);
esp_err_t wifi_manager_disconnect(void);
wifi_status_t wifi_manager_get_status(void);
esp_err_t wifi_manager_get_ip_info(char *ip_str, size_t ip_str_len, char *netmask_str, size_t netmask_str_len, char *gateway_str, size_t gateway_str_len);
void wifi_manager_deinit(void);

#endif