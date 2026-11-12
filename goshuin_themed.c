#include "goshuin_themed.h"

#include <string.h>

static uint32_t fnv_update(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for(size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 16777619UL;
    return hash;
}

static size_t theme_band_index(GoshuinThemeId theme) {
    switch(theme) {
    case GoshuinTheme315:
        return 0U;
    case GoshuinTheme426:
        return 1U;
    case GoshuinTheme429:
        return 2U;
    case GoshuinTheme433:
        return 3U;
    case GoshuinTheme920:
        return 4U;
    default:
        return GOSHUIN_MULTIBAND_COUNT;
    }
}

static bool theme_valid(GoshuinThemeId theme) {
    return (theme >= GoshuinThemeCombined) && (theme <= GoshuinTheme920);
}

static uint32_t themed_seed(const GoshuinStampPayloadV4* stamp) {
    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, stamp->location, strlen(stamp->location));
    seed = fnv_update(seed, &stamp->year, 7U);
    seed = fnv_update(seed, &stamp->theme_id, 1U);
    seed = fnv_update(seed, &stamp->design_algorithm_version, 1U);
    const GoshuinThemeId theme = (GoshuinThemeId)stamp->theme_id;
    const size_t selected = theme_band_index(theme);
    const size_t first = theme == GoshuinThemeCombined ? 0U : selected;
    const size_t end = theme == GoshuinThemeCombined ? GOSHUIN_MULTIBAND_COUNT : selected + 1U;
    for(size_t i = first; i < end; ++i) {
        const GoshuinBandPayloadV3* band = &stamp->bands[i];
        seed = fnv_update(seed, &band->band_id, sizeof(band->band_id));
        seed = fnv_update(seed, &band->actual_frequency_hz, sizeof(band->actual_frequency_hz));
        seed = fnv_update(seed, band->rssi_samples, sizeof(band->rssi_samples));
    }
    return seed ? seed : 0x6D2B79F5UL;
}

static bool render(GoshuinStampPayloadV4* stamp) {
    GoshuinThemeBandInput input[GOSHUIN_MULTIBAND_COUNT];
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        input[i].samples = stamp->bands[i].rssi_samples;
        input[i].sample_count = GOSHUIN_MULTIBAND_SAMPLES;
    }
    const GoshuinThemeId theme = (GoshuinThemeId)stamp->theme_id;
    const size_t selected = theme_band_index(theme);
    if(theme == GoshuinThemeCombined) {
        return goshuin_theme_render(theme, input, GOSHUIN_MULTIBAND_COUNT, stamp->seed, stamp->seal);
    }
    return goshuin_theme_render(theme, &input[selected], 1U, stamp->seed, stamp->seal);
}

static bool metadata_valid(const GoshuinStampPayloadV4* stamp) {
    if((stamp->measurement_source != GoshuinMeasurementThemedMultiBand) ||
       (stamp->measurement_status != GoshuinMeasurementStatusCompleted) ||
       (stamp->band_count != GOSHUIN_MULTIBAND_COUNT) ||
       !theme_valid((GoshuinThemeId)stamp->theme_id) ||
       (stamp->design_algorithm_version != GOSHUIN_THEME_ALGORITHM_VERSION) ||
       (stamp->samples_per_band != GOSHUIN_MULTIBAND_SAMPLES) ||
       (stamp->preset_id != GOSHUIN_MULTIBAND_PRESET_OOK650) || stamp->reserved0[0] ||
       stamp->reserved0[1] || stamp->reserved1[0] || stamp->reserved1[1] || stamp->reserved1[2]) {
        return false;
    }
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        if(stamp->bands[i].band_id != goshuin_multiband_ids[i]) return false;
    }
    return true;
}

static void v4_as_v3(const GoshuinStampPayloadV4* stamp, GoshuinStampPayloadV3* legacy) {
    memset(legacy, 0, sizeof(*legacy));
    legacy->year = stamp->year;
    legacy->month = stamp->month;
    legacy->day = stamp->day;
    legacy->hour = stamp->hour;
    legacy->minute = stamp->minute;
    legacy->second = stamp->second;
    legacy->measurement_source = GoshuinMeasurementMultiBandReceive;
    legacy->measurement_status = stamp->measurement_status;
    legacy->band_count = stamp->band_count;
    legacy->algorithm_version = GOSHUIN_MULTIBAND_ALGORITHM_VERSION;
    legacy->samples_per_band = stamp->samples_per_band;
    legacy->preset_id = stamp->preset_id;
    legacy->observation_window_ms = stamp->observation_window_ms;
    legacy->settle_time_ms = stamp->settle_time_ms;
    legacy->peak_read_interval_ms = stamp->peak_read_interval_ms;
    memcpy(legacy->bands, stamp->bands, sizeof(legacy->bands));
    memcpy(legacy->location, stamp->location, sizeof(legacy->location));
}

bool goshuin_themed_create(
    GoshuinStampPayloadV4* stamp,
    const GoshuinStampPayloadV3* observation,
    GoshuinThemeId theme) {
    if(!stamp || !observation || !theme_valid(theme)) return false;
    GoshuinStampPayloadV3 validated;
    if(!goshuin_multiband_decode(observation, sizeof(*observation), &validated)) return false;
    memset(stamp, 0, sizeof(*stamp));
    stamp->year = validated.year;
    stamp->month = validated.month;
    stamp->day = validated.day;
    stamp->hour = validated.hour;
    stamp->minute = validated.minute;
    stamp->second = validated.second;
    stamp->measurement_source = GoshuinMeasurementThemedMultiBand;
    stamp->measurement_status = GoshuinMeasurementStatusCompleted;
    stamp->band_count = GOSHUIN_MULTIBAND_COUNT;
    stamp->theme_id = (uint8_t)theme;
    stamp->design_algorithm_version = GOSHUIN_THEME_ALGORITHM_VERSION;
    stamp->samples_per_band = GOSHUIN_MULTIBAND_SAMPLES;
    stamp->preset_id = validated.preset_id;
    stamp->observation_window_ms = validated.observation_window_ms;
    stamp->settle_time_ms = validated.settle_time_ms;
    stamp->peak_read_interval_ms = validated.peak_read_interval_ms;
    memcpy(stamp->bands, validated.bands, sizeof(stamp->bands));
    memcpy(stamp->location, validated.location, sizeof(stamp->location));
    stamp->seed = themed_seed(stamp);
    return render(stamp);
}

bool goshuin_themed_decode(
    const void* payload,
    size_t payload_size,
    GoshuinStampPayloadV4* stamp) {
    if(!payload || !stamp || (payload_size != sizeof(*stamp))) return false;
    memcpy(stamp, payload, sizeof(*stamp));
    if((stamp->location[GOSHUIN_LOCATION_CAPACITY - 1U] != '\0') || !metadata_valid(stamp)) {
        return false;
    }
    GoshuinStampPayloadV3 legacy;
    v4_as_v3(stamp, &legacy);
    if(!goshuin_multiband_finalize(&legacy)) return false;
    if(memcmp(legacy.bands, stamp->bands, sizeof(stamp->bands))) return false;
    if(stamp->seed != themed_seed(stamp)) return false;
    uint8_t stored[GOSHUIN_SEAL_BYTES];
    memcpy(stored, stamp->seal, sizeof(stored));
    if(!render(stamp)) return false;
    return !memcmp(stored, stamp->seal, sizeof(stored));
}
