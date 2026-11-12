#include "goshuin_collectible.h"

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

static void build_inputs(
    const GoshuinStampPayloadV5* stamp,
    GoshuinThemeBandInput* inputs) {
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        inputs[i].samples = stamp->bands[i].rssi_samples;
        inputs[i].sample_count = GOSHUIN_MULTIBAND_SAMPLES;
    }
}

static uint32_t collectible_seed(const GoshuinStampPayloadV5* stamp) {
    uint32_t seed = 2166136261UL;
    seed = fnv_update(seed, stamp->location, strlen(stamp->location));
    seed = fnv_update(seed, &stamp->year, 7U);
    seed = fnv_update(seed, &stamp->theme_id, 1U);
    seed = fnv_update(seed, &stamp->design_algorithm_version, 1U);
    seed = fnv_update(seed, &stamp->season_id, 1U);
    seed = fnv_update(seed, &stamp->rarity_id, 1U);
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

static bool render(GoshuinStampPayloadV5* stamp) {
    GoshuinThemeBandInput inputs[GOSHUIN_MULTIBAND_COUNT];
    build_inputs(stamp, inputs);
    const GoshuinThemeId theme = (GoshuinThemeId)stamp->theme_id;
    bool success;
    if(theme == GoshuinThemeCombined) {
        success = goshuin_theme_render(
            theme, inputs, GOSHUIN_MULTIBAND_COUNT, stamp->seed, stamp->seal);
    } else {
        success = goshuin_theme_render(
            theme, &inputs[theme_band_index(theme)], 1U, stamp->seed, stamp->seal);
    }
    if(!success) return false;
    return goshuin_theme_apply_collectible(
        stamp->seal,
        (GoshuinSeasonId)stamp->season_id,
        (GoshuinRarityId)stamp->rarity_id,
        stamp->seed);
}

static bool metadata_valid(const GoshuinStampPayloadV5* stamp) {
    if((stamp->measurement_source != GoshuinMeasurementCollectibleMultiBand) ||
       (stamp->measurement_status != GoshuinMeasurementStatusCompleted) ||
       (stamp->band_count != GOSHUIN_MULTIBAND_COUNT) ||
       !theme_valid((GoshuinThemeId)stamp->theme_id) ||
       (stamp->design_algorithm_version != GOSHUIN_COLLECTIBLE_ALGORITHM_VERSION) ||
       (stamp->samples_per_band != GOSHUIN_MULTIBAND_SAMPLES) ||
       (stamp->preset_id != GOSHUIN_MULTIBAND_PRESET_OOK650) ||
       (stamp->season_id != goshuin_theme_season_from_month(stamp->month)) ||
       (stamp->rarity_id > GoshuinRarityHarmony) || stamp->reserved1[0] ||
       stamp->reserved1[1] || stamp->reserved1[2]) {
        return false;
    }
    for(size_t i = 0; i < GOSHUIN_MULTIBAND_COUNT; ++i) {
        if(stamp->bands[i].band_id != goshuin_multiband_ids[i]) return false;
    }
    GoshuinThemeBandInput inputs[GOSHUIN_MULTIBAND_COUNT];
    build_inputs(stamp, inputs);
    return stamp->rarity_id ==
           goshuin_theme_classify_rarity(inputs, GOSHUIN_MULTIBAND_COUNT);
}

static void v5_as_v3(const GoshuinStampPayloadV5* stamp, GoshuinStampPayloadV3* legacy) {
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

bool goshuin_collectible_create(
    GoshuinStampPayloadV5* stamp,
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
    stamp->measurement_source = GoshuinMeasurementCollectibleMultiBand;
    stamp->measurement_status = GoshuinMeasurementStatusCompleted;
    stamp->band_count = GOSHUIN_MULTIBAND_COUNT;
    stamp->theme_id = (uint8_t)theme;
    stamp->design_algorithm_version = GOSHUIN_COLLECTIBLE_ALGORITHM_VERSION;
    stamp->samples_per_band = GOSHUIN_MULTIBAND_SAMPLES;
    stamp->preset_id = validated.preset_id;
    stamp->observation_window_ms = validated.observation_window_ms;
    stamp->settle_time_ms = validated.settle_time_ms;
    stamp->peak_read_interval_ms = validated.peak_read_interval_ms;
    memcpy(stamp->bands, validated.bands, sizeof(stamp->bands));
    memcpy(stamp->location, validated.location, sizeof(stamp->location));
    stamp->season_id = goshuin_theme_season_from_month(stamp->month);
    GoshuinThemeBandInput inputs[GOSHUIN_MULTIBAND_COUNT];
    build_inputs(stamp, inputs);
    stamp->rarity_id = goshuin_theme_classify_rarity(inputs, GOSHUIN_MULTIBAND_COUNT);
    stamp->seed = collectible_seed(stamp);
    return render(stamp);
}

bool goshuin_collectible_decode(
    const void* payload,
    size_t payload_size,
    GoshuinStampPayloadV5* stamp) {
    if(!payload || !stamp || (payload_size != sizeof(*stamp))) return false;
    memcpy(stamp, payload, sizeof(*stamp));
    if((stamp->location[GOSHUIN_LOCATION_CAPACITY - 1U] != '\0') || !metadata_valid(stamp)) {
        return false;
    }
    GoshuinStampPayloadV3 legacy;
    v5_as_v3(stamp, &legacy);
    if(!goshuin_multiband_finalize(&legacy) ||
       memcmp(legacy.bands, stamp->bands, sizeof(stamp->bands)) ||
       (stamp->seed != collectible_seed(stamp))) {
        return false;
    }
    uint8_t stored[GOSHUIN_SEAL_BYTES];
    memcpy(stored, stamp->seal, sizeof(stored));
    if(!render(stamp)) return false;
    return !memcmp(stored, stamp->seal, sizeof(stored));
}
