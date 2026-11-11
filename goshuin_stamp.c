#include "goshuin_stamp.h"

#include <stdbool.h>
#include <string.h>

static uint32_t goshuin_fnv1a_update(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for(size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

static uint32_t goshuin_prng_next(uint32_t* state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value ? value : 0x6D2B79F5UL;
    return *state;
}

static void goshuin_set_pixel(uint8_t* bitmap, int32_t x, int32_t y) {
    if((x < 0) || (x >= (int32_t)GOSHUIN_SEAL_WIDTH) || (y < 0) ||
       (y >= (int32_t)GOSHUIN_SEAL_HEIGHT)) {
        return;
    }
    bitmap[(size_t)y * (GOSHUIN_SEAL_WIDTH / 8U) + ((size_t)x / 8U)] |=
        (uint8_t)(1U << ((uint32_t)x & 7U));
}

static void goshuin_draw_line(uint8_t* bitmap, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    const int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    const int32_t sx = (x0 < x1) ? 1 : -1;
    const int32_t dy_abs = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    const int32_t dy = -dy_abs;
    const int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t error = dx + dy;

    while(true) {
        goshuin_set_pixel(bitmap, x0, y0);
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

static void goshuin_generate_seal(GoshuinStampPayload* stamp) {
    memset(stamp->seal, 0, sizeof(stamp->seal));

    for(int32_t x = 0; x < (int32_t)GOSHUIN_SEAL_WIDTH; ++x) {
        goshuin_set_pixel(stamp->seal, x, 0);
        goshuin_set_pixel(stamp->seal, x, 2);
        goshuin_set_pixel(stamp->seal, x, (int32_t)GOSHUIN_SEAL_HEIGHT - 3);
        goshuin_set_pixel(stamp->seal, x, (int32_t)GOSHUIN_SEAL_HEIGHT - 1);
    }
    for(int32_t y = 0; y < (int32_t)GOSHUIN_SEAL_HEIGHT; ++y) {
        goshuin_set_pixel(stamp->seal, 0, y);
        goshuin_set_pixel(stamp->seal, 2, y);
        goshuin_set_pixel(stamp->seal, (int32_t)GOSHUIN_SEAL_WIDTH - 3, y);
        goshuin_set_pixel(stamp->seal, (int32_t)GOSHUIN_SEAL_WIDTH - 1, y);
    }

    uint32_t random_state = stamp->seed;
    for(size_t i = 0; i < 220U; ++i) {
        const int32_t x = 4 + (int32_t)(goshuin_prng_next(&random_state) % 60U);
        const int32_t y = 4 + (int32_t)(goshuin_prng_next(&random_state) % 28U);
        goshuin_set_pixel(stamp->seal, x, y);
        goshuin_set_pixel(stamp->seal, 127 - x, y);
        goshuin_set_pixel(stamp->seal, x, 63 - y);
        goshuin_set_pixel(stamp->seal, 127 - x, 63 - y);
    }

    int32_t previous_x = 4;
    int32_t previous_y = 32;
    for(size_t i = 0; i < GOSHUIN_RSSI_SAMPLE_COUNT; ++i) {
        const int32_t x = 4 + (int32_t)i * 8;
        int32_t y = 54 - ((int32_t)stamp->rssi_samples[i] + 100);
        if(y < 7) y = 7;
        if(y > 56) y = 56;
        goshuin_draw_line(stamp->seal, previous_x, previous_y, x, y);
        goshuin_draw_line(stamp->seal, previous_x, 63 - previous_y, x, 63 - y);
        previous_x = x;
        previous_y = y;
    }

    const int32_t center_x = 64;
    const int32_t center_y = 32;
    for(int32_t radius = 5; radius <= 19; radius += 7) {
        goshuin_draw_line(
            stamp->seal, center_x - radius, center_y, center_x, center_y - (radius / 2));
        goshuin_draw_line(
            stamp->seal, center_x, center_y - (radius / 2), center_x + radius, center_y);
        goshuin_draw_line(
            stamp->seal, center_x + radius, center_y, center_x, center_y + (radius / 2));
        goshuin_draw_line(
            stamp->seal, center_x, center_y + (radius / 2), center_x - radius, center_y);
    }
}

void goshuin_stamp_create(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime) {
    memset(stamp, 0, sizeof(*stamp));
    stamp->year = datetime->year;
    stamp->month = datetime->month;
    stamp->day = datetime->day;
    stamp->hour = datetime->hour;
    stamp->minute = datetime->minute;
    stamp->second = datetime->second;
    stamp->measurement_source = GoshuinMeasurementDummy;
    stamp->sample_count = GOSHUIN_RSSI_SAMPLE_COUNT;
    strncpy(stamp->location, location, sizeof(stamp->location) - 1U);

    uint32_t seed = 2166136261UL;
    seed = goshuin_fnv1a_update(seed, stamp->location, strlen(stamp->location));
    seed = goshuin_fnv1a_update(seed, &stamp->year, sizeof(stamp->year));
    seed = goshuin_fnv1a_update(seed, &stamp->month, 6U);

    uint32_t random_state = seed;
    int32_t total = 0;
    stamp->rssi_min = 0;
    stamp->rssi_max = -127;
    for(size_t i = 0; i < GOSHUIN_RSSI_SAMPLE_COUNT; ++i) {
        const int8_t sample = (int8_t)(-92 + (int32_t)(goshuin_prng_next(&random_state) % 53U));
        stamp->rssi_samples[i] = sample;
        if(sample < stamp->rssi_min) stamp->rssi_min = sample;
        if(sample > stamp->rssi_max) stamp->rssi_max = sample;
        total += sample;
    }
    stamp->rssi_average = (int8_t)(total / (int32_t)GOSHUIN_RSSI_SAMPLE_COUNT);

    seed = goshuin_fnv1a_update(seed, stamp->rssi_samples, sizeof(stamp->rssi_samples));
    stamp->seed = seed ? seed : 0x6D2B79F5UL;
    goshuin_generate_seal(stamp);
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
