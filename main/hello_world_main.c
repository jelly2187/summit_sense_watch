/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("# SummitSense heartbeat, idf=%s, target=%s, cores=%d, reset_reason=%d\n",
           esp_get_idf_version(), CONFIG_IDF_TARGET, chip_info.cores,
           (int)esp_reset_reason());

    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        printf("# flash_bytes=%" PRIu32 "\n", flash_size);
    } else {
        printf("# WARNING: flash size query failed\n");
    }

    const size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    printf("# psram_heap_total_bytes=%zu\n", psram_size);
#if CONFIG_SPIRAM
    if (psram_size == 0) {
        printf("# WARNING: PSRAM is configured but unavailable to the heap\n");
    }
#else
    printf("# PSRAM is disabled in project configuration\n");
#endif

    printf("type,seq,uptime_us,internal_free_bytes,internal_min_free_bytes,psram_free_bytes\n");
    fflush(stdout);

    uint64_t seq = 0;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        const int64_t uptime_us = esp_timer_get_time();
        const size_t internal_free = heap_caps_get_free_size(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t internal_min = heap_caps_get_minimum_free_size(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        printf("SYS,%" PRIu64 ",%" PRId64 ",%zu,%zu,%zu\n",
               seq++, uptime_us, internal_free, internal_min, psram_free);
        fflush(stdout);

        // Keep a one-second cadence without an intentional restart.
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}
