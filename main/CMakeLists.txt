idf_component_register(SRCS "main.c" "sdio.c" "bitmap.c" "ImageData.c" "epaper_driver.c" "gdep073e01.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver fatfs)