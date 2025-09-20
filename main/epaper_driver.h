#ifndef EPAPER_DRIVER_H
#define EPAPER_DRIVER_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
    gpio_num_t dc_pin;
    gpio_num_t rst_pin;
    gpio_num_t busy_pin;
    uint16_t width;
    uint16_t height;
} epaper_handle_t;

typedef enum {
    EPAPER_COLOR_BLACK = 0x00,
    EPAPER_COLOR_WHITE = 0x01,
    EPAPER_COLOR_YELLOW = 0x02,
    EPAPER_COLOR_RED = 0x03,
    EPAPER_COLOR_BLUE = 0x05,
    EPAPER_COLOR_GREEN = 0x06,
    EPAPER_COLOR_CLEAN = 0x07
} epaper_color_t;

#define Black   0x00
#define White   0x11
#define Green   0x66
#define Blue    0x55
#define Red     0x33
#define Yellow  0x22
#define Clean   0x77

typedef enum {
    EPAPER_OK = 0,
    EPAPER_ERR_INIT = -1,
    EPAPER_ERR_SPI = -2,
    EPAPER_ERR_TIMEOUT = -3,
    EPAPER_ERR_BUSY = -4,
    EPAPER_ERR_MEMORY = -5,
    EPAPER_ERR_INVALID_PARAM = -6
} epaper_error_t;

esp_err_t epaper_init(epaper_handle_t *handle);

esp_err_t epaper_init_fast(epaper_handle_t *handle);

esp_err_t epaper_deinit(epaper_handle_t *handle);

esp_err_t epaper_clear(epaper_handle_t *handle, epaper_color_t color);

esp_err_t epaper_display_frame(epaper_handle_t *handle, uint8_t *frame_buffer);

esp_err_t epaper_partial_update(epaper_handle_t *handle,
                                uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                uint8_t *buffer);

esp_err_t epaper_sleep(epaper_handle_t *handle);

esp_err_t epaper_wake(epaper_handle_t *handle);

esp_err_t epaper_wait_busy(epaper_handle_t *handle, uint32_t timeout_ms);

esp_err_t epaper_set_pixel(uint8_t *buffer, uint16_t x, uint16_t y,
                           epaper_color_t color, uint16_t width, uint16_t height);

esp_err_t epaper_refresh(epaper_handle_t *handle, bool partial_update);

esp_err_t epaper_fill_screen(epaper_handle_t *handle, epaper_color_t color);

esp_err_t epaper_display(epaper_handle_t *handle);

esp_err_t epaper_display_partial(epaper_handle_t *handle, bool partial_update);

#endif