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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOSHUIN_FILE_MAGIC        "RGSF"
#define GOSHUIN_FILE_VERSION      1U
#define GOSHUIN_MAX_SAVED_STAMPS  32U
#define GOSHUIN_PATH_CAPACITY     96U
#define GOSHUIN_FILENAME_CAPACITY 80U
#define GOSHUIN_LABEL_CAPACITY    48U

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
} GoshuinViewId;

typedef enum {
    GoshuinMainNew,
    GoshuinMainSaved,
    GoshuinMainAbout,
} GoshuinMainItem;

typedef enum {
    GoshuinCustomCreateStamp = 1,
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

typedef struct {
    GoshuinDisplayMode mode;
    bool show_info;
    bool saved;
    GoshuinStampPayload stamp;
    char message[64];
} GoshuinDisplayModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    Submenu* saved_menu;
    TextInput* text_input;
    View* display_view;
    Storage* storage;
    GoshuinViewId current_view;
    GoshuinReturnTarget display_return;
    char location[GOSHUIN_LOCATION_CAPACITY];
    char saved_paths[GOSHUIN_MAX_SAVED_STAMPS][GOSHUIN_PATH_CAPACITY];
    char saved_labels[GOSHUIN_MAX_SAVED_STAMPS][GOSHUIN_LABEL_CAPACITY];
    size_t saved_count;
    size_t pending_saved_index;
    GoshuinStampPayload working_stamp;
} RadioGoshuinchoApp;

static void goshuin_switch_view(RadioGoshuinchoApp* app, GoshuinViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

static bool goshuin_has_suffix(const char* text, const char* suffix) {
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return (text_length >= suffix_length) &&
           (strcmp(text + text_length - suffix_length, suffix) == 0);
}

static bool goshuin_stamp_write(
    RadioGoshuinchoApp* app,
    const GoshuinStampPayload* stamp,
    char* output_path,
    size_t output_path_size) {
    GoshuinFileHeader header = {
        .magic = GOSHUIN_FILE_MAGIC,
        .format_version = GOSHUIN_FILE_VERSION,
        .header_size = sizeof(GoshuinFileHeader),
        .payload_size = sizeof(GoshuinStampPayload),
        .payload_crc32 = goshuin_crc32(stamp, sizeof(GoshuinStampPayload)),
    };
    snprintf(
        output_path,
        output_path_size,
        APP_DATA_PATH("stamp_%04u%02u%02u_%02u%02u%02u_%08lx.rgs"),
        stamp->year,
        stamp->month,
        stamp->day,
        stamp->hour,
        stamp->minute,
        stamp->second,
        (unsigned long)stamp->seed);

    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, output_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) success = storage_file_write(file, &header, sizeof(header)) == sizeof(header);
    if(success) success = storage_file_write(file, stamp, sizeof(*stamp)) == sizeof(*stamp);
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static bool
    goshuin_stamp_read(RadioGoshuinchoApp* app, const char* path, GoshuinStampPayload* stamp) {
    GoshuinFileHeader header;
    File* file = storage_file_alloc(app->storage);
    bool success = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) success = storage_file_read(file, &header, sizeof(header)) == sizeof(header);
    if(success) {
        success = (memcmp(header.magic, GOSHUIN_FILE_MAGIC, sizeof(header.magic)) == 0) &&
                  (header.format_version == GOSHUIN_FILE_VERSION) &&
                  (header.header_size == sizeof(GoshuinFileHeader)) &&
                  (header.payload_size == sizeof(GoshuinStampPayload));
    }
    if(success) success = storage_file_read(file, stamp, sizeof(*stamp)) == sizeof(*stamp);
    storage_file_close(file);
    storage_file_free(file);
    if(success) success = header.payload_crc32 == goshuin_crc32(stamp, sizeof(*stamp));
    if(success) {
        stamp->location[GOSHUIN_LOCATION_CAPACITY - 1U] = '\0';
        success = stamp->sample_count <= GOSHUIN_RSSI_SAMPLE_COUNT;
    }
    return success;
}

static void goshuin_display_set_stamp(
    RadioGoshuinchoApp* app,
    const GoshuinStampPayload* stamp,
    bool saved,
    GoshuinReturnTarget return_target) {
    app->display_return = return_target;
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
    goshuin_switch_view(app, GoshuinViewDisplay);
}

static void goshuin_display_set_message(
    RadioGoshuinchoApp* app,
    GoshuinDisplayMode mode,
    const char* message,
    GoshuinReturnTarget return_target) {
    app->display_return = return_target;
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
    goshuin_switch_view(app, GoshuinViewDisplay);
}

static void goshuin_display_draw(Canvas* canvas, void* context) {
    const GoshuinDisplayModel* model = context;
    canvas_clear(canvas);

    if(model->mode == GoshuinDisplayAbout) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Radio Goshuincho");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 22, "Phase A - receive only");
        canvas_draw_str(canvas, 3, 32, "Dummy RSSI measurements");
        canvas_draw_str(canvas, 3, 42, "No Sub-GHz TX capability");
        canvas_draw_str(canvas, 3, 52, "Version 0.1.4 / microSD");
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
        "RSSI avg %d  min %d  max %d",
        model->stamp.rssi_average,
        model->stamp.rssi_min,
        model->stamp.rssi_max);
    canvas_draw_str(canvas, 2, 42, line);
    canvas_draw_str(canvas, 2, 52, model->saved ? "Dummy RSSI / Saved" : "Dummy RSSI");
    elements_button_center(canvas, "Seal");
}

static bool goshuin_display_input(InputEvent* event, void* context) {
    RadioGoshuinchoApp* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyBack) {
        goshuin_switch_view(
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

static bool goshuin_location_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(text[0] == '\0') {
        furi_string_set(error, "Enter a place name");
        return false;
    }
    for(const char* cursor = text; *cursor; ++cursor) {
        const char character = *cursor;
        const bool valid = ((character >= 'A') && (character <= 'Z')) ||
                           ((character >= 'a') && (character <= 'z')) ||
                           ((character >= '0') && (character <= '9')) || (character == ' ') ||
                           (character == '-');
        if(!valid) {
            furi_string_set(error, "Use A-Z, 0-9, space or -");
            return false;
        }
    }
    return true;
}

static void goshuin_location_done(void* context) {
    RadioGoshuinchoApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GoshuinCustomCreateStamp);
}

static void goshuin_create_stamp(RadioGoshuinchoApp* app) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    goshuin_stamp_create(&app->working_stamp, app->location, &datetime);
    char saved_path[GOSHUIN_PATH_CAPACITY];
    if(goshuin_stamp_write(app, &app->working_stamp, saved_path, sizeof(saved_path))) {
        goshuin_display_set_stamp(app, &app->working_stamp, true, GoshuinReturnMain);
    } else {
        goshuin_display_set_message(
            app, GoshuinDisplayError, "microSD save failed", GoshuinReturnMain);
    }
}

static void goshuin_saved_item_callback(void* context, uint32_t index) {
    RadioGoshuinchoApp* app = context;
    if(index >= app->saved_count) return;
    app->pending_saved_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, GoshuinCustomOpenSaved);
}

static void goshuin_open_saved_stamp(RadioGoshuinchoApp* app) {
    if(app->pending_saved_index >= app->saved_count) return;
    if(goshuin_stamp_read(app, app->saved_paths[app->pending_saved_index], &app->working_stamp)) {
        goshuin_display_set_stamp(app, &app->working_stamp, true, GoshuinReturnSaved);
    } else {
        goshuin_display_set_message(
            app, GoshuinDisplayError, "Invalid stamp file", GoshuinReturnSaved);
    }
}

static bool goshuin_custom_event_callback(void* context, uint32_t event) {
    RadioGoshuinchoApp* app = context;
    if(event == GoshuinCustomCreateStamp) {
        goshuin_create_stamp(app);
        return true;
    }
    if(event == GoshuinCustomOpenSaved) {
        goshuin_open_saved_stamp(app);
        return true;
    }
    return false;
}

static void goshuin_load_saved_menu(RadioGoshuinchoApp* app) {
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
            if(file_info_is_dir(&info) || (strncmp(name, "stamp_", 6U) != 0) ||
               !goshuin_has_suffix(name, ".rgs")) {
                continue;
            }
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

    size_t valid_count = 0;
    for(size_t i = 0; i < app->saved_count; ++i) {
        if(!goshuin_stamp_read(app, app->saved_paths[i], &app->working_stamp)) continue;
        if(valid_count != i) {
            memcpy(
                app->saved_paths[valid_count],
                app->saved_paths[i],
                sizeof(app->saved_paths[valid_count]));
        }
        snprintf(
            app->saved_labels[valid_count],
            GOSHUIN_LABEL_CAPACITY,
            "%04u-%02u-%02u %02u:%02u %s",
            app->working_stamp.year,
            app->working_stamp.month,
            app->working_stamp.day,
            app->working_stamp.hour,
            app->working_stamp.minute,
            app->working_stamp.location);
        submenu_add_item(
            app->saved_menu,
            app->saved_labels[valid_count],
            valid_count,
            goshuin_saved_item_callback,
            app);
        ++valid_count;
    }
    app->saved_count = valid_count;

    if(!opened) {
        submenu_add_item(app->saved_menu, "microSD read error", UINT32_MAX, NULL, NULL);
    } else if(app->saved_count == 0U) {
        submenu_add_item(app->saved_menu, "No saved stamps", UINT32_MAX, NULL, NULL);
    }
}

static void goshuin_main_menu_callback(void* context, uint32_t index) {
    RadioGoshuinchoApp* app = context;
    if(index == GoshuinMainNew) {
        app->location[0] = '\0';
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Place name (A-Z / 0-9)");
        text_input_set_minimum_length(app->text_input, 1U);
        text_input_set_validator(app->text_input, goshuin_location_validator, app);
        text_input_set_result_callback(
            app->text_input,
            goshuin_location_done,
            app,
            app->location,
            sizeof(app->location),
            true);
        goshuin_switch_view(app, GoshuinViewTextInput);
    } else if(index == GoshuinMainSaved) {
        goshuin_load_saved_menu(app);
        goshuin_switch_view(app, GoshuinViewSaved);
    } else if(index == GoshuinMainAbout) {
        goshuin_display_set_message(
            app, GoshuinDisplayAbout, "Radio Goshuincho Phase A", GoshuinReturnMain);
    }
}

static bool goshuin_navigation_callback(void* context) {
    RadioGoshuinchoApp* app = context;
    if(app->current_view == GoshuinViewMain) {
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        goshuin_switch_view(app, GoshuinViewMain);
    }
    return true;
}

static RadioGoshuinchoApp* goshuin_app_alloc(void) {
    RadioGoshuinchoApp* app = malloc(sizeof(RadioGoshuinchoApp));
    furi_check(app);
    memset(app, 0, sizeof(*app));

    Gui* gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->view_dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->saved_menu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->display_view = view_alloc();

    view_allocate_model(app->display_view, ViewModelTypeLocking, sizeof(GoshuinDisplayModel));
    view_set_context(app->display_view, app);
    view_set_draw_callback(app->display_view, goshuin_display_draw);
    view_set_input_callback(app->display_view, goshuin_display_input);

    submenu_set_header(app->main_menu, "Radio Goshuincho");
    submenu_add_item(
        app->main_menu, "Create Stamp", GoshuinMainNew, goshuin_main_menu_callback, app);
    submenu_add_item(
        app->main_menu, "Saved Stamps", GoshuinMainSaved, goshuin_main_menu_callback, app);
    submenu_add_item(app->main_menu, "About", GoshuinMainAbout, goshuin_main_menu_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewMain, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, GoshuinViewSaved, submenu_get_view(app->saved_menu));
    view_dispatcher_add_view(app->view_dispatcher, GoshuinViewDisplay, app->display_view);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, goshuin_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, goshuin_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void goshuin_app_free(RadioGoshuinchoApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewSaved);
    view_dispatcher_remove_view(app->view_dispatcher, GoshuinViewDisplay);
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
    RadioGoshuinchoApp* app = goshuin_app_alloc();
    goshuin_switch_view(app, GoshuinViewMain);
    view_dispatcher_run(app->view_dispatcher);
    goshuin_app_free(app);
    return 0;
}
