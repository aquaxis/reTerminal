#include "file_handler.h"
#include "sdio.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cJSON.h>
#include <unistd.h>

static const char *TAG = "FILE_HANDLER";

typedef struct {
    const char *ext;
    const char *mime;
} mime_map_t;

static const mime_map_t mime_map[] = {
    {".html", "text/html"},
    {".htm",  "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".bmp",  "image/bmp"},
    {".ico",  "image/x-icon"},
    {".svg",  "image/svg+xml"},
    {".txt",  "text/plain"},
    {".pdf",  "application/pdf"},
    {".zip",  "application/zip"},
    {".xml",  "text/xml"},
    {NULL, NULL}
};

const char* get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) {
        return "application/octet-stream";
    }

    for (int i = 0; mime_map[i].ext != NULL; i++) {
        if (strcasecmp(dot, mime_map[i].ext) == 0) {
            return mime_map[i].mime;
        }
    }
    return "application/octet-stream";
}

static bool is_safe_path(const char *path) {
    if (strstr(path, "..") != NULL) {
        return false;
    }
    if (path[0] != '/') {
        return false;
    }
    return true;
}

esp_err_t handle_file_get(httpd_req_t *req) {
    char filepath[1024];
    char *buf = NULL;
    size_t buf_len = 0;
    FILE *fd = NULL;
    struct stat file_stat;
    esp_err_t ret;

    ESP_LOGI(TAG, "GET request for URI: %s", req->uri);

    // Initialize and mount SDIO
    ret = sdio_init(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card initialization failed");
        return ESP_FAIL;
    }

    ret = sdio_mount(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card mount failed");
        return ESP_FAIL;
    }
    sdio_ctx.is_mounted = true;

    if (!sdio_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card not mounted");
        return ESP_FAIL;
    }

    if (!is_safe_path(req->uri)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid path");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT,uri);

    if (stat(filepath, &file_stat) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        return handle_directory_list(req);
    }

    fd = fopen(filepath, "rb");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, get_mime_type(filepath));

    buf = malloc(SCRATCH_BUFSIZE);
    if (!buf) {
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    do {
        buf_len = fread(buf, 1, SCRATCH_BUFSIZE, fd);
        if (buf_len > 0) {
            if (httpd_resp_send_chunk(req, buf, buf_len) != ESP_OK) {
                fclose(fd);
                free(buf);
                ESP_LOGE(TAG, "Failed to send file chunk");
                sdio_deinit(&sdio_ctx);
                return ESP_FAIL;
            }
        }
    } while (buf_len > 0);

    httpd_resp_send_chunk(req, NULL, 0);
    fclose(fd);
    free(buf);

    ESP_LOGI(TAG, "File sent: %s", filepath);
    sdio_deinit(&sdio_ctx);
    return ESP_OK;
}

esp_err_t handle_file_post(httpd_req_t *req) {
    char filepath[1024];
    char *buf = NULL;
    size_t received = 0;
    int remaining = req->content_len;
    FILE *fd = NULL;
    esp_err_t ret;

    ESP_LOGI(TAG, "POST request for URI: %s, Content-Length: %d", req->uri, remaining);

    // Check if this is an API request
    if (strncmp(req->uri, "/api/update", 11) == 0) {
        return handle_api_update(req);
    }

    // Initialize and mount SDIO
    ret = sdio_init(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card initialization failed");
        return ESP_FAIL;
    }

    ret = sdio_mount(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card mount failed");
        return ESP_FAIL;
    }
    sdio_ctx.is_mounted = true;

    if (!sdio_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card not mounted");
        return ESP_FAIL;
    }

    if (!is_safe_path(req->uri)) {
        ESP_LOGE(TAG, "Unsafe path: %s", req->uri);
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid path");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    if (remaining > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large: %d bytes (max: %d)", remaining, MAX_FILE_SIZE);
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "File too large");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    // Ensure proper path construction
    if (strlen(req->uri) == 0 || req->uri[0] != '/') {
        ESP_LOGE(TAG, "Invalid URI format: %s", req->uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI format");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT, req->uri);
    ESP_LOGI(TAG, "Attempting to create file: %s", filepath);

    // Try to create file with retry mechanism
    int retry_count = 0;
    while (retry_count < 3) {
        fd = fopen(filepath, "wb");
        if (fd) {
            break;
        }
        retry_count++;
        ESP_LOGW(TAG, "Failed to create file (attempt %d/3): %s", retry_count, filepath);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Short delay before retry
    }
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    buf = malloc(SCRATCH_BUFSIZE);
    if (!buf) {
        fclose(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        sdio_deinit(&sdio_ctx);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        size_t to_read = (remaining < SCRATCH_BUFSIZE) ? remaining : SCRATCH_BUFSIZE;
        received = httpd_req_recv(req, buf, to_read);

        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(fd);
            free(buf);
            ESP_LOGE(TAG, "File reception failed");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            sdio_deinit(&sdio_ctx);
            return ESP_FAIL;
        }

        if (fwrite(buf, 1, received, fd) != received) {
            fclose(fd);
            free(buf);
            ESP_LOGE(TAG, "File write failed");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file");
            sdio_deinit(&sdio_ctx);
            return ESP_FAIL;
        }

        // Force write to SD card periodically
        if ((remaining % (SCRATCH_BUFSIZE * 4)) == 0) {
            fflush(fd);
        }

        remaining -= received;
    }

    // Ensure all data is written to SD card
    fflush(fd);
    fsync(fileno(fd));
    fclose(fd);
    free(buf);

    httpd_resp_send(req, "File uploaded successfully", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "File uploaded: %s (%d bytes)", filepath, req->content_len);
    sdio_deinit(&sdio_ctx);
    return ESP_OK;
}

esp_err_t handle_directory_list(httpd_req_t *req) {
    char dirpath[1024];
    DIR *dir = NULL;
    struct dirent *entry;
    struct stat file_stat;
    char fullpath[1024];

    if (!sdio_is_mounted()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card not mounted");
        return ESP_FAIL;
    }

    if (!is_safe_path(req->uri)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid path");
        return ESP_FAIL;
    }

    snprintf(dirpath, sizeof(dirpath), "%s%s", MOUNT_POINT, req->uri);

    dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", dirpath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory not found");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "files", files);
    cJSON_AddStringToObject(root, "path", req->uri);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        int path_len = snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        if (path_len >= sizeof(fullpath)) {
            continue;
        }

        if (stat(fullpath, &file_stat) == 0) {
            cJSON *file = cJSON_CreateObject();
            cJSON_AddStringToObject(file, "name", entry->d_name);
            cJSON_AddNumberToObject(file, "size", file_stat.st_size);
            cJSON_AddBoolToObject(file, "isDir", S_ISDIR(file_stat.st_mode));
            cJSON_AddNumberToObject(file, "modified", file_stat.st_mtime);
            cJSON_AddItemToArray(files, file);
        }
    }

    closedir(dir);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_Delete(root);
    free(json_str);

    ESP_LOGI(TAG, "Directory listing sent: %s", dirpath);
    return ESP_OK;
}

esp_err_t handle_file_delete(httpd_req_t *req) {
    char filepath[1024];

    if (!sdio_is_mounted()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card not mounted");
        return ESP_FAIL;
    }

    if (!is_safe_path(req->uri)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid path");
        return ESP_FAIL;
    }

    snprintf(filepath, sizeof(filepath), "%s%s", MOUNT_POINT,req->uri);

    if (remove(filepath) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "File deleted successfully", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "File deleted: %s", filepath);
    return ESP_OK;
}

esp_err_t handle_api_update(httpd_req_t *req) {
    esp_err_t ret;
    bmp_image_t image;

    ESP_LOGI(TAG, "API UPDATE request received");

    // Initialize SDIO
    ret = sdio_init(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card initialization failed");
        return ESP_FAIL;
    }

    ret = sdio_mount(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card mount failed");
        return ESP_FAIL;
    }
    sdio_ctx.is_mounted = true;

    // Load BMP image from SD card
    ESP_LOGI(TAG, "Loading BMP image from SD card...");
    ret = load_bmp_from_sd("/sdcard/test.bmp", &image);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load BMP image");
        sdio_deinit(&sdio_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load test.bmp");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BMP image loaded successfully");
    sdio_deinit(&sdio_ctx);

    // Initialize e-Paper display
    epaper_handle_t epaper = {
        .cs_pin = 10,
        .dc_pin = 11,
        .rst_pin = 12,
        .busy_pin = 13,
        .width = 800,
        .height = 480
    };

    ESP_LOGI(TAG, "Initializing e-Paper display...");
    ret = epaper_init(&epaper);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "e-Paper initialization failed");
        free_bmp_image(&image);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "e-Paper initialization failed");
        return ESP_FAIL;
    }

    // Clear e-Paper display
    ESP_LOGI(TAG, "Clearing e-Paper display...");
    ret = epaper_clear(&epaper, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "e-Paper clear failed, continuing anyway");
    }

    // Display image on e-Paper
    ESP_LOGI(TAG, "Displaying image on e-Paper...");
    ret = epaper_display_frame(&epaper, image.data);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Image displayed successfully");
        httpd_resp_send(req, "{\"status\":\"success\",\"message\":\"Image displayed successfully\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        ESP_LOGE(TAG, "Failed to display image");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to display image on e-Paper");
    }

    // Cleanup
    epaper_deinit(&epaper);
    free_bmp_image(&image);

    return ret == ESP_OK ? ESP_OK : ESP_FAIL;
}