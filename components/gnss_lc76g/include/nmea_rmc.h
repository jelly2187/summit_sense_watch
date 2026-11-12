#pragma once
#include <stdint.h>

typedef enum { NMEA_IGNORED, NMEA_NO_FIX, NMEA_FIX } nmea_rmc_result_t;
nmea_rmc_result_t nmea_rmc_parse(const char *line, int32_t *lat_e7, int32_t *lon_e7);
