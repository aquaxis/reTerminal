#ifndef SDIO_H
#define SDIO_H

#include "esp_err.h"

#define MOUNT_POINT "/sdcard"

#define SD_MISO_PIN  8
#define SD_MOSI_PIN  9
#define SD_SCLK_PIN  7
#define SD_CS_PIN    14
#define SD_EN_PIN    16
#define SD_DET_PIN   15

#define SD_SPI_HOST  SPI3_HOST

esp_err_t init_sd_card(void);
esp_err_t deinit_sd_card(void);

#endif