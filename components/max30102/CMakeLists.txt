idf_component_register(
    SRCS "max30102.c" "max30102_sampling.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES board_config esp_driver_i2c freertos esp_timer physiology
)
