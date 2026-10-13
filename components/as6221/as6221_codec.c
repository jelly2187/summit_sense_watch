#include "as6221_codec.h"
float as6221_decode_temperature(uint8_t msb,uint8_t lsb)
{
    uint32_t raw=((uint32_t)msb<<8)|lsb;
    int32_t value=raw>=32768 ? (int32_t)raw-65536 : (int32_t)raw;
    return value/128.0f;
}
bool as6221_temperature_in_range(float value)
{
    return value>=-40.0f && value<=125.0f;
}
