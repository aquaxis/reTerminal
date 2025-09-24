idf_component_register(SRCS "wifi_manager.c" "logger.c" "config_parser.c" "main.c" "sdio.c" "bitmap.c" "ImageData.c" "epaper_driver.c" "gdep073e01.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver fatfs nvs_flash esp_wifi lwip esp_netif esp_event)