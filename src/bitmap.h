#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    unsigned char *data;
    int width;
    int height;
    int bits_per_pixel;
} bmp_image_t;

typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} __attribute__((packed)) bmp_header_t;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;
} __attribute__((packed)) bmp_info_header_t;

esp_err_t load_bmp_from_sd(const char *filename, bmp_image_t *image);
void free_bmp_image(bmp_image_t *image);
esp_err_t convert_bmp_to_epaper(bmp_image_t *bmp, uint8_t *epaper_buffer);

#endif