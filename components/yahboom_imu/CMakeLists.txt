idf_component_register(SRCS "imu_protocol.c" "yahboom_imu.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES board_config esp_driver_uart freertos esp_timer)
