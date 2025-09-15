#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "epaper_driver.h"

static const char *TAG = "MAIN";

void app_main(void)
{

    // Enable SD Card Power
    gpio_set_direction(16, GPIO_MODE_OUTPUT);
    gpio_set_level(16, 0);

    ESP_LOGI(TAG, "reTerminal E1002 e-Paper Application Starting...");
    ESP_LOGI(TAG, "Target: XIAO-ESP32-S3");
    ESP_LOGI(TAG, "Display: GDEP073E01 (7.3inch E-Ink Spectra6)");
#if 1
    gpio_set_direction(9, GPIO_MODE_OUTPUT);
    gpio_set_direction(7, GPIO_MODE_OUTPUT);
#endif 
    gpio_set_direction(10, GPIO_MODE_OUTPUT);
    gpio_set_direction(11, GPIO_MODE_OUTPUT);
    gpio_set_direction(12, GPIO_MODE_OUTPUT);

    gpio_set_level(12, 1);
    gpio_set_level(12, 0);
    gpio_set_level(12, 1);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32-S3 Chip: %d cores, WiFi%s%s, revision %d",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            chip_info.revision);

    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());

    epaper_handle_t epaper = {
        .cs_pin = CONFIG_EPAPER_CS_GPIO,
        .dc_pin = CONFIG_EPAPER_DC_GPIO,
        .rst_pin = CONFIG_EPAPER_RST_GPIO,
        .busy_pin = CONFIG_EPAPER_BUSY_GPIO,
        .width = 0,
        .height = 0
    };

    esp_err_t ret = gdep073e01_init_specific(&epaper);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GDEP073E01 specific settings");
    }

    ret = epaper_init_fast(&epaper);

    ret = epaper_init(&epaper);

    ret = epaper_display_white(&epaper);

/*
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize e-Paper display");
    } else {
        ESP_LOGI(TAG, "E-Paper display initialized successfully");

        ESP_LOGI(TAG, "Clearing display to white...");
        ret = gdep073e01_clear_white(&epaper);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear display to white");
        } else {
            ESP_LOGI(TAG, "Display cleared to white successfully");
        }
    }
*/
    // LED
    gpio_reset_pin(GPIO_NUM_6);
    gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(GPIO_NUM_6, 1);
        ESP_LOGI(TAG, "LED ON - System running...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_6, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}