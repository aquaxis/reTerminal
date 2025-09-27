#include "http_server.h"
#include "file_handler.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HTTP_SERVER";
static httpd_handle_t server = NULL;

static esp_err_t register_uri_handlers(void) {
    httpd_uri_t file_get = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = handle_file_get,
        .user_ctx  = NULL
    };

    httpd_uri_t file_post = {
        .uri       = "/*",
        .method    = HTTP_POST,
        .handler   = handle_file_post,
        .user_ctx  = NULL
    };

    httpd_uri_t file_delete = {
        .uri       = "/*",
        .method    = HTTP_DELETE,
        .handler   = handle_file_delete,
        .user_ctx  = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &file_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &file_post));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &file_delete));

    ESP_LOGI(TAG, "URI handlers registered");
    return ESP_OK;
}

esp_err_t http_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_open_sockets = 5;
    config.max_uri_handlers = 10;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Increase timeouts for file uploads
    config.recv_wait_timeout = 30;  // 30 seconds for receiving data
    config.send_wait_timeout = 30;  // 30 seconds for sending data

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started successfully");
        register_uri_handlers();
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t http_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "HTTP server already started");
        return ESP_OK;
    }
    return http_server_init();
}

esp_err_t http_server_stop(void) {
    if (server == NULL) {
        ESP_LOGW(TAG, "HTTP server not running");
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(server);
    if (ret == ESP_OK) {
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
    }

    return ret;
}

httpd_handle_t http_server_get_handle(void) {
    return server;
}