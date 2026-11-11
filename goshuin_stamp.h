#pragma once

#include <datetime/datetime.h>
#include <stddef.h>
#include <stdint.h>

#define GOSHUIN_LOCATION_CAPACITY 25U
#define GOSHUIN_RSSI_SAMPLE_COUNT 16U
#define GOSHUIN_SEAL_WIDTH        128U
#define GOSHUIN_SEAL_HEIGHT       64U
#define GOSHUIN_SEAL_BYTES        ((GOSHUIN_SEAL_WIDTH * GOSHUIN_SEAL_HEIGHT) / 8U)

typedef enum {
    GoshuinMeasurementDummy = 0,
    GoshuinMeasurementSubGhzReceive = 1,
} GoshuinMeasurementSource;

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t measurement_source;
    uint8_t sample_count;
    int8_t rssi_samples[GOSHUIN_RSSI_SAMPLE_COUNT];
    int8_t rssi_min;
    int8_t rssi_max;
    int8_t rssi_average;
    uint8_t reserved0;
    uint32_t seed;
    char location[GOSHUIN_LOCATION_CAPACITY];
    uint8_t reserved1[3];
    uint8_t seal[GOSHUIN_SEAL_BYTES];
} GoshuinStampPayload;

void goshuin_stamp_create(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime);
uint32_t goshuin_crc32(const void* data, size_t size);
