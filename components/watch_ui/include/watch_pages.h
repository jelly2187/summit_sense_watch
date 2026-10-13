#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#define WATCH_PAGE_COUNT 7
// All values are demonstration fixtures; no implicit sensor validity.
void watch_pages_show(unsigned page, bool values_available, bool autoplay);
void watch_pages_tick(uint32_t elapsed_seconds);
const char *watch_pages_name(unsigned page);
