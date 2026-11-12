#pragma once

#include <datetime/datetime.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GOSHUIN_LOCATION_CAPACITY     25U
#define GOSHUIN_V1_RSSI_SAMPLE_COUNT  16U
#define GOSHUIN_MAX_RSSI_SAMPLE_COUNT 50U
#define GOSHUIN_SEAL_WIDTH            128U
#define GOSHUIN_SEAL_HEIGHT           64U
#define GOSHUIN_SEAL_BYTES            ((GOSHUIN_SEAL_WIDTH * GOSHUIN_SEAL_HEIGHT) / 8U)
#define GOSHUIN_FILE_VERSION_V1       1U
#define GOSHUIN_FILE_VERSION_CURRENT  2U

typedef enum {
    GoshuinMeasurementDummy = 0,
    GoshuinMeasurementSubGhzReceive = 1,
    GoshuinMeasurementMultiBandReceive = 2,
    GoshuinMeasurementThemedMultiBand = 3,
    GoshuinMeasurementCollectibleMultiBand = 4,
} GoshuinMeasurementSource;

typedef enum {
    GoshuinMeasurementStatusUnknown = 0,
    GoshuinMeasurementStatusCompleted = 1,
    GoshuinMeasurementStatusInterrupted = 2,
    GoshuinMeasurementStatusFailed = 3,
} GoshuinMeasurementStatus;

/* Exact Phase A layout: do not change. */
typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    uint8_t measurement_source;
    uint8_t sample_count;
    int8_t rssi_samples[GOSHUIN_V1_RSSI_SAMPLE_COUNT];
    int8_t rssi_min, rssi_max, rssi_average;
    uint8_t reserved0;
    uint32_t seed;
    char location[GOSHUIN_LOCATION_CAPACITY];
    uint8_t reserved1[3];
    uint8_t seal[GOSHUIN_SEAL_BYTES];
} GoshuinStampPayloadV1;

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    uint8_t measurement_source;
    uint8_t measurement_status;
    uint8_t sample_count;
    uint8_t planned_sample_count;
    uint16_t sample_interval_ms;
    uint32_t frequency_hz;
    int8_t rssi_min, rssi_max, rssi_average;
    uint8_t reserved0;
    uint32_t seed;
    char location[GOSHUIN_LOCATION_CAPACITY];
    uint8_t reserved1[3];
    int8_t rssi_samples[GOSHUIN_MAX_RSSI_SAMPLE_COUNT];
    uint8_t seal[GOSHUIN_SEAL_BYTES];
} GoshuinStampPayload;

_Static_assert(sizeof(GoshuinStampPayloadV1) == 1085U, "Version 1 layout changed");
_Static_assert(sizeof(GoshuinStampPayload) == 1127U, "Version 2 layout changed");

void goshuin_stamp_create_demo(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime);
bool goshuin_stamp_create_measured(
    GoshuinStampPayload* stamp,
    const char* location,
    const DateTime* datetime,
    uint32_t frequency_hz,
    uint16_t sample_interval_ms,
    const int8_t* samples,
    size_t sample_count);
bool goshuin_stamp_decode_payload(
    uint16_t format_version,
    const void* payload,
    size_t payload_size,
    GoshuinStampPayload* stamp);
uint32_t goshuin_crc32(const void* data, size_t size);
