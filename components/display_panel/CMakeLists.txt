idf_component_register(SRCS "display_panel.c" INCLUDE_DIRS "include"
    REQUIRES esp_common PRIV_REQUIRES board_config esp_lcd esp_driver_spi freertos)
