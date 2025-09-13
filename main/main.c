#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "reTerminal E1002 e-Paper Application Starting...");
    ESP_LOGI(TAG, "Target: XIAO-ESP32-S3");
    ESP_LOGI(TAG, "Display: GDEP073E01 (7.3inch E-Ink Spectra6)");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32-S3 Chip: %d cores, WiFi%s%s, revision %d",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            chip_info.revision);

    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());

    gpio_reset_pin(GPIO_NUM_21);
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(GPIO_NUM_21, 1);
        ESP_LOGI(TAG, "LED ON - System running...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_21, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}