#include "goshuin_themes.h"

#include <string.h>

static void pixel(uint8_t* seal, int32_t x, int32_t y) {
    if((x < 0) || (x >= 128) || (y < 0) || (y >= 64)) return;
    seal[(size_t)y * 16U + (size_t)x / 8U] |= (uint8_t)(1U << ((uint32_t)x & 7U));
}

static void line(uint8_t* seal, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    const int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int32_t sx = x0 < x1 ? 1 : -1;
    const int32_t ay = y1 > y0 ? y1 - y0 : y0 - y1;
    const int32_t dy = -ay;
    const int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    while(true) {
        pixel(seal, x0, y0);
        if((x0 == x1) && (y0 == y1)) break;
        const int32_t twice = error * 2;
        if(twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if(twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void thick_line(uint8_t* seal, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    line(seal, x0, y0, x1, y1);
    if((x1 - x0 > 0 ? x1 - x0 : x0 - x1) >= (y1 - y0 > 0 ? y1 - y0 : y0 - y1)) {
        line(seal, x0, y0 + 1, x1, y1 + 1);
    } else {
        line(seal, x0 + 1, y0, x1 + 1, y1);
    }
}

static void rectangle(uint8_t* seal, int32_t left, int32_t top, int32_t right, int32_t bottom) {
    line(seal, left, top, right, top);
    line(seal, right, top, right, bottom);
    line(seal, right, bottom, left, bottom);
    line(seal, left, bottom, left, top);
}

static void circle(uint8_t* seal, int32_t cx, int32_t cy, int32_t radius) {
    int32_t x = radius;
    int32_t y = 0;
    int32_t error = 1 - radius;
    while(x >= y) {
        pixel(seal, cx + x, cy + y);
        pixel(seal, cx + y, cy + x);
        pixel(seal, cx - y, cy + x);
        pixel(seal, cx - x, cy + y);
        pixel(seal, cx - x, cy - y);
        pixel(seal, cx - y, cy - x);
        pixel(seal, cx + y, cy - x);
        pixel(seal, cx + x, cy - y);
        ++y;
        if(error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

static void diamond(uint8_t* seal, int32_t cx, int32_t cy, int32_t rx, int32_t ry) {
    line(seal, cx, cy - ry, cx + rx, cy);
    line(seal, cx + rx, cy, cx, cy + ry);
    line(seal, cx, cy + ry, cx - rx, cy);
    line(seal, cx - rx, cy, cx, cy - ry);
}

static void hexagon(uint8_t* seal, int32_t cx, int32_t cy, int32_t rx, int32_t ry) {
    line(seal, cx - rx / 2, cy - ry, cx + rx / 2, cy - ry);
    line(seal, cx + rx / 2, cy - ry, cx + rx, cy);
    line(seal, cx + rx, cy, cx + rx / 2, cy + ry);
    line(seal, cx + rx / 2, cy + ry, cx - rx / 2, cy + ry);
    line(seal, cx - rx / 2, cy + ry, cx - rx, cy);
    line(seal, cx - rx, cy, cx - rx / 2, cy - ry);
}

static void stamp_border(uint8_t* seal) {
    rectangle(seal, 1, 1, 126, 62);
    rectangle(seal, 3, 3, 124, 60);
    pixel(seal, 6, 6);
    pixel(seal, 121, 6);
    pixel(seal, 6, 57);
    pixel(seal, 121, 57);
}

static uint8_t strength_bucket(const GoshuinRssiFeatures* features) {
    int32_t level = features->average;
    if(level < -120) level = -120;
    if(level > -40) level = -40;
    return (uint8_t)((level + 120) / 20);
}

bool goshuin_theme_extract_features(
    const int8_t* samples,
    size_t count,
    GoshuinRssiFeatures* result) {
    if(!samples || !count || !result) return false;
    memset(result, 0, sizeof(*result));
    result->minimum = samples[0];
    result->maximum = samples[0];
    int32_t total = 0;
    for(size_t i = 0; i < count; ++i) {
        if(samples[i] < result->minimum) result->minimum = samples[i];
        if(samples[i] > result->maximum) result->maximum = samples[i];
        total += samples[i];
    }
    result->average = (int8_t)(total / (int32_t)count);
    result->range = (uint8_t)((int32_t)result->maximum - result->minimum);
    result->trend = (int8_t)((int32_t)samples[count - 1U] - samples[0]);
    const int32_t relative_threshold = result->average + (result->range / 4U);
    int32_t last_direction = 0;
    for(size_t i = 0; i < count; ++i) {
        if(samples[i] >= relative_threshold) ++result->high_count;
        if((i > 0U) && ((i + 1U) < count) && (samples[i] > samples[i - 1U]) &&
           (samples[i] >= samples[i + 1U]) && (samples[i] >= result->average + 4)) {
            ++result->peak_count;
        }
        if(i) {
            const int32_t difference = (int32_t)samples[i] - samples[i - 1U];
            const int32_t direction = (difference > 2) ? 1 : (difference < -2 ? -1 : 0);
            if(direction && last_direction && (direction != last_direction)) ++result->turn_count;
            if(direction) last_direction = direction;
        }
    }
    return true;
}

static void demo_theme(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    rectangle(seal, 28, 2, 99, 61);
    rectangle(seal, 31, 5, 96, 58);
    const int32_t layers = 2 + f->peak_count % 3U + strength_bucket(f) / 2U;
    for(int32_t i = 0; i < layers; ++i) diamond(seal, 64, 31, 10 + i * 6, 6 + i * 4);
    circle(seal, 64, 31, 5 + f->range / 12U);
    thick_line(seal, 64, 12, 64, 50);
    thick_line(seal, 43, 31, 85, 31);
    const int32_t marks = 2 + f->high_count % 5U;
    for(int32_t i = 0; i < marks; ++i) {
        const int32_t y = 12 + i * 9;
        pixel(seal, 36 + (seed >> (i % 8)) % 5U, y);
        pixel(seal, 91 - (seed >> (i % 8)) % 5U, 63 - y);
    }
}

static void theme_315(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    stamp_border(seal);
    const int32_t rings = 2 + f->peak_count % 3U + strength_bucket(f) / 2U;
    for(int32_t i = 0; i < rings; ++i) circle(seal, 64, 27, 10 + i * 6);
    circle(seal, 64, 25, 7 + f->range / 16U);
    thick_line(seal, 60, 31, 60, 52);
    thick_line(seal, 68, 31, 68, 52);
    line(seal, 60, 52, 68, 52);
    line(seal, 68, 45, 75 + (seed % 8U), 45);
    const int32_t waves = 3 + (int32_t)(f->turn_count % 3U);
    for(int32_t i = 0; i < waves; ++i) {
        const int32_t offset = 8 + i * 7;
        line(seal, 18, 32 - i * 3, 36 + offset / 2, 32);
        line(seal, 110, 32 - i * 3, 92 - offset / 2, 32);
    }
}

static void theme_426(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    stamp_border(seal);
    for(int32_t x = 10; x <= 38; x += 7) line(seal, x, 10, x, 54);
    for(int32_t y = 10; y <= 54; y += 11) line(seal, 10, y, 38, y);
    for(int32_t x = 90; x <= 118; x += 7) line(seal, x, 10, x, 54);
    for(int32_t y = 10; y <= 54; y += 11) line(seal, 90, y, 118, y);
    thick_line(seal, 64, 8, 48, 55);
    thick_line(seal, 64, 8, 80, 55);
    line(seal, 48, 55, 80, 55);
    for(int32_t y = 20; y < 52; y += 8) line(seal, 52, y, 76, y);
    for(int32_t i = 0; i <= strength_bucket(f); ++i) {
        rectangle(seal, 14 + i * 5, 44 - i * 2, 17 + i * 5, 51);
    }
    const int32_t rays = 2 + f->peak_count % 4U;
    for(int32_t i = 0; i < rays; ++i) {
        line(seal, 64, 12, 45 - i * 5, 8 + i * 4);
        line(seal, 64, 12, 83 + i * 5, 8 + i * 4);
    }
    if(seed & 1U) diamond(seal, 64, 36, 7 + f->range / 15U, 5);
}

static void theme_429(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    stamp_border(seal);
    const int32_t radius = 18 + f->range / 10U;
    hexagon(seal, 64, 32, radius, radius > 24 ? 24 : radius);
    hexagon(seal, 64, 32, radius - 7, radius > 20 ? 17 : radius - 5);
    circle(seal, 64, 32, 6 + f->high_count % 4U + strength_bucket(f));
    static const int8_t directions[8][2] = {
        {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
    const int32_t teeth = 5 + f->peak_count % 4U;
    for(int32_t i = 0; i < teeth; ++i) {
        const size_t d = (i + seed) % 8U;
        line(
            seal,
            64 + directions[d][0] * 25,
            32 + directions[d][1] * 20,
            64 + directions[d][0] * 31,
            32 + directions[d][1] * 26);
    }
    thick_line(seal, 34, 8, 94, 56);
    thick_line(seal, 94, 8, 34, 56);
}

static void theme_433(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    stamp_border(seal);
    const int32_t radius = 15 + f->range / 12U + strength_bucket(f);
    circle(seal, 45, 32, radius);
    circle(seal, 45, 32, radius - 4);
    circle(seal, 45, 32, 4);
    const int32_t spokes = 4 + f->peak_count % 4U + strength_bucket(f) / 2U;
    for(int32_t i = 0; i < spokes; ++i) {
        const int32_t dx = (i & 1) ? radius : 0;
        const int32_t dy = (i & 1) ? 0 : radius;
        line(seal, 45 - dx, 32 - dy, 45 + dx, 32 + dy);
    }
    const int32_t bend = (f->trend > 0 ? -1 : 1) * (3 + f->turn_count % 5U);
    for(int32_t track = 0; track < 3; ++track) {
        int32_t last_x = 68;
        int32_t last_y = 18 + track * 14;
        for(int32_t step = 1; step <= 5; ++step) {
            const int32_t x = 68 + step * 10;
            const int32_t y = 18 + track * 14 + ((step & 1) ? bend : -bend) +
                              (int32_t)((seed >> step) & 1U);
            line(seal, last_x, last_y, x, y);
            last_x = x;
            last_y = y;
        }
    }
}

static void draw_node(uint8_t* seal, int32_t x, int32_t y, uint8_t style) {
    switch(style % 4U) {
    case 0:
        circle(seal, x, y, 3);
        break;
    case 1:
        diamond(seal, x, y, 4, 3);
        break;
    case 2:
        rectangle(seal, x - 3, y - 3, x + 3, y + 3);
        break;
    default:
        line(seal, x - 4, y, x + 4, y);
        line(seal, x, y - 4, x, y + 4);
        break;
    }
    pixel(seal, x, y);
}

static void theme_920(uint8_t* seal, const GoshuinRssiFeatures* f, uint32_t seed) {
    stamp_border(seal);
    static const int8_t nodes[10][2] = {
        {18, 14}, {42, 10}, {67, 16}, {96, 9}, {113, 25},
        {103, 49}, {75, 53}, {48, 45}, {20, 51}, {61, 32}};
    const size_t count = 6U + (f->high_count + strength_bucket(f)) % 5U;
    for(size_t i = 0; i < count; ++i) {
        const size_t next = (i + 1U + f->peak_count) % count;
        line(seal, nodes[i][0], nodes[i][1], nodes[next][0], nodes[next][1]);
        if((i & 1U) && (f->range > 12U)) line(seal, nodes[i][0], nodes[i][1], 61, 32);
    }
    for(size_t i = 0; i < count; ++i) {
        draw_node(seal, nodes[i][0], nodes[i][1], (uint8_t)(i + seed));
    }
    circle(seal, 61, 32, 6 + f->peak_count % 4U);
}

static void combined_theme(
    uint8_t* seal,
    const GoshuinRssiFeatures* features,
    uint32_t seed) {
    stamp_border(seal);
    circle(seal, 64, 32, 29);
    circle(seal, 64, 32, 26);
    static const int8_t centers[5][2] = {{64, 12}, {87, 25}, {79, 49}, {49, 49}, {41, 25}};
    for(size_t i = 0; i < 5U; ++i) {
        const int32_t x = centers[i][0];
        const int32_t y = centers[i][1];
        line(seal, 64, 32, x, y);
        draw_node(seal, x, y, (uint8_t)i);
        const int32_t radius = 3 + features[i].range / 18U + features[i].peak_count % 3U +
                               strength_bucket(&features[i]) / 2U;
        circle(seal, x, y, radius > 8 ? 8 : radius);
        if(features[i].trend > 2) line(seal, x, y, x + 5, y - 5);
        else if(features[i].trend < -2) line(seal, x, y, x - 5, y + 5);
    }
    diamond(seal, 64, 32, 11 + (seed % 5U), 8 + ((seed >> 4) % 4U));
    circle(seal, 64, 32, 4);
    for(size_t i = 0; i < 5U; ++i) {
        const size_t next = (i + 1U) % 5U;
        line(seal, centers[i][0], centers[i][1], centers[next][0], centers[next][1]);
    }
}

bool goshuin_theme_render(
    GoshuinThemeId theme,
    const GoshuinThemeBandInput* bands,
    size_t band_count,
    uint32_t seed,
    uint8_t* seal) {
    if(!bands || !band_count || !seal) return false;
    GoshuinRssiFeatures features[5];
    if(band_count > 5U) return false;
    for(size_t i = 0; i < band_count; ++i) {
        if(!goshuin_theme_extract_features(bands[i].samples, bands[i].sample_count, &features[i])) {
            return false;
        }
    }
    memset(seal, 0, GOSHUIN_SEAL_BYTES);
    if(theme == GoshuinThemeCombined) {
        if(band_count != 5U) return false;
        combined_theme(seal, features, seed);
        return true;
    }
    const GoshuinRssiFeatures* f = &features[0];
    switch(theme) {
    case GoshuinThemeDemo:
        demo_theme(seal, f, seed);
        break;
    case GoshuinTheme315:
        theme_315(seal, f, seed);
        break;
    case GoshuinTheme426:
        theme_426(seal, f, seed);
        break;
    case GoshuinTheme429:
        theme_429(seal, f, seed);
        break;
    case GoshuinTheme433:
        theme_433(seal, f, seed);
        break;
    case GoshuinTheme920:
        theme_920(seal, f, seed);
        break;
    default:
        return false;
    }
    return true;
}

const char* goshuin_theme_name(GoshuinThemeId theme) {
    switch(theme) {
    case GoshuinThemeDemo:
        return "Demo Seal";
    case GoshuinThemeCombined:
        return "5 Bands";
    case GoshuinTheme315:
        return "315 MHz";
    case GoshuinTheme426:
        return "426 MHz";
    case GoshuinTheme429:
        return "429 MHz";
    case GoshuinTheme433:
        return "433 MHz";
    case GoshuinTheme920:
        return "920 MHz";
    default:
        return "Unknown";
    }
}
