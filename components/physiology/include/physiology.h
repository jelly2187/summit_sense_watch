#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PHYSIOLOGY_WINDOW 800
#define PHYSIOLOGY_FS 100.0f

typedef enum {
    PHYS_WARMUP, PHYS_NO_CONTACT, PHYS_GAP, PHYS_SATURATED,
    PHYS_TIMING, PHYS_UNSTABLE, PHYS_WEAK, PHYS_NO_PERIOD,
    PHYS_IRREGULAR, PHYS_OK_EXPERIMENTAL, PHYS_RATIO_RANGE
} physiology_reason_t;

typedef struct {
    int64_t time_us;
    float hr_bpm, spo2_est_pct, sqi, ratio, pi_ir_pct;
    bool hr_available, spo2_est_available;
    physiology_reason_t reason;
} physiology_result_t;

typedef struct {
    uint32_t red[PHYSIOLOGY_WINDOW], ir[PHYSIOLOGY_WINDOW];
    int64_t times[PHYSIOLOGY_WINDOW];
    float ac_red[PHYSIOLOGY_WINDOW], ac_ir[PHYSIOLOGY_WINDOW];
    unsigned count, write_index;
    uint64_t last_seq;
    uint32_t last_epoch;
    int64_t last_time, last_report;
    bool has_last;
    physiology_reason_t last_reason;
} physiology_context_t;

void physiology_init(physiology_context_t *ctx);
// Called by one task only. True means a fresh result (including invalid state).
bool physiology_feed(physiology_context_t *ctx, uint64_t seq, int64_t time_us,
                     uint32_t red, uint32_t ir, uint32_t flags, uint32_t epoch,
                     physiology_result_t *result);
const char *physiology_reason_name(physiology_reason_t reason);
