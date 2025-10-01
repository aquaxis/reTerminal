#include "wifi_manager.h"
#include "logger.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet.h"
#include <sys/socket.h>

#define TAG "WIFI"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_MAX_RETRY     5

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static wifi_status_t s_wifi_status = WIFI_STATUS_DISCONNECTED;
static esp_netif_t *s_wifi_netif = NULL;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        s_wifi_status = WIFI_STATUS_CONNECTING;
        log_info(TAG, "WiFi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            s_wifi_status = WIFI_STATUS_CONNECTING;
            log_info(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_status = WIFI_STATUS_ERROR;
            log_error(TAG, "Failed to connect to AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_info(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_wifi_status = WIFI_STATUS_CONNECTED;
    }
}

esp_err_t wifi_manager_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_wifi_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    log_info(TAG, "WiFi initialized successfully");
    return ESP_OK;
}

static esp_err_t parse_bssid_string(const char *bssid_str, uint8_t *bssid_arr) {
    int values[6];
    if (sscanf(bssid_str, "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 6; i++) {
        bssid_arr[i] = (uint8_t)values[i];
    }
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const wifi_config_data_t *config) {
    if (config == NULL || !config->is_valid) {
        return ESP_ERR_INVALID_ARG;
    }

    // 静的IP設定の場合
    if (config->ip_mode == IP_MODE_STATIC) {
        esp_netif_ip_info_t ip_info;
        esp_netif_dns_info_t dns_info;

        // DHCPクライアントを停止（既に停止している場合はエラーを無視）
        esp_err_t dhcp_ret = esp_netif_dhcpc_stop(s_wifi_netif);
        if (dhcp_ret != ESP_OK && dhcp_ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            log_error(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(dhcp_ret));
            return dhcp_ret;
        }

        // 静的IP設定
        inet_pton(AF_INET, config->static_ip, &ip_info.ip);
        inet_pton(AF_INET, config->static_netmask, &ip_info.netmask);
        inet_pton(AF_INET, config->static_gateway, &ip_info.gw);

        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_wifi_netif, &ip_info));

        // DNS設定（指定されている場合）
        if (strlen(config->static_dns) > 0) {
            inet_pton(AF_INET, config->static_dns, &dns_info.ip);
            ESP_ERROR_CHECK(esp_netif_set_dns_info(s_wifi_netif, ESP_NETIF_DNS_MAIN, &dns_info));
        }

        log_info(TAG, "Static IP configured: %s, Netmask: %s, Gateway: %s",
                 config->static_ip, config->static_netmask, config->static_gateway);
    } else {
        // DHCP設定の場合
        ESP_ERROR_CHECK(esp_netif_dhcpc_start(s_wifi_netif));
        log_info(TAG, "DHCP client started");
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
            .scan_method = config->is_hidden_ssid ? WIFI_ALL_CHANNEL_SCAN : WIFI_FAST_SCAN,
            .bssid_set = config->use_bssid ? 1 : 0,
        },
    };

    strcpy((char*)wifi_config.sta.ssid, config->ssid);
    strcpy((char*)wifi_config.sta.password, config->password);

    if (config->use_bssid) {
        if (parse_bssid_string(config->bssid, wifi_config.sta.bssid) != ESP_OK) {
            log_error(TAG, "Failed to parse BSSID: %s", config->bssid);
            return ESP_ERR_INVALID_ARG;
        }
        log_info(TAG, "Using BSSID: %s", config->bssid);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    log_info(TAG, "Connecting to SSID: %s%s", config->ssid,
             config->is_hidden_ssid ? " (Hidden)" : "");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        log_info(TAG, "Connected to AP successfully");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        log_error(TAG, "Failed to connect to AP");
        return ESP_FAIL;
    } else {
        log_error(TAG, "Unexpected event");
        return ESP_FAIL;
    }
}

esp_err_t wifi_manager_disconnect(void) {
    esp_err_t ret;

    // WiFi接続を切断
    ret = esp_wifi_disconnect();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
        log_error(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
    }

    // WiFiを停止
    ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
        log_error(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_status = WIFI_STATUS_DISCONNECTED;
    s_retry_num = 0;  // リトライカウンタをリセット
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);  // イベントビットをクリア
    log_info(TAG, "Disconnected from WiFi");
    return ESP_OK;
}

wifi_status_t wifi_manager_get_status(void) {
    return s_wifi_status;
}

esp_err_t wifi_manager_get_ip_info(char *ip_str, size_t ip_str_len, char *netmask_str, size_t netmask_str_len, char *gateway_str, size_t gateway_str_len) {
    if (s_wifi_netif == NULL || s_wifi_status != WIFI_STATUS_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_wifi_netif, &ip_info);
    if (ret != ESP_OK) {
        log_error(TAG, "Failed to get IP info: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ip_str != NULL && ip_str_len > 0) {
        snprintf(ip_str, ip_str_len, IPSTR, IP2STR(&ip_info.ip));
    }

    if (netmask_str != NULL && netmask_str_len > 0) {
        snprintf(netmask_str, netmask_str_len, IPSTR, IP2STR(&ip_info.netmask));
    }

    if (gateway_str != NULL && gateway_str_len > 0) {
        snprintf(gateway_str, gateway_str_len, IPSTR, IP2STR(&ip_info.gw));
    }

    return ESP_OK;
}

void wifi_manager_deinit(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    esp_netif_deinit();
    vEventGroupDelete(s_wifi_event_group);
    log_info(TAG, "WiFi deinitialized");
}