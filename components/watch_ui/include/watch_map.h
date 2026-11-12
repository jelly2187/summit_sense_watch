#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void watch_map_show(lv_obj_t *root, bool animate_demo);
void watch_map_close(void);
void watch_map_tick(uint32_t seconds);
void watch_map_fix(bool valid, int32_t lat_e7, int32_t lon_e7);
// Returns true if the gesture started on the map, so it won't also flip pages.
bool watch_map_touch(int x0, int y0, int x1, int y1);
