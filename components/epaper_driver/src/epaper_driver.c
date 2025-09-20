#include "epaper_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define GPIO_SPI 0

static const char *TAG = "EPAPER";

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

// 
/*
CONFIG_EPAPER_MOSI_GPIO=9
CONFIG_EPAPER_SCLK_GPIO=7
CONFIG_EPAPER_CS_GPIO=10
CONFIG_EPAPER_DC_GPIO=11
CONFIG_EPAPER_RST_GPIO=12
CONFIG_EPAPER_BUSY_GPIO=13
*/
#define EPD_W21_MOSI_0	gpio_set_level(9, 0)
#define EPD_W21_MOSI_1	gpio_set_level(9, 1)

#define EPD_W21_CLK_0	gpio_set_level(7, 0)
#define EPD_W21_CLK_1	gpio_set_level(7, 1)

#define EPD_W21_CS_0	gpio_set_level(10, 0)
#define EPD_W21_CS_1	gpio_set_level(10, 1)

#define EPD_W21_DC_0	gpio_set_level(11, 0)
#define EPD_W21_DC_1	gpio_set_level(11, 1)

#define EPD_W21_RST_0	gpio_set_level(12, 0)
#define EPD_W21_RST_1	gpio_set_level(12, 1)

void SPI_Delay(unsigned char xrate)
{
	while(xrate--);

}

void SPI_Write(unsigned char value)                                    
{                                                           
	unsigned char i;
	 SPI_Delay(1);
	for(i=0; i<8; i++)   
	{
		EPD_W21_CLK_0;
		SPI_Delay(1);
		if(value & 0x80)
			EPD_W21_MOSI_1;
		else
			EPD_W21_MOSI_0;		
		value = (value << 1);
		SPI_Delay(1);				
		EPD_W21_CLK_1;
		SPI_Delay(1);				
	}
}
#ifdef GPIO_SPI
//void EPD_W21_WriteCMD(unsigned char command)
static esp_err_t epaper_send_command(epaper_handle_t *handle, uint8_t command)
{
	SPI_Delay(1);
    EPD_W21_CS_0;                   
	EPD_W21_DC_0;		// command write
	SPI_Write(command);
	EPD_W21_CS_1;
    ESP_LOGI(TAG, "COMMAND: %02x", command);

    return 0;
}
#endif
void EPD_W21_WriteDATA_Start()
{
	SPI_Delay(1);
    EPD_W21_CS_0;                   
	EPD_W21_DC_1;		// data write
//	SPI_Write(data);
//	EPD_W21_CS_1;
//    ESP_LOGI(TAG, "DATA: %02x", data);
}

void EPD_W21_WriteDATA(unsigned char data)
{
	SPI_Delay(1);
//    EPD_W21_CS_0;                   
//	EPD_W21_DC_1;		// data write
	SPI_Write(data);
//	EPD_W21_CS_1;
//    ESP_LOGI(TAG, "DATA: %02x", data);
}

void EPD_W21_WriteDATA_End()
{
	SPI_Delay(1);
    EPD_W21_CS_1;                   
	EPD_W21_DC_1;		// data write
//	SPI_Write(data);
//	EPD_W21_CS_1;
//    ESP_LOGI(TAG, "DATA: %02x", data);
}
#ifdef GPIO_SPI
static esp_err_t epaper_send_data(epaper_handle_t *handle, const uint8_t *data, size_t len)
{
    EPD_W21_WriteDATA_Start();
    for(int i=0;i<len;i++)
    {
        EPD_W21_WriteDATA(data[i]);
    }
    EPD_W21_WriteDATA_End();
    return 0;
}
#else
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
    gpio_set_level(handle->dc_pin, 1);

    return ret;
}
#endif

static uint8_t epaper_color_get(uint8_t color)
{
    uint8_t data;
    switch(color)
    {
        case 0x00:
            data = 0x00;
            break;
        case 0xff:
            data = 0x01;
            break;
        case 0xfc:
            data = 0x02;
            break;
        case 0xE0:
            data = 0x03;
            break;
        case 0x03:
            data = 0x05;
            break;
        case 0x1c:
            data = 0x06;
            break;
        default:
            data = 0x00;
            break;
    }
    return data;
}

esp_err_t epaper_init_fast(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Initializing e-Paper display (fast mode)...");
#ifndef GPIO_SPI
    gpio_set_direction(handle->cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handle->rst_pin, 1);
    gpio_set_direction(handle->dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handle->dc_pin, 1);
    gpio_set_direction(handle->busy_pin, GPIO_MODE_INPUT);
/*
    gpio_set_level(handle->rst_pin, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(handle->rst_pin, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
*/
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_EPAPER_MOSI_GPIO,
        .sclk_io_num = CONFIG_EPAPER_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000,
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
#endif
    epaper_send_command(handle, 0xAA);
    uint8_t cmdh_data[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    epaper_send_data(handle, cmdh_data, sizeof(cmdh_data));

    epaper_send_command(handle, PWRR);
    uint8_t pwrr_data[] = {0x3F, 0x00, 0x32, 0x2A, 0x0E, 0x2A};
    epaper_send_data(handle, pwrr_data, sizeof(pwrr_data));

    epaper_send_command(handle, PSR);
    uint8_t psr_data[] = {0x5F, 0x69};
    epaper_send_data(handle, psr_data, sizeof(psr_data));

    epaper_send_command(handle, POFS);
    uint8_t pofs_data[] = {0x00, 0x54, 0x00, 0x44};
    epaper_send_data(handle, pofs_data, sizeof(pofs_data));

    epaper_send_command(handle, BTST1);
    uint8_t btst1_data[] = {0x40, 0x1F, 0x1F, 0x2C};
    epaper_send_data(handle, btst1_data, sizeof(btst1_data));

    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x16, 0x25};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));

    epaper_send_command(handle, BTST3);
    uint8_t btst3_data[] = {0x6F, 0x1F, 0x1F, 0x22};
    epaper_send_data(handle, btst3_data, sizeof(btst3_data));

    epaper_send_command(handle, 0x13);
    uint8_t ipc_data[] = {0x00, 0x04};
    epaper_send_data(handle, ipc_data, sizeof(ipc_data));

    epaper_send_command(handle, PLL);
    uint8_t pll_data[] = {0x02};
    epaper_send_data(handle, pll_data, sizeof(pll_data));

    epaper_send_command(handle, 0x41);
    uint8_t tse_data[] = {0x00};
    epaper_send_data(handle, tse_data, sizeof(tse_data));

    epaper_send_command(handle, CDI);
    uint8_t cdi_data[] = {0x3F};
    epaper_send_data(handle, cdi_data, sizeof(cdi_data));

    epaper_send_command(handle, TCON);
    uint8_t tcon_data[] = {0x02, 0x00};
    epaper_send_data(handle, tcon_data, sizeof(tcon_data));

    epaper_send_command(handle, TRES);
    uint8_t tres_data[] = {0x03, 0x20, 0x01, 0xE0};
    epaper_send_data(handle, tres_data, sizeof(tres_data));

    epaper_send_command(handle, VDCS);
    uint8_t vdcs_data[] = {0x1E};
    epaper_send_data(handle, vdcs_data, sizeof(vdcs_data));

    epaper_send_command(handle, T_VDCS);
    uint8_t tvdcs_data[] = {0x00};
    epaper_send_data(handle, tvdcs_data, sizeof(tvdcs_data));

    epaper_send_command(handle, 0x86);
    uint8_t agid_data[] = {0x00};
    epaper_send_data(handle, agid_data, sizeof(agid_data));

    epaper_send_command(handle, PWS);
    uint8_t pws_data[] = {0x2F};
    epaper_send_data(handle, pws_data, sizeof(pws_data));

    epaper_send_command(handle, 0xE0);
    uint8_t ccset_data[] = {0x00};
    epaper_send_data(handle, ccset_data, sizeof(ccset_data));

    epaper_send_command(handle, 0xE6);
    uint8_t tsset_data[] = {0x00};
    epaper_send_data(handle, tsset_data, sizeof(tsset_data));

    epaper_send_command(handle, 0x04);
    epaper_wait_busy(handle, 5000);

    ESP_LOGI(TAG, "e-Paper initialized successfully (fast mode)");
    return ESP_OK;
}

esp_err_t epaper_init(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Initializing e-Paper display...");
/*
    gpio_set_direction(handle->cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(handle->rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handle->rst_pin, 1);
    gpio_set_direction(handle->dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(handle->dc_pin, 1);
    gpio_set_direction(handle->busy_pin, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "cs_pin: %d", handle->cs_pin);
    ESP_LOGI(TAG, "dc_pin: %d", handle->dc_pin);
    ESP_LOGI(TAG, "rst_pin: %d", handle->rst_pin);
    ESP_LOGI(TAG, "busy_pin: %d", handle->busy_pin);

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
        .clock_speed_hz = 2 * 1000 * 1000,
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
*/
    epaper_send_command(handle, 0xAA);
    uint8_t cmdh_data[] = {0x49, 0x55, 0x20, 0x08, 0x09, 0x18};
    epaper_send_data(handle, cmdh_data, sizeof(cmdh_data));

    epaper_send_command(handle, PWRR);
    uint8_t pwrr_data[] = {0x3F};
    epaper_send_data(handle, pwrr_data, sizeof(pwrr_data));

    epaper_send_command(handle, PSR);
    uint8_t psr_data[] = {0x5F, 0x69};
    epaper_send_data(handle, psr_data, sizeof(psr_data));

    epaper_send_command(handle, POFS);
    uint8_t pofs_data[] = {0x00, 0x54, 0x00, 0x44};
    epaper_send_data(handle, pofs_data, sizeof(pofs_data));

    epaper_send_command(handle, BTST1);
    uint8_t btst1_data[] = {0x40, 0x1F, 0x1F, 0x2C};
    epaper_send_data(handle, btst1_data, sizeof(btst1_data));

    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x17, 0x49};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));

    epaper_send_command(handle, BTST3);
    uint8_t btst3_data[] = {0x6F, 0x1F, 0x1F, 0x22};
    epaper_send_data(handle, btst3_data, sizeof(btst3_data));

    epaper_send_command(handle, PLL);
    uint8_t pll_data[] = {0x08};
    epaper_send_data(handle, pll_data, sizeof(pll_data));

    epaper_send_command(handle, CDI);
    uint8_t cdi_data[] = {0x3F};
    epaper_send_data(handle, cdi_data, sizeof(cdi_data));

    epaper_send_command(handle, TCON);
    uint8_t tcon_data[] = {0x02, 0x00};
    epaper_send_data(handle, tcon_data, sizeof(tcon_data));

    epaper_send_command(handle, TRES);
    uint8_t tres_data[] = {0x03, 0x20, 0x01, 0xE0};
    epaper_send_data(handle, tres_data, sizeof(tres_data));

    epaper_send_command(handle, T_VDCS);
    uint8_t tvdcs_data[] = {0x01};
    epaper_send_data(handle, tvdcs_data, sizeof(tvdcs_data));

    epaper_send_command(handle, PWS);
    uint8_t pws_data[] = {0x2F};
    epaper_send_data(handle, pws_data, sizeof(pws_data));

    epaper_send_command(handle, 0x04);
    epaper_wait_busy(handle, 5000);

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

    epaper_send_command(handle, DTM);

    uint32_t frame_size = (handle->width * handle->height) / 2;

    const size_t chunk_size = 4096;
    for (size_t i = 0; i < frame_size; i += chunk_size) {
        size_t len = (i + chunk_size > frame_size) ? (frame_size - i) : chunk_size;
        epaper_send_data(handle, frame_buffer + i, len);
    }

    epaper_send_command(handle, PON);
    epaper_wait_busy(handle, 5000);

    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x17, 0x49};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));

    epaper_send_command(handle, DRF);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
    epaper_wait_busy(handle, 30000);

    epaper_send_command(handle, POF);
    uint8_t pof_data[] = {0x00};
    epaper_send_data(handle, pof_data, sizeof(pof_data));
    epaper_wait_busy(handle, 5000);

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

    epaper_send_command(handle, POF);
    uint8_t pof_data[] = {0x00};
    epaper_send_data(handle, pof_data, sizeof(pof_data));
    epaper_wait_busy(handle, 5000);

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

    ESP_LOGI(TAG, "Waiting for BUSY pin (GPIO%d) to go LOW, timeout: %d ms", handle->busy_pin, timeout_ms);

    int busy_level = gpio_get_level(handle->busy_pin);
    ESP_LOGI(TAG, "Initial BUSY pin level: %d", busy_level);

    if (busy_level == 1) {
        ESP_LOGI(TAG, "BUSY pin already HIGH, no wait needed");
        return ESP_OK;
    }

    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t wait_count = 0;

    while (gpio_get_level(handle->busy_pin) == 0) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        wait_count++;

        if (wait_count % 100 == 0) {
            ESP_LOGI(TAG, "Still waiting for BUSY pin, elapsed: %d ms",
                     (int)(xTaskGetTickCount() * portTICK_PERIOD_MS - start));
        }

        if ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) > timeout_ms) {
            ESP_LOGE(TAG, "Timeout waiting for busy signal after %d ms", timeout_ms);
            ESP_LOGE(TAG, "BUSY pin (GPIO%d) stuck at HIGH", handle->busy_pin);
            return EPAPER_ERR_TIMEOUT;
        }
    }

    ESP_LOGI(TAG, "BUSY pin went LOW after %d ms",
             (int)(xTaskGetTickCount() * portTICK_PERIOD_MS - start));
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

esp_err_t epaper_display_white(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Displaying white screen...");

    epaper_send_command(handle, DTM);

    EPD_W21_WriteDATA_Start();
    for(uint32_t i = 0; i < 192000; i++) {
        uint8_t data = 0x22;
//        epaper_send_data(handle, &data, 1);
        EPD_W21_WriteDATA(data);
    }
    EPD_W21_WriteDATA_End();

    epaper_send_command(handle, PON);
//    epaper_wait_busy(handle, 5000);
/*
    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x17, 0x49};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));
*/
    epaper_send_command(handle, DRF);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
//    epaper_wait_busy(handle, 30000);

    epaper_send_command(handle, POF);
    uint8_t pof_data[] = {0x00};
    epaper_send_data(handle, pof_data, sizeof(pof_data));
//    epaper_wait_busy(handle, 5000);

    return ESP_OK;
}

esp_err_t epaper_display_black(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Displaying black screen...");

    epaper_send_command(handle, DTM);

    for(uint32_t i = 0; i < 192000; i++) {
        uint8_t data = 0x00;
        epaper_send_data(handle, &data, 1);
    }

    epaper_send_command(handle, PON);
    epaper_wait_busy(handle, 5000);

    epaper_send_command(handle, BTST2);
    uint8_t btst2_data[] = {0x6F, 0x1F, 0x17, 0x49};
    epaper_send_data(handle, btst2_data, sizeof(btst2_data));

    epaper_send_command(handle, DRF);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
    epaper_wait_busy(handle, 30000);

    epaper_send_command(handle, POF);
    uint8_t pof_data[] = {0x00};
    epaper_send_data(handle, pof_data, sizeof(pof_data));
    epaper_wait_busy(handle, 5000);

    return ESP_OK;
}

esp_err_t epaper_display_picture(epaper_handle_t *handle, const uint8_t *pic_data)
{
    if (handle == NULL || pic_data == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Displaying picture...");

    epaper_send_command(handle, 0x10);

    for(uint32_t i = 0; i < 480; i++) {
        uint32_t k = 0;
        for(uint32_t j = 0; j < 800/2; j++) {
            uint8_t temp1 = pic_data[i*800 + k++];
            uint8_t temp2 = pic_data[i*800 + k++];
            uint8_t data_h = epaper_color_get(temp1) << 4;
            uint8_t data_l = epaper_color_get(temp2);
            uint8_t data = data_h | data_l;
            epaper_send_data(handle, &data, 1);
        }
    }

    epaper_send_command(handle, 0x12);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
    vTaskDelay(1 / portTICK_PERIOD_MS);
    epaper_wait_busy(handle, 30000);

    return ESP_OK;
}

esp_err_t epaper_display_clear(epaper_handle_t *handle)
{
    if (handle == NULL) {
        return EPAPER_ERR_INVALID_PARAM;
    }

    ESP_LOGI(TAG, "Clearing display...");

    epaper_send_command(handle, 0x10);

    for(uint32_t i = 0; i < 480; i++) {
        for(uint32_t j = 0; j < 800/2; j++) {
            uint8_t data = 0x11;
            epaper_send_data(handle, &data, 1);
        }
    }

    epaper_send_command(handle, 0x12);
    uint8_t refresh_data[] = {0x00};
    epaper_send_data(handle, refresh_data, sizeof(refresh_data));
    vTaskDelay(1 / portTICK_PERIOD_MS);
    epaper_wait_busy(handle, 30000);

    return ESP_OK;
}