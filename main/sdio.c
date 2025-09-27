#include "sdio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_FILE_SIZE 4096

static const char *TAG = "SDIO";
sdio_context_t sdio_ctx = {.card = NULL, .is_mounted = false};
static sdmmc_card_t *card = NULL;

esp_err_t init_sd_card(void)
{
    ESP_LOGI(TAG, "Initializing SD card");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SD_SPI_HOST;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");

    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t sdio_init(sdio_context_t *ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->card = NULL;
    ctx->is_mounted = false;
    return ESP_OK;
}

esp_err_t sdio_deinit(sdio_context_t *ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->is_mounted) {
        sdio_unmount(ctx);
    }
    return ESP_OK;
}

esp_err_t sdio_mount(sdio_context_t *ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return init_sd_card();
}

esp_err_t sdio_unmount(sdio_context_t *ctx) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return deinit_sd_card();
}

esp_err_t sdio_read_file(sdio_context_t *ctx, const char *filename, char *buffer, size_t buffer_size) {
    if (ctx == NULL || filename == NULL || buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT, filename);

    log_info(TAG, "Reading file: %s", filepath);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        log_error(TAG, "Failed to open file for reading: %s", filepath);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, f);
    buffer[bytes_read] = '\0';

    fclose(f);

    log_info(TAG, "Read %d bytes from %s", bytes_read, filename);

    return ESP_OK;
}

esp_err_t deinit_sd_card(void)
{
    ESP_LOGI(TAG, "Unmounting SD card");

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_bus_free(SD_SPI_HOST);
    card = NULL;

    ESP_LOGI(TAG, "SD card unmounted successfully");
    return ESP_OK;
}

esp_err_t sdio_get_info(sd_card_info_t *info) {
    if (!sdio_ctx.is_mounted || !card) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree(MOUNT_POINT, &fre_clust, &fs) != FR_OK) {
        ESP_LOGE(TAG, "Failed to get SD card info");
        return ESP_FAIL;
    }

    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    info->total_bytes = tot_sect * FF_SS_SDCARD;
    info->free_bytes = fre_sect * FF_SS_SDCARD;
    info->used_bytes = info->total_bytes - info->free_bytes;

    ESP_LOGI(TAG, "SD Card Info - Total: %zu KB, Used: %zu KB, Free: %zu KB",
             info->total_bytes / 1024,
             info->used_bytes / 1024,
             info->free_bytes / 1024);

    return ESP_OK;
}

bool sdio_is_mounted(void) {
    return sdio_ctx.is_mounted;
}