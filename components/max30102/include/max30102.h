#pragma once

#include "esp_err.h"

// Startup-only communication check; does not enable LEDs or sampling.
esp_err_t max30102_probe(void);

// Start independent FIFO acquisition and serial output tasks.
esp_err_t max30102_start_sampling(void);
