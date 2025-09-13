#include "epaper_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "EPAPER";

#define EPAPER_CMD_POWER_ON        0x04
#define EPAPER_CMD_POWER_OFF       0x02
#define EPAPER_CMD_PANEL_SETTING   0x00
#define EPAPER_CMD_RESOLUTION      0x61
#define EPAPER_CMD_DATA_START      0x10
#define EPAPER_CMD_DISPLAY_REFRESH 0x12
#define EPAPER_CMD_DEEP_SLEEP      0x07

static esp_err_t epaper_send_command(epaper_handle_t *handle, uint8_t cmd)
{
    gpio_set_level(handle->dc_pin, 0);
    gpio_set_level(handle->cs_pin, 0);

    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd
    };

    esp_err_t ret = spi_device_transmit(handle->spi, &trans);
    gpio_set_level(handle->cs_pin, 1);

    return ret;
}

static esp_err_t epaper_send_data(epaper_handle_t *handle, const uint8_t *data, size_t len)
{
    gpio_set_level(handle->dc_pin, 1);
    gpio_set_level(handle->cs_pin, 0);

    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data
    };

    esp_err_t ret = spi_device_transmit(handle->spi, &trans);
    gpio_set_level(handle->cs_pin, 1);

    return ret;
}

esp_err_t epaper_init(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Initializing e-Paper display...");

    gpio_set_direction(handle->cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->busy_pin, GPIO_MODE_INPUT);

    gpio_set_level(handle->cs_pin, 1);

    gpio_set_level(handle->rst_pin, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(handle->rst_pin, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_EPAPER_MOSI_GPIO,
        .sclk_io_num = CONFIG_EPAPER_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 7
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return EPAPER_ERR_SPI;
    }

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return EPAPER_ERR_SPI;
    }

    epaper_send_command(handle, EPAPER_CMD_POWER_ON);
    epaper_wait_busy(handle, 5000);

    epaper_send_command(handle, EPAPER_CMD_PANEL_SETTING);
    uint8_t panel_data[] = {0x0F};
    epaper_send_data(handle, panel_data, sizeof(panel_data));

    epaper_send_command(handle, EPAPER_CMD_RESOLUTION);
    uint8_t resolution_data[] = {
        (handle->width >> 8) & 0xFF,
        handle->width & 0xFF,
        (handle->height >> 8) & 0xFF,
        handle->height & 0xFF
    };
    epaper_send_data(handle, resolution_data, sizeof(resolution_data));

    ESP_LOGI(TAG, "e-Paper initialized successfully");
    return ESP_OK;
}

esp_err_t epaper_deinit(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    epaper_sleep(handle);
    spi_bus_remove_device(handle->spi);
    spi_bus_free(SPI2_HOST);

    return ESP_OK;
}

esp_err_t epaper_clear(epaper_handle_t *handle, epaper_color_t color)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint32_t frame_size = (handle->width * handle->height * 3) / 8;
    uint8_t *buffer = heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return EPAPER_ERR_MEMORY;
    }

    memset(buffer, color, frame_size);

    esp_err_t ret = epaper_display_frame(handle, buffer);

    free(buffer);
    return ret;
}

esp_err_t epaper_display_frame(epaper_handle_t *handle, uint8_t *frame_buffer)
{
    if (handle == NULL || frame_buffer == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Displaying frame...");

    epaper_send_command(handle, EPAPER_CMD_DATA_START);

    uint32_t frame_size = (handle->width * handle->height * 3) / 8;

    const size_t chunk_size = 4096;
    for (size_t i = 0; i < frame_size; i += chunk_size) {
        size_t len = (i + chunk_size > frame_size) ? (frame_size - i) : chunk_size;
        epaper_send_data(handle, frame_buffer + i, len);
    }

    epaper_send_command(handle, EPAPER_CMD_DISPLAY_REFRESH);
    epaper_wait_busy(handle, 10000);

    ESP_LOGI(TAG, "Frame displayed successfully");
    return ESP_OK;
}

esp_err_t epaper_partial_update(epaper_handle_t *handle,
                                uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                uint8_t *buffer)
{
    if (handle == NULL || buffer == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    if (x + width > handle->width || y + height > handle->height) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Partial update not yet implemented");
    return EPAPER_ERR_INIT;
}

esp_err_t epaper_sleep(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Entering sleep mode...");

    epaper_send_command(handle, EPAPER_CMD_POWER_OFF);
    epaper_wait_busy(handle, 5000);

    epaper_send_command(handle, EPAPER_CMD_DEEP_SLEEP);
    uint8_t sleep_data[] = {0xA5};
    epaper_send_data(handle, sleep_data, sizeof(sleep_data));

    return ESP_OK;
}

esp_err_t epaper_wake(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Waking up from sleep...");

    gpio_set_level(handle->rst_pin, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(handle->rst_pin, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    return epaper_init(handle);
}

esp_err_t epaper_wait_busy(epaper_handle_t *handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (gpio_get_level(handle->busy_pin) == 1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) > timeout_ms) {
            ESP_LOGE(TAG, "Timeout waiting for busy signal");
            return EPAPER_ERR_TIMEOUT;
        }
    }

    return ESP_OK;
}

esp_err_t epaper_set_pixel(uint8_t *buffer, uint16_t x, uint16_t y,
                           epaper_color_t color, uint16_t width, uint16_t height)
{
    if (buffer == NULL || x >= width || y >= height) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint32_t pixel_index = y * width + x;
    uint32_t byte_index = (pixel_index * 3) / 8;
    uint8_t bit_offset = (pixel_index * 3) % 8;

    buffer[byte_index] &= ~(0x07 << bit_offset);
    buffer[byte_index] |= (color & 0x07) << bit_offset;

    if (bit_offset > 5) {
        buffer[byte_index + 1] &= ~(0x07 >> (8 - bit_offset));
        buffer[byte_index + 1] |= (color & 0x07) >> (8 - bit_offset);
    }

    return ESP_OK;
}