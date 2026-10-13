idf_component_register(SRCS "lcd_test.c" "lcd_test_pattern.c" INCLUDE_DIRS "include"
    REQUIRES esp_common PRIV_REQUIRES display_panel heap freertos)
