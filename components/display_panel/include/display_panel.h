#pragma once
#include "esp_err.h"
// Single owner, SPI2, MSB-first RGB565. Bounds are inclusive. At most 240*40 pixels.
esp_err_t display_panel_init(void);
esp_err_t display_panel_flush_rgb565(int x0,int y0,int x1,int y1,const void *msb_pixels);
