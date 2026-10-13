#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t buffer[11];
    unsigned used;
    uint64_t frames, checksum_errors, discarded_bytes;
} imu_parser_t;
typedef struct {
    uint8_t type, valid_mask;
    const char *kind;
    double values[4];
} imu_frame_t;
typedef void (*imu_frame_callback_t)(const imu_frame_t *frame, void *user);
void imu_parser_init(imu_parser_t *parser);
void imu_parser_feed(imu_parser_t *parser, const uint8_t *data, size_t size,
                     imu_frame_callback_t callback, void *user);
