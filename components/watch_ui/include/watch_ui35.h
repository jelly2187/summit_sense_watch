#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

enum { WATCH35_WIDTH = 320, WATCH35_HEIGHT = 480, WATCH35_DRAW_ROWS = 32, WATCH35_NAV_TOP = 382, WATCH35_NAV_BOTTOM = 448 };
typedef struct {
    void *context;
    esp_err_t (*flush)(void *context, int x0, int y0, int x1, int y1,
                       const uint8_t *pixels, size_t bytes);
    // False indicates a read error, multiple fingers or an invalid coordinate.
    bool (*touch)(void *context, bool *pressed, int *x, int *y);
} watch_ui35_port_t;

// Runs LVGL on the calling task; returns only on a display/setup failure.
esp_err_t watch_ui35_run(const watch_ui35_port_t *port);
