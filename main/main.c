#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "driver/gpio.h"
#include "sdio.h"
#include "bitmap.h"
#include "epaper_driver.h"

static const char *TAG = "MAIN";

#define EPAPER_CS_PIN    10
#define EPAPER_DC_PIN    11
#define EPAPER_RST_PIN   12
#define EPAPER_BUSY_PIN  13

void app_main(void)
{
    esp_err_t ret;

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

    // SD_EN(IO16)
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 1);

    gpio_reset_pin(GPIO_NUM_6);
    gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);

    // ----------
    // SDIO
    // ----------

    ESP_LOGI(TAG, "Initializing SD card...");
    ret = init_sd_card();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed");
        return;
    }

#if 1
    ESP_LOGI(TAG, "Loading BMP image from SD card...");
    bmp_image_t image;
    ret = load_bmp_from_sd("/sdcard/test.bmp", &image);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BMP image loaded successfully");
    }else{
        ESP_LOGE(TAG, "Failed to load BMP image, using test pattern");
    }

/*
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BMP image loaded successfully");

        uint8_t *epaper_buffer = malloc(800 * 480);
        if (epaper_buffer) {
            ret = convert_bmp_to_epaper(&image, epaper_buffer);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Displaying image on e-Paper...");
                ret = epaper_display_frame(&epaper, epaper_buffer);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Image displayed successfully");
                } else {
                    ESP_LOGE(TAG, "Failed to display image");
                }
            }
            free(epaper_buffer);
        } else {
            ESP_LOGE(TAG, "Failed to allocate e-Paper buffer");
        }

        free_bmp_image(&image);
    } else {
        ESP_LOGE(TAG, "Failed to load BMP image, using test pattern");

        uint8_t *test_buffer = malloc(800 * 480);
        if (test_buffer) {
            memset(test_buffer, EPAPER_COLOR_WHITE, 800 * 480);

            for (int y = 100; y < 200; y++) {
                for (int x = 100; x < 200; x++) {
                    test_buffer[y * 800 + x] = EPAPER_COLOR_BLACK;
                }
            }

            ESP_LOGI(TAG, "Displaying test pattern...");
            ret = epaper_display_frame(&epaper, test_buffer);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Test pattern displayed successfully");
            }
            free(test_buffer);
        }
    }
*/
#endif

    // ----------
    // e-Paper
    // ----------
    epaper_handle_t epaper = {
        .cs_pin = EPAPER_CS_PIN,
        .dc_pin = EPAPER_DC_PIN,
        .rst_pin = EPAPER_RST_PIN,
        .busy_pin = EPAPER_BUSY_PIN,
        .width = 800,
        .height = 480
    };

    ESP_LOGI(TAG, "Initializing e-Paper display...");
    ret = epaper_init(&epaper);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "e-Paper initialization failed");
        deinit_sd_card();
        return;
    }

    ESP_LOGI(TAG, "Clearing e-Paper display...");
    ret = epaper_clear(&epaper, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "e-Paper clear failed");
    }

    ESP_LOGI(TAG, "Displaying image on e-Paper...");
    ret = epaper_display_frame(&epaper, image.data);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Image displayed successfully");
    } else {
        ESP_LOGE(TAG, "Failed to display image");
    }

/*
    uint8_t *test_buffer = malloc(800 * 480 / 2);
    if (test_buffer) {
        memset(test_buffer, 0x11, 800 * 480 / 2);

        for (int y = 100; y < 200; y++) {
            for (int x = 100; x < 200; x++) {
                test_buffer[y * 800 + x] = 0x00;
            }
        }

        ESP_LOGI(TAG, "Displaying test pattern...");
        ret = epaper_display_frame(&epaper, test_buffer);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Test pattern displayed successfully");
        }
        free(test_buffer);
    }
*/
    while (1) {
        gpio_set_level(GPIO_NUM_6, 1);
        ESP_LOGI(TAG, "LED ON - System running with e-Paper display...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_6, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}