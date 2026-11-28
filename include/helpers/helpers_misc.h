#ifndef HELPERS_MISC_H
#define HELPERS_MISC_H

#include <stdbool.h>
#include <stddef.h>
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "helpers/helpers_text.h"

char *file_read_all_alloc(const char *path);
void resolve_window_list(const char *const *wl, const char *const *bl,
                         const char *const **out_windows, bool *out_whitelist);
bool setting_parse_bool(const char *v, bool *out);
bool setting_parse_int(const char *v, int *out);
bool setting_parse_float(const char *v, float *out);
int config_index_of(const void *entries, size_t count, size_t elem_size,
                    size_t key_off, size_t key_size, const char *key);

#define CONFIG_INDEX_OF(entries, count, type, field, key)                      \
  config_index_of((entries), (count), sizeof(type), offsetof(type, field),     \
                  sizeof(((type *)0)->field), (key))

bool canonical_path(const char *path, char *out, size_t cap);
bool paths_equal(const char *a, const char *b);
bool rel_to_abs(Pos pos, Pos *out);
bool pixel_color_matches_tol(RgbColor c, RgbColor target, int tolerance);
bool feature_shortcut_parse(const char *target, char *feature,
                            size_t feature_cap, char *filter,
                            size_t filter_cap);
void feature_shortcut_select(ListBox *lb, const char *filter);
PromptCallbacks feature_shortcut_make_callbacks(const PromptSpec *spec,
                                                const PromptCallbacks *saved);

PromptState *current_prompt_state(void);
unsigned long draw_color(unsigned long color);
bool resolve_caller_path_silent(void *caller_ret, const char *name, char *full,
                                size_t cap);
bool resolve_caller_path(void *caller_ret, const char *name, char *full,
                         size_t cap, const char *func);
bool resolve_icon_path(void *caller, const char *icon, char *out, size_t cap);
StringList load_file_list_from(void *caller, const char *filepath);
bool write_lines_from(void *caller, const char *filepath, const char *lines[],
                      int count);
bool filter_lines_from(void *caller, const char *path, const char *needle,
                       bool match_substr);
bool type_text_common(const char *s);
ImageBuffer image_take(ImageBuffer *img);
bool draw_rect_raw(const DrawRect *spec, unsigned long color, float thickness,
                   bool filled, bool transparent);

typedef struct {
  const char *name;
  char *buffer;
  size_t buffer_size;
  int x;
  int y;
  int w;
  int h;
  const char *placeholder;
  const char *initial_text;
  ColorRef color_bg;
  ColorRef color_text;
  FontHandle font;
  int font_size;
  int caret_h;
  int caret_w;
  int caret_x;
  int caret_y;
  int caret_pos;
  int text_y;
  bool auto_focus;
  bool hide_caret;
  bool primary;
  bool no_tab;
  bool wrapped;
} EditSpec;

bool place_editbox(const EditSpec *spec);

bool is_safe_process_name(const char *name);
bool lookup_ci_eq(const char *a, const char *b);
char *lookup_first_line(char *out);
bool path_next_entry(const char **cursor, char sep, char *out, size_t out_cap);
const char *compiler_find_usable(const char *const candidates[],
                                 bool (*probe)(const char *));
size_t compiler_argv_base(const char *cc, const char *argv[]);
void *lazy_mutex_get(void **slot);
bool svg_file_size(const char *path, int *out_w, int *out_h);

#endif
