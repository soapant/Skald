#ifndef FLAPI_H
#define FLAPI_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/types.h"
#include "config.h"
#include "core/platform.h"
#include "core/context.h"
#include "core/modal.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "helpers/helpers_hotkey.h"
#include "core/flapi_input.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"

#define WHITELIST 1
#define BLACKLIST 0

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

typedef struct {
    FileItem* items;
    size_t count;
    size_t capacity;
} ListOfFiles;



// =============================================================================
// CLIPBOARD
// =============================================================================

bool fl_copy_to_clipboard(const char* text);
bool fl_paste_from_clipboard();
bool fl_paste(char* content);
char* fl_get_last_clipboard_entry();

typedef enum {
    FL_CLIPBOARD_TEXT,
    FL_CLIPBOARD_IMAGE,
} FlClipboardType;

typedef struct {
    FlClipboardType type;
    uint64_t timestamp;
    char* text;
    char* image_path;
    uint32_t image_width;
    uint32_t image_height;
} FlClipboardEntry;

typedef struct {
    FlClipboardEntry* entries;
    size_t count;
    size_t capacity;
} FlClipboardHistory;

size_t fl_clipboard_history_count(void);
const FlClipboardEntry* fl_clipboard_history_get(size_t index);
const FlClipboardEntry* fl_clipboard_latest(void);
void fl_clipboard_clear_history(void);

// =============================================================================
// FILES / FILESYSTEM
// =============================================================================

char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path);
bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore);
typedef struct {
    char** items;
    int count;
} FlStringList;
FlStringList fl_load_file_list(const char* filepath);
char* fl_read_file_to_string(const char* path, size_t* out_len);
bool fl_write_string_to_file(const char* path, const char* content);
bool fl_file_exists(const char* path);
bool fl_append_unique_line(const char* path, const char* line);
bool fl_remove_line(const char* path, const char* line);
bool fl_remove_line_contains(const char* path, const char* substr);
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay);
char* fl_get_icon_path(const char* app_path_path);
void fl_free_file_list(FlStringList* list);
bool fl_free_list(const char* list[], int count);
bool fl_free_window_list(Fl_WindowList *list);
bool fl_append_to_file(const char* filepath, const char* line);
bool fl_write_to_file(const char* filepath, const char* lines[], int count);

// =============================================================================
// LAUNCH / PROCESS
// =============================================================================

bool fl_launch_with_args(const char* path, const char* args);
bool fl_launch_with_args_as_admin(const char* path, const char* args);
bool fl_launch_terminal_at_path(char* path,char* args);
bool fl_launch_file_manager_at_path(char* path,char* args);
char* fl_run_terminal_command(char* command);
char* fl_run_terminal_command_blocking(char* command);
char* fl_run_terminal_command_and_open(char* command);
int fl_collect_running_processes(char process_list[][128], int max_count);
bool fl_terminate_process_by_name(const char* exe_name);
int fl_get_icon_size(const char* icon_path);

// =============================================================================
// FEATURES
// =============================================================================

void fl_register_feature(const char* name, void (*cb)(void));
void fl_register_feature_with_icon(const char* name, void (*cb)(void), const char* icon_path);
int fl_get_feature_list(const char** out_names, void (**out_cbs)(void), int max);
int fl_get_feature_list_with_icons(const char** out_names, void (**out_cbs)(void), const char*** out_icons, int max);

// =============================================================================
// HOTKEYS
// =============================================================================

void fl_init_all_hotkeys(void);
void fl_send_hotkey(const char* hotkey_string);

// =============================================================================
// BROWSER / ZOXIDE / SYSTEM
// =============================================================================

bool fl_launch_browser(char* url);
bool fl_query_browser(const char* query);
bool fl_launch_browser_in_a_new_window(char* url);
char* fl_get_zoxide_match(char* query);
bool fl_focus_window_by_title(char* title);

// =============================================================================
// ADB
// =============================================================================

char* fl_adb_tap(int x,int y);
char* fl_adb_type_text(char* text);
char* fl_adb_get_devices();
char* fl_adb_install(char* app_path);
char* fl_adb_uninstall(char* app_name);
char* fl_adb_force_close(char* app_name);
char* fl_adb_clear_app_data(char* app_name);
char* fl_adb_get_file(char* filepath,char* output_file_path);
char* fl_adb_put_file(char* filepath,char* output_file_path);
char* fl_adb_reboot();

// =============================================================================
// STRING UTILITIES (all returned char* are heap-allocated, caller must free())
// =============================================================================

char* fl_string_cut_at(const char* str, char delim);
char* fl_string_cut_before(const char* str, char delim);
char* fl_string_cut_after(const char* str, char delim);
char* fl_string_between(const char* str, char open, char close);
char* fl_string_inside(const char* str, char delim);
char* fl_string_trim(const char* str);
char* fl_string_to_lower(const char* str);
char* fl_string_to_upper(const char* str);
bool fl_string_starts_with(const char* str, const char* prefix);
bool fl_string_ends_with(const char* str, const char* suffix);
char* fl_string_replace(const char* str, const char* from, const char* to);
char* fl_string_slice(const char* str, int start, int end);

// =============================================================================
// POPUPS / UI
// =============================================================================

typedef struct {
    const char* initial_text;
    const char* title;
    int x;
    int y;
    int w;
    int h;
    const char* align;
    ListBoxInfo* lb;
    int* out_index;
} FlEditPopupSpec;

char* fl_edit_text_popup_impl(const FlEditPopupSpec* spec);
#define fl_edit_text_popup(...) fl_edit_text_popup_impl(&(FlEditPopupSpec){__VA_ARGS__})
bool fl_replace_line_in_file(const char* filepath, int line_index, const char* new_text);

char* fl_popup_impl(const FlPopupSpec *spec);
#define fl_popup(...) fl_popup_impl(&(FlPopupSpec){__VA_ARGS__})

// =============================================================================
// RAW WINDOW ACCESS (hardcore)
// =============================================================================
// Hardcore dev gets the real handle and does whatever. Flint just hosts popup.

#if !defined(_WIN32)
void* fl_get_display(void);
void* fl_get_window(void);
void* fl_get_xftdraw(void);
void* fl_get_visual(void);
unsigned long fl_get_colormap(void);
#else
void* fl_get_hdc(void);
void* fl_get_hwnd(void);
#endif
int fl_get_popup_width(void);
int fl_get_popup_height(void);

// =============================================================================
// ANCHOR PRESETS
// =============================================================================
// Used by anchored placement. Raw still exists for pixel control.

typedef enum {
    FL_TOP_LEFT,
    FL_TOP_CENTER,
    FL_TOP_RIGHT,
    FL_CENTER_LEFT,
    FL_CENTER,
    FL_CENTER_RIGHT,
    FL_BOTTOM_LEFT,
    FL_BOTTOM_CENTER,
    FL_BOTTOM_RIGHT,
    FL_FILL_WIDTH,
    FL_FILL_HEIGHT
} FlAnchor;

// =============================================================================
// RAW PLACEMENT (pixel-perfect)
// =============================================================================

typedef struct {
    const char *text;
    int x;
    int y;
    unsigned long color;
    FontHandle font;
    int fontsize;
    const char *align;
    int pad;
} FlTextSpec;

typedef struct {
    int x;
    int y;
} FlCharPos;

int fl_draw_text_wrapped_ex(int x, int y, int max_width, int line_height, const char *text, int fontsize, unsigned long color, FlCharPos *out_positions, int max_positions);

typedef struct {
    int x;
    int y;
    int max_width;
    int line_height;
    const char *text;
    int fontsize;
    FlCharPos *out_positions;
} FlMeasureSpec;

int fl_measure_text_wrapped_impl(const FlMeasureSpec *spec);
#define fl_measure_text_wrapped(...) fl_measure_text_wrapped_impl(&(FlMeasureSpec){__VA_ARGS__})

typedef struct {
    const char *buffer;
    int x; int y; int w; int h;
    unsigned long color;
    FontHandle font;
} FlEditSpec;

typedef struct {
    ListBoxInfo *lb;
    const char **items;
    int count;
    int x; int y; int w; int h;
    int item_height;
} FlListSpec;

typedef struct {
    int idx;
    bool selected;
    int x; int y; int w; int h;
    const char *text;
} FlListItemCtx;

typedef struct {
    const char *path;
    int x; int y; int w; int h;
} FlImageSpec;

bool fl_draw_text_impl(const FlTextSpec *spec);
bool fl_place_editbox_impl(const FlEditSpec *spec);
bool fl_place_listbox_impl(const FlListSpec *spec);
bool fl_place_image_impl(const FlImageSpec *spec);

#define fl_draw_text(...) fl_draw_text_impl(&(FlTextSpec){__VA_ARGS__})
#define fl_place_editbox(...) fl_place_editbox_impl(&(FlEditSpec){__VA_ARGS__})
#define fl_place_listbox(...) fl_place_listbox_impl(&(FlListSpec){__VA_ARGS__})
#define fl_place_image(...) fl_place_image_impl(&(FlImageSpec){__VA_ARGS__})

bool fl_listbox_begin(const FlListSpec *spec);
bool fl_listbox_next(FlListItemCtx *out);
void fl_listbox_end(void);

// =============================================================================
// ANCHORED PLACEMENT (preset positions)
// =============================================================================

bool fl_draw_text_anchored(FlAnchor anchor, int offset_x, int offset_y, const char *text);
bool fl_place_editbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *buffer);
bool fl_place_listbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char **items, int count);
bool fl_place_image_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *path);

// =============================================================================
// AUTO LAYOUT (column / row math)
// =============================================================================

void fl_layout_begin_column_anchored(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_column(int start_x, int start_y, int gap);
void fl_layout_end_column(void);
void fl_layout_begin_row_anchored(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_row(int start_x, int start_y, int gap);
void fl_layout_end_row(void);
bool fl_draw_text_auto(const char *text);
bool fl_place_editbox_auto(int w, int h, const char *buffer);
bool fl_place_listbox_auto(int w, int h, const char **items, int count);
bool fl_place_image_auto(int w, int h, const char *path);

// =============================================================================
// PRE-RENDER HOOK (global overridable)
// =============================================================================

bool Platform_PreRender(void);
bool fl_prerender(void);
#if !defined(_WIN32)
void Platform_SetPreRenderState(PromptState *state);
#else
void Platform_SetPreRenderStateWin32(const char *text, void *rect);
#endif

// =============================================================================
// IMAGES / ICONS
// =============================================================================

void* fl_load_image(const char *path);
void fl_free_image(void *image);
bool fl_draw_image(void *image, int x, int y, int w, int h);

// =============================================================================
// BASIC UI DRAWING (stubs for now, platform does logic later)
// =============================================================================

typedef struct {
    int x; int y; int w; int h;
    unsigned long color;
} FlRectSpec;

bool fl_draw_rect_impl(const FlRectSpec *spec);
bool fl_draw_line(int x1, int y1, int x2, int y2, unsigned long color);
bool fl_draw_circle(int x, int y, int radius, unsigned long color);
bool fl_draw_rounded_rect(int x, int y, int w, int h, int radius, unsigned long color);

#define fl_draw_rect(...) fl_draw_rect_impl(&(FlRectSpec){__VA_ARGS__})

// =============================================================================
// HELPERS
// =============================================================================

int fl_get_text_width(const char *text);
int fl_get_text_height(const char *text);
bool fl_is_visible(void *widget);

typedef struct {
    char name[64];
    char value[256];
    bool has_value;
} Fl_Flag;

typedef struct {
    Fl_Flag* flags;
    size_t count;
} Fl_FlagList;

Fl_FlagList fl_parse_flags(const char* input);
void fl_free_flags(Fl_FlagList* list);
int fl_parse_time(const char* time_str);

// =============================================================================
// HOTKEY MACROS DONT LOOK AT 
// =============================================================================

bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows);

// =============================================================================
// AUDIO / VOLUME
// =============================================================================

void fl_toggle_play_pause(void);
int fl_get_current_volume(void);
bool fl_set_current_volume(int volume);
bool fl_increment_volume_by(int increment);
bool fl_decrement_volume_by(int decrement);
bool fl_increment_volume_by_percent(int increment_percent);
bool fl_decrement_volume_by_percent(int decrement_percent);

// =============================================================================
// MISC
// =============================================================================

typedef struct { uint64_t ticks; } FlTimer;
FlTimer fl_start_timer(void);
double fl_end_timer(FlTimer t);
void fl_run_after_time(double seconds, void (*callback)(void));
char* fl_on_dir_changed(char* dir_path,void(callback)(void));
bool fl_string_match_insensitive(char* needle,char* haystack);
char* fl_get_working_dir();

// =============================================================================
// WINDOW
// =============================================================================

Fl_WindowList fl_get_all_window_titles();
Fl_WindowList fl_get_all_window_names();
bool fl_is_window_active(char* title);

// =============================================================================
// SCREENSHOT
// =============================================================================

void fl_screenshot_copy_area(void);
void fl_screenshot_copy(void);
void fl_ocr_area_to_clipboard(void);
bool fl_copy_image_to_clipboard(const unsigned char* png, size_t len);
void fl_screenshot_save_area(char* save_path);
void fl_screenshot_save(char* save_path);


Fl_Pos fl_get_cursor_pos(void);
bool fl_type_text(const char *s);
void fl_sleep(double seconds);
bool fl_is_key_down(int vk);
bool fl_is_key_pressed(int vk);
bool fl_is_key_released(int vk);
bool fl_is_key_held(int vk);
void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);
int fl_get_key_pressed(void);
char fl_get_char_pressed(void);

char* fl_send_ai_message(char* message);

#define _FL_HOTKEY_EXPAND(hotkey, trigger, cond, whitelist, ...) \
    fl_register_hotkey_impl(hotkey, trigger, cond, whitelist, (const char*[]){__VA_ARGS__, NULL})

#define _FL_AS_COND(x)   _Generic((x)+0, int: (bool(*)(void))NULL, default: (bool(*)(void))(x))
#define _FL_AS_BOOL(x)   _Generic((x)+0, int: (bool)(x), default: false)
#define _FL_AS_STR(x)    _Generic((x)+0, int: (const char*)NULL, default: (const char*)(x))

#define _FL_HOTKEY_ROUTER_2(hk, tr)              _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, NULL)
#define _FL_HOTKEY_ROUTER_3(hk, tr, s1)          _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, s1)

#define _FL_HOTKEY_ROUTER_4(hk, tr, arg3, arg4) \
    _Generic((arg3), \
        int: _FL_HOTKEY_EXPAND(hk, tr, NULL, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4)), \
        char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4), \
        const char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4), \
        default: _Generic((arg3), \
            bool(*)(void): _FL_HOTKEY_EXPAND(hk, tr, _FL_AS_COND(arg3), false, _FL_AS_STR(arg4)), \
            default: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4) \
        ) \
    )

#define _FL_HOTKEY_ROUTER_5(hk, tr, arg3, arg4, arg5) \
    _Generic((arg3), \
        int: _FL_HOTKEY_EXPAND(hk, tr, NULL, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4), _FL_AS_STR(arg5)), \
        char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4, arg5), \
        const char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4, arg5), \
        default: _FL_HOTKEY_EXPAND(hk, tr, _FL_AS_COND(arg3), _FL_AS_BOOL(arg4), _FL_AS_STR(arg5)) \
    )

#define _FL_HOTKEY_ROUTER_6(hk, tr, arg3, arg4, arg5, arg6) \
    _Generic((arg3), \
        int: _FL_HOTKEY_EXPAND(hk, tr, NULL, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4), _FL_AS_STR(arg5), _FL_AS_STR(arg6)), \
        char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4, arg5, arg6), \
        const char*: _FL_HOTKEY_EXPAND(hk, tr, NULL, 0, arg3, arg4, arg5, arg6), \
        default: _FL_HOTKEY_EXPAND(hk, tr, _FL_AS_COND(arg3), _FL_AS_BOOL(arg4), _FL_AS_STR(arg5), _FL_AS_STR(arg6)) \
    )

#define _FL_GET_MACRO(_1, _2, _3, _4, _5, _6, NAME, ...) NAME

#define fl_register_hotkey(...) \
    _FL_GET_MACRO(__VA_ARGS__, _FL_HOTKEY_ROUTER_6, _FL_HOTKEY_ROUTER_5, _FL_HOTKEY_ROUTER_4, _FL_HOTKEY_ROUTER_3, _FL_HOTKEY_ROUTER_2)(__VA_ARGS__)

bool fl_register_remapping_impl(const char* src, const char* dst, bool whitelist, const char** windows);

#define _FL_REMAP_EXPAND(src, dst, whitelist, ...) \
    fl_register_remapping_impl(src, dst, whitelist, (const char*[]){__VA_ARGS__, NULL})

#define _FL_REMAP_ROUTER_2(src, dst)             _FL_REMAP_EXPAND(src, dst, 0, NULL)
#define _FL_REMAP_ROUTER_3(src, dst, w1)         _FL_REMAP_EXPAND(src, dst, 0, w1)

#define _FL_REMAP_ROUTER_4(src, dst, arg3, arg4) \
    _Generic((arg3)+0, \
        int: _FL_REMAP_EXPAND(src, dst, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4)), \
        char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4), \
        const char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4), \
        default: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4) \
    )

#define _FL_REMAP_ROUTER_5(src, dst, arg3, arg4, arg5) \
    _Generic((arg3)+0, \
        int: _FL_REMAP_EXPAND(src, dst, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4), _FL_AS_STR(arg5)), \
        char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5), \
        const char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5), \
        default: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5) \
    )

#define _FL_REMAP_ROUTER_6(src, dst, arg3, arg4, arg5, arg6) \
    _Generic((arg3)+0, \
        int: _FL_REMAP_EXPAND(src, dst, _FL_AS_BOOL(arg3), _FL_AS_STR(arg4), _FL_AS_STR(arg5), _FL_AS_STR(arg6)), \
        char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5, arg6), \
        const char*: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5, arg6), \
        default: _FL_REMAP_EXPAND(src, dst, 0, arg3, arg4, arg5, arg6) \
    )

#define fl_register_remapping(...) \
    _FL_GET_MACRO(__VA_ARGS__, _FL_REMAP_ROUTER_6, _FL_REMAP_ROUTER_5, _FL_REMAP_ROUTER_4, _FL_REMAP_ROUTER_3, _FL_REMAP_ROUTER_2)(__VA_ARGS__)

#if defined(_WIN32)
#define FLINT_FEATURE_EXPORT static
#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

#define FLINT_FEATURE_INIT FLINT_FEATURE_EXPORT __attribute__((constructor)) void FlintFeatureInit(void)

#endif
