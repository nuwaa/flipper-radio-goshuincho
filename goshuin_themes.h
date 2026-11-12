#pragma once

#include "goshuin_stamp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GOSHUIN_THEME_ALGORITHM_VERSION 1U

typedef enum {
    GoshuinThemeDemo = 0,
    GoshuinThemeCombined = 1,
    GoshuinTheme315 = 2,
    GoshuinTheme426 = 3,
    GoshuinTheme429 = 4,
    GoshuinTheme433 = 5,
    GoshuinTheme920 = 6,
} GoshuinThemeId;

typedef struct {
    const int8_t* samples;
    size_t sample_count;
} GoshuinThemeBandInput;

typedef struct {
    int8_t minimum;
    int8_t maximum;
    int8_t average;
    int8_t trend;
    uint8_t range;
    uint8_t peak_count;
    uint8_t high_count;
    uint8_t turn_count;
} GoshuinRssiFeatures;

bool goshuin_theme_extract_features(
    const int8_t* samples,
    size_t sample_count,
    GoshuinRssiFeatures* features);
bool goshuin_theme_render(
    GoshuinThemeId theme,
    const GoshuinThemeBandInput* bands,
    size_t band_count,
    uint32_t seed,
    uint8_t* seal);
const char* goshuin_theme_name(GoshuinThemeId theme);

