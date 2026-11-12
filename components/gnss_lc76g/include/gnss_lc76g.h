#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t gnss_lc76g_start(void);
bool gnss_lc76g_fix(int32_t *lat_e7, int32_t *lon_e7);
