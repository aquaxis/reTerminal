#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t http_server_init(void);
esp_err_t http_server_start(void);
esp_err_t http_server_stop(void);
httpd_handle_t http_server_get_handle(void);

#endif