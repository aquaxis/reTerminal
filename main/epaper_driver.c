#include "epaper_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "EPAPER";

#define EPAPER_WIDTH               800
#define EPAPER_HEIGHT              480
#define MAX_DISPLAY_BUFFER_SIZE    (EPAPER_WIDTH * EPAPER_HEIGHT / 2)

#define EPAPER_CMD_POWER_ON        0x04
#define EPAPER_CMD_POWER_OFF       0x02
#define EPAPER_CMD_PANEL_SETTING   0x00
#define EPAPER_CMD_RESOLUTION      0x61
#define EPAPER_CMD_DATA_START      0x10
#define EPAPER_CMD_DISPLAY_REFRESH 0x12
#define EPAPER_CMD_DEEP_SLEEP      0x07
#define EPAPER_CMD_PSR_1           0xAA
#define EPAPER_CMD_PSR_2           0x01
#define EPAPER_CMD_POF             0x05
#define EPAPER_CMD_PFS             0x08
#define EPAPER_CMD_PON             0x06
#define EPAPER_CMD_PSET            0x03
#define EPAPER_CMD_TSCON           0x60
#define EPAPER_CMD_TCON            0xE0
#define EPAPER_CMD_CDI             0x30
#define EPAPER_CMD_PLL             0x50
#define EPAPER_CMD_AUTO_MEASURING  0x84

static bool _init_display_done = false;
static uint8_t _pixel_buffer[MAX_DISPLAY_BUFFER_SIZE];
static uint16_t _current_page = 0;
static uint16_t _page_height = 0;
static uint16_t _pages = 0;

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
    gpio_set_level(handle->dc_pin, 1);

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

static esp_err_t epaper_reset(epaper_handle_t *handle)
{
    if (handle->rst_pin >= 0) {
        gpio_set_level(handle->rst_pin, 0);
        vTaskDelay(20 / portTICK_PERIOD_MS);
        gpio_set_level(handle->rst_pin, 1);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    return ESP_OK;
}

#define PSR         0x00
#define PWRR        0x01
#define POF         0x02
#define POFS        0x03
#define PON         0x04
#define BTST1       0x05
#define BTST2       0x06
#define DSLP        0x07
#define BTST3       0x08
#define DTM         0x10
#define DRF         0x12
#define PLL         0x30
#define CDI         0x50
#define TCON        0x60
#define TRES        0x61
#define REV         0x70
#define VDCS        0x82
#define T_VDCS      0x84
#define PWS         0xE3

static esp_err_t epaper_init_display_sequence(epaper_handle_t *handle)
{
    if (_init_display_done) return ESP_OK;

    ESP_LOGI(TAG, "function: epaper_init_display_sequence()");

    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 1);

    gpio_set_direction(handle->cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->busy_pin, GPIO_MODE_INPUT);

    // SD_EN(IO16)
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 1);

    epaper_reset(handle);
    epaper_wait_busy(handle, 1000);
    epaper_reset(handle);
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, 0xAA);
    uint8_t psr1_data[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    epaper_send_data(handle, psr1_data, sizeof(psr1_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, PWRR);
    uint8_t pwrr_data[] = {0x3F};
    epaper_send_data(handle, pwrr_data, sizeof(pwrr_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, PSR);
    uint8_t psr_data[] = {0x5F, 0x69};
    epaper_send_data(handle, psr_data, sizeof(psr_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, POFS);
    uint8_t pofs_data[] = {0x00, 0x54, 0x00, 0x44};
    epaper_send_data(handle, pofs_data, sizeof(pofs_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, BTST1);
    uint8_t btst1_data[] = {0x40, 0x1F, 0x1F, 0x2C};
    epaper_send_data(handle, btst1_data, sizeof(btst1_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x17, 0x49};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, BTST3);
    uint8_t btst3_data[] = {0x6F, 0x1F, 0x1F, 0x22};
    epaper_send_data(handle, btst3_data, sizeof(btst3_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, PLL);
    uint8_t pll_data[] = {0x00};
    epaper_send_data(handle, pll_data, sizeof(pll_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, CDI);
    uint8_t cdi_data[] = {0x3F};
    epaper_send_data(handle, cdi_data, sizeof(cdi_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, TCON);
    uint8_t tcon_data[] = {0x02, 0x00};
    epaper_send_data(handle, tcon_data, sizeof(tcon_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, TRES);
    uint8_t tres_data[] = {0x03, 0x20, 0x01, 0xe0};
    epaper_send_data(handle, tres_data, sizeof(tres_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, T_VDCS);
    uint8_t vdcs_data[] = {0x01};
    epaper_send_data(handle, vdcs_data, sizeof(vdcs_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, PWS);
    uint8_t pws_data[] = {0x2F};
    epaper_send_data(handle, pws_data, sizeof(pws_data));
    epaper_wait_busy(handle, 1000);

    // PON
    epaper_send_command(handle, 0x04);
//    uint8_t pon_data[] = {0x00};
//    epaper_send_data(handle, pon_data, sizeof(pon_data));
    epaper_wait_busy(handle, 1000);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    _init_display_done = true;
    return ESP_OK;
}
#if 0
static esp_err_t epaper_init_display_sequence(epaper_handle_t *handle)
{
    if (_init_display_done) return ESP_OK;

    ESP_LOGI(TAG, "function: epaper_init_display_sequence()");

    epaper_reset(handle);
    epaper_wait_busy(handle, 1000);
    epaper_reset(handle);
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PSR_1);
    uint8_t psr1_data[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    epaper_send_data(handle, psr1_data, sizeof(psr1_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PSR_2);
    uint8_t psr2_data[] = {0x3F};
    epaper_send_data(handle, psr2_data, sizeof(psr2_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PANEL_SETTING);
    uint8_t panel_data[] = {0x5F, 0x69};
    epaper_send_data(handle, panel_data, sizeof(panel_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_POF);
    uint8_t pof_data[] = {0x00, 0x54, 0x00, 0x44};
    epaper_send_data(handle, pof_data, sizeof(pof_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PFS);
    uint8_t pfs_data[] = {0x40, 0x1F, 0x1F, 0x22};
    epaper_send_data(handle, pfs_data, sizeof(pfs_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PON);
    uint8_t pon_data[] = {0x6F, 0x1F, 0x14, 0x00};
    epaper_send_data(handle, pon_data, sizeof(pon_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PSET);
    uint8_t pset_data[] = {0x00, 0x54, 0x00, 0x44};
    epaper_send_data(handle, pset_data, sizeof(pset_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_TSCON);
    uint8_t tscon_data[] = {0x02, 0x00};
    epaper_send_data(handle, tscon_data, sizeof(tscon_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_TCON);
    uint8_t tcon_data[] = {0x00};
    epaper_send_data(handle, tcon_data, sizeof(tcon_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_CDI);
    uint8_t cdi_data[] = {0x08};
    epaper_send_data(handle, cdi_data, sizeof(cdi_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_PLL);
    uint8_t pll_data[] = {0x3F};
    epaper_send_data(handle, pll_data, sizeof(pll_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_RESOLUTION);
    uint8_t resolution_data[] = {0x03, 0x20, 0x01, 0xE0};
    epaper_send_data(handle, resolution_data, sizeof(resolution_data));
    epaper_wait_busy(handle, 1000);

    epaper_send_command(handle, EPAPER_CMD_AUTO_MEASURING);
    uint8_t auto_data[] = {0x01};
    epaper_send_data(handle, auto_data, sizeof(auto_data));
    epaper_wait_busy(handle, 1000);

    _init_display_done = true;
    return ESP_OK;
}
#endif
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
    gpio_set_level(handle->dc_pin, 1);
    gpio_set_level(handle->rst_pin, 1);

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = 9,
        .sclk_io_num = 7,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
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

    handle->width = EPAPER_WIDTH;
    handle->height = EPAPER_HEIGHT;

    _page_height = EPAPER_HEIGHT;
    _pages = 1;
    _current_page = 0;

    memset(_pixel_buffer, 0x11, sizeof(_pixel_buffer));

    esp_err_t init_ret = epaper_init_display_sequence(handle);
    if (init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display sequence");
        return init_ret;
    }

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

static uint8_t color7(epaper_color_t color)
{
    switch (color) {
        case EPAPER_COLOR_BLACK:  return 0x00;
        case EPAPER_COLOR_WHITE:  return 0x01;
        case EPAPER_COLOR_GREEN:  return 0x02;
        case EPAPER_COLOR_BLUE:   return 0x03;
        case EPAPER_COLOR_RED:    return 0x04;
        case EPAPER_COLOR_YELLOW: return 0x05;
        default: return 0x01;
    }
}

esp_err_t epaper_clear(epaper_handle_t *handle, epaper_color_t color)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint8_t pv = color7(color);
    uint8_t pv2 = pv | (pv << 4);

    memset(_pixel_buffer, pv2, sizeof(_pixel_buffer));

    esp_err_t ret = epaper_display_frame(handle, _pixel_buffer);
    if (ret == ESP_OK) {
        epaper_sleep(handle);
    }

    return ret;
}

esp_err_t epaper_display_frame(epaper_handle_t *handle, uint8_t *frame_buffer)
{
    if (handle == NULL || frame_buffer == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    if (!_init_display_done) {
        epaper_init_display_sequence(handle);
    }

    ESP_LOGI(TAG, "Displaying frame...");

    epaper_send_command(handle, EPAPER_CMD_DATA_START);

    gpio_set_level(handle->cs_pin, 0);

    uint32_t frame_size = sizeof(_pixel_buffer);
    const size_t chunk_size = 4096;

    for (size_t i = 0; i < frame_size; i += chunk_size) {
        size_t len = (i + chunk_size > frame_size) ? (frame_size - i) : chunk_size;

        spi_transaction_t trans = {
            .length = len * 8,
            .tx_buffer = frame_buffer + i
        };

        spi_device_transmit(handle->spi, &trans);
    }

    gpio_set_level(handle->cs_pin, 1);

    esp_err_t ret = epaper_refresh(handle, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh display");
        return ret;
    }

    ESP_LOGI(TAG, "Frame displayed successfully");
    return ESP_OK;
}

esp_err_t epaper_refresh(epaper_handle_t *handle, bool partial_update)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    epaper_send_command(handle, EPAPER_CMD_POWER_ON);
    epaper_wait_busy(handle, 45000);

    epaper_send_command(handle, EPAPER_CMD_DISPLAY_REFRESH);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
    epaper_wait_busy(handle, 45000);

    if (!partial_update) {
        epaper_send_command(handle, EPAPER_CMD_POWER_OFF);
        epaper_wait_busy(handle, 5000);
    }

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

    epaper_send_command(handle, EPAPER_CMD_DEEP_SLEEP);
    uint8_t sleep_data[] = {0xA5};
    epaper_send_data(handle, sleep_data, sizeof(sleep_data));

    vTaskDelay(200 / portTICK_PERIOD_MS);
    _init_display_done = false;

    return ESP_OK;
}

esp_err_t epaper_wake(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Waking up from sleep...");

    epaper_reset(handle);
    epaper_wait_busy(handle, 1000);

    return epaper_init_display_sequence(handle);
}

esp_err_t epaper_wait_busy(epaper_handle_t *handle, uint32_t timeout_ms)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (gpio_get_level(handle->busy_pin) == 0) {
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

    if ((x < 0) || (x >= EPAPER_WIDTH) || (y < 0) || (y >= EPAPER_HEIGHT)) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint32_t i = x / 2 + (uint32_t)y * (width / 2);
    uint8_t pv = color7(color);

    if (x & 1) {
        buffer[i] = (buffer[i] & 0xF0) | pv;
    } else {
        buffer[i] = (buffer[i] & 0x0F) | (pv << 4);
    }

    return ESP_OK;
}

esp_err_t epaper_fill_screen(epaper_handle_t *handle, epaper_color_t color)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    uint8_t pv = color7(color);
    uint8_t pv2 = pv | (pv << 4);

    memset(_pixel_buffer, pv2, sizeof(_pixel_buffer));

    return ESP_OK;
}

esp_err_t epaper_display(epaper_handle_t *handle)
{
    return epaper_display_frame(handle, _pixel_buffer);
}

esp_err_t epaper_display_partial(epaper_handle_t *handle, bool partial_update)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    esp_err_t ret = epaper_display_frame(handle, _pixel_buffer);
    if (ret == ESP_OK && !partial_update) {
        epaper_sleep(handle);
    }

    return ret;
}

esp_err_t gdep073e01_init_specific(epaper_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing GDEP073E01 specific settings...");

    handle->width = 800;
    handle->height = 480;

    return ESP_OK;
}

esp_err_t gdep073e01_clear_white(epaper_handle_t *handle)
{
    ESP_LOGI(TAG, "Clearing GDEP073E01 to white...");

    return epaper_clear(handle, EPAPER_COLOR_WHITE);
}