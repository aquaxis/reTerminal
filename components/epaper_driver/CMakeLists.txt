idf_component_register(SRCS "src/epaper_driver.c"
                           "src/gdep073e01.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver freertos)