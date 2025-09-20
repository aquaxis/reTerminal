#include "bitmap.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "BITMAP";

static inline uint32_t read_le32(const unsigned char *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline uint16_t read_le16(const unsigned char *p) {
    return p[0] | (p[1] << 8);
}

esp_err_t load_bmp_from_sd(const char *filename, bmp_image_t *image)
{
    FILE *file = NULL;
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Loading BMP file: %s", filename);

    file = fopen(filename, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    bmp_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        ESP_LOGE(TAG, "Failed to read BMP header");
        goto cleanup;
    }

    if (header.type != 0x4D42) {
        ESP_LOGE(TAG, "Invalid BMP signature");
        goto cleanup;
    }

    bmp_info_header_t info_header;
    if (fread(&info_header, sizeof(info_header), 1, file) != 1) {
        ESP_LOGE(TAG, "Failed to read BMP info header");
        goto cleanup;
    }

    ESP_LOGI(TAG, "BMP info: %dx%d, %d bits", info_header.width, info_header.height, info_header.bits_per_pixel);

    if (info_header.bits_per_pixel != 4) {
        ESP_LOGE(TAG, "Only 4-bit BMP files are supported");
        goto cleanup;
    }

    if (info_header.width != 800 || info_header.height != 480) {
        ESP_LOGE(TAG, "BMP dimensions must be 800x480");
        goto cleanup;
    }

    uint32_t palette_size = (1 << info_header.bits_per_pixel) * 4;
    uint8_t *palette = malloc(palette_size);
    if (!palette) {
        ESP_LOGE(TAG, "Failed to allocate palette memory");
        goto cleanup;
    }

    if (fread(palette, palette_size, 1, file) != 1) {
        ESP_LOGE(TAG, "Failed to read palette");
        free(palette);
        goto cleanup;
    }

    uint32_t row_size = ((info_header.width * info_header.bits_per_pixel + 31) / 32) * 4;
    uint32_t image_data_size = row_size * info_header.height;

    image->data = malloc(image_data_size);
    if (!image->data) {
        ESP_LOGE(TAG, "Failed to allocate image data memory");
        free(palette);
        goto cleanup;
    }

    if (fseek(file, header.offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to image data");
        free(palette);
        free(image->data);
        image->data = NULL;
        goto cleanup;
    }

    for (int y = info_header.height - 1; y >= 0; y--) {
        uint8_t *row_ptr = image->data + y * row_size;
        if (fread(row_ptr, row_size, 1, file) != 1) {
            ESP_LOGE(TAG, "Failed to read row %d", y);
            free(palette);
            free(image->data);
            image->data = NULL;
            goto cleanup;
        }
    }

    image->width = info_header.width;
    image->height = info_header.height;
    image->bits_per_pixel = info_header.bits_per_pixel;

    free(palette);
    ret = ESP_OK;
    ESP_LOGI(TAG, "BMP file loaded successfully");

cleanup:
    if (file) {
        fclose(file);
    }
    return ret;
}

void free_bmp_image(bmp_image_t *image)
{
    if (image && image->data) {
        free(image->data);
        image->data = NULL;
    }
}

esp_err_t convert_bmp_to_epaper(bmp_image_t *bmp, uint8_t *epaper_buffer)
{
    if (!bmp || !bmp->data || !epaper_buffer) {
        ESP_LOGE(TAG, "Invalid parameters for BMP to e-Paper conversion");
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t row_size = ((bmp->width * bmp->bits_per_pixel + 31) / 32) * 4;

    for (int y = 0; y < bmp->height; y++) {
        uint8_t *row_ptr = bmp->data + y * row_size;

        for (int x = 0; x < bmp->width; x += 2) {
            uint8_t pixel_data = row_ptr[x / 2];
            uint8_t pixel1 = (pixel_data >> 4) & 0x0F;
            uint8_t pixel2 = pixel_data & 0x0F;

            uint32_t epaper_index = y * bmp->width + x;
            if (epaper_index < bmp->width * bmp->height) {
                epaper_buffer[epaper_index] = pixel1;
            }
            if (x + 1 < bmp->width && epaper_index + 1 < bmp->width * bmp->height) {
                epaper_buffer[epaper_index + 1] = pixel2;
            }
        }
    }

    ESP_LOGI(TAG, "BMP to e-Paper conversion completed");
    return ESP_OK;
}