#include "goshuin_stamp.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/preset.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOSHUIN_FILE_MAGIC        "RGSF"
#define GOSHUIN_MAX_SAVED_STAMPS  32U
#define GOSHUIN_PATH_CAPACITY     96U
#define GOSHUIN_FILENAME_CAPACITY 80U
#define GOSHUIN_LABEL_CAPACITY    56U
#define GOSHUIN_RF_FREQUENCY_HZ   433920000UL
#define GOSHUIN_RF_INTERVAL_MS    200U
#define GOSHUIN_RF_SAMPLE_COUNT   50U

typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint16_t format_version;
    uint16_t header_size;
    uint32_t payload_size;
    uint32_t payload_crc32;
} GoshuinFileHeader;

typedef enum {
    GoshuinViewMain,
    GoshuinViewTextInput,
    GoshuinViewSaved,
    GoshuinViewDisplay,
    GoshuinViewMeasure,
} GoshuinViewId;

typedef enum {
    GoshuinMainReal,
    GoshuinMainDemo,
    GoshuinMainSaved,
    GoshuinMainAbout,
} GoshuinMainItem;

typedef enum {
    GoshuinCustomLocationDone = 1,
    GoshuinCustomOpenSaved,
} GoshuinCustomEvent;

typedef enum {
    GoshuinDisplayStamp,
    GoshuinDisplayAbout,
    GoshuinDisplayError,
} GoshuinDisplayMode;

typedef enum {
    GoshuinReturnMain,
    GoshuinReturnSaved,
} GoshuinReturnTarget;

typedef enum {
    GoshuinMeasureIdle,
    GoshuinMeasureRunning,
    GoshuinMeasureCompleted,
    GoshuinMeasureInterrupted,
    GoshuinMeasureFailed,
} GoshuinMeasureState;

typedef struct {
    GoshuinDisplayMode mode;
    bool show_info;
    bool saved;
    GoshuinStampPayload stamp;
    char message[64];
} GoshuinDisplayModel;

typedef struct {
    GoshuinMeasureState state;
    uint8_t sample_count;
    int8_t current_rssi;
    int8_t samples[GOSHUIN_RF_SAMPLE_COUNT];
    char message[32];
} GoshuinMeasureModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    Submenu* saved_menu;
    TextInput* text_input;
    View* display_view;
    View* measure_view;
    Storage* storage;
    GoshuinViewId current_view;
    GoshuinReturnTarget display_return;
    GoshuinMeasurementSource pending_source;
    char location[GOSHUIN_LOCATION_CAPACITY];
    char saved_paths[GOSHUIN_MAX_SAVED_STAMPS][GOSHUIN_PATH_CAPACITY];
    char saved_labels[GOSHUIN_MAX_SAVED_STAMPS][GOSHUIN_LABEL_CAPACITY];
    size_t saved_count;
    size_t pending_saved_index;
    GoshuinStampPayload working_stamp;
    DateTime measurement_datetime;
    uint32_t measurement_frequency_hz;
    const SubGhzDevice* radio;
    bool devices_initialized;
    bool radio_begun;
    bool radio_receiving;
} RadioGoshuinchoApp;

static void switch_view(RadioGoshuinchoApp* app, GoshuinViewId id) {
    app->current_view = id;
    view_dispatcher_switch_to_view(app->view_dispatcher, id);
}

static bool has_suffix(const char* text, const char* suffix) {
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return (text_length >= suffix_length) && !strcmp(text + text_length - suffix_length, suffix);
}

static bool stamp_write(
    RadioGoshuinchoApp* app,
    const GoshuinStampPayload* stamp,
    char* output_path,
    size_t output_size) {
    GoshuinFileHeader header = {
        .magic = GOSHUIN_FILE_MAGIC,
        .format_version = GOSHUIN_FILE_VERSION_CURRENT,
        .header_size = sizeof(GoshuinFileHeader),
        .payload_size = sizeof(GoshuinStampPayload),
        .payload_crc32 = goshuin_crc32(stamp, sizeof(*stamp)),
    };
    char stem[72];
    snprintf(
        stem,
        sizeof(stem),
        "stamp_%04u%02u%02u_%02u%02u%02u_%08lx",
        stamp->year,
        stamp->month,
        stamp->day,
        stamp->hour,
        stamp->minute,
        stamp->second,
        (unsigned long)stamp->seed);
    bool available = false;
    for(uint32_t suffix = 0; suffix < 100U; ++suffix) {
        if(suffix == 0U) {
            snprintf(output_path, output_size, APP_DATA_PATH("%s.rgs"), stem);
        } else {
            snprintf(
                output_path,
                output_size,
                APP_DATA_PATH("%s_%02lu.rgs"),
                stem,
                (unsigned long)suffix);
        }
        if(!storage_file_exists(app->storage, output_path)) {
            available = true;
            break;
        }
    }
    if(!available) return false;

    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, output_path, FSAM_WRITE, FSOM_CREATE_NEW);
    if(success) success = storage_file_write(file, &header, sizeof(header)) == sizeof(header);
    if(success) success = storage_file_write(file, stamp, sizeof(*stamp)) == sizeof(*stamp);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static bool stamp_read(RadioGoshuinchoApp* app, const char* path, GoshuinStampPayload* stamp) {
    GoshuinFileHeader header;
    union {
        GoshuinStampPayloadV1 v1;
        GoshuinStampPayload v2;
    } payload;
    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) success = storage_file_read(file, &header, sizeof(header)) == sizeof(header);
    if(success) {
        success = !memcmp(header.magic, GOSHUIN_FILE_MAGIC, sizeof(header.magic)) &&
                  (header.header_size == sizeof(header)) &&
                  (((header.format_version == GOSHUIN_FILE_VERSION_V1) &&
                    (header.payload_size == sizeof(GoshuinStampPayloadV1))) ||
                   ((header.format_version == GOSHUIN_FILE_VERSION_CURRENT) &&
                    (header.payload_size == sizeof(GoshuinStampPayload))));
    }
    if(success) {
        success = storage_file_read(file, &payload, header.payload_size) == header.payload_size;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(success) success = header.payload_crc32 == goshuin_crc32(&payload, header.payload_size);
    if(success) {
        success = goshuin_stamp_decode_payload(
            header.format_version, &payload, header.payload_size, stamp);
    }
    return success;
}

static void display_stamp(
    RadioGoshuinchoApp* app,
    const GoshuinStampPayload* stamp,
    bool saved,
    GoshuinReturnTarget target) {
    app->display_return = target;
    with_view_model(
        app->display_view,
        GoshuinDisplayModel * model,
        {
            model->mode = GoshuinDisplayStamp;
            model->show_info = true;
            model->saved = saved;
            model->stamp = *stamp;
            model->message[0] = '\0';
        },
        true);
    switch_view(app, GoshuinViewDisplay);
}

static void display_message(
    RadioGoshuinchoApp* app,
    GoshuinDisplayMode mode,
    const char* message,
    GoshuinReturnTarget target) {
    app->display_return = target;
    with_view_model(
        app->display_view,
        GoshuinDisplayModel * model,
        {
            model->mode = mode;
            model->show_info = true;
            model->saved = false;
            strncpy(model->message, message, sizeof(model->message) - 1U);
            model->message[sizeof(model->message) - 1U] = '\0';
        },
        true);
    switch_view(app, GoshuinViewDisplay);
}

static void display_draw(Canvas* canvas, void* context) {
    const GoshuinDisplayModel* model = context;
    canvas_clear(canvas);
    if(model->mode == GoshuinDisplayAbout) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Radio Goshuincho");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 22, "Phase B - RX only");
        canvas_draw_str(canvas, 3, 32, "Real RF + Demo RSSI");
        canvas_draw_str(canvas, 3, 42, "No Sub-GHz TX capability");
        canvas_draw_str(canvas, 3, 52, "Version 0.2.1 / microSD");
        elements_button_left(canvas, "Back");
        return;
    }
    if(model->mode == GoshuinDisplayError) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 13, AlignCenter, AlignBottom, "Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, model->message);
        elements_button_left(canvas, "Back");
        return;
    }
    if(!model->show_info) {
        canvas_draw_xbm(canvas, 0, 0, GOSHUIN_SEAL_WIDTH, GOSHUIN_SEAL_HEIGHT, model->stamp.seal);
        return;
    }
    char line[48];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Radio Goshuincho");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Place: %s", model->stamp.location);
    canvas_draw_str(canvas, 2, 20, line);
    snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u",
        model->stamp.year,
        model->stamp.month,
        model->stamp.day,
        model->stamp.hour,
        model->stamp.minute,
        model->stamp.second);
    canvas_draw_str(canvas, 2, 31, line);
    snprintf(
        line,
        sizeof(line),
        "RSSI %d avg  %d..%d dBm",
        model->stamp.rssi_average,
        model->stamp.rssi_min,
        model->stamp.rssi_max);
    canvas_draw_str(canvas, 2, 42, line);
    if(model->stamp.measurement_source == GoshuinMeasurementSubGhzReceive) {
        const uint32_t frequency_hundredths = (model->stamp.frequency_hz + 5000UL) / 10000UL;
        snprintf(
            line,
            sizeof(line),
            "Real %lu.%02luMHz / %u",
            (unsigned long)(frequency_hundredths / 100UL),
            (unsigned long)(frequency_hundredths % 100UL),
            model->stamp.sample_count);
    } else {
        snprintf(line, sizeof(line), "Demo / %u samples", model->stamp.sample_count);
    }
    canvas_draw_str(canvas, 2, 52, line);
    elements_button_center(canvas, "Seal");
}

static bool display_input(InputEvent* event, void* context) {
    RadioGoshuinchoApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyBack) {
        switch_view(
            app, app->display_return == GoshuinReturnSaved ? GoshuinViewSaved : GoshuinViewMain);
        return true;
    }
    if((event->key == InputKeyLeft) || (event->key == InputKeyRight) ||
       (event->key == InputKeyOk)) {
        with_view_model(
            app->display_view,
            GoshuinDisplayModel * model,
            {
                if(model->mode == GoshuinDisplayStamp) model->show_info = !model->show_info;
            },
            true);
        return true;
    }
    return false;
}

static void radio_release(RadioGoshuinchoApp* app) {
    if(app->radio_receiving && app->radio) {
        subghz_devices_idle(app->radio);
        app->radio_receiving = false;
    }
    if(app->radio_begun && app->radio) {
        subghz_devices_sleep(app->radio);
        subghz_devices_end(app->radio);
        app->radio_begun = false;
    }
    app->radio = NULL;
    if(app->devices_initialized) {
        subghz_devices_deinit();
        app->devices_initialized = false;
    }
}

static void
    measure_set_state(RadioGoshuinchoApp* app, GoshuinMeasureState state, const char* message) {
    with_view_model(
        app->measure_view,
        GoshuinMeasureModel * model,
        {
            model->state = state;
            if(message) {
                strncpy(model->message, message, sizeof(model->message) - 1U);
                model->message[sizeof(model->message) - 1U] = '\0';
            }
        },
        true);
}

static bool radio_start(RadioGoshuinchoApp* app) {
    subghz_devices_init();
    app->devices_initialized = true;
    app->radio = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->radio) return false;
    /* The internal CC1101 has no begin hook and therefore returns false here.
       The official Sub-GHz app also calls begin unconditionally and ignores that value. */
    subghz_devices_begin(app->radio);
    app->radio_begun = true;
    if(!subghz_devices_is_frequency_valid(app->radio, GOSHUIN_RF_FREQUENCY_HZ)) return false;
    subghz_devices_reset(app->radio);
    subghz_devices_idle(app->radio);
    subghz_devices_load_preset(app->radio, FuriHalSubGhzPresetOok650Async, NULL);
    app->measurement_frequency_hz =
        subghz_devices_set_frequency(app->radio, GOSHUIN_RF_FREQUENCY_HZ);
    if(!subghz_devices_is_frequency_valid(app->radio, app->measurement_frequency_hz)) {
        return false;
    }
    subghz_devices_flush_rx(app->radio);
    subghz_devices_set_rx(app->radio);
    app->radio_receiving = true;
    return true;
}

static void measure_start(RadioGoshuinchoApp* app) {
    furi_hal_rtc_get_datetime(&app->measurement_datetime);
    with_view_model(
        app->measure_view,
        GoshuinMeasureModel * model,
        {
            memset(model, 0, sizeof(*model));
            model->state = GoshuinMeasureRunning;
            model->current_rssi = -127;
            strcpy(model->message, "Receiving only");
        },
        true);
    switch_view(app, GoshuinViewMeasure);
    if(!radio_start(app)) {
        radio_release(app);
        measure_set_state(app, GoshuinMeasureFailed, "Radio unavailable");
    }
}

static void measure_finish_and_save(RadioGoshuinchoApp* app) {
    int8_t samples[GOSHUIN_RF_SAMPLE_COUNT];
    uint8_t count = 0;
    with_view_model(
        app->measure_view,
        GoshuinMeasureModel * model,
        {
            count = model->sample_count;
            memcpy(samples, model->samples, count);
        },
        false);
    if(!goshuin_stamp_create_measured(
           &app->working_stamp,
           app->location,
           &app->measurement_datetime,
           app->measurement_frequency_hz,
           GOSHUIN_RF_INTERVAL_MS,
           samples,
           count)) {
        display_message(app, GoshuinDisplayError, "Measurement invalid", GoshuinReturnMain);
        return;
    }
    char path[GOSHUIN_PATH_CAPACITY];
    if(stamp_write(app, &app->working_stamp, path, sizeof(path))) {
        display_stamp(app, &app->working_stamp, true, GoshuinReturnMain);
    } else {
        display_message(app, GoshuinDisplayError, "microSD save failed", GoshuinReturnMain);
    }
}

static void measure_tick(void* context) {
    RadioGoshuinchoApp* app = context;
    if((app->current_view != GoshuinViewMeasure) || !app->radio_receiving) return;
    float raw = subghz_devices_get_rssi(app->radio);
    if(raw < -127.0f) raw = -127.0f;
    if(raw > 0.0f) raw = 0.0f;
    const int8_t rssi = (int8_t)raw;
    bool complete = false;
    with_view_model(
        app->measure_view,
        GoshuinMeasureModel * model,
        {
            if(model->sample_count < GOSHUIN_RF_SAMPLE_COUNT) {
                model->samples[model->sample_count++] = rssi;
                model->current_rssi = rssi;
            }
            complete = model->sample_count >= GOSHUIN_RF_SAMPLE_COUNT;
        },
        true);
    if(complete) {
        radio_release(app);
        measure_set_state(app, GoshuinMeasureCompleted, "Measurement complete");
    }
}

static int32_t graph_y(int8_t rssi) {
    int32_t y = 52 - (((int32_t)rssi + 110) * 27 / 80);
    if(y < 25) y = 25;
    if(y > 52) y = 52;
    return y;
}

static void measure_draw(Canvas* canvas, void* context) {
    const GoshuinMeasureModel* model = context;
    canvas_clear(canvas);
    char line[40];
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, "Real RF 433.92 MHz");
    canvas_set_font(canvas, FontSecondary);
    if(model->state == GoshuinMeasureCompleted) {
        snprintf(line, sizeof(line), "Complete: %u samples", model->sample_count);
    } else {
        snprintf(
            line,
            sizeof(line),
            "RSSI %d dBm  %u.%us / 10s",
            model->current_rssi,
            model->sample_count / 5U,
            (model->sample_count % 5U) * 2U);
    }
    canvas_draw_str(canvas, 2, 20, line);
    canvas_draw_frame(canvas, 2, 24, 124, 30);
    if(model->sample_count > 0U) {
        int32_t px = 3;
        int32_t py = graph_y(model->samples[0]);
        for(size_t i = 1; i < model->sample_count; ++i) {
            const int32_t x = 3 + (int32_t)(i * 121U / (GOSHUIN_RF_SAMPLE_COUNT - 1U));
            const int32_t y = graph_y(model->samples[i]);
            canvas_draw_line(canvas, px, py, x, y);
            px = x;
            py = y;
        }
    }
    const uint8_t progress = (uint8_t)(model->sample_count * 124U / GOSHUIN_RF_SAMPLE_COUNT);
    canvas_draw_box(canvas, 2, 55, progress, 2);
    if(model->state == GoshuinMeasureRunning) {
        elements_button_left(canvas, "Cancel");
    } else if(model->state == GoshuinMeasureCompleted) {
        elements_button_center(canvas, "Create");
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 10, 29, 108, 18);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, model->message);
        elements_button_left(canvas, "Back");
    }
}

static bool measure_input(InputEvent* event, void* context) {
    RadioGoshuinchoApp* app = context;
    if(event->type != InputTypeShort) return false;
    GoshuinMeasureState state;
    with_view_model(
        app->measure_view, GoshuinMeasureModel * model, { state = model->state; }, false);
    if(event->key == InputKeyBack) {
        if(state == GoshuinMeasureRunning) {
            radio_release(app);
            measure_set_state(app, GoshuinMeasureInterrupted, "Measurement cancelled");
        } else {
            switch_view(app, GoshuinViewMain);
        }
        return true;
    }
    if((event->key == InputKeyOk) && (state == GoshuinMeasureCompleted)) {
        measure_finish_and_save(app);
        return true;
    }
    return false;
}

static bool location_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(!text[0]) {
        furi_string_set(error, "Enter a place name");
        return false;
    }
    for(const char* cursor = text; *cursor; ++cursor) {
        const char c = *cursor;
        if(!(((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) ||
             ((c >= '0') && (c <= '9')) || (c == ' ') || (c == '-'))) {
            furi_string_set(error, "Use A-Z, 0-9, space or -");
            return false;
        }
    }
    return true;
}

static void location_done(void* context) {
    RadioGoshuinchoApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GoshuinCustomLocationDone);
}

static void create_demo(RadioGoshuinchoApp* app) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    goshuin_stamp_create_demo(&app->working_stamp, app->location, &datetime);
    char path[GOSHUIN_PATH_CAPACITY];
    if(stamp_write(app, &app->working_stamp, path, sizeof(path))) {
        display_stamp(app, &app->working_stamp, true, GoshuinReturnMain);
    } else {
        display_message(app, GoshuinDisplayError, "microSD save failed", GoshuinReturnMain);
    }
}

static void saved_item_callback(void* context, uint32_t index) {
    RadioGoshuinchoApp* app = context;
    if(index >= app->saved_count) return;
    app->pending_saved_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, GoshuinCustomOpenSaved);
}

static void open_saved(RadioGoshuinchoApp* app) {
    if((app->pending_saved_index < app->saved_count) &&
       stamp_read(app, app->saved_paths[app->pending_saved_index], &app->working_stamp)) {
        display_stamp(app, &app->working_stamp, true, GoshuinReturnSaved);
    } else {
        display_message(app, GoshuinDisplayError, "Invalid stamp file", GoshuinReturnSaved);
    }
}

static bool custom_event(void* context, uint32_t event) {
    RadioGoshuinchoApp* app = context;
    if(event == GoshuinCustomLocationDone) {
        if(app->pending_source == GoshuinMeasurementSubGhzReceive)
            measure_start(app);
        else
            create_demo(app);
        return true;
    }
    if(event == GoshuinCustomOpenSaved) {
        open_saved(app);
        return true;
    }
    return false;
}

static void load_saved_menu(RadioGoshuinchoApp* app) {
    submenu_reset(app->saved_menu);
    submenu_set_header(app->saved_menu, "Saved Stamps");
    app->saved_count = 0;
    File* directory = storage_file_alloc(app->storage);
    const bool opened = storage_dir_open(directory, STORAGE_APP_DATA_PATH_PREFIX);
    if(opened) {
        FileInfo info;
        char name[GOSHUIN_FILENAME_CAPACITY];
        while((app->saved_count < GOSHUIN_MAX_SAVED_STAMPS) &&
              storage_dir_read(directory, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) || strncmp(name, "stamp_", 6U) || !has_suffix(name, ".rgs"))
                continue;
            snprintf(
                app->saved_paths[app->saved_count],
                GOSHUIN_PATH_CAPACITY,
                APP_DATA_PATH("%s"),
                name);
            ++app->saved_count;
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    for(size_t i = 0; i < app->saved_count; ++i) {
        for(size_t j = i + 1U; j < app->saved_count; ++j) {
            if(strcmp(app->saved_paths[i], app->saved_paths[j]) < 0) {
                char swap[GOSHUIN_PATH_CAPACITY];
                memcpy(swap, app->saved_paths[i], sizeof(swap));
                memcpy(app->saved_paths[i], app->saved_paths[j], sizeof(swap));
                memcpy(app->saved_paths[j], swap, sizeof(swap));
            }
        }
    }
    size_t valid = 0;
    for(size_t i = 0; i < app->saved_count; ++i) {
        if(!stamp_read(app, app->saved_paths[i], &app->working_stamp)) continue;
        if(valid != i)
            memcpy(app->saved_paths[valid], app->saved_paths[i], sizeof(app->saved_paths[valid]));
        snprintf(
            app->saved_labels[valid],
            GOSHUIN_LABEL_CAPACITY,
            "%c %04u-%02u-%02u %02u:%02u %s",
            app->working_stamp.measurement_source == GoshuinMeasurementSubGhzReceive ? 'R' : 'D',
            app->working_stamp.year,
            app->working_stamp.month,
            app->working_stamp.day,
            app->working_stamp.hour,
            app->working_stamp.minute,
            app->working_stamp.location);
        submenu_add_item(
            app->saved_menu, app->saved_labels[valid], valid, saved_item_callback, app);
        ++valid;
    }
    app->saved_count = valid;
    if(!opened)
        submenu_add_item(app->saved_menu, "microSD read error", UINT32_MAX, NULL, NULL);
    else if(!app->saved_count)
        submenu_add_item(app->saved_menu, "No saved stamps", UINT32_MAX, NULL, NULL);
}

static void begin_location(RadioGoshuinchoApp* app, GoshuinMeasurementSource source) {
    app->pending_source = source;
    app->location[0] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input,
        source == GoshuinMeasurementSubGhzReceive ? "Real RF place name" : "Demo place name");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, location_validator, app);
    text_input_set_result_callback(
        app->text_input, location_done, app, app->location, sizeof(app->location), true);
    switch_view(app, GoshuinViewTextInput);
}

static void main_menu_callback(void* context, uint32_t index) {
    RadioGoshuinchoApp* app = context;
    if(index == GoshuinMainReal)
        begin_location(app, GoshuinMeasurementSubGhzReceive);
    else if(index == GoshuinMainDemo)
        begin_location(app, GoshuinMeasurementDummy);
    else if(index == GoshuinMainSaved) {
        load_saved_menu(app);
        switch_view(app, GoshuinViewSaved);
    } else if(index == GoshuinMainAbout) {
        display_message(app, GoshuinDisplayAbout, "Radio Goshuincho Phase B", GoshuinReturnMain);
    }
}

static bool navigation(void* context) {
    RadioGoshuinchoApp* app = context;
    if(app->current_view == GoshuinViewMain)
        view_dispatcher_stop(app->view_dispatcher);
    else
        switch_view(app, GoshuinViewMain);
    return true;
}

static RadioGoshuinchoApp* app_alloc(void) {
    RadioGoshuinchoApp* app = malloc(sizeof(*app));
    furi_check(app);
    memset(app, 0, sizeof(*app));
    Gui* gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->view_dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->saved_menu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->display_view = view_alloc();
    app->measure_view = view_alloc();
    view_allocate_model(app->display_view, ViewModelTypeLocking, sizeof(GoshuinDisplayModel));
    view_set_context(app->display_view, app);
    view_set_draw_callback(app->display_view, display_draw);
    view_set_input_callback(app->display_view, display_input);
    view_allocate_model(app->measure_view, ViewModelTypeLocking, sizeof(GoshuinMeasureModel));
    view_set_context(app->measure_view, app);
    view_set_draw_callback(app->measure_view, measure_draw);
    view_set_input_callback(app->measure_view, measure_input);
    submenu_set_header(app->main_menu, "Radio Goshuincho");
    submenu_add_item(app->main_menu, "Real RF", GoshuinMainReal, main_menu_callback, app);
    submenu_add_item(app->main_menu, "Demo", GoshuinMainDemo, main_menu_callback, app);
    submenu_add_item(app->main_menu, "Saved Stamps", GoshuinMainSaved, main_menu_callback, app);
    submenu_add_item(app->main_menu, "About", GoshuinMainAbout, main_menu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewMain, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewSaved, submenu_get_view(app->saved_menu));
    view_dispatcher_add_view(app->view_dispatcher, GoshuinViewDisplay, app->display_view);
    view_dispatcher_add_view(app->view_dispatcher, GoshuinViewMeasure, app->measure_view);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, measure_tick, GOSHUIN_RF_INTERVAL_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void app_free(RadioGoshuinchoApp* app) {
    radio_release(app);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewSaved);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewDisplay);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewMeasure);
    view_free(app->measure_view);
    view_free(app->display_view);
    text_input_free(app->text_input);
    submenu_free(app->saved_menu);
    submenu_free(app->main_menu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t radio_goshuincho_app(void* context) {
    UNUSED(context);
    RadioGoshuinchoApp* app = app_alloc();
    switch_view(app, GoshuinViewMain);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
