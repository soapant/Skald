#ifndef FLAPI_H
#define FLAPI_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion"

#include <stdbool.h>
#include <stddef.h>
#include "core/types.h"
#include "core/window_manager.h"
#include "platform/widget.h"
#include "core/widget_registry.h"
#include "platform/platform.h"
#include "core/types/prompt.h"
#include "core/types/flapi_types.h"
#include "core/checkbox.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include "core/flapi_input.h"
#include "core/clipboard_history.h"
#include "config.h"
#include "core/cc_registry.h"


typedef StringList FlStringList;


typedef HotkeyKey FlKey;

typedef struct PlatformWidget PlatformWidget;


typedef PlColor FlColor;

typedef PlMetrics FlMetrics;
typedef PlEvent FlEvent;
typedef PlEventCallback FlEventCallback;
typedef Widget FlWidget;
typedef WidgetSpec FlWidgetSpec;
typedef WidgetState FlWidgetState;
typedef WidgetRenderFn FlWidgetRenderFn;


typedef PlRect FlRect;


typedef PlPos FlPos;


typedef PlListSpec FlListSpec;


typedef PlWindowList FlWindowList;

typedef CheckboxSpec FlCheckboxSpec;


typedef ClipboardEntry FlClipboardEntry;


typedef ClipboardHistory FlClipboardHistory;


bool fl_copy_to_clipboard(const char *text);


bool fl_copy_file_path(const char *path);


bool fl_paste_from_clipboard(void);


bool fl_paste(char *content);


char *fl_get_last_clipboard_entry(void);


size_t fl_clipboard_history_count(void);


const FlClipboardEntry *fl_clipboard_history_get(size_t index);


const FlClipboardEntry *fl_clipboard_latest(void);


void fl_clipboard_clear_history(void);


FlNetStatus fl_net_status(void);


unsigned long long fl_net_rx_bytes(void);


unsigned long long fl_net_rx_bytes_over(double seconds);



#define FL_DIR_WALK_UNLIMITED 0

char *fl_search_file_in_dir(const char *directory, const char *search_string,
                            char *output_path);

bool fl_get_all_files_from_dir(const char *dir_path,
                               char **list_of_allowed_extensions,
                               ListOfFiles *list_of_files,
                               char **list_of_names_to_ignore,
                               bool strip_extension, int max_depth);


FlStringList fl_load_file_list(const char *filepath);


long fl_load_file(const char *filepath, char *buffer, long buffer_size);


char *fl_read_file_to_string(const char *path, size_t *out_len);


long fl_read_entire_file(const char *full_path, char *buffer, long buffer_size);


bool fl_write_string_to_file(const char *path, const char *content);


bool fl_file_exists(const char *path);


bool fl_append_unique_line(const char *path, const char *line);


bool fl_remove_line(const char *path, const char *line);


bool fl_remove_line_contains(const char *path, const char *substr);


bool fl_parse_desktop_file(const char *path, char *out_name, size_t n,
                           bool *out_nodisplay);


char *fl_get_icon_path(const char *app_path_path);


bool fl_append_to_file(const char *filepath, const char *line);


bool fl_write_to_file(const char *filepath, const char *lines[], int count);


bool fl_write_temp_file(const char *name, const void *data, size_t len,
                        char *out_path, size_t out_len);


int fl_get_icon_size(const char *icon_path);


void fl_free_file_list(FlStringList *list);


bool fl_free_list(const char *list[], int count);


bool fl_free_window_list(FlWindowList *list);


void fl_string_list_free(StringList *list);


void fl_format_bytes(unsigned long long bytes, char *out_str, size_t out_len);


bool fl_launch_with_args(const char *path, const char *args);


bool fl_launch_with_args_as_admin(const char *path, const char *args);


bool fl_launch_terminal_at_path(char *path, char *args);


char *fl_run_terminal_command(char *command);


char *fl_run_terminal_command_blocking(char *command);


char *fl_run_terminal_command_and_open(char *command);


char *fl_run_argv(const char *const argv[], const char *stdin_data,
                  int *out_status);


bool fl_launch_detached(char *command);


bool fl_terminate_process_by_name(const char *exe_name);


int fl_get_feature_list(const char **out_names, void (**out_cbs)(void),
                        int max);


int fl_get_feature_list_with_icons(const char **out_names,
                                   void (**out_cbs)(void),
                                   const char ***out_icons, int max);


void fl_init_all_hotkeys(void);


void fl_send_hotkey(const char *hotkey_string);

#define fl_send_key fl_send_hotkey


char *fl_hotkey_to_string(FlKey key);


FlKey fl_hotkey_from_string(const char *str);


bool fl_hotkey_down(FlKey key);


bool fl_launch_browser(char *url);


bool fl_query_browser(const char *query);


bool fl_focus_window_by_title(char *title);


bool fl_close_active_window(void);


bool fl_dismiss_message(const char *name);


bool fl_message_active(const char *name);


bool fl_toggle_fullscreen_active(void);


bool fl_move_active_window_to_monitor(bool right);


bool fl_move_active_window_left(void);


bool fl_move_active_window_right(void);


bool fl_focus_window_left(void);


bool fl_focus_window_right(void);


bool fl_toggle_minimized_active(void);


int fl_get_screen_width(void);


int fl_get_screen_height(void);


FlRect fl_get_monitor_geometry(void);




char *fl_string_cut_at(const char *str, char delim);


char *fl_string_cut_before(const char *str, char delim);


char *fl_string_cut_after(const char *str, char delim);


char *fl_string_between(const char *str, char open, char close);


char *fl_string_inside(const char *str, char delim);


char *fl_string_trim(const char *str);


char *fl_string_to_lower(const char *str);


char *fl_string_to_upper(const char *str);


bool fl_string_starts_with(const char *str, const char *prefix);


bool fl_string_ends_with(const char *str, const char *suffix);


char *fl_string_replace(const char *str, const char *from, const char *to);


char *fl_string_slice(const char *str, int start, int end);


FlStringList fl_string_split(const char *str, const char *delimiter);


void fl_populate_listbox(ListBoxInfo *lb, const char *filter,
                         const char **items, int count);


void fl_populate_listbox_ex(ListBoxInfo *lb, const char *filter,
                            const char **items, int count,
                            bool (*filter_match)(const char *item,
                                                 const char *filter));


bool fl_is_selected(ListBoxInfo *lb);


void fl_set_edit_text(const char *text);


void fl_set_edit_cursor(int position);


int fl_get_edit_cursor(void);


void fl_get_edit_text(char *buffer, size_t cap);


typedef struct {
  const char *initial_text;
  int x;
  int y;
  int w;
  int h;
  ListBoxInfo *lb;
  int *out_index;
} FlEditPopupSpec;

#define fl_edit_text_popup(...)                                                \
  fl_edit_text_popup_impl(&(FlEditPopupSpec){__VA_ARGS__})


char *fl_edit_text_popup_impl(const FlEditPopupSpec *spec);


bool fl_replace_line_in_file(const char *filepath, int line_index,
                             const char *new_text);


#define fl_popup(...) fl_popup_impl(&(PopupSpec){__VA_ARGS__})


char *fl_popup_impl(const PopupSpec *spec);

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
} FlMessageSpec;

typedef struct {
  void (*render)(void);
  int w;
  int h;
  unsigned long frame;
  FlTimer created_at;
  int pad;
} FlHoverPopupCtx;


#define fl_message(...) fl_message_impl(&(FlMessageSpec){__VA_ARGS__})

void fl_message_impl(const FlMessageSpec *spec);

#define fl_overlay(...) fl_message(__VA_ARGS__, .timeout = 10000000.0)
#define fl_dismiss_overlay(...) fl_dismiss_message(__VA_ARGS__)
#define fl_overlay_active(...) fl_message_active(__VA_ARGS__)
#define fl_subwindow(...) fl_window(__VA_ARGS__, .subwindow = true)

typedef PromptSpec FlPromptSpec;


#define fl_window(...) fl_window_impl(&(FlPromptSpec){__VA_ARGS__})


void fl_window_impl(const FlPromptSpec *spec);

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
} FlAnchor;


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


int fl_draw_text_wrapped_ex(int x, int y, int max_width, int line_height,
                            const char *text, int fontsize, unsigned long color,
                            FlCharPos *out_positions, int max_positions);


typedef struct {
  int x;
  int y;
  int max_width;
  int line_height;
  const char *text;
  int fontsize;
  FlCharPos *out_positions;
} FlMeasureSpec;


#define fl_measure_text_wrapped(...)                                           \
  fl_measure_text_wrapped_impl(&(FlMeasureSpec){__VA_ARGS__})


int fl_measure_text_wrapped_impl(const FlMeasureSpec *spec);



typedef struct {
  const char *path;
  int x;
  int y;
  int w;
  int h;
} FlImageSpec;

bool fl_draw_text_impl(const FlTextSpec *spec);
bool fl_draw_image_impl(const FlImageSpec *spec);
bool fl_place_editbox_impl(const FlEditSpec *spec);
bool fl_place_listbox_impl(const FlListSpec *spec);
bool fl_place_checkbox_impl(const FlCheckboxSpec *spec);

bool fl_checkbox_is_checked(const char *name);
bool fl_checkbox_is_hovered(const char *name);

#define fl_checkbox_hovered fl_checkbox_is_hovered
#define fl_checkbox_clicked fl_checkbox_is_clicked

#define fl_place_button(...)                                                   \
  fl_place_checkbox_impl(                                                      \
      &(FlCheckboxSpec){.border = true, __VA_ARGS__, .button = true})
#define fl_place_checkbox(...)                                                 \
  fl_place_checkbox_impl(                                                      \
      &(FlCheckboxSpec){.border = true, __VA_ARGS__, .checkbox = true})

#define fl_button_is_clicked fl_checkbox_is_checked
#define fl_button_is_hovered fl_checkbox_is_hovered
#define fl_button_clicked fl_checkbox_checked
#define fl_button_hovered fl_checkbox_hovered

#define fl_draw_text(...) fl_draw_text_impl(&(FlTextSpec){__VA_ARGS__})
#define fl_draw_image(...) fl_draw_image_impl(&(FlImageSpec){__VA_ARGS__})
#define fl_place_editbox(...) fl_place_editbox_impl(&(FlEditSpec){__VA_ARGS__})
#define fl_place_listbox(...) fl_place_listbox_impl(&(FlListSpec){__VA_ARGS__})

bool fl_listbox_begin(const FlListSpec *spec);
bool fl_listbox_next(FlListItemCtx *out);
void fl_listbox_end(void);

bool fl_draw_text_anchored(FlAnchor anchor, int offset_x, int offset_y,
                           const char *text);
bool fl_place_editbox_anchored(FlAnchor anchor, int offset_x, int offset_y,
                               int w, int h, const char *buffer);
bool fl_place_listbox_anchored(FlAnchor anchor, int offset_x, int offset_y,
                               int w, int h, const char **items, int count);
bool fl_draw_image_anchored(FlAnchor anchor, int offset_x, int offset_y, int w,
                            int h, const char *path);

void fl_layout_begin_column_anchored(FlAnchor anchor, int start_x, int start_y,
                                     int gap);
void fl_layout_begin_column(int start_x, int start_y, int gap);
void fl_layout_end_column(void);
void fl_layout_begin_row_anchored(FlAnchor anchor, int start_x, int start_y,
                                  int gap);
void fl_layout_begin_row(int start_x, int start_y, int gap);
void fl_layout_end_row(void);
bool fl_draw_text_auto(const char *text);
bool fl_place_editbox_auto(int w, int h, const char *buffer);
bool fl_place_listbox_auto(int w, int h, const char **items, int count);
bool fl_draw_image_auto(int w, int h, const char *path);



#define fl_widget(...) fl_widget_create(&(FlWidgetSpec){__VA_ARGS__})


FlWidget *fl_widget_create(const FlWidgetSpec *spec);


bool fl_widget_valid(FlWidget *widget);


FlPos fl_get_widget_pos(const char *name);


void fl_widget_show(FlWidget *widget);


void fl_widget_hide(FlWidget *widget);


void fl_widget_set_render(FlWidget *widget, FlWidgetRenderFn render);


void *fl_load_image(const char *path);


void fl_free_image(void *image);

#define FL_DRAW_ALPHA_OPAQUE 255
#define FL_DRAW_ALPHA_TRANSPARENT 128


typedef struct {
  int x;
  int y;
  int w;
  int h;
  unsigned long color;
  float thickness;
  bool transparent;
  bool filled;
  int radius;
  bool rounded;
} FlRectSpec;


typedef struct {
  int x1;
  int y1;
  int x2;
  int y2;
  unsigned long color;
  float thickness;
} FlLineSpec;


typedef struct {
  int x;
  int y;
  int radius;
  unsigned long color;
  float thickness;
} FlCircleSpec;


bool fl_draw_rect_impl(const FlRectSpec *spec);


bool fl_draw_line_impl(const FlLineSpec *spec);


bool fl_draw_circle_impl(const FlCircleSpec *spec);


#define fl_draw_rect(...) fl_draw_rect_impl(&(FlRectSpec){__VA_ARGS__})


#define fl_draw_line(...) fl_draw_line_impl(&(FlLineSpec){__VA_ARGS__})


#define fl_draw_circle(...) fl_draw_circle_impl(&(FlCircleSpec){__VA_ARGS__})


int fl_rand_num_between(int min, int max);


int fl_get_text_width(const char *text, int fontsize);


int fl_get_text_height(const char *text);


int fl_get_glyph_ink_height(const char *text, int fontsize);


char *fl_http_get(const char *url);


char *fl_http_get_limited(const char *url, size_t max_bytes, size_t *out_len);


bool fl_http_download(const char *url, const char *save_path);


char *fl_http_post(const char *url, const char *content_type, const char *body);


typedef void (*FlHttpCallback)(const char *body, bool ok, void *userdata);


void fl_http_get_async(const char *url, FlHttpCallback cb, void *userdata);


typedef struct {
  char name[64];
  char value[256];
  bool has_value;
} FlFlag;


typedef struct {
  FlFlag *flags;
  size_t count;
} FlFlagList;


FlFlagList fl_parse_flags(const char *input);


void fl_free_flags(FlFlagList *list);


FlTimer fl_start_timer(void);


double fl_end_timer(FlTimer t);


FlTimer fl_profile_start(void);


void fl_profile_end(FlTimer timer);


void fl_run_after_time(double seconds, void (*callback)(void));


bool fl_string_contains(const char *haystack, const char *needle);


char *fl_get_working_dir(void);


char *fl_resolve_data_path(const char *name);


char *fl_resolve_feature_path(const char *name);


char *fl_resolve_user_path(void);


char *fl_get_user_name(void);


bool fl_is_root(void);


void fl_run_every(double seconds, void (*callback)(void));

typedef void *FlLibrary;


FlLibrary fl_library_open(const char *path);


void fl_library_register(const char *name, FlLibrary handle);


FlLibrary fl_library_get(const char *name);


void *fl_library_symbol(FlLibrary lib, const char *name);


void fl_library_close(FlLibrary lib);


const char *fl_library_error(void);

typedef void *FlThread;
typedef void *FlMutex;
typedef void *(*FlThreadFunc)(void *arg);


FlThread fl_thread_create(FlThreadFunc fn, void *arg);


void fl_thread_join(FlThread thread);


FlMutex fl_mutex_create(void);


void fl_mutex_destroy(FlMutex mutex);


void fl_mutex_lock(FlMutex mutex);


void fl_mutex_unlock(FlMutex mutex);


typedef struct {
  char *key;
  long value;
} FlHashMap;


void fl_hashmap_init(FlHashMap **map);


void fl_hashmap_free(FlHashMap **map);


void fl_hashmap_put(FlHashMap **map, const char *key, long value);


void fl_hashmap_add(FlHashMap **map, const char *key, long delta);


long fl_hashmap_get(FlHashMap *map, const char *key, long def);


bool fl_hashmap_has(FlHashMap *map, const char *key);


bool fl_hashmap_del(FlHashMap **map, const char *key);


size_t fl_hashmap_len(FlHashMap *map);


const FlHashMap *fl_hashmap_at(FlHashMap *map, size_t index);


FlWindowList fl_get_all_window_titles(void);


FlWindowList fl_get_all_window_names(void);


char *fl_get_active_window_title(void);


char *fl_get_active_window_app_id(void);


bool fl_is_window_active(char *title);


FlColor fl_get_pixel_color(FlPos pos);


bool fl_move_to_pos(FlPos pos);


bool fl_click_at_pos(FlPos pos);


bool fl_left_click(void);


bool fl_right_click(void);


bool fl_emit_mouse_button(int button, bool down);


bool fl_emit_mouse_button_at(FlPos pos, int button, bool down);


bool fl_click_button_at(FlPos pos, int button);


bool fl_mouse_scroll(int notches);


void fl_emit_key(int vk, bool down);

#define fl_move_mouse fl_move_to_pos
#define fl_click() fl_left_click()


bool fl_move_to_pos_rel(int rx, int ry);


bool fl_click_at_pos_rel(int rx, int ry);


bool fl_emit_mouse_button_at_rel(int rx, int ry, int button, bool down);


bool fl_click_button_at_rel(int rx, int ry, int button);


bool fl_type_text(const char *s);


void fl_sleep(double seconds);


bool fl_is_key_down(int vk);


bool fl_is_key_pressed(int vk);


bool fl_is_key_released(int vk);


bool fl_ctrl_pressed(void);


bool fl_shift_pressed(void);


bool fl_alt_pressed(void);


bool fl_super_pressed(void);


void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);


int fl_get_key_pressed(void);


char fl_get_char_pressed(void);

#define fl_key_pressed fl_is_key_pressed
#define fl_key_released fl_is_key_released
#define fl_ctrl_down fl_ctrl_pressed
#define fl_shift_down fl_shift_pressed
#define fl_alt_down fl_alt_pressed
#define fl_super_down fl_super_pressed
#define fl_is_ctrl_pressed fl_ctrl_pressed
#define fl_is_shift_pressed fl_shift_pressed
#define fl_is_alt_pressed fl_alt_pressed
#define fl_is_super_pressed fl_super_pressed


typedef struct {
  int vk;
  int scancode;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} FlKeyEvent;

#define FL_MOUSE_LEFT 1
#define FL_MOUSE_RIGHT 2
#define FL_MOUSE_MIDDLE 3
#define FL_MOUSE_SIDE1 4
#define FL_MOUSE_SIDE2 5
#define FL_MOUSE_BACK 6
#define FL_MOUSE_FORWARD 7
#define FL_MOUSE_WHEEL 8


typedef struct {
  int button;
  int x;
  int y;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} FlMouseEvent;


bool fl_poll_mouse(FlMouseEvent *out);


int fl_mouse_queue_len(void);


unsigned long fl_mouse_queue_dropped(void);


void fl_clear_mouse_queue(void);


FlPos fl_get_cursor_pos(void);

#define fl_get_mouse_pos() fl_get_cursor_pos()


bool fl_mouse_pressed(int button);


bool fl_poll_key(FlKeyEvent *out);


int fl_key_queue_len(void);


unsigned long fl_key_queue_dropped(void);


void fl_clear_key_queue(void);


int fl_char_for_key(int vk, int scancode, int mods);


typedef CcEntrySpec FlCcEntrySpec;


#define fl_add_to_cc(...) fl_add_to_cc_impl(&(FlCcEntrySpec){__VA_ARGS__})


bool fl_add_to_cc_impl(const FlCcEntrySpec *spec);

typedef struct {
  const char *alias;
  const char *target;
} FlAliasSpec;


#define fl_add_alias(...) fl_add_alias_impl(&(FlAliasSpec){__VA_ARGS__})


bool fl_add_alias_impl(const FlAliasSpec *spec);


const char *fl_alias_for(const char *target);


int fl_cc_entry_count(void);


const CcEntrySpec *fl_cc_entry_at(int index);


bool fl_add_dep_impl(const char *dep_name, const char *owner_file);


#define fl_add_dep(...) fl_add_dep_impl(__VA_ARGS__, __FILE__)


typedef struct {
  const char *hotkey;
  void (*cb)(void);
  bool (*should_swallow)(void);
  const char *whitelist[16];
  const char *blacklist[16];
  bool swallow;
} FlHotkeySpec;


#define fl_register_hotkey(...)                                                \
  fl_register_hotkey_impl(&(FlHotkeySpec){__VA_ARGS__})


bool fl_register_hotkey_impl(const FlHotkeySpec *spec);


typedef struct {
  const char *src;
  const char *dst;
  const char *whitelist[16];
  const char *blacklist[16];
} FlRemappingSpec;


#define fl_register_remapping(...)                                             \
  fl_register_remapping_impl(&(FlRemappingSpec){__VA_ARGS__})


bool fl_register_remapping_impl(const FlRemappingSpec *spec);


bool fl_replay_hotkey(int vk, int mods);

typedef void (*FlSnippetFunc)(void);


typedef struct {
  const char *trigger;
  const char *text;
  FlSnippetFunc fn;
  const char *whitelist[16];
  const char *blacklist[16];
} FlSnippetSpec;


#define fl_register_snippet(...)                                               \
  fl_register_snippet_impl(&(FlSnippetSpec){__VA_ARGS__})


bool fl_register_snippet_impl(const FlSnippetSpec *spec);

void fl_reload_feature(char *name);
void fl_reload_all_features(void);
int fl_get_pid();

#if defined(_WIN32)
#define FLINT_FEATURE_EXPORT __declspec(dllexport)
#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif


#define FLINT_FEATURE_INIT                                                     \
  FLINT_FEATURE_EXPORT __attribute__((constructor)) void FlintFeatureInit(void)

#define FLINT_FEATURE_SHUTDOWN                                                 \
  FLINT_FEATURE_EXPORT void FlintFeatureShutdown(void)


#define fl_cap_register(key, fn) fl_cap_register_abi((key), CAP_ABI_V1, (fn))


#define fl_cap_resolve(key) fl_cap_resolve_abi((key), CAP_ABI_V1)


#define FL_CAP_BIND(var, key, type)                                            \
  static type var##_fn;                                                        \
  static unsigned var##_gen;                                                   \
  type var = NULL;                                                             \
  if (var##_gen != fl_cap_generation()) {                                      \
    fl_cap_bind_declare(key, (void *)&var##_fn);                               \
    var##_fn = (type)fl_cap_resolve(key);                                      \
    var##_gen = fl_cap_generation();                                           \
  }                                                                            \
  var = var##_fn

#define FL_WHITE RGB(255, 255, 255)
#define FL_BLACK RGB(1, 1, 1)
#define FL_RED RGB(255, 0, 0)
#define FL_GREEN RGB(0, 255, 0)
#define FL_BLUE RGB(0, 0, 255)
#define FL_YELLOW RGB(255, 255, 0)
#define FL_CYAN RGB(0, 255, 255)
#define FL_MAGENTA RGB(255, 0, 255)
#define FL_ORANGE RGB(255, 165, 0)
#define FL_PURPLE RGB(128, 0, 128)
#define FL_PINK RGB(255, 192, 203)
#define FL_BROWN RGB(139, 69, 19)
#define FL_LIME RGB(50, 205, 50)
#define FL_TEAL RGB(0, 128, 128)
#define FL_NAVY RGB(0, 0, 128)
#define FL_MAROON RGB(128, 0, 0)
#define FL_OLIVE RGB(128, 128, 0)
#define FL_SILVER RGB(192, 192, 192)
#define FL_GOLD RGB(255, 215, 0)
#define FL_INDIGO RGB(75, 0, 130)
#define FL_VIOLET RGB(238, 130, 238)
#define FL_TURQUOISE RGB(64, 224, 208)
#define FL_SALMON RGB(250, 128, 114)
#define FL_CORAL RGB(255, 127, 80)
#define FL_CRIMSON RGB(220, 20, 60)
#define FL_CHOCOLATE RGB(210, 105, 30)
#define FL_GRAY RGB(128, 128, 128)
#define FL_GREY RGB(128, 128, 128)
#define FL_LIGHT_GRAY RGB(211, 211, 211)
#define FL_DARK_GRAY RGB(14, 14, 14)
#define FL_LIGHT_GREY RGB(211, 211, 211)
#define FL_DARK_GREY RGB(14, 14, 14)


FlMetrics *fl_get_metrics(void);

void *fl_get_active_prompt(void);

typedef PlMetrics FlMetrics;
FlMetrics *fl_get_metrics(void);

#define FL_EVENT_PROMPT_OPEN EVENT_PROMPT_OPEN
#define FL_EVENT_PROMPT_CLOSE EVENT_PROMPT_CLOSE

void fl_register_event(FlEvent event, FlEventCallback cb);
void fl_unregister_event(FlEvent event, FlEventCallback cb);

typedef bool (*FlKeyInterceptorFn)(const PopupKeyEvent *key);


void fl_register_key_interceptor(FlKeyInterceptorFn fn);

void fl_close_window();


#endif
