idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash esp_timer esp_system heap freertos esp_psram
                       INCLUDE_DIRS "")
