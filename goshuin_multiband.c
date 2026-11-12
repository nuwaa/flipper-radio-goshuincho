#include "goshuin_multiband.h"

#include <string.h>

#define GOSHUIN_FREQUENCY_TOLERANCE_HZ 1000U

const uint16_t goshuin_multiband_ids[GOSHUIN_MULTIBAND_COUNT] = {
    GoshuinBand315,
    GoshuinBand426,
    GoshuinBand429,
    GoshuinBand433,
    GoshuinBand920,
};

const uint32_t goshuin_multiband_requested_hz[GOSHUIN_MULTIBAND_COUNT] = {
    315000000UL,
    426075000UL,
    429350000UL,
    433920000UL,
    922500000UL,
};

static uint32_t fnv_update(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for(size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 16777619UL;
    return hash;
}

static uint32_t prng_next(uint32_t* state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value ? value : 0x6D2B79F5UL;
    return *state;
}

static void set_pixel(uint8_t* bitmap, int32_t x, int32_t y) {
    if((x < 0) || (x >= (int32_t)GOSHUIN_SEAL_WIDTH) || (y < 0) ||
       (y >= (int32_t)GOSHUIN_SEAL_HEIGHT)) {
        return;
    }
    bitmap[(size_t)y * (GOSHUIN_SEAL_WIDTH / 8U) + ((size_t)x / 8U)] |=
        (uint8_t)(1U << ((uint32_t)x & 7U));
}

static void draw_line(uint8_t* bitmap, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    const int32_t dx = (x1 > x0) ? x1 - x0 : x0 - x1;
    const int32_t sx = x0 < x1 ? 1 : -1;
    const int32_t abs_dy = (y1 > y0) ? y1 - y0 : y0 - y1;
    const int32_t dy = -abs_dy;
    const int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    while(true) {
        set_pixel(bitmap, x0, y0);
        if((x0 == x1) && (y0 == y1)) break;
        const int32_t twice_error = error * 2;
        if(twice_error >= dy) {
            error += dy;
            x0 += sx;
        }
        if(twice_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static int32_t sample_y(int8_t sample) {
    int32_t y = 57 - (((int32_t)sample + 110) * 50 / 80);
    if(y < 6) y = 6;
    if(y > 57) y = 57;
    return y;
}

static void generate_seal(GoshuinStampPayloadV3* stamp) {
    memset(stamp->seal, 0, sizeof(stamp->seal));
    for(int32_t x = 0; x < 128; ++x) {
        set_pixel(stamp->seal, x, 0);
        set_pixel(stamp->seal, x, 63);
    }
    for(int32_t y = 0; y < 64; ++y) {
        set_pixel(stamp->seal, 0, y);
        set_pixel(stamp->seal, 127, y);
    }

    for(size_t band_index = 0; band_index < GOSHUIN_MULTIBAND_COUNT; ++band_index) {
        const int32_t left = 2 + (int32_t)band_index * 25;
        const int32_t right = (band_index == 4U) ? 125 : left + 23;
        if(band_index) {
            for(int32_t y = 2; y < 62; ++y)
                set_pixel(stamp->seal, left - 1, y);
        }
        const GoshuinBandPayloadV3* band = &stamp->bands[band_index];
        int32_t last_x = left;
        int32_t last_y = sample_y(band->rssi_samples[0]);
        for(size_t i = 0; i < GOSHUIN_MULTIBAND_SAMPLES; ++i) {
            const int32_t x =
                left + (int32_t)(i * (size_t)(right - left) / (GOSHUIN_MULTIBAND_SAMPLES - 1U));
            const int32_t y = sample_y(band->rssi_samples[i]);
            draw_line(stamp->seal, last_x, last_y, x, y);
            last_x = x;
            last_y = y;
        }
        uint32_t state = stamp->seed ^ band->band_id ^ band->actual_frequency_hz;
        const size_t density = (size_t)((int32_t)band->rssi_average + 128) / 2U + 8U;
        for(size_t i = 0; i < density; ++i) {
            const int32_t x = left + (int32_t)(prng_next(&state) % (uint32_t)(right - left + 1));
            const int32_t y = 3 + (int32_t)(prng_next(&state) % 58U);
            set_pixel(stamp->seal, x, y);
        }
    }
}

bool goshuin_multiband_generate_band_seal(
    const GoshuinStampPayloadV3* stamp,
    size_t band_index,
    uint8_t* seal) {
    if(!stamp || !seal || (band_index >= GOSHUIN_MULTIBAND_COUNT)) return false;
    const GoshuinBandPayloadV3* band = &stamp->bands[band_index];
    if((band->valid_count != GOSHUIN_MULTIBAND_SAMPLES) ||
       (band->valid_mask != GOSHUIN_MULTIBAND_VALID_MASK)) {
        return false;
    }

    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, stamp->location, strlen(stamp->location));
    seed = fnv_update(seed, &stamp->year, 7U);
    seed = fnv_update(seed, &stamp->algorithm_version, 1U);
    seed = fnv_update(seed, &band->band_id, sizeof(band->band_id));
    seed = fnv_update(seed, &band->requested_frequency_hz, sizeof(uint32_t));
    seed = fnv_update(seed, &band->actual_frequency_hz, sizeof(uint32_t));
    seed = fnv_update(seed, band->rssi_samples, sizeof(band->rssi_samples));
    if(!seed) seed = 0x6D2B79F5UL;

    memset(seal, 0, GOSHUIN_SEAL_BYTES);
    for(int32_t x = 0; x < 128; ++x) {
        set_pixel(seal, x, 0);
        set_pixel(seal, x, 2);
        set_pixel(seal, x, 61);
        set_pixel(seal, x, 63);
    }
    for(int32_t y = 0; y < 64; ++y) {
        set_pixel(seal, 0, y);
        set_pixel(seal, 2, y);
        set_pixel(seal, 125, y);
        set_pixel(seal, 127, y);
    }

    uint32_t state = seed;
    const size_t density = (size_t)((int32_t)band->rssi_average + 128) + 80U;
    for(size_t i = 0; i < density; ++i) {
        const int32_t x = 4 + (int32_t)(prng_next(&state) % 60U);
        const int32_t y = 4 + (int32_t)(prng_next(&state) % 28U);
        set_pixel(seal, x, y);
        set_pixel(seal, 127 - x, y);
        set_pixel(seal, x, 63 - y);
        set_pixel(seal, 127 - x, 63 - y);
    }

    int32_t last_x = 4;
    int32_t last_y = sample_y(band->rssi_samples[0]);
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_SAMPLES; ++i) {
        const int32_t x = 4 + (int32_t)(i * 119U / (GOSHUIN_MULTIBAND_SAMPLES - 1U));
        const int32_t y = sample_y(band->rssi_samples[i]);
        draw_line(seal, last_x, last_y, x, y);
        draw_line(seal, last_x, 63 - last_y, x, 63 - y);
        last_x = x;
        last_y = y;
    }

    for(size_t marker = 0; marker < GOSHUIN_MULTIBAND_COUNT; ++marker) {
        const int32_t x = 44 + (int32_t)marker * 10;
        set_pixel(seal, x, 5);
        set_pixel(seal, x + 1, 5);
        if(marker == band_index) {
            set_pixel(seal, x, 6);
            set_pixel(seal, x + 1, 6);
            set_pixel(seal, x, 7);
            set_pixel(seal, x + 1, 7);
        }
    }
    return true;
}

bool goshuin_multiband_frequency_valid(uint32_t frequency_hz) {
    return ((frequency_hz >= 299999755UL) && (frequency_hz <= 348000335UL)) ||
           ((frequency_hz >= 386999938UL) && (frequency_hz <= 464000000UL)) ||
           ((frequency_hz >= 778999847UL) && (frequency_hz <= 928000000UL));
}

void goshuin_multiband_initialize(
    GoshuinStampPayloadV3* stamp,
    const char* location,
    const DateTime* datetime,
    uint16_t window_ms,
    uint16_t settle_ms,
    uint16_t peak_interval_ms) {
    memset(stamp, 0, sizeof(*stamp));
    stamp->year = datetime->year;
    stamp->month = datetime->month;
    stamp->day = datetime->day;
    stamp->hour = datetime->hour;
    stamp->minute = datetime->minute;
    stamp->second = datetime->second;
    stamp->measurement_source = GoshuinMeasurementMultiBandReceive;
    stamp->measurement_status = GoshuinMeasurementStatusUnknown;
    stamp->band_count = GOSHUIN_MULTIBAND_COUNT;
    stamp->algorithm_version = GOSHUIN_MULTIBAND_ALGORITHM_VERSION;
    stamp->samples_per_band = GOSHUIN_MULTIBAND_SAMPLES;
    stamp->preset_id = GOSHUIN_MULTIBAND_PRESET_OOK650;
    stamp->observation_window_ms = window_ms;
    stamp->settle_time_ms = settle_ms;
    stamp->peak_read_interval_ms = peak_interval_ms;
    strncpy(stamp->location, location, sizeof(stamp->location) - 1U);
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        stamp->bands[i].band_id = goshuin_multiband_ids[i];
        stamp->bands[i].requested_frequency_hz = goshuin_multiband_requested_hz[i];
    }
}

static bool frequency_matches(uint32_t requested, uint32_t actual) {
    if(!goshuin_multiband_frequency_valid(requested) ||
       !goshuin_multiband_frequency_valid(actual)) {
        return false;
    }
    const uint32_t difference = requested > actual ? requested - actual : actual - requested;
    return difference <= GOSHUIN_FREQUENCY_TOLERANCE_HZ;
}

static bool location_valid(const char* location) {
    if(!location[0]) return false;
    bool terminated = false;
    for(size_t i = 0; i < GOSHUIN_LOCATION_CAPACITY; ++i) {
        const char c = location[i];
        if(!c) {
            terminated = true;
            continue;
        }
        if(terminated) return false;
        if(!(((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) ||
             ((c >= '0') && (c <= '9')) || (c == ' ') || (c == '-'))) {
            return false;
        }
    }
    return terminated;
}

static bool metadata_valid(const GoshuinStampPayloadV3* stamp, bool completed) {
    if((stamp->measurement_source != GoshuinMeasurementMultiBandReceive) ||
       (stamp->band_count != GOSHUIN_MULTIBAND_COUNT) ||
       (stamp->samples_per_band != GOSHUIN_MULTIBAND_SAMPLES) ||
       (stamp->algorithm_version != GOSHUIN_MULTIBAND_ALGORITHM_VERSION) ||
       (stamp->preset_id != GOSHUIN_MULTIBAND_PRESET_OOK650) || !stamp->observation_window_ms ||
       !stamp->settle_time_ms || !stamp->peak_read_interval_ms ||
       (stamp->observation_window_ms < stamp->peak_read_interval_ms) ||
       (stamp->observation_window_ms % stamp->peak_read_interval_ms) || stamp->reserved0 ||
       !location_valid(stamp->location) || !stamp->month || (stamp->month > 12U) || !stamp->day ||
       (stamp->day > 31U) || (stamp->hour > 23U) || (stamp->minute > 59U) ||
       (stamp->second > 59U) ||
       (completed && (stamp->measurement_status != GoshuinMeasurementStatusCompleted))) {
        return false;
    }
    uint8_t seen = 0;
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        const GoshuinBandPayloadV3* band = &stamp->bands[i];
        size_t known_index = GOSHUIN_MULTIBAND_COUNT;
        for(size_t j = 0; j < GOSHUIN_MULTIBAND_COUNT; ++j) {
            if(band->band_id == goshuin_multiband_ids[j]) known_index = j;
        }
        if((known_index == GOSHUIN_MULTIBAND_COUNT) || (seen & (1U << known_index)) ||
           (band->requested_frequency_hz != goshuin_multiband_requested_hz[known_index]) ||
           !frequency_matches(band->requested_frequency_hz, band->actual_frequency_hz) ||
           band->reserved0 || band->reserved1) {
            return false;
        }
        seen |= (uint8_t)(1U << known_index);
        if(completed && ((band->valid_count != GOSHUIN_MULTIBAND_SAMPLES) ||
                         (band->valid_mask != GOSHUIN_MULTIBAND_VALID_MASK))) {
            return false;
        }
    }
    return seen == 0x1FU;
}

bool goshuin_multiband_finalize(GoshuinStampPayloadV3* stamp) {
    if(!stamp || !metadata_valid(stamp, false)) return false;
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        GoshuinBandPayloadV3* band = &stamp->bands[i];
        if((band->valid_count != GOSHUIN_MULTIBAND_SAMPLES) ||
           (band->valid_mask != GOSHUIN_MULTIBAND_VALID_MASK)) {
            return false;
        }
        int32_t total = 0;
        band->rssi_min = band->rssi_samples[0];
        band->rssi_max = band->rssi_samples[0];
        for(size_t j = 0; j < GOSHUIN_MULTIBAND_SAMPLES; ++j) {
            const int8_t sample = band->rssi_samples[j];
            if(sample < band->rssi_min) band->rssi_min = sample;
            if(sample > band->rssi_max) band->rssi_max = sample;
            total += sample;
        }
        band->rssi_average = (int8_t)(total / (int32_t)GOSHUIN_MULTIBAND_SAMPLES);
    }
    stamp->measurement_status = GoshuinMeasurementStatusCompleted;
    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, stamp->location, strlen(stamp->location));
    seed = fnv_update(seed, &stamp->year, 7U);
    seed = fnv_update(seed, &stamp->algorithm_version, 1U);
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        const GoshuinBandPayloadV3* band = &stamp->bands[i];
        seed = fnv_update(seed, &band->band_id, sizeof(band->band_id));
        seed = fnv_update(seed, &band->requested_frequency_hz, sizeof(uint32_t));
        seed = fnv_update(seed, &band->actual_frequency_hz, sizeof(uint32_t));
        seed = fnv_update(seed, band->rssi_samples, sizeof(band->rssi_samples));
    }
    stamp->seed = seed ? seed : 0x6D2B79F5UL;
    generate_seal(stamp);
    return true;
}

bool goshuin_multiband_decode(
    const void* payload,
    size_t payload_size,
    GoshuinStampPayloadV3* stamp) {
    if(!payload || !stamp || (payload_size != sizeof(*stamp))) return false;
    const GoshuinStampPayloadV3* raw = payload;
    if(raw->location[GOSHUIN_LOCATION_CAPACITY - 1U] || raw->reserved1) return false;
    memcpy(stamp, payload, sizeof(*stamp));
    if(!metadata_valid(stamp, true)) return false;
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        const GoshuinBandPayloadV3* band = &stamp->bands[i];
        int32_t total = 0;
        int8_t minimum = band->rssi_samples[0];
        int8_t maximum = band->rssi_samples[0];
        for(size_t j = 0; j < GOSHUIN_MULTIBAND_SAMPLES; ++j) {
            const int8_t sample = band->rssi_samples[j];
            if(sample < minimum) minimum = sample;
            if(sample > maximum) maximum = sample;
            total += sample;
        }
        if((band->rssi_min != minimum) || (band->rssi_max != maximum) ||
           (band->rssi_average != (int8_t)(total / (int32_t)GOSHUIN_MULTIBAND_SAMPLES))) {
            return false;
        }
    }
    GoshuinStampPayloadV3 regenerated = *stamp;
    if(!goshuin_multiband_finalize(&regenerated)) return false;
    return !memcmp(stamp, &regenerated, sizeof(*stamp));
}
