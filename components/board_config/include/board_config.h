#pragma once

// Waveshare ESP32-S3-DEV-KIT-N16R8 wiring. GPIO numbers, not header positions.
#define BOARD_I2C_SDA_GPIO 8
#define BOARD_I2C_SCL_GPIO 9
#define BOARD_I2C_SPEED_HZ 100000
#define BOARD_MAX30102_INT_GPIO 10  // Wired, but not used by the startup probe.
#define BOARD_HEARTBEAT_PERIOD_MS 1000

#define BOARD_IMU_RX_GPIO 18  // IMU TX -> S3 RX
#define BOARD_IMU_TX_GPIO 17  // S3 TX -> IMU RX, required to enable pressure output
#define BOARD_IMU_BAUD 9600
