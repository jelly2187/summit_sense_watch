idf_component_register(SRCS "as6221.c" "as6221_codec.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES board_config esp_driver_i2c esp_timer freertos)
