#ifndef SKAPI_H
#define SKAPI_H

#include <stdbool.h>
#include <stddef.h>
#include "core/types.h"
#include "core/window_manager.h"
#include "platform/widget.h"
#include "core/widget_registry.h"
#include "platform/platform.h"
#include "core/types/prompt.h"
#include "core/types/skapi_types.h"
#include "core/checkbox.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "core/skapi_input.h"
#include "core/clipboard_history.h"
#include "config.h"
#include "core/cc_registry.h"

typedef StringList SkStringList;
typedef HotkeyKey SkKey;
typedef WindowHandle SkWindowHandle;

typedef struct PlatformWidget PlatformWidget;
typedef PlColor SkColor;
typedef PlMetrics SkMetrics;
typedef Event SkEvent;
typedef EventCallback SkEventCallback;
typedef Widget SkWidget;
typedef WidgetSpec SkWidgetSpec;
typedef WidgetState SkWidgetState;
typedef WidgetRenderFn SkWidgetRenderFn;
typedef PlRect SkRect;
typedef PlLine SkLine;
typedef PlCircle SkCircle;
typedef PlPos SkPos;
typedef PlListSpec SkListSpec;
typedef PlWindowList SkWindowList;
typedef struct {
  const char *name;
  void (*callback)(void);
  const char *icon;
} SkFeature;
typedef struct {
  SkFeature *items;
  size_t count;
} SkFeatureList;
typedef CheckboxSpec SkCheckboxSpec;
typedef PlaceState SkPlaceState;
typedef ClipboardEntry SkClipboardEntry;
typedef ClipboardHistory SkClipboardHistory;

typedef struct {
  int index;
  char name[128];
  int x, y, w, h;
  bool is_primary;
} SkMonitorInfo;

bool sk_copy_to_clipboard(const char *text);

bool sk_copy_file_path(const char *path);

bool sk_paste_from_clipboard(void);

bool sk_paste(const char *content);

char *sk_get_last_clipboard_entry(void);

size_t sk_clipboard_history_count(void);

const SkClipboardEntry *sk_clipboard_history_get(size_t index);

const SkClipboardEntry *sk_clipboard_latest(void);

void sk_clipboard_clear_history(void);

SkNetStatus sk_net_status(void);

unsigned long long sk_net_rx_bytes(void);

unsigned long long sk_net_rx_bytes_over(double seconds);

#define SK_DIR_WALK_UNLIMITED 0

char *sk_search_file_in_dir(const char *directory, const char *search_string);

bool sk_get_all_files_from_dir(const char *dir_path,
                               char **list_of_allowed_extensions,
                               ListOfFiles *list_of_files,
                               char **list_of_names_to_ignore,
                               bool strip_extension, int max_depth);

SkStringList sk_load_file_list(const char *filepath);

long sk_load_file(const char *filepath, char *buffer, long buffer_size);

char *sk_read_file_to_string(const char *path, size_t *out_len);

long sk_read_entire_file(const char *full_path, char *buffer, long buffer_size);

bool sk_write_string_to_file(const char *path, const char *content);

bool sk_file_exists(const char *path);

bool sk_append_unique_line(const char *path, const char *line);

bool sk_remove_line(const char *path, const char *line);

bool sk_remove_line_contains(const char *path, const char *substr);

bool sk_parse_desktop_file(const char *path, char *out_name, size_t n,
                           bool *out_nodisplay);

char *sk_get_icon_path(const char *path);
bool sk_prewarm_image(const char *path);

bool sk_append_to_file(const char *filepath, const char *line);

bool sk_write_to_file(const char *filepath, const char *lines[], int count);

bool sk_write_temp_file(const char *name, const void *data, size_t len,
                        char *out_path, size_t out_len);

int sk_get_icon_size(const char *icon_path);

void sk_free_string_list(SkStringList *list);
void sk_free_files_list(ListOfFiles *list);

bool sk_free_list(const char *list[], int count);

bool sk_free_window_list(SkWindowList *list);

void sk_string_list_free(StringList *list);

void sk_format_bytes(unsigned long long bytes, char *out_str, size_t out_len);

bool sk_launch_with_args(const char *path, const char *args);

bool sk_launch_with_args_as_admin(const char *path, const char *args);

bool sk_launch_terminal_at_path(const char *path, const char *args);

char *sk_run_terminal_command(const char *command);

char *sk_run_terminal_command_blocking(const char *command);

char *sk_run_terminal_command_and_open(const char *command);

char *sk_run_argv(const char *const argv[], const char *stdin_data,
                  int *out_status);

char *sk_which(const char *command);

bool sk_launch_detached(const char *command);

bool sk_terminate_process_by_name(const char *exe_name);
bool sk_process_running(const char *name);

SkFeatureList sk_get_feature_list(void);
void sk_free_feature_list(SkFeatureList *list);

void sk_init_all_hotkeys(void);

void sk_send_hotkey(const char *hotkey_string);

#define sk_send_key sk_send_hotkey

bool sk_hotkey_to_string(SkKey key, char *out, size_t cap);

SkKey sk_hotkey_from_string(const char *str);

bool ski_hotkey_down(int vk_code, int mods);

bool sk_hotkey_down(SkKey key);

bool sk_launch_browser(const char *url);

bool sk_query_browser(const char *query);

bool sk_focus_window_by_title(const char *title);

SkWindowHandle sk_get_window_id(const char *title);
SkWindowHandle sk_get_window_by_root_atom(const char *atom);

bool sk_window_set_string_property(SkWindowHandle window, const char *atom,
                                   const char *value);
bool sk_window_set_long_property(SkWindowHandle window, const char *atom,
                                 long value);
bool sk_window_send_message(SkWindowHandle window, const char *atom, long data);

bool sk_close_active_window(void);

bool sk_dismiss_message(const char *name);

bool sk_message_active(const char *name);

bool sk_toggle_fullscreen_active(void);

bool sk_move_active_window_to_monitor(bool right);

bool sk_move_active_window_left(void);
bool sk_move_active_window_up(void);
bool sk_move_active_window_down(void);

bool sk_move_active_window_right(void);

bool sk_focus_window_left(void);
bool sk_focus_window_up(void);
bool sk_focus_window_down(void);

bool sk_focus_window_right(void);

bool sk_toggle_minimized_active(void);

bool sk_toggle_maximize_active(void);
bool sk_toggle_always_on_top_active(void);
bool sk_raise_active_window(void);
bool sk_lower_active_window(void);
bool sk_center_active_window(void);
bool sk_snap_active_window(SnapPreset preset);
long sk_get_active_window_pid(void);

bool sk_switch_to_workspace(int index);
bool sk_move_active_window_to_workspace(int index);
bool sk_set_active_window_opacity(int percent);
bool sk_close_windows_by_title(const char *needle);
bool sk_close_windows_by_id(const char *needle);

bool sk_reload_config(void);

int sk_get_screen_width(void);

int sk_get_screen_height(void);

SkRect sk_get_monitor_geometry(void);

char *sk_string_cut_at(const char *str, char delim);

char *sk_string_cut_before(const char *str, char delim);

char *sk_string_cut_after(const char *str, char delim);

char *sk_string_between(const char *str, char open, char close);

char *sk_string_inside(const char *str, char delim);

char *sk_string_trim(const char *str);

char *sk_string_to_lower(const char *str);

char *sk_string_to_upper(const char *str);

bool sk_string_starts_with(const char *str, const char *prefix);
bool sk_string_starts_with_ci(const char *str, const char *prefix);

bool sk_string_ends_with(const char *str, const char *suffix);
bool sk_string_equals_ci(const char *str, const char *other);

char *sk_string_replace(const char *str, const char *from, const char *to);

char *sk_string_slice(const char *str, int start, int end);

SkStringList sk_string_split(const char *str, const char *delimiter);

typedef struct {
  ListBoxInfo *lb;
  const char *filter;
  const char **items;
  int count;
  bool (*filter_match)(const char *item, const char *filter);
} SkPopulateListboxSpec;

void ski_populate_listbox(const SkPopulateListboxSpec *spec);
#define sk_populate_listbox(...)                                               \
  ski_populate_listbox(&(SkPopulateListboxSpec){__VA_ARGS__})

bool sk_is_selected(ListBoxInfo *lb);

void sk_set_edit_text(const char *text);

void sk_set_edit_cursor(int position);

int sk_get_edit_cursor(void);

void sk_get_edit_text(char *buffer, size_t cap);

typedef struct {
  const char *initial_text;
  int x;
  int y;
  int w;
  int h;
  ListBoxInfo *lb;
  int *out_index;
} SkEditPopupSpec;

#define sk_edit_text_popup(...)                                                \
  ski_edit_text_popup(&(SkEditPopupSpec){__VA_ARGS__})

char *ski_edit_text_popup(const SkEditPopupSpec *spec);

bool sk_replace_line_in_file(const char *filepath, int line_index,
                             const char *new_text);

#define sk_popup(...) ski_popup(&(PopupSpec){__VA_ARGS__})

char *ski_popup(const PopupSpec *spec);

typedef struct {
  const char *message;
  MessageSlot slot;
  int x;
  int y;
  int w;
  int h;
  int mw;
  int mh;
  double timeout;
  int prompt_gap;
  ColorRef color_text;
  const char *hover_key;
  void (*render)(void);
  int fps;
  bool no_bg;
  ColorRef color_bg;
  const char *name;
  bool close_on_keypress;
} SkMessageSpec;

typedef struct {
  void (*render)(void);
  int w;
  int h;
  unsigned long frame;
  SkTimer created_at;
  int pad;
} SkHoverPopupCtx;

#define sk_message(...) ski_message(&(SkMessageSpec){__VA_ARGS__})

void ski_message(const SkMessageSpec *spec);

#define sk_overlay(...) sk_message(__VA_ARGS__, .timeout = 10000000.0)
#define sk_dismiss_overlay(...) sk_dismiss_message(__VA_ARGS__)
#define sk_overlay_active(...) sk_message_active(__VA_ARGS__)
#define sk_subwindow(...) sk_window(__VA_ARGS__, .subwindow = true)

typedef PromptSpec SkPromptSpec;

#define sk_window(...) ski_window(&(SkPromptSpec){__VA_ARGS__})

void ski_window(const SkPromptSpec *spec);

typedef enum {
  FLANCHOR_TOP_LEFT,
  FLANCHOR_TOP_CENTER,
  FLANCHOR_TOP_RIGHT,
  FLANCHOR_CENTER_LEFT,
  FLANCHOR_CENTER,
  FLANCHOR_CENTER_RIGHT,
  FLANCHOR_BOTTOM_LEFT,
  FLANCHOR_BOTTOM_CENTER,
  FLANCHOR_BOTTOM_RIGHT,
  FLANCHOR_FILL_WIDTH,
  FLANCHOR_FILL_HEIGHT
} SkAnchor;

typedef struct {
  const char *text;
  int x;
  int y;
  unsigned long color;
  FontHandle font;
  int fontsize;
  const char *align;
  int pad;
  bool no_hscroll;
} SkTextSpec;

typedef struct {
  int x;
  int y;
} SkCharPos;

typedef struct {
  int x;
  int y;
  int max_width;
  int line_height;
  const char *text;
  int fontsize;
  SkCharPos *out_positions;
} SkMeasureSpec;

#define sk_measure_text_wrapped(...)                                           \
  ski_measure_text_wrapped(&(SkMeasureSpec){__VA_ARGS__})

int ski_measure_text_wrapped(const SkMeasureSpec *spec);

typedef struct {
  int x;
  int y;
  const char *text;
  int fontsize;
  const char *align;
  int pad;
} SkTextBoundsSpec;

SkRect ski_text_bounds(const SkTextBoundsSpec *spec);

#define sk_text_bounds(...) ski_text_bounds(&(SkTextBoundsSpec){__VA_ARGS__})

typedef struct {
  const char *path;
  int x;
  int y;
  int w;
  int h;
} SkImageSpec;

bool ski_draw_text(const SkTextSpec *spec);
bool ski_draw_image(const SkImageSpec *spec);
bool ski_place_editbox(const SkEditSpec *spec);
bool ski_place_listbox(const SkListSpec *spec);
bool ski_place_checkbox(const SkCheckboxSpec *spec);

bool sk_checkbox_is_checked(const char *name);
bool sk_checkbox_is_hovered(const char *name);
bool sk_checkbox_is_clicked(const char *name);
bool sk_button_is_checked(const char *name);
bool sk_button_is_hovered(const char *name);
bool sk_button_is_clicked(const char *name);

#define sk_checkbox_hovered sk_checkbox_is_hovered
#define sk_checkbox_clicked sk_checkbox_is_clicked

#define sk_place_button(...)                                                   \
  ski_place_checkbox(                                                          \
      &(SkCheckboxSpec){.border = true, __VA_ARGS__, .button = true})
#define sk_place_checkbox(...)                                                 \
  ski_place_checkbox(                                                          \
      &(SkCheckboxSpec){.border = true, __VA_ARGS__, .checkbox = true})

#define sk_checkbox_checked sk_checkbox_is_checked
#define sk_button_hovered sk_button_is_hovered
#define sk_button_clicked sk_button_is_clicked
#define sk_button_checked sk_button_is_checked

#define sk_draw_text(...) ski_draw_text(&(SkTextSpec){__VA_ARGS__})
#define sk_draw_image(...) ski_draw_image(&(SkImageSpec){__VA_ARGS__})
#define sk_place_editbox(...) ski_place_editbox(&(SkEditSpec){__VA_ARGS__})
#define sk_place_listbox(...) ski_place_listbox(&(SkListSpec){__VA_ARGS__})

unsigned long sk_listbox_get_current_standard_color(void);
bool sk_listbox_begin(const SkListSpec *spec);
bool sk_listbox_next(SkListItemCtx *out);
void sk_listbox_end(void);
int sk_listbox_get_count(void);

bool sk_draw_text_anchored(SkAnchor anchor, int offset_x, int offset_y,
                           const char *text);
bool sk_place_editbox_anchored(SkAnchor anchor, int offset_x, int offset_y,
                               int w, int h, const char *buffer);
bool sk_place_listbox_anchored(SkAnchor anchor, int offset_x, int offset_y,
                               int w, int h, const char **items, int count);
bool sk_draw_image_anchored(SkAnchor anchor, int offset_x, int offset_y, int w,
                            int h, const char *path);

void sk_layout_begin_column_anchored(SkAnchor anchor, int start_x, int start_y,
                                     int gap);
void sk_layout_begin_column(int start_x, int start_y, int gap);
void sk_layout_end_column(void);
void sk_layout_begin_row_anchored(SkAnchor anchor, int start_x, int start_y,
                                  int gap);
void sk_layout_begin_row(int start_x, int start_y, int gap);
void sk_layout_end_row(void);
bool sk_draw_text_auto(const char *text);
bool sk_place_editbox_auto(int w, int h, const char *buffer);
bool sk_place_listbox_auto(int w, int h, const char **items, int count);
bool sk_draw_image_auto(int w, int h, const char *path);

#define sk_widget(...) sk_widget_create(&(SkWidgetSpec){__VA_ARGS__})

SkWidget *sk_widget_create(const SkWidgetSpec *spec);
int sk_monitor_count(void);
bool sk_monitor_at(int index, SkMonitorInfo *out);

bool sk_widget_valid(SkWidget *widget);

SkPos sk_get_widget_pos(const char *name);

void sk_widget_show(SkWidget *widget);

void sk_widget_hide(SkWidget *widget);
void sk_widget_destroy(SkWidget *widget);

void sk_widget_set_render(SkWidget *widget, SkWidgetRenderFn render);

void *sk_load_image(const char *path);

void sk_free_image(void *image);

#define SK_DRAW_ALPHA_OPAQUE 255
#define SK_DRAW_ALPHA_TRANSPARENT 128

bool ski_draw_rect(const SkRect *spec);

bool ski_draw_line(const SkLine *spec);

bool ski_draw_circle(const SkCircle *spec);

#define sk_draw_rect(...) ski_draw_rect(&(PlRect){__VA_ARGS__})

#define sk_draw_line(...) ski_draw_line(&(PlLine){__VA_ARGS__})

#define sk_draw_circle(...) ski_draw_circle(&(PlCircle){__VA_ARGS__})

int sk_rand_num_between(int min, int max);

int sk_get_text_width(const char *text, int fontsize);

int sk_get_text_height(int fontsize);

char *sk_http_get(const char *url);

char *sk_http_get_limited(const char *url, size_t max_bytes, size_t *out_len);

bool sk_http_download(const char *url, const char *save_path);

char *sk_http_post(const char *url, const char *content_type, const char *body);

typedef void (*SkHttpCallback)(const char *body, bool ok, void *userdata);

void sk_http_get_async(const char *url, SkHttpCallback cb, void *userdata);

typedef struct {
  char name[64];
  char value[256];
  bool has_value;
} SkFlag;

typedef struct {
  SkFlag *flags;
  size_t count;
} SkFlagList;

SkFlagList sk_parse_flags(const char *input);
bool sk_is_flag_prefix(char c);
size_t sk_extract_flag_name(char *p, char *out, size_t cap);
bool sk_extract_quoted(char **pp, char *out, size_t cap);
bool sk_extract_value(char **pp, char *out, size_t cap);
bool sk_next_flag(char *p);

void sk_free_flags(SkFlagList *list);

SkTimer sk_start_timer(void);

double ski_end_timer(uint64_t ticks);

double sk_end_timer(SkTimer t);

SkTimer sk_profile_start(void);

void ski_profile_end(uint64_t ticks);

void sk_profile_end(SkTimer timer);

void sk_run_after_time(double seconds, void (*callback)(void));

bool sk_string_contains(const char *haystack, const char *needle);
bool sk_string_contains_ci(const char *haystack, const char *needle);

char *sk_get_working_dir(void);

char *sk_resolve_data_path(const char *name);

char *sk_resolve_feature_path(const char *name);

const char *sk_resolve_user_path(void);

const char *sk_get_user_name(void);

bool sk_is_root(void);

void sk_run_every(double seconds, void (*callback)(void));

typedef void *SkLibrary;

SkLibrary sk_library_open(const char *path);

void sk_library_register(const char *name, SkLibrary handle);

SkLibrary sk_library_get(const char *name);

void *sk_library_symbol(SkLibrary lib, const char *name);

void sk_library_close(SkLibrary lib);

const char *sk_library_error(void);

typedef void *SkThread;
typedef void *SkMutex;
typedef void *(*SkThreadFunc)(void *arg);

SkThread sk_thread_create(SkThreadFunc fn, void *arg);

void sk_thread_join(SkThread thread);

SkMutex sk_mutex_create(void);

void sk_mutex_destroy(SkMutex mutex);

void sk_mutex_lock(SkMutex mutex);

void sk_mutex_unlock(SkMutex mutex);

typedef struct {
  char *key;
  long value;
} SkHashMap;

void sk_hashmap_init(SkHashMap **map);

void sk_hashmap_free(SkHashMap **map);

void sk_hashmap_put(SkHashMap **map, const char *key, long value);

void sk_hashmap_add(SkHashMap **map, const char *key, long delta);

long sk_hashmap_get(SkHashMap *map, const char *key, long def);

bool sk_hashmap_has(SkHashMap *map, const char *key);

bool sk_hashmap_del(SkHashMap **map, const char *key);

size_t sk_hashmap_len(SkHashMap *map);

const SkHashMap *sk_hashmap_at(SkHashMap *map, size_t index);

SkWindowList sk_get_all_window_titles(void);

SkWindowList sk_get_all_window_ids(void);

char *sk_get_active_window_title(void);

char *sk_get_active_window_id(void);

bool sk_is_window_open(const char *title);
#define sk_window_open sk_is_window_open

SkColor ski_get_pixel_color(int x, int y);

SkColor sk_get_pixel_color(SkPos pos);

bool ski_color_matches(int x, int y, size_t r, size_t g, size_t b);

bool sk_color_matches(SkPos pos, SkColor color);

bool ski_move_to_pos(int x, int y);

bool sk_move_to_pos(SkPos pos);

bool ski_click_at(int x, int y);

bool sk_click_at(SkPos pos);

bool sk_left_click(void);

bool sk_right_click(void);

bool sk_emit_mouse_button(int button, bool down);

bool sk_emit_mouse_button_at(SkPos pos, int button, bool down);

bool sk_click_button_at(SkPos pos, int button);

bool sk_mouse_scroll(int notches);

void sk_emit_key(int vk, bool down);

#define sk_move_mouse sk_move_to_pos
#define sk_click() sk_left_click()

bool sk_move_to_pos_rel(int rx, int ry);

bool sk_click_at_rel(int rx, int ry);

bool sk_emit_mouse_button_at_rel(int rx, int ry, int button, bool down);

bool sk_click_button_at_rel(int rx, int ry, int button);

bool sk_type_text(const char *s);

void sk_sleep(double seconds);

bool sk_is_key_down(int vk);

bool sk_is_key_pressed(int vk);

bool sk_is_key_released(int vk);

bool sk_ctrl_pressed(void);

bool sk_shift_pressed(void);

bool sk_alt_pressed(void);

bool sk_super_pressed(void);

void sk_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);

int sk_get_key_pressed(void);

char sk_get_char_pressed(void);

bool sk_vk_to_name(int vk, char *out, size_t cap);

#define sk_key_pressed sk_is_key_pressed
#define sk_key_released sk_is_key_released
#define sk_ctrl_down sk_ctrl_pressed
#define sk_shift_down sk_shift_pressed
#define sk_alt_down sk_alt_pressed
#define sk_super_down sk_super_pressed
#define sk_is_ctrl_pressed sk_ctrl_pressed
#define sk_is_shift_pressed sk_shift_pressed
#define sk_is_alt_pressed sk_alt_pressed
#define sk_is_super_pressed sk_super_pressed

typedef struct {
  int vk;
  int scancode;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} SkKeyEvent;

#define SK_MOUSE_LEFT 1
#define SK_MOUSE_RIGHT 2
#define SK_MOUSE_MIDDLE 3
#define SK_MOUSE_SIDE1 4
#define SK_MOUSE_SIDE2 5
#define SK_MOUSE_BACK 6
#define SK_MOUSE_FORWARD 7
#define SK_MOUSE_WHEEL 8

typedef struct {
  int button;
  int x;
  int y;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} SkMouseEvent;

bool sk_poll_mouse(SkMouseEvent *out);

int sk_mouse_queue_len(void);

unsigned long sk_mouse_queue_dropped(void);

void sk_clear_mouse_queue(void);

SkPos sk_get_cursor_pos(void);

#define sk_get_mouse_pos() sk_get_cursor_pos()

bool sk_mouse_pressed(int button);

bool sk_poll_key(SkKeyEvent *out);

int sk_key_queue_len(void);

unsigned long sk_key_queue_dropped(void);

void sk_clear_key_queue(void);

typedef CcEntrySpec SkCcEntrySpec;

#define sk_add_to_cc(...) ski_add_to_cc(&(SkCcEntrySpec){__VA_ARGS__})

#define sk_add_feature(...) sk_add_to_cc(.type = "Feature", __VA_ARGS__)

bool ski_add_to_cc(const SkCcEntrySpec *spec);

typedef struct {
  const char *alias;
  const char *target;
} SkAliasSpec;

#define sk_register_alias(...) ski_add_alias(&(SkAliasSpec){__VA_ARGS__})

bool ski_add_alias(const SkAliasSpec *spec);
bool ski_remove_alias(const char *alias);
#define sk_remove_alias(alias) ski_remove_alias((alias))

const char *sk_alias_for(const char *target);

typedef void (*SkConfigFn)(void *ud);

typedef struct {
  const char *name;
  const char *hotkey;
  SkConfigFn cb;
  void *ud;
} SkConfigHotkeySpec;

#define sk_config_set_hotkey(...)                                              \
  ski_config_set_hotkey(&(SkConfigHotkeySpec){__VA_ARGS__})

typedef struct {
  const char *target;
  const char *alias;
} SkConfigAliasSpec;

#define sk_config_set_alias(...)                                               \
  ski_config_set_alias(&(SkConfigAliasSpec){__VA_ARGS__})

void sk_config_init(void);

bool ski_config_set_hotkey(const SkConfigHotkeySpec *spec);
bool sk_config_get_hotkey(const char *name, char *out, size_t cap);
bool sk_config_clear_hotkey(const char *name);

bool sk_config_bind_hotkey(const char *name, SkConfigFn cb, void *ud);

int sk_config_hotkey_count(void);
bool sk_config_hotkey_at(int index, char *name_out, size_t name_cap,
                         char *hotkey_out, size_t hotkey_cap);

bool ski_config_set_alias(const SkConfigAliasSpec *spec);
bool sk_config_get_alias(const char *target, char *out, size_t cap);
bool sk_config_clear_alias(const char *target);

int sk_config_alias_count(void);
bool sk_config_alias_at(int index, char *target_out, size_t target_cap,
                        char *alias_out, size_t alias_cap);

int sk_cc_entry_count(void);

const CcEntrySpec *sk_cc_entry_at(int index);

bool ski_add_dep(const char *dep_name, const char *owner_file);

#define sk_add_dep(...) ski_add_dep(__VA_ARGS__, __FILE__)

typedef struct {
  const char *hotkey;
  void (*cb)(void);
  void (*cb_ud)(void *);
  bool (*should_swallow)(void);
  const char *whitelist[16];
  const char *blacklist[16];
  bool swallow;
  void *ud;
} SkHotkeySpec;

#define sk_register_hotkey(...)                                                \
  ski_register_hotkey(&(SkHotkeySpec){__VA_ARGS__})

bool ski_register_hotkey(const SkHotkeySpec *spec);
bool ski_unregister_hotkey(const char *hotkey);
#define sk_unregister_hotkey(hk) ski_unregister_hotkey((hk))

void sk_disable_global_hotkeys(void);
void sk_enable_global_hotkeys(void);

int sk_hotkey_count(void);
bool sk_hotkey_string_at(int index, char *out, size_t cap);
bool sk_hotkey_taken(const char *hotkey);
bool sk_hotkey_for_callback(void (*cb)(void), char *out, size_t cap);
bool sk_hotkey_for_ud(void *ud, char *out, size_t cap);
void sk_register_deferred_init(void (*cb)(void));

typedef struct {
  const char *src;
  const char *dst;
  const char *whitelist[16];
  const char *blacklist[16];
} SkRemappingSpec;

#define sk_register_remapping(...)                                             \
  ski_register_remapping(&(SkRemappingSpec){__VA_ARGS__})

bool ski_register_remapping(const SkRemappingSpec *spec);

bool sk_replay_hotkey(int vk, int mods);

typedef void (*SkSnippetFunc)(void);

typedef struct {
  const char *trigger;
  const char *text;
  SkSnippetFunc fn;
  const char *whitelist[16];
  const char *blacklist[16];
} SkSnippetSpec;

#define sk_register_snippet(...)                                               \
  ski_register_snippet(&(SkSnippetSpec){__VA_ARGS__})

bool ski_register_snippet(const SkSnippetSpec *spec);
bool ski_unregister_snippet(const char *trigger);
#define sk_unregister_snippet(trigger) ski_unregister_snippet((trigger))

void sk_reload_feature(const char *name);
int sk_reload_all_features(void);
int sk_get_pid(void);

#if defined(_WIN32)
#define SKALD_FEATURE_EXPORT __declspec(dllexport)
#else
#define SKALD_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

#define SKALD_FEATURE_INIT                                                     \
  SKALD_FEATURE_EXPORT __attribute__((constructor)) void skald_feature_init(   \
      void)

#define SKALD_FEATURE_SHUTDOWN                                                 \
  SKALD_FEATURE_EXPORT void skald_feature_shutdown(void)

void sk_cap_register_abi(const char *key, unsigned abi, void *fn);
void *sk_cap_resolve_abi(const char *key, unsigned abi);

void ski_cap_register(const char *key, void *fn);
void *ski_cap_resolve(const char *key);
unsigned sk_cap_generation(void);
void sk_cap_bind_declare(const char *key, void *consumer);

#define sk_cap_register(key, fn) ski_cap_register((key), (void *)(fn))

#define sk_cap_resolve(key) ski_cap_resolve((key))

#define sk_cap_bind(var, key, type)                                            \
  static type var##_fn;                                                        \
  static unsigned var##_gen;                                                   \
  type var = NULL;                                                             \
  if (var##_gen != sk_cap_generation()) {                                      \
    sk_cap_bind_declare(key, (void *)&var##_fn);                               \
    var##_fn = (type)sk_cap_resolve(key);                                      \
    var##_gen = sk_cap_generation();                                           \
  }                                                                            \
  var = var##_fn

#define SK_WHITE RGB(255, 255, 255)
#define SK_BLACK RGB(1, 1, 1)
#define SK_RED RGB(255, 0, 0)
#define SK_DARK_RED RGB(128, 0, 0)
#define SK_GREEN RGB(0, 255, 0)
#define SK_DARK_GREEN RGB(0, 128, 0)
#define SK_BLUE RGB(0, 0, 255)
#define SK_YELLOW RGB(255, 255, 0)
#define SK_CYAN RGB(0, 255, 255)
#define SK_MAGENTA RGB(255, 0, 255)
#define SK_ORANGE RGB(255, 165, 0)
#define SK_PURPLE RGB(128, 0, 128)
#define SK_PINK RGB(255, 192, 203)
#define SK_BROWN RGB(139, 69, 19)
#define SK_LIME RGB(50, 205, 50)
#define SK_TEAL RGB(0, 128, 128)
#define SK_NAVY RGB(0, 0, 128)
#define SK_MAROON RGB(128, 0, 0)
#define SK_OLIVE RGB(128, 128, 0)
#define SK_SILVER RGB(192, 192, 192)
#define SK_GOLD RGB(255, 215, 0)
#define SK_INDIGO RGB(75, 0, 130)
#define SK_VIOLET RGB(238, 130, 238)
#define SK_TURQUOISE RGB(64, 224, 208)
#define SK_SALMON RGB(250, 128, 114)
#define SK_CORAL RGB(255, 127, 80)
#define SK_CRIMSON RGB(220, 20, 60)
#define SK_CHOCOLATE RGB(210, 105, 30)
#define SK_GRAY RGB(50, 50, 50)
#define SK_LIGHT_GRAY RGB(211, 211, 211)
#define SK_DARK_GRAY RGB(14, 14, 14)

SkMetrics *sk_get_metrics(void);

void *sk_get_active_prompt(void);

#define SK_EVENT_PROMPT_OPEN EVENT_PROMPT_OPEN
#define SK_EVENT_PROMPT_CLOSE EVENT_PROMPT_CLOSE

void sk_register_event(SkEvent event, SkEventCallback cb);
void sk_unregister_event(SkEvent event, SkEventCallback cb);

typedef bool (*SkKeyInterceptorFn)(const PopupKeyEvent *key);

void sk_register_key_interceptor(SkKeyInterceptorFn fn);

void sk_close_window(void);

#endif
