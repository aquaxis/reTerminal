#ifndef SDIO_H
#define SDIO_H

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include <stddef.h>
#include <stdbool.h>

#define MOUNT_POINT "/sdcard"
//#define CONFIG_FILE "wifi.cfg"
#define CONFIG_FILE "config"

#define SD_MISO_PIN  8
#define SD_MOSI_PIN  9
#define SD_SCLK_PIN  7
#define SD_CS_PIN    14
#define SD_EN_PIN    16
#define SD_DET_PIN   15

#define SD_SPI_HOST  SPI3_HOST

typedef struct {
    sdmmc_card_t *card;
    bool is_mounted;
} sdio_context_t;

typedef struct {
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
} sd_card_info_t;

extern sdio_context_t sdio_ctx;

esp_err_t init_sd_card(void);
esp_err_t deinit_sd_card(void);
esp_err_t sdio_init(sdio_context_t *ctx);
esp_err_t sdio_deinit(sdio_context_t *ctx);
esp_err_t sdio_mount(sdio_context_t *ctx);
esp_err_t sdio_unmount(sdio_context_t *ctx);
esp_err_t sdio_read_file(sdio_context_t *ctx, const char *filename, char *buffer, size_t buffer_size);
esp_err_t sdio_get_info(sd_card_info_t *info);
bool sdio_is_mounted(void);

#endif