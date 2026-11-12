idf_component_register(SRCS "lcd_test.c" "lcd35_test.c" "lcd_test_pattern.c" INCLUDE_DIRS "include"
    REQUIRES esp_common PRIV_REQUIRES display_panel board_config esp_lcd esp_driver_spi esp_driver_gpio esp_driver_i2c nvs_flash watch_ui heap freertos)
