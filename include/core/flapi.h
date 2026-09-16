#ifndef FLAPI_H
#define FLAPI_H

#include <stdbool.h>
#include <stddef.h>
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
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
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

typedef struct {
    size_t r, g, b;
} FlColor;

// =============================================================================
// CLIPBOARD
// =============================================================================

bool fl_copy_to_clipboard(const char* text);
bool fl_paste_from_clipboard();
bool fl_paste(char* content);
char* fl_get_last_clipboard_entry();

// =============================================================================
// FILES / FILESYSTEM
// =============================================================================

char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path);
bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore);
int fl_load_file_list(const char* filepath, const char* out[], int max);
char* fl_read_file_to_string(const char* path, size_t* out_len);
bool fl_write_string_to_file(const char* path, const char* content);
bool fl_file_exists(const char* path);
bool fl_append_unique_line(const char* path, const char* line);
bool fl_remove_line(const char* path, const char* line);
bool fl_remove_line_contains(const char* path, const char* substr);
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay);
char* fl_get_icon_path(const char* app_path_path);
bool fl_free_file_list(const char* list[], int count);
bool fl_free_list(const char* list[], int count);
bool fl_free_window_list(WindowList *list);
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

// =============================================================================
// BROWSER / ZOXIDE / SYSTEM
// =============================================================================

bool fl_launch_browser(char* url);
bool fl_query_browser(const char* query);
bool fl_launch_browser_in_a_new_window(char* url);
char* fl_get_zoxide_match(char* query);
bool fl_focus_window_by_title(char* title);
WindowList fl_get_all_window_titles();
WindowList fl_get_all_window_names();

// =============================================================================
// WATCH / ADB / MISC
// =============================================================================

char* fl_on_dir_changed(char* dir_path,void(callback)(void));
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
bool fl_string_match_insensitive(char* needle,char* haystack);

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

bool fl_edit_text_popup(const char* initial_text, const char* title, int x, int y, const char* filepath, ListBoxInfo* lb, RefreshCallback refresh);
void fl_edit_confirm_callback(const char* new_text, void* user_data);

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
// x,y,w,h are absolute popup coords. No magic.
// Macro trick like PROMPT — unspecified fields default (color=COLOR_TEXT, font=default, align="CENTER")

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

bool fl_place_text_raw_impl(const FlTextSpec *spec);
bool fl_place_editbox_raw_impl(const FlEditSpec *spec);
bool fl_place_listbox_raw_impl(const FlListSpec *spec);
bool fl_place_image_raw_impl(const FlImageSpec *spec);

#define fl_place_text_raw(...) fl_place_text_raw_impl(&(FlTextSpec){__VA_ARGS__})
#define fl_place_editbox_raw(...) fl_place_editbox_raw_impl(&(FlEditSpec){__VA_ARGS__})
#define fl_place_listbox_raw(...) fl_place_listbox_raw_impl(&(FlListSpec){__VA_ARGS__})
#define fl_place_image_raw(...) fl_place_image_raw_impl(&(FlImageSpec){__VA_ARGS__})

bool fl_listbox_begin(const FlListSpec *spec);
bool fl_listbox_next(FlListItemCtx *out);
void fl_listbox_end(void);
#define fl_place_listbox(...) fl_place_listbox_raw(__VA_ARGS__)

// =============================================================================
// ANCHORED PLACEMENT (preset positions)
// =============================================================================
// Same widgets but x,y calculated from anchor. offset_x/y are fine tuning.

bool fl_place_text_anchored(FlAnchor anchor, int offset_x, int offset_y, const char *text);
bool fl_place_editbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *buffer);
bool fl_place_listbox_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char **items, int count);
bool fl_place_image_anchored(FlAnchor anchor, int offset_x, int offset_y, int w, int h, const char *path);

// =============================================================================
// AUTO LAYOUT (column / row math)
// =============================================================================
// Stack widgets without calculating y. Gap is space between items.

void fl_layout_begin_column(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_column_raw(int start_x, int start_y, int gap);
void fl_layout_end_column(void);
void fl_layout_begin_row(FlAnchor anchor, int start_x, int start_y, int gap);
void fl_layout_begin_row_raw(int start_x, int start_y, int gap);
void fl_layout_end_row(void);
bool fl_place_text_auto(const char *text);
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

bool fl_draw_rect(int x, int y, int w, int h, unsigned long color);
bool fl_draw_line(int x1, int y1, int x2, int y2, unsigned long color);
bool fl_draw_circle(int x, int y, int radius, unsigned long color);
bool fl_draw_rounded_rect(int x, int y, int w, int h, int radius, unsigned long color);

// =============================================================================
// HELPERS
// =============================================================================

int fl_get_text_width(const char *text);
int fl_get_text_height(const char *text);
bool fl_is_visible(void *widget);

// =============================================================================
// HOTKEY MACROS DONT LOOK AT 
// =============================================================================

bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows);

// =============================================================================
// REMAP
// =============================================================================

bool fl_register_remapping(const char* src, const char* dst);
// =============================================================================
// AUDIO
// =============================================================================
void fl_toggle_play_pause(void);

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

#if defined(_WIN32)
#define FLINT_FEATURE_EXPORT static

#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

#define FLINT_FEATURE_INIT FLINT_FEATURE_EXPORT __attribute__((constructor)) void FlintFeatureInit(void)

#endif
