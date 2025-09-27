#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "esp_http_server.h"
#include "esp_err.h"
#include "bitmap.h"
#include "epaper_driver.h"

#define MAX_FILE_SIZE (5*1024*1024)
#define SCRATCH_BUFSIZE 4096

esp_err_t handle_file_get(httpd_req_t *req);
esp_err_t handle_file_post(httpd_req_t *req);
esp_err_t handle_directory_list(httpd_req_t *req);
esp_err_t handle_file_delete(httpd_req_t *req);
esp_err_t handle_api_update(httpd_req_t *req);

const char* get_mime_type(const char *filename);

#endif