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
#include "logger.h"
#include "config_parser.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

#define EPAPER_CS_PIN    10
#define EPAPER_DC_PIN    11
#define EPAPER_RST_PIN   12
#define EPAPER_BUSY_PIN  13

void app_main(void)
{
    esp_err_t ret;

    logger_init();
    log_info(TAG, "ESP32-S3 WiFi Sample Application Starting");

    wifi_config_data_t wifi_cfg;
    char config_buffer[1024] = {0};

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

    sdio_init(&sdio_ctx);

    gpio_reset_pin(GPIO_NUM_6);
    gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);

    // ----------
    // SDIO
    // ----------

    ESP_LOGI(TAG, "Initializing SD card...");
    ret = sdio_mount(&sdio_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed");
        return;
    }
    sdio_ctx.is_mounted = true;

    // WiFi

    ret = sdio_read_file(&sdio_ctx, CONFIG_FILE, config_buffer, sizeof(config_buffer));
    if (ret != ESP_OK) {
        log_error(TAG, "Failed to read config file");
        sdio_unmount(&sdio_ctx);
        sdio_deinit(&sdio_ctx);
        return;
    }

    ret = config_parse_file(config_buffer, &wifi_cfg);
    if (ret != ESP_OK) {
        log_error(TAG, "Failed to parse config file");
        sdio_unmount(&sdio_ctx);
        sdio_deinit(&sdio_ctx);
        return;
    }

//    sdio_unmount(&sdio_ctx);
//    sdio_deinit(&sdio_ctx);

    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        log_error(TAG, "Failed to initialize WiFi");
        return;
    }

    ret = wifi_manager_connect(&wifi_cfg);
    if (ret != ESP_OK) {
        log_error(TAG, "Failed to connect to WiFi");
        wifi_manager_deinit();
        return;
    }

    log_info(TAG, "Successfully connected to WiFi network: %s", config_get_ssid(&wifi_cfg));

    char ip_str[16];
    char netmask_str[16];
    char gateway_str[16];

    if (wifi_manager_get_ip_info(ip_str, sizeof(ip_str), netmask_str, sizeof(netmask_str), gateway_str, sizeof(gateway_str)) == ESP_OK) {
        log_info(TAG, "IP Address: %s", ip_str);
        log_info(TAG, "Netmask: %s", netmask_str);
        log_info(TAG, "Gateway: %s", gateway_str);
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
        wifi_status_t status = wifi_manager_get_status();
        if (status == WIFI_STATUS_CONNECTED) {
            if (wifi_manager_get_ip_info(ip_str, sizeof(ip_str), NULL, 0, NULL, 0) == ESP_OK) {
                log_debug(TAG, "WiFi connected - IP: %s", ip_str);
            } else {
                log_debug(TAG, "WiFi connected - IP retrieval failed");
            }
        } else {
            log_info(TAG, "WiFi status: %d", status);
        }

        gpio_set_level(GPIO_NUM_6, 1);
        ESP_LOGI(TAG, "LED ON - System running with e-Paper display...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        gpio_set_level(GPIO_NUM_6, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}