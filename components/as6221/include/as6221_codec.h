#pragma once
#include <stdbool.h>
#include <stdint.h>
float as6221_decode_temperature(uint8_t msb,uint8_t lsb);
bool as6221_temperature_in_range(float value);
