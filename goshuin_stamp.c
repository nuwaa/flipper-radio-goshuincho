#include "goshuin_stamp.h"
#include "goshuin_themes.h"

#include <string.h>

static uint32_t fnv_update(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for(size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * 16777619UL;
    }
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
    const int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    const int32_t sx = (x0 < x1) ? 1 : -1;
    const int32_t abs_dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    const int32_t dy = -abs_dy;
    const int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t error = dx + dy;
    while(true) {
        set_pixel(bitmap, x0, y0);
        if((x0 == x1) && (y0 == y1)) break;
        const int32_t twice_error = 2 * error;
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
    int32_t y = 55 - (((int32_t)sample + 110) * 48 / 80);
    if(y < 7) y = 7;
    if(y > 55) y = 55;
    return y;
}

static void generate_seal(GoshuinStampPayload* stamp) {
    memset(stamp->seal, 0, sizeof(stamp->seal));
    for(int32_t x = 0; x < (int32_t)GOSHUIN_SEAL_WIDTH; ++x) {
        set_pixel(stamp->seal, x, 0);
        set_pixel(stamp->seal, x, 2);
        set_pixel(stamp->seal, x, 61);
        set_pixel(stamp->seal, x, 63);
    }
    for(int32_t y = 0; y < (int32_t)GOSHUIN_SEAL_HEIGHT; ++y) {
        set_pixel(stamp->seal, 0, y);
        set_pixel(stamp->seal, 2, y);
        set_pixel(stamp->seal, 125, y);
        set_pixel(stamp->seal, 127, y);
    }
    uint32_t state = stamp->seed;
    for(size_t i = 0; i < 220U; ++i) {
        const int32_t x = 4 + (int32_t)(prng_next(&state) % 60U);
        const int32_t y = 4 + (int32_t)(prng_next(&state) % 28U);
        set_pixel(stamp->seal, x, y);
        set_pixel(stamp->seal, 127 - x, y);
        set_pixel(stamp->seal, x, 63 - y);
        set_pixel(stamp->seal, 127 - x, 63 - y);
    }
    int32_t last_x = 4;
    int32_t last_y = sample_y(stamp->rssi_samples[0]);
    for(size_t i = 0; i < stamp->sample_count; ++i) {
        const int32_t x = (stamp->sample_count > 1U) ?
                              4 + (int32_t)((i * 119U) / (stamp->sample_count - 1U)) :
                              64;
        const int32_t y = sample_y(stamp->rssi_samples[i]);
        draw_line(stamp->seal, last_x, last_y, x, y);
        draw_line(stamp->seal, last_x, 63 - last_y, x, 63 - y);
        last_x = x;
        last_y = y;
    }
    for(int32_t radius = 5; radius <= 19; radius += 7) {
        draw_line(stamp->seal, 64 - radius, 32, 64, 32 - radius / 2);
        draw_line(stamp->seal, 64, 32 - radius / 2, 64 + radius, 32);
        draw_line(stamp->seal, 64 + radius, 32, 64, 32 + radius / 2);
        draw_line(stamp->seal, 64, 32 + radius / 2, 64 - radius, 32);
    }
}

static void fill_stamp(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime,
    GoshuinMeasurementSource source,
    uint32_t frequency_hz,
    uint16_t interval_ms,
    const int8_t* samples,
    size_t count) {
    memset(stamp, 0, sizeof(*stamp));
    stamp->year = datetime->year;
    stamp->month = datetime->month;
    stamp->day = datetime->day;
    stamp->hour = datetime->hour;
    stamp->minute = datetime->minute;
    stamp->second = datetime->second;
    stamp->measurement_source = source;
    stamp->measurement_status = GoshuinMeasurementStatusCompleted;
    stamp->sample_count = (uint8_t)count;
    stamp->planned_sample_count = (uint8_t)count;
    stamp->sample_interval_ms = interval_ms;
    stamp->frequency_hz = frequency_hz;
    strncpy(stamp->location, location, sizeof(stamp->location) - 1U);
    memcpy(stamp->rssi_samples, samples, count);
    int32_t total = 0;
    stamp->rssi_min = samples[0];
    stamp->rssi_max = samples[0];
    for(size_t i = 0; i < count; ++i) {
        if(samples[i] < stamp->rssi_min) stamp->rssi_min = samples[i];
        if(samples[i] > stamp->rssi_max) stamp->rssi_max = samples[i];
        total += samples[i];
    }
    stamp->rssi_average = (int8_t)(total / (int32_t)count);
    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, stamp->location, strlen(stamp->location));
    seed = fnv_update(seed, &stamp->year, sizeof(stamp->year));
    seed = fnv_update(seed, &stamp->month, 6U);
    seed = fnv_update(seed, &stamp->measurement_source, 1U);
    seed = fnv_update(seed, &stamp->frequency_hz, sizeof(stamp->frequency_hz));
    seed = fnv_update(seed, &stamp->sample_count, 1U);
    seed = fnv_update(seed, stamp->rssi_samples, count);
    stamp->seed = seed ? seed : 0x6D2B79F5UL;
    const GoshuinThemeBandInput input = {
        .samples = stamp->rssi_samples,
        .sample_count = stamp->sample_count,
    };
    const GoshuinThemeId theme = source == GoshuinMeasurementDummy ? GoshuinThemeDemo :
                                                                     GoshuinTheme433;
    if(!goshuin_theme_render(theme, &input, 1U, stamp->seed, stamp->seal)) {
        generate_seal(stamp);
    }
}

void goshuin_stamp_create_demo(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime) {
    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, location, strlen(location));
    seed = fnv_update(seed, &datetime->year, sizeof(datetime->year));
    seed = fnv_update(seed, &datetime->month, 6U);
    uint32_t state = seed;
    int8_t samples[GOSHUIN_V1_RSSI_SAMPLE_COUNT];
    for(size_t i = 0; i < GOSHUIN_V1_RSSI_SAMPLE_COUNT; ++i) {
        samples[i] = (int8_t)(-92 + (int32_t)(prng_next(&state) % 53U));
    }
    fill_stamp(stamp, location, datetime, GoshuinMeasurementDummy, 0, 0, samples, 16U);
}

bool goshuin_stamp_create_measured(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime,
    uint32_t frequency_hz,
    uint16_t interval_ms,
    const int8_t* samples,
    size_t count) {
    if(!stamp || !location || !datetime || !samples || !count ||
       (count > GOSHUIN_MAX_RSSI_SAMPLE_COUNT)) {
        return false;
    }
    fill_stamp(
        stamp,
        location,
        datetime,
        GoshuinMeasurementSubGhzReceive,
        frequency_hz,
        interval_ms,
        samples,
        count);
    return true;
}

bool goshuin_stamp_decode_payload(
    uint16_t version,
    const void* payload,
    size_t size,
    GoshuinStampPayload* stamp) {
    if(!payload || !stamp) return false;
    if(version == GOSHUIN_FILE_VERSION_CURRENT) {
        if(size != sizeof(*stamp)) return false;
        memcpy(stamp, payload, sizeof(*stamp));
        if(!stamp->sample_count || (stamp->sample_count > GOSHUIN_MAX_RSSI_SAMPLE_COUNT) ||
           (stamp->planned_sample_count > GOSHUIN_MAX_RSSI_SAMPLE_COUNT) ||
           (stamp->planned_sample_count < stamp->sample_count) ||
           (stamp->measurement_source > GoshuinMeasurementSubGhzReceive) ||
           (stamp->measurement_status > GoshuinMeasurementStatusFailed)) {
            return false;
        }
    } else if(version == GOSHUIN_FILE_VERSION_V1) {
        if(size != sizeof(GoshuinStampPayloadV1)) return false;
        const GoshuinStampPayloadV1* old = payload;
        if(!old->sample_count || (old->sample_count > GOSHUIN_V1_RSSI_SAMPLE_COUNT) ||
           (old->measurement_source > GoshuinMeasurementSubGhzReceive)) {
            return false;
        }
        memset(stamp, 0, sizeof(*stamp));
        stamp->year = old->year;
        stamp->month = old->month;
        stamp->day = old->day;
        stamp->hour = old->hour;
        stamp->minute = old->minute;
        stamp->second = old->second;
        stamp->measurement_source = old->measurement_source;
        stamp->measurement_status = GoshuinMeasurementStatusCompleted;
        stamp->sample_count = old->sample_count;
        stamp->planned_sample_count = old->sample_count;
        stamp->rssi_min = old->rssi_min;
        stamp->rssi_max = old->rssi_max;
        stamp->rssi_average = old->rssi_average;
        stamp->seed = old->seed;
        memcpy(stamp->location, old->location, sizeof(stamp->location));
        memcpy(stamp->rssi_samples, old->rssi_samples, old->sample_count);
        memcpy(stamp->seal, old->seal, sizeof(stamp->seal));
    } else {
        return false;
    }
    stamp->location[GOSHUIN_LOCATION_CAPACITY - 1U] = '\0';
    return true;
}

uint32_t goshuin_crc32(const void* data, size_t size) {
    const uint8_t* bytes = data;
    uint32_t crc = 0xFFFFFFFFUL;
    for(size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for(uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t) - (int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}
