#include "epaper_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GDEP073E01";

__attribute__((unused)) static const uint8_t gdep073e01_init_sequence[] = {
    0x00, 0x0F,
    0x61, 0x03, 0x20, 0x01, 0xE0,
    0x15, 0x00,
    0x50, 0x11, 0x07,
    0x60, 0x22
};

esp_err_t gdep073e01_init_specific(epaper_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing GDEP073E01 specific settings...");

    handle->width = 800;
    handle->height = 480;

    return ESP_OK;
}

esp_err_t gdep073e01_clear_white(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Clearing display to white...");

    return epaper_clear(handle, EPAPER_COLOR_WHITE);
}