idf_component_register(SRCS "watch_ui.c" "watch_pages.c" "fonts/watch_cn_16.c" "fonts/watch_cn_20.c"
    INCLUDE_DIRS "include" REQUIRES esp_common lvgl__lvgl
    PRIV_REQUIRES display_panel esp_driver_gpio esp_timer heap freertos)
