#pragma once

// Waveshare ESP32-S3-DEV-KIT-N16R8 wiring. GPIO numbers, not header positions.
#define BOARD_I2C_SDA_GPIO 8
#define BOARD_I2C_SCL_GPIO 9
#define BOARD_I2C_SPEED_HZ 100000
#define BOARD_MAX30102_INT_GPIO 10  // Wired, but not used by the startup probe.
#define BOARD_HEARTBEAT_PERIOD_MS 1000
