#pragma once

#include "goshuin_multiband.h"
#include "goshuin_themes.h"

#define GOSHUIN_FILE_VERSION_V4 4U

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    uint8_t measurement_source;
    uint8_t measurement_status;
    uint8_t band_count;
    uint8_t theme_id;
    uint8_t design_algorithm_version;
    uint8_t samples_per_band;
    uint8_t preset_id;
    uint16_t observation_window_ms;
    uint16_t settle_time_ms;
    uint16_t peak_read_interval_ms;
    uint8_t reserved0[2];
    GoshuinBandPayloadV3 bands[GOSHUIN_MULTIBAND_COUNT];
    uint32_t seed;
    char location[GOSHUIN_LOCATION_CAPACITY];
    uint8_t reserved1[3];
    uint8_t seal[GOSHUIN_SEAL_BYTES];
} GoshuinStampPayloadV4;

_Static_assert(sizeof(GoshuinStampPayloadV4) == 1248U, "Version 4 layout changed");

bool goshuin_themed_create(
    GoshuinStampPayloadV4* stamp,
    const GoshuinStampPayloadV3* observation,
    GoshuinThemeId theme);
bool goshuin_themed_decode(
    const void* payload,
    size_t payload_size,
    GoshuinStampPayloadV4* stamp);

