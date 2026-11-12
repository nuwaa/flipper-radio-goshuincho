#pragma once

#include "goshuin_stamp.h"

#define GOSHUIN_MULTIBAND_COUNT             5U
#define GOSHUIN_MULTIBAND_SAMPLES           16U
#define GOSHUIN_MULTIBAND_VALID_MASK        0xFFFFU
#define GOSHUIN_MULTIBAND_ALGORITHM_VERSION 1U
#define GOSHUIN_MULTIBAND_PRESET_OOK650     1U
#define GOSHUIN_FILE_VERSION_V3             3U

typedef enum {
    GoshuinBand315 = 315,
    GoshuinBand426 = 426,
    GoshuinBand429 = 429,
    GoshuinBand433 = 433,
    GoshuinBand920 = 920,
} GoshuinBandId;

typedef struct __attribute__((packed)) {
    uint16_t band_id;
    uint8_t valid_count;
    uint8_t reserved0;
    uint16_t valid_mask;
    uint32_t requested_frequency_hz;
    uint32_t actual_frequency_hz;
    int8_t rssi_min;
    int8_t rssi_max;
    int8_t rssi_average;
    uint8_t reserved1;
    int8_t rssi_samples[GOSHUIN_MULTIBAND_SAMPLES];
} GoshuinBandPayloadV3;

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    uint8_t measurement_source;
    uint8_t measurement_status;
    uint8_t band_count;
    uint8_t algorithm_version;
    uint8_t samples_per_band;
    uint8_t preset_id;
    uint16_t observation_window_ms;
    uint16_t settle_time_ms;
    uint16_t peak_read_interval_ms;
    uint8_t reserved0;
    GoshuinBandPayloadV3 bands[GOSHUIN_MULTIBAND_COUNT];
    uint32_t seed;
    char location[GOSHUIN_LOCATION_CAPACITY];
    uint8_t reserved1;
    uint8_t seal[GOSHUIN_SEAL_BYTES];
} GoshuinStampPayloadV3;

_Static_assert(sizeof(GoshuinBandPayloadV3) == 34U, "Version 3 band layout changed");
_Static_assert(sizeof(GoshuinStampPayloadV3) == 1244U, "Version 3 layout changed");

extern const uint16_t goshuin_multiband_ids[GOSHUIN_MULTIBAND_COUNT];
extern const uint32_t goshuin_multiband_requested_hz[GOSHUIN_MULTIBAND_COUNT];

void goshuin_multiband_initialize(
    GoshuinStampPayloadV3* stamp,
    const char* location,
    const DateTime* datetime,
    uint16_t observation_window_ms,
    uint16_t settle_time_ms,
    uint16_t peak_read_interval_ms);

bool goshuin_multiband_finalize(GoshuinStampPayloadV3* stamp);
bool goshuin_multiband_generate_band_seal(
    const GoshuinStampPayloadV3* stamp,
    size_t band_index,
    uint8_t* seal);
bool goshuin_multiband_decode(
    const void* payload,
    size_t payload_size,
    GoshuinStampPayloadV3* stamp);
bool goshuin_multiband_frequency_valid(uint32_t frequency_hz);
