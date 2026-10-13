#include "imu_protocol.h"
#include <string.h>

static int32_t signed16(const uint8_t *p)
{
    uint32_t value=(uint32_t)p[0]|((uint32_t)p[1]<<8);
    return value>=32768 ? (int32_t)value-65536 : (int32_t)value;
}
static uint32_t unsigned32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int64_t signed32(const uint8_t *p)
{
    uint32_t value=unsigned32(p);
    return value>=0x80000000U ? (int64_t)value-4294967296LL : value;
}
static bool decode(const uint8_t *p, imu_frame_t *out)
{
    memset(out,0,sizeof(*out)); out->type=p[1];
    double scale=1;
    switch(p[1]) {
    case 0x51: out->kind="ACC"; scale=16.0/32768; out->valid_mask=15; break;
    case 0x52: out->kind="GYRO"; scale=2000.0/32768; out->valid_mask=7; break;
    case 0x53: out->kind="ANGLE"; scale=180.0/32768; out->valid_mask=15; break;
    case 0x54: out->kind="MAG_RAW"; out->valid_mask=15; break;
    case 0x56:
        out->kind="PRESS"; out->values[0]=unsigned32(p+2);
        out->values[1]=signed32(p+6)/100.0; out->valid_mask=3;
        // Barometer module range; preserve raw values but mark implausible packets invalid.
        if(out->values[0]<30000 || out->values[0]>110000) out->valid_mask=0;
        return true;
    case 0x59: out->kind="QUAT"; out->valid_mask=15;
        for(int j=0;j<4;++j) out->values[j]=signed16(p+2+j*2)/32768.0;
        return true;
    case 0x5F: out->kind="REG"; out->valid_mask=15;
        for(int j=0;j<4;++j) out->values[j]=p[2+j*2]|((uint16_t)p[3+j*2]<<8);
        return true;
    default: return false; // Valid but unused time/GPS/port frames are counted, not misdecoded.
    }
    for(int j=0;j<3;++j) out->values[j]=signed16(p+2+j*2)*scale;
    if(p[1]==0x51 || p[1]==0x54) out->values[3]=signed16(p+8)/100.0;
    if(p[1]==0x53) out->values[3]=p[8]|((uint16_t)p[9]<<8); // Firmware version, not temperature.
    return true;
}
void imu_parser_init(imu_parser_t *parser) { memset(parser,0,sizeof(*parser)); }
void imu_parser_feed(imu_parser_t *parser, const uint8_t *data, size_t size,
                     imu_frame_callback_t callback, void *user)
{
    for(size_t j=0;j<size;++j) {
        if(!parser->used && data[j]!=0x55) { ++parser->discarded_bytes; continue; }
        parser->buffer[parser->used++]=data[j];
        if(parser->used<11) continue;
        uint8_t checksum=0;
        for(int k=0;k<10;++k) checksum=(uint8_t)(checksum+parser->buffer[k]);
        if(checksum==parser->buffer[10]) {
            ++parser->frames;
            imu_frame_t frame;
            if(decode(parser->buffer,&frame) && callback) callback(&frame,user);
            parser->used=0;
        } else {
            ++parser->checksum_errors;
            unsigned next=1;
            while(next<11 && parser->buffer[next]!=0x55) ++next;
            parser->discarded_bytes+=next;
            parser->used=11-next;
            if(parser->used) memmove(parser->buffer,parser->buffer+next,parser->used);
        }
    }
}
