idf_component_register(SRCS "gnss_lc76g.c" "nmea_rmc.c" INCLUDE_DIRS "include"
    REQUIRES esp_common PRIV_REQUIRES board_config esp_driver_uart esp_driver_gpio esp_timer freertos)
