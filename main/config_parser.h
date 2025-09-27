#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdbool.h>
#include "esp_err.h"

#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64
#define MAX_IP_LEN 16

typedef enum {
    IP_MODE_DHCP = 0,
    IP_MODE_STATIC = 1
} ip_mode_t;

typedef struct {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWORD_LEN];
    uint8_t auth_mode;
    bool is_hidden_ssid;
    char bssid[18];  // MAC address string format "XX:XX:XX:XX:XX:XX"
    bool use_bssid;
    ip_mode_t ip_mode;
    char static_ip[MAX_IP_LEN];
    char static_netmask[MAX_IP_LEN];
    char static_gateway[MAX_IP_LEN];
    char static_dns[MAX_IP_LEN];
    bool is_valid;
} wifi_config_data_t;

esp_err_t config_parse_file(const char *content, wifi_config_data_t *config);
const char* config_get_ssid(const wifi_config_data_t *config);
const char* config_get_password(const wifi_config_data_t *config);
esp_err_t config_validate(const wifi_config_data_t *config);

#endif