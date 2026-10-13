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

#define BOARD_TEMPERATURE_SDA_GPIO 4
#define BOARD_TEMPERATURE_SCL_GPIO 5
#define BOARD_AS6221_ADDRESS 0x48
#define BOARD_TEMPERATURE_PERIOD_MS 250

// TK013F2261 V1.3: two 4-pin connectors, no exposed reset/backlight control.
#define BOARD_LCD_MOSI_GPIO 11
#define BOARD_LCD_SCK_GPIO 12
#define BOARD_LCD_CS_GPIO 13
#define BOARD_LCD_DC_GPIO 14
#define BOARD_LCD_CLOCK_HZ 10000000
#define BOARD_LCD_BGR 1
#define BOARD_LCD_INVERT 0
#define BOARD_LCD_MIRROR_X 1
#define BOARD_LCD_MIRROR_Y 0
#define BOARD_LCD_X_GAP 0
#define BOARD_LCD_Y_GAP 0
