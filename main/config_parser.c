#include "config_parser.h"
#include "logger.h"
#include <string.h>
#include <ctype.h>

#define TAG "CONFIG"

static char* trim_whitespace(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';

    return str;
}

static esp_err_t parse_line(const char *line, char *key, char *value) {
    const char *equals = strchr(line, '=');
    if (equals == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t key_len = equals - line;
    if (key_len >= MAX_SSID_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    strncpy(key, line, key_len);
    key[key_len] = '\0';

    strcpy(value, equals + 1);

    strcpy(key, trim_whitespace(key));
    strcpy(value, trim_whitespace(value));

    return ESP_OK;
}

esp_err_t config_parse_file(const char *content, wifi_config_data_t *config) {
    if (content == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(wifi_config_data_t));
    config->is_valid = false;
    config->ip_mode = IP_MODE_DHCP; // デフォルトはDHCP

    char *content_copy = strdup(content);
    if (content_copy == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char *line = strtok(content_copy, "\n");

    while (line != NULL) {
        line = trim_whitespace(line);

        if (strlen(line) == 0 || line[0] == '#') {
            line = strtok(NULL, "\n");
            continue;
        }

        char key[MAX_SSID_LEN] = {0};
        char value[MAX_PASSWORD_LEN] = {0};

        if (parse_line(line, key, value) == ESP_OK) {
            if (strcasecmp(key, "SSID") == 0) {
                strncpy(config->ssid, value, MAX_SSID_LEN - 1);
                log_info(TAG, "Found SSID: %s", config->ssid);
            } else if (strcasecmp(key, "PASSWORD") == 0) {
                strncpy(config->password, value, MAX_PASSWORD_LEN - 1);
                log_info(TAG, "Found password (length: %d)", strlen(config->password));
            } else if (strcasecmp(key, "HIDDEN_SSID") == 0) {
                config->is_hidden_ssid = (strcasecmp(value, "true") == 0 ||
                                         strcasecmp(value, "yes") == 0 ||
                                         strcmp(value, "1") == 0);
                log_info(TAG, "Hidden SSID: %s", config->is_hidden_ssid ? "true" : "false");
            } else if (strcasecmp(key, "BSSID") == 0) {
                strncpy(config->bssid, value, sizeof(config->bssid) - 1);
                config->use_bssid = true;
                log_info(TAG, "Found BSSID: %s", config->bssid);
            } else if (strcasecmp(key, "IP_MODE") == 0) {
                if (strcasecmp(value, "static") == 0) {
                    config->ip_mode = IP_MODE_STATIC;
                    log_info(TAG, "IP mode: Static");
                } else {
                    config->ip_mode = IP_MODE_DHCP;
                    log_info(TAG, "IP mode: DHCP");
                }
            } else if (strcasecmp(key, "STATIC_IP") == 0) {
                strncpy(config->static_ip, value, MAX_IP_LEN - 1);
                log_info(TAG, "Static IP: %s", config->static_ip);
            } else if (strcasecmp(key, "STATIC_NETMASK") == 0) {
                strncpy(config->static_netmask, value, MAX_IP_LEN - 1);
                log_info(TAG, "Static Netmask: %s", config->static_netmask);
            } else if (strcasecmp(key, "STATIC_GATEWAY") == 0) {
                strncpy(config->static_gateway, value, MAX_IP_LEN - 1);
                log_info(TAG, "Static Gateway: %s", config->static_gateway);
            } else if (strcasecmp(key, "STATIC_DNS") == 0) {
                strncpy(config->static_dns, value, MAX_IP_LEN - 1);
                log_info(TAG, "Static DNS: %s", config->static_dns);
            }
        }

        line = strtok(NULL, "\n");
    }

    free(content_copy);

    config->auth_mode = 4;
    config->is_valid = (strlen(config->ssid) > 0);

    return config_validate(config);
}

const char* config_get_ssid(const wifi_config_data_t *config) {
    if (config == NULL || !config->is_valid) {
        return NULL;
    }
    return config->ssid;
}

const char* config_get_password(const wifi_config_data_t *config) {
    if (config == NULL || !config->is_valid) {
        return NULL;
    }
    return config->password;
}

esp_err_t config_validate(const wifi_config_data_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(config->ssid) == 0) {
        log_error(TAG, "SSID is empty");
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(config->ssid) > 32) {
        log_error(TAG, "SSID is too long");
        return ESP_ERR_INVALID_SIZE;
    }

    if (strlen(config->password) > 0 && strlen(config->password) < 8) {
        log_error(TAG, "Password must be at least 8 characters");
        return ESP_ERR_INVALID_SIZE;
    }

    if (strlen(config->password) > 64) {
        log_error(TAG, "Password is too long");
        return ESP_ERR_INVALID_SIZE;
    }

    // 固定IP設定の検証
    if (config->ip_mode == IP_MODE_STATIC) {
        if (strlen(config->static_ip) == 0) {
            log_error(TAG, "Static IP is required when IP_MODE is static");
            return ESP_ERR_INVALID_ARG;
        }
        if (strlen(config->static_netmask) == 0) {
            log_error(TAG, "Static netmask is required when IP_MODE is static");
            return ESP_ERR_INVALID_ARG;
        }
        if (strlen(config->static_gateway) == 0) {
            log_error(TAG, "Static gateway is required when IP_MODE is static");
            return ESP_ERR_INVALID_ARG;
        }
    }

    log_info(TAG, "Configuration validated successfully");
    return ESP_OK;
}