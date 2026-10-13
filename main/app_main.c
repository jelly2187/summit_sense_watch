/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "max30102.h"
#include "system_monitor.h"
#include "yahboom_imu.h"
#include "as6221.h"
#include "watch_ui.h"

void app_main(void)
{
    system_monitor_print_boot_info();

    // A sensor connection error must not abort system health monitoring.
    esp_err_t result = max30102_probe();
    if (result == ESP_OK) {
        result = max30102_start_sampling();
    }
    if (result != ESP_OK) {
        printf("# PPG: startup failed: %s; heartbeat continues\n", esp_err_to_name(result));
    }

    result = yahboom_imu_start();
    if (result != ESP_OK) {
        printf("# IMU: startup failed: %s; other tasks continue\n", esp_err_to_name(result));
    }

    result = as6221_start();
    if (result != ESP_OK) {
        printf("# AS6221: startup failed: %s; other tasks continue\n", esp_err_to_name(result));
    }

    result = watch_ui_start();
    if (result != ESP_OK) {
        printf("# LCD: startup failed: %s; sensor tasks continue\n", esp_err_to_name(result));
    }

    system_monitor_run_heartbeat();
}
