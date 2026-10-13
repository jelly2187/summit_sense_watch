idf_component_register(
    SRCS "system_monitor.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES board_config spi_flash esp_timer esp_system heap freertos esp_psram
)
