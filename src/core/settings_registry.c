#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/context.h"
#include "core/window_manager.h"
#include "core/focus.h"
#include "core/scroll_area.h"
#include "core/search_index.h"
#include "core/draw_state.h"
#include "core/prompt_geometry.h"
#include "core/checkbox.h"
#include "core/slider.h"
#include "core/editbox.h"
#include "core/feature_path.h"
#include "core/types/file.h"
#include "core/listbox_hover.h"
#include "core/config_section.h"
#include "core/config_toml.h"
#include "config_values.h"
#include "deps/log.h"
#include "helpers/helpers_fs.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "platform/platform.h"
#include "platform/draw.h"
#include "core/settings_registry.h"

#define SETTINGS_CONFIG_KEY_MAX 384
#define SETTINGS_CONFIG_VALUE_MAX 1024

typedef struct {
  char key[SETTINGS_CONFIG_KEY_MAX];
  char value[SETTINGS_CONFIG_VALUE_MAX];
} SettingsConfigEntry;

static int settings_config_count = 0;
static SettingsConfigEntry settings_config_entries[256];
static char settings_config_path[1024];
static bool settings_config_loaded = false;
static void (*settings_config_notify)(const char *key, const char *value) = NULL;

void settings_config_set_notify(void (*notify)(const char *key,
                                               const char *value)) {
  settings_config_notify = notify;
}

static const char *settings_config_find(const char *key) {
  if (!key)
    return NULL;
  for (int i = 0; i < settings_config_count; i++)
    if (strcmp(settings_config_entries[i].key, key) == 0)
      return settings_config_entries[i].value;
  return NULL;
}

static bool settings_config_apply(const ConfigTomlEntry *entry, void *ud) {
  (void)ud;
  if (!entry || strcmp(entry->section, "settings") != 0)
    return true;
  char value[SETTINGS_CONFIG_VALUE_MAX];
  if (!config_toml_decode_string(entry->raw, value, sizeof(value)))
    return true;
  const char *existing = settings_config_find(entry->key);
  if (existing) {
    snprintf((char *)existing, SETTINGS_CONFIG_VALUE_MAX, "%s", value);
    return true;
  }
  if (settings_config_count >= 256)
    return true;
  SettingsConfigEntry *out = &settings_config_entries[settings_config_count++];
  snprintf(out->key, sizeof(out->key), "%s", entry->key);
  snprintf(out->value, sizeof(out->value), "%s", value);
  return true;
}

static void settings_config_init(void) {
  if (settings_config_loaded)
    return;
  settings_config_loaded = true;
  char dir[512];
  if (!data_dir(dir, sizeof(dir)))
    return;
  Platform_MkdirIfNotExists(dir);
  char sep = '/';
#ifdef _WIN32
  sep = '\\';
#endif
  snprintf(settings_config_path, sizeof(settings_config_path),
           "%s%ccore%cskald.toml", dir, sep, sep);
  config_toml_parse(settings_config_path, settings_config_apply, NULL);
}

static bool settings_config_has(const char *key) {
  settings_config_init();
  return settings_config_find(key) != NULL;
}

static bool settings_config_get_bool(const char *key) {
  settings_config_init();
  const char *value = settings_config_find(key);
  if (!value)
    return false;
  return !strcmp(value, "true") || !strcmp(value, "TRUE") ||
         !strcmp(value, "True") || !strcmp(value, "1") ||
         !strcmp(value, "yes") || !strcmp(value, "on");
}

static float settings_config_get_float(const char *key) {
  settings_config_init();
  const char *value = settings_config_find(key);
  if (!value || !*value)
    return 0.0f;
  char *end = NULL;
  float out = strtof(value, &end);
  return end && *end == '\0' ? out : 0.0f;
}

static char *settings_config_get(const char *key) {
  settings_config_init();
  const char *value = settings_config_find(key);
  return value ? strdup(value) : NULL;
}

static bool settings_config_set(const char *key, const char *value) {
  if (!key || !key[0] || !value)
    return false;
  settings_config_init();
  const char *existing = settings_config_find(key);
  if (existing)
    snprintf((char *)existing, SETTINGS_CONFIG_VALUE_MAX, "%s", value);
  else if (settings_config_count < 256) {
    SettingsConfigEntry *out = &settings_config_entries[settings_config_count++];
    snprintf(out->key, sizeof(out->key), "%s", key);
    snprintf(out->value, sizeof(out->value), "%s", value);
  }
  if (settings_config_notify)
    settings_config_notify(key, value);
  if (!settings_config_path[0])
    return true;
  return config_section_set(settings_config_path, "settings", key, value);
}

static bool settings_capturing(void) { return search_index_current() != NULL; }

static unsigned long settings_color(unsigned long color) {
  if (color == 0 || color == COLOR_AUTO)
    color = COLOR_TEXT;
  return rgb_to_hex(color);
}

static ColorRef settings_color_lerp(ColorRef a, ColorRef b, float t) {
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  int ar = (int)(a & 0xFF), ag = (int)((a >> 8) & 0xFF),
      ab = (int)((a >> 16) & 0xFF);
  int br = (int)(b & 0xFF), bg = (int)((b >> 8) & 0xFF),
      bb = (int)((b >> 16) & 0xFF);
  unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
  unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
  unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
  return (ColorRef)(rr | (rg << 8) | (rb << 16));
}

static int settings_text_width(const char *text, int font_size) {
  if (!text)
    return 0;
  return Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = text, .len = (int)strlen(text), .font_size = font_size});
}

static bool settings_draw_rect_local(const DrawRect *spec) {
  if (!spec)
    return false;
  if (settings_capturing())
    return true;
  return Platform_DrawRectRaw((DrawRectRawSpec){
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .color = settings_color(spec->color),
      .radius = spec->radius ? spec->radius : 0,
      .rounded = spec->rounded ? spec->rounded : RoundedCorners,
      .opaque = !spec->transparent,
      .thickness = spec->thickness,
      .filled = spec->filled,
      .angle = spec->angle});
}

typedef struct {
  const char *text;
  int x, y;
  ColorRef color;
  const char *align;
  int font_size;
} SettingsDrawTextSpec;

typedef struct {
  const char *text;
  int x, y, w, h;
  ColorRef color;
  int font_size;
} SettingsDrawTextCenteredSpec;

typedef struct {
  const char *path;
  int x, y, w, h;
} SettingsDrawImageSpec;

static bool settings_draw_text_local(const SettingsDrawTextSpec *spec) {
  if (!spec || !spec->text)
    return false;
  if (settings_capturing()) {
    if (spec->text[0])
      search_index_add_text(NULL, spec->text, SEARCH_KIND_TEXT, NULL, NULL);
    return true;
  }
  const char *align = spec->align ? spec->align : "LEFT";
  return Platform_DrawTextRaw((DrawTextRawSpec){.x = spec->x,
                                                .y = spec->y,
                                                .text = spec->text,
                                                .color = settings_color(
                                                    spec->color),
                                                .align = (char *)align,
                                                .font_size = spec->font_size});
}

static bool settings_draw_text_centered_local(
    const SettingsDrawTextCenteredSpec *spec) {
  if (!spec || !spec->text)
    return false;
  if (settings_capturing()) {
    if (spec->text[0])
      search_index_add_text(NULL, spec->text, SEARCH_KIND_TEXT, NULL, NULL);
    return true;
  }
  Platform_DrawTextCentered((DrawTextCenteredSpec){
      .x = spec->x,
      .y = spec->y,
      .w = spec->w,
      .h = spec->h,
      .text = spec->text,
      .color = settings_color(spec->color),
      .font_size = spec->font_size});
  return true;
}

static bool settings_draw_image_local(const SettingsDrawImageSpec *spec) {
  if (!spec || !spec->path || !spec->path[0])
    return false;
  if (settings_capturing())
    return true;
  char asset[MAX_PATH];
  const char *path = spec->path;
  if ((path[0] == '/' || path[0] == '\\') &&
      feature_path_resolve_asset_path(__builtin_return_address(0), path, asset,
                                      sizeof(asset)))
    path = asset;
  return Platform_DrawImageAt(path, spec->x, spec->y, spec->w, spec->h);
}

static bool settings_draw_line_local(const Line *spec) {
  if (!spec)
    return false;
  if (settings_capturing())
    return true;
  return Platform_DrawLineRaw((DrawLineRawSpec){.x1 = spec->x1,
                                                .y1 = spec->y1,
                                                .x2 = spec->x2,
                                                .y2 = spec->y2,
                                                .color = settings_color(
                                                    spec->color),
                                                .thickness = spec->thickness});
}

static const char *settings_editbox_text(const char *name) {
  PromptState *state = prompt_popup_get_top_prompt_state();
  if (!state)
    return NULL;
  if (name && name[0]) {
    PromptEditbox *entry = editbox_registry_find(state, name);
    return entry ? entry->eb.filter : NULL;
  }
  return state->eb ? state->eb->filter : NULL;
}

static bool settings_editbox_set_text(const char *name, const char *text) {
  if (!text)
    return false;
  PromptState *state = prompt_popup_get_top_prompt_state();
  if (!state)
    return false;
  EditBox *eb = NULL;
  if (name && name[0]) {
    PromptEditbox *entry = editbox_registry_find(state, name);
    if (!entry)
      return false;
    eb = &entry->eb;
  } else {
    eb = state->eb;
  }
  if (!eb)
    return false;
  snprintf(eb->filter, sizeof(eb->filter), "%s", text);
  eb->cursor = strlen(eb->filter);
  eb->sel_start = -1;
  eb->sel_end = -1;
  eb->scroll_offset = 0;
  editbox_registry_sync(state);
  return true;
}

typedef struct {
  const char *name;
  const char *text;
  int x, y, w, h;
  bool checked;
  bool button;
  void (*on_click)(void);
  void (*on_hover)(void);
} SettingsPlaceCheckboxSpec;

typedef struct {
  const char *name;
  int x, y, w, h;
  float min_value, max_value, step, value;
  void (*on_change)(float value);
  void (*on_hover)(void);
} SettingsPlaceSliderSpec;

typedef struct {
  const char *name;
  char *buffer;
  size_t buffer_size;
  int x, y, w, h;
  int font_size;
  int caret_h;
  const char *placeholder;
  bool no_tab;
} SettingsPlaceEditboxSpec;

static bool settings_place_checkbox_impl(const SettingsPlaceCheckboxSpec *spec,
                                         bool button) {
  if (!spec)
    return false;
  if (settings_capturing()) {
    const char *text = (spec->text && spec->text[0]) ? spec->text : spec->name;
    search_index_add_text(NULL, text,
                          button ? SEARCH_KIND_BUTTON : SEARCH_KIND_CHECKBOX,
                          spec->name, NULL);
    return true;
  }
  CheckboxSpec cb = {0};
  cb.name = spec->name;
  cb.text = (char *)spec->text;
  cb.x = spec->x;
  cb.y = spec->y;
  cb.w = spec->w;
  cb.h = spec->h;
  cb.checked = spec->checked;
  cb.button = button;
  cb.checkbox = !button;
  cb.border = true;
  cb.thickness = 1;
  cb.on_click = spec->on_click;
  cb.on_hover = spec->on_hover;
  if (spec->name && settings_focus_ctx_has(spec->name) &&
      spec->on_click == settings_dispatch_widget_click)
    cb.on_click = settings_dispatch_focus_click;
  return checkbox_place(&cb);
}

static bool settings_place_slider_impl(const SettingsPlaceSliderSpec *spec) {
  if (!spec)
    return false;
  if (settings_capturing()) {
    char value[32];
    snprintf(value, sizeof(value), "%g", spec->value);
    search_index_add_text(NULL, spec->name, SEARCH_KIND_SLIDER, spec->name,
                          value);
    return true;
  }
  SliderSpec sl = {0};
  sl.name = spec->name;
  sl.x = spec->x;
  sl.y = spec->y;
  sl.w = spec->w;
  sl.h = spec->h;
  sl.orientation = SLIDER_HORIZONTAL;
  sl.min_value = spec->min_value;
  sl.max_value = spec->max_value;
  sl.step = spec->step;
  sl.value = spec->value;
  sl.thickness = 4;
  sl.border = true;
  sl.on_input = spec->on_change;
  sl.on_change = spec->on_change;
  sl.on_hover = spec->on_hover;
  if (spec->name && settings_focus_ctx_has(spec->name) &&
      spec->on_change == settings_dispatch_slider_change)
    sl.on_change = settings_dispatch_focus_change;
  return slider_place(&sl);
}

static bool settings_place_editbox_impl(const SettingsPlaceEditboxSpec *spec) {
  if (!spec)
    return false;
  const char *name = spec->name;
  if (!name || !name[0])
    name = PROMPT_PRIMARY_EDITBOX_NAME;
  if (settings_capturing()) {
    if (strncmp(name, "settings-search:", 16) != 0)
      search_index_add_text(NULL, spec->buffer, SEARCH_KIND_EDITBOX, name,
                            NULL);
    return true;
  }
  SkaldContext_Get()->window.editbox_placed = true;
  PromptState *state = prompt_popup_get_top_prompt_state();
  int dx = 0, dy = 0;
  get_draw_offset(&dx, &dy);
  int idx = editbox_registry_place(state, name, spec->buffer, spec->buffer_size,
                                   spec->x + dx, spec->y + dy, spec->w,
                                   spec->h);
  if (idx < 0)
    return false;
  PromptEditbox *entry = &state->editboxes[idx];
  if (state->active_editbox < 0)
    state->active_editbox = idx;
  entry->no_tab = spec->no_tab;
  bool active = state->active_editbox == idx;
  EditBox *saved = state->eb;
  int saved_active = state->active_editbox;
  state->eb = &entry->eb;
  state->active_editbox = idx;

  EditboxPlace place;
  editbox_resolve_place(&(EditboxPlaceSpec){.buffer = entry->eb.filter,
                                            .x = spec->x,
                                            .y = spec->y + 2,
                                            .w = spec->w,
                                            .h = spec->h,
                                            .font_size = spec->font_size},
                        state, &place);
  int fs = place.font_size;
  int cursor_x = Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = place.buf, .len = place.cursor, .font_size = fs});
  int text_w = Platform_MeasureTextWidth((MeasureTextWidthSpec){
      .text = place.buf, .len = place.text_len, .font_size = fs});
  int line_h = Platform_GetTextLineHeight(fs);
  EditboxUiSpec ui = {.color_text = COLOR_TEXT,
                      .color_bg = COLOR_BG,
                      .placeholder = spec->placeholder};
  EditboxSpec eb;
  editbox_spec_from_place(
      &eb, &place, state, &ui,
      &(EditboxMetrics){.text_w = text_w,
                        .cursor_x = cursor_x,
                        .ascent = (line_h * 3) / 4,
                        .descent = line_h - (line_h * 3) / 4,
                        .font = FH_NULL});
  eb.caret_h = spec->caret_h;
  int y_off = Platform_EditboxPlaceholderOffset();
  eb.text_y += y_off;
  eb.placeholder_dy = 0;
  eb.caret_y += y_off;
  int parts = active ? EDITBOX_PART_ALL
                     : (EDITBOX_PART_TINT | EDITBOX_PART_TEXT);
  EditboxLayout lay;
  editbox_layout(&eb, &lay);
  editbox_draw(&eb, &lay, parts);
  entry->eb.scroll_offset = eb.scroll;
  state->active_editbox = saved_active;
  state->eb = saved;
  return true;
}

#define settings_place_checkbox(...)                                           \
  settings_place_checkbox_impl(&(SettingsPlaceCheckboxSpec){__VA_ARGS__}, false)
#define settings_place_button(...)                                             \
  settings_place_checkbox_impl(                                                 \
      &(SettingsPlaceCheckboxSpec){.button = true, __VA_ARGS__}, true)
#define settings_place_slider(...)                                             \
  settings_place_slider_impl(&(SettingsPlaceSliderSpec){__VA_ARGS__})
#define settings_place_editbox(...)                                            \
  settings_place_editbox_impl(                                                 \
      &(SettingsPlaceEditboxSpec){.no_tab = true, __VA_ARGS__})
#define settings_draw_rect(...)                                                \
  settings_draw_rect_local(&(DrawRect){__VA_ARGS__})
#define settings_draw_text(...)                                                \
  settings_draw_text_local(&(SettingsDrawTextSpec){__VA_ARGS__})
#define settings_draw_text_centered(...)                                       \
  settings_draw_text_centered_local(&(SettingsDrawTextCenteredSpec){__VA_ARGS__})
#define settings_draw_image(...)                                               \
  settings_draw_image_local(&(SettingsDrawImageSpec){__VA_ARGS__})
#define settings_draw_line(...) settings_draw_line_local(&(Line){__VA_ARGS__})

#define FOCUS_CTX_MAX 512
typedef struct {
  char name[256];
  SettingsEntry *entry;
  int index;
  int option;
} FocusBlockCtx;

static FocusBlockCtx s_focus_ctx[FOCUS_CTX_MAX];
static int s_focus_ctx_count = 0;

static FocusBlockCtx *focus_ctx_find(const char *name) {
  if (!name || !name[0])
    return NULL;
  for (int i = 0; i < s_focus_ctx_count; i++) {
    if (strcmp(s_focus_ctx[i].name, name) == 0)
      return &s_focus_ctx[i];
  }
  return NULL;
}

static void focus_ctx_remember(const char *name, SettingsEntry *entry,
                               int index, int option) {
  if (!name || !name[0])
    return;
  FocusBlockCtx *ctx = focus_ctx_find(name);
  if (!ctx) {
    if (s_focus_ctx_count >= FOCUS_CTX_MAX)
      return;
    ctx = &s_focus_ctx[s_focus_ctx_count++];
    snprintf(ctx->name, sizeof(ctx->name), "%s", name);
  }
  ctx->entry = entry;
  ctx->index = index;
  ctx->option = option;
}

bool settings_focus_ctx_has(const char *name) {
  return focus_ctx_find(name) != NULL;
}

#define SETTINGS_MAX_ENTRIES 128
#define SETTINGS_MAX_BLOCKS 128

#define SETTINGS_BLOCK_CHECKBOX 1
#define SETTINGS_BLOCK_BUTTON 2
#define SETTINGS_BLOCK_SLIDER 3
#define SETTINGS_BLOCK_DROPDOWN 4
#define SETTINGS_BLOCK_EDITBOX 5

#define SETTINGS_EDITBOX_MAX_FIELDS 8
#define SETTINGS_EDITBOX_KEY_MAX 192

#define SETTINGS_ROW_X 10
#define SETTINGS_ROW_W_PAD 20
#define SETTINGS_ROW_EDGE_PAD 10
#define SETTINGS_EDITBOX_BASE_X 360
#define SETTINGS_EDITBOX_FIELD_GAP 30

#define SETTINGS_VIEW_TOP 260
#define SETTINGS_ROW_START SETTINGS_VIEW_TOP
#define SETTINGS_ROW_STEP 70
#define SETTINGS_ROW_LEAD (SETTINGS_ROW_START - SETTINGS_VIEW_TOP)

static int settings_view_height(int base_off_y) {
  int h = PromptH - base_off_y - SETTINGS_VIEW_TOP;
  if (h < SETTINGS_ROW_STEP)
    h = SETTINGS_ROW_STEP;
  return (h / SETTINGS_ROW_STEP) * SETTINGS_ROW_STEP;
}

#define SETTINGS_DROPDOWN_MAX_VISIBLE 6
#define SETTINGS_DROPDOWN_OPT_H 28
#define SETTINGS_DROPDOWN_OPT_GAP 2
#define SETTINGS_DROPDOWN_OPT_PAD 30
#define SETTINGS_DROPDOWN_OPT_MIN_W 64

typedef struct {
  char name[64];
  char label[128];
  char key[SETTINGS_EDITBOX_KEY_MAX];
  char *buffer;
  size_t buffer_size;
  int width;
  char last_value[512];
} SettingsEditboxFieldBlock;

typedef struct {
  unsigned char type;
  char title[128];
  char sub[256];
  char label[64];
  bool initial;
  float min_value;
  float max_value;
  float value;
  SettingsDropdownOptionsFn refresh_options;
  int selected;
  bool open;
  int dropdown_scroll_y;
  void (*on_click)(void);
  void (*on_change)(void);
  void (*on_hover)(void);
  SettingsDropdownSelectFn on_select;
  void *ud;
  SettingsEditboxFieldBlock fields[SETTINGS_EDITBOX_MAX_FIELDS];
  int field_count;
  int x_offset;
  SettingsEditboxChangeFn on_edit_change;
} SettingsBlock;

struct SettingsEntry {
  SettingsEntrySpec pub;
  char name[256];
  char icon[1024];
  char owner[OWNER_PATH_MAX];
  char desc[512];
  int block_count;
  int scroll_y;
  char search_filter[128];
  char last_search_filter[128];
  bool search_capturing;
  SearchIndex *search;
  SettingsBlock blocks[SETTINGS_MAX_BLOCKS];
};

static int settings_entry_total = 0;
static SettingsEntry settings_entries[SETTINGS_MAX_ENTRIES];

static void st_dup(char *dst, size_t cap, const char *src) {
  snprintf(dst, cap, "%s", src ? src : "");
}

static void settings_entry_builtin_render(SettingsEntry *entry, void *ud);

static void settings_entry_capture_render(void *ud) {
  SettingsEntry *entry = (SettingsEntry *)ud;
  if (!entry)
    return;
  char saved[sizeof(entry->search_filter)];
  memcpy(saved, entry->search_filter, sizeof(saved));
  entry->search_filter[0] = '\0';
  entry->search_capturing = true;
  settings_entry_builtin_render(entry, NULL);
  entry->search_capturing = false;
  memcpy(entry->search_filter, saved, sizeof(saved));
}

static SettingsEntry *s_settings_scroll_entry = NULL;

SettingsEntry *settings_entry_add(const SettingsEntrySpec *spec) {
  if (!spec || !spec->name || !spec->name[0])
    return NULL;
  int slot = -1;
  for (int i = 0; i < settings_entry_total; i++) {
    if (settings_entries[i].name[0] &&
        strcmp(settings_entries[i].name, spec->name) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    if (settings_entry_total >= SETTINGS_MAX_ENTRIES)
      return NULL;
    slot = settings_entry_total++;
  }
  SettingsEntry *e = &settings_entries[slot];
  search_index_destroy(e->search);
  memset(e, 0, sizeof(*e));
  st_dup(e->name, sizeof(e->name), spec->name);
  st_dup(e->desc, sizeof(e->desc), spec->desc ? spec->desc : "");
  st_dup(e->icon, sizeof(e->icon), spec->icon);
  st_dup(e->owner, sizeof(e->owner), spec->owner);
  e->pub.name = e->name;
  e->pub.icon = e->icon;
  e->pub.desc = e->desc;
  e->pub.render = spec->render ? spec->render : settings_entry_builtin_render;
  e->pub.ud = spec->ud;
  return e;
}

static SettingsEntry *settings_entry_resolve(SettingsEntry *e) {
  if (!e)
    return NULL;
  if (e < settings_entries || e >= settings_entries + SETTINGS_MAX_ENTRIES)
    return NULL;
  return e;
}

static SettingsBlock *settings_block_at(SettingsEntry *entry, int index) {
  SettingsEntry *e = settings_entry_resolve(entry);
  if (!e || index < 0 || index >= e->block_count)
    return NULL;
  return &e->blocks[index];
}

static int dropdown_options(const SettingsBlock *b,
                            const char ***out_options) {
  if (!b) {
    if (out_options)
      *out_options = NULL;
    return 0;
  }
  if (out_options)
    *out_options = NULL;
  if (!b->refresh_options)
    return 0;
  int count = b->refresh_options(out_options);
  return count > 0 ? count : 0;
}

static int dropdown_option_width(const char *text) {
  int w = settings_text_width(text ? text : "", FontSize) +
          SETTINGS_DROPDOWN_OPT_PAD;
  return w < SETTINGS_DROPDOWN_OPT_MIN_W ? SETTINGS_DROPDOWN_OPT_MIN_W : w;
}

static bool settings_block_title_ok(const char *title) {
  return title && title[0];
}

static int settings_entry_add_block_typed(SettingsEntry *entry, int type,
                                          const char *title, const char *sub,
                                          void *anchor) {
  SettingsEntry *e = settings_entry_resolve(entry);
  if (!e || !settings_block_title_ok(title))
    return -1;
  if (e->block_count >= SETTINGS_MAX_BLOCKS)
    return -1;
  if (e->owner[0] == '\0' && anchor)
    owner_resolve_for_func(anchor, e->owner, sizeof(e->owner));
  SettingsBlock *b = &e->blocks[e->block_count];
  memset(b, 0, sizeof(*b));
  b->type = (unsigned char)type;
  snprintf(b->title, sizeof(b->title), "%s", title);
  snprintf(b->sub, sizeof(b->sub), "%s", sub ? sub : "");
  return e->block_count++;
}

int settings_entry_add_checkbox(SettingsEntry *entry,
                                    const SettingsCheckboxSpec *spec) {
  if (!spec)
    return -1;
  int idx = settings_entry_add_block_typed(entry, SETTINGS_BLOCK_CHECKBOX,
                                           spec->title, spec->sub,
                                           __builtin_return_address(0));
  SettingsBlock *b = settings_block_at(entry, idx);
  if (!b)
    return -1;
  b->initial = spec->initial;
  b->on_click = spec->on_click;
  b->on_hover = spec->on_hover;
  {
    char key[SETTINGS_CONFIG_KEY_MAX];
    SettingsEntry *e = settings_entry_resolve(entry);
    if (e) {
      snprintf(key, sizeof(key), "%s.%s", e->name, b->title);
      if (settings_config_has(key))
        b->initial = settings_config_get_bool(key);
    }
  }
  return idx;
}

int settings_entry_add_button(SettingsEntry *entry,
                                  const SettingsButtonSpec *spec) {
  if (!spec)
    return -1;
  int idx =
      settings_entry_add_block_typed(entry, SETTINGS_BLOCK_BUTTON, spec->title,
                                     spec->sub, __builtin_return_address(0));
  SettingsBlock *b = settings_block_at(entry, idx);
  if (!b)
    return -1;
  snprintf(b->label, sizeof(b->label), "%s", spec->label ? spec->label : "");
  b->on_click = spec->on_click;
  b->on_hover = spec->on_hover;
  return idx;
}

int settings_entry_add_slider(SettingsEntry *entry,
                                  const SettingsSliderSpec *spec) {
  if (!spec)
    return -1;
  float min_value = spec->min_value;
  float max_value = spec->max_value;
  if (min_value == 0.0f && max_value == 0.0f && spec->initial == 0.0f) {
    min_value = 0.0f;
    max_value = 100.0f;
  } else if (max_value == min_value) {
    log_error("settings slider '%s' has empty range (%g..%g), using 0..100",
              spec->title ? spec->title : "", (double)min_value,
              (double)max_value);
    min_value = 0.0f;
    max_value = 100.0f;
  } else if (min_value > max_value) {
    float t = min_value;
    min_value = max_value;
    max_value = t;
  }
  float initial = spec->initial;
  if (initial < min_value)
    initial = min_value;
  if (initial > max_value)
    initial = max_value;
  int idx =
      settings_entry_add_block_typed(entry, SETTINGS_BLOCK_SLIDER, spec->title,
                                     spec->sub, __builtin_return_address(0));
  SettingsBlock *b = settings_block_at(entry, idx);
  if (!b)
    return -1;
  b->min_value = min_value;
  b->max_value = max_value;
  b->value = initial;
  b->on_change = spec->on_change;
  b->on_hover = spec->on_hover;
  {
    char key[SETTINGS_CONFIG_KEY_MAX];
    SettingsEntry *e = settings_entry_resolve(entry);
    if (e) {
      snprintf(key, sizeof(key), "%s.%s", e->name, b->title);
      if (settings_config_has(key)) {
        float seeded = settings_config_get_float(key);
        if (seeded < min_value)
          seeded = min_value;
        if (seeded > max_value)
          seeded = max_value;
        b->value = seeded;
      }
    }
  }
  return idx;
}

int settings_entry_add_dropdown(SettingsEntry *entry,
                                    const SettingsDropdownSpec *spec) {
  if (!spec)
    return -1;
  const char **options = NULL;
  int count = 0;
  if (spec->refresh_options) {
    count = spec->refresh_options(&options);
    if (count < 0)
      count = 0;
  }
  int selected = spec->initial >= 0 ? spec->initial : (count > 0 ? 0 : -1);
  if (selected >= count)
    selected = count > 0 ? 0 : -1;
  int idx = settings_entry_add_block_typed(entry, SETTINGS_BLOCK_DROPDOWN,
                                           spec->title, spec->sub,
                                           __builtin_return_address(0));
  SettingsBlock *b = settings_block_at(entry, idx);
  if (!b)
    return -1;
  b->refresh_options = spec->refresh_options;
  b->selected = selected;
  b->on_select = spec->on_select;
  b->on_hover = spec->on_hover;
  b->ud = spec->ud;
  {
    char key[SETTINGS_CONFIG_KEY_MAX];
    SettingsEntry *e = settings_entry_resolve(entry);
    if (e) {
      snprintf(key, sizeof(key), "%s.%s", e->name, b->title);
      char *saved = settings_config_get(key);
      if (saved) {
        for (int i = 0; i < count; i++)
          if (options && options[i] && strcmp(saved, options[i]) == 0) {
            b->selected = i;
            break;
          }
        free(saved);
      }
    }
  }
  return idx;
}

static SettingsBlock *
settings_editbox_group_block(SettingsEditboxGroup group) {
  if (group.entry < 0 || group.entry >= settings_entry_total || group.block < 0)
    return NULL;
  SettingsEntry *e = &settings_entries[group.entry];
  SettingsBlock *b = settings_block_at(e, group.block);
  if (!b || b->type != SETTINGS_BLOCK_EDITBOX)
    return NULL;
  return b;
}

static SettingsEditboxFieldBlock *
settings_editbox_field_find(SettingsBlock *b, const char *name) {
  if (!b || b->type != SETTINGS_BLOCK_EDITBOX || !name || !name[0])
    return NULL;
  for (int i = 0; i < b->field_count; i++)
    if (strcmp(b->fields[i].name, name) == 0)
      return &b->fields[i];
  return NULL;
}

static void settings_editbox_field_copy(char *dst, size_t dst_cap,
                                        const char *src, size_t src_cap) {
  size_t n = 0;
  if (src && src_cap > 0) {
    n = strnlen(src, src_cap - 1);
    memcpy(dst, src, n);
  }
  if (n >= dst_cap)
    n = dst_cap - 1;
  dst[n] = '\0';
}

SettingsEditboxGroup
settings_entry_add_editbox(SettingsEntry *entry,
                               const SettingsEditboxSpec *spec) {
  SettingsEditboxGroup invalid = SETTINGS_EDITBOX_INVALID;
  if (!spec || !spec->fields || spec->field_count < 1 ||
      spec->field_count > SETTINGS_EDITBOX_MAX_FIELDS)
    return invalid;
  for (int i = 0; i < spec->field_count; i++) {
    if (!spec->fields[i].name || !spec->fields[i].name[0] ||
        !spec->fields[i].buffer || spec->fields[i].buffer_size < 2)
      return invalid;
  }
  int idx = settings_entry_add_block_typed(
      entry, SETTINGS_BLOCK_EDITBOX, spec->title ? spec->title : "Editbox",
      spec->sub, __builtin_return_address(0));
  if (idx < 0)
    return invalid;
  SettingsEntry *e = settings_entry_resolve(entry);
  if (!e)
    return invalid;
  SettingsBlock *b = settings_block_at(e, idx);
  if (!b)
    return invalid;
  b->field_count = spec->field_count;
  b->x_offset = spec->x_offset;
  b->on_edit_change = spec->on_change;
  for (int i = 0; i < spec->field_count; i++) {
    const SettingsEditboxField *src = &spec->fields[i];
    SettingsEditboxFieldBlock *dst = &b->fields[i];
    snprintf(dst->name, sizeof(dst->name), "%s", src->name);
    snprintf(dst->label, sizeof(dst->label), "%s",
             src->label ? src->label : src->name);
    const char *prefix = e->owner[0] ? e->owner : e->name;
    size_t key_len = strnlen(prefix, sizeof(dst->key) - 1);
    memcpy(dst->key, prefix, key_len);
    if (key_len + 1 < sizeof(dst->key)) {
      dst->key[key_len++] = '/';
      size_t name_len = strnlen(src->name, sizeof(dst->key) - key_len - 1);
      memcpy(dst->key + key_len, src->name, name_len);
      key_len += name_len;
    }
    dst->key[key_len] = '\0';
    dst->buffer = src->buffer;
    dst->buffer_size = src->buffer_size;
    dst->width = src->width > 0 ? src->width : 60;
    settings_editbox_field_copy(dst->last_value, sizeof(dst->last_value),
                                dst->buffer, dst->buffer_size);
  }
  SettingsEditboxGroup group = {(int)(e - settings_entries), idx};
  return group;
}

const char *settings_editbox_get(SettingsEditboxGroup group,
                                    const char *name) {
  SettingsBlock *b = settings_editbox_group_block(group);
  SettingsEditboxFieldBlock *f = settings_editbox_field_find(b, name);
  if (!f)
    return NULL;
  const char *live = settings_editbox_text(f->key);
  return live ? live : f->buffer;
}

bool settings_editbox_set(SettingsEditboxGroup group, const char *name,
                             const char *value) {
  if (!value)
    return false;
  SettingsBlock *b = settings_editbox_group_block(group);
  SettingsEditboxFieldBlock *f = settings_editbox_field_find(b, name);
  if (!f)
    return false;
  if (settings_editbox_set_text(f->key, value))
    settings_editbox_field_copy(f->last_value, sizeof(f->last_value), value,
                                strlen(value) + 1);
  safe_copy(f->buffer, f->buffer_size, value);
  settings_editbox_field_copy(f->last_value, sizeof(f->last_value), f->buffer,
                              f->buffer_size);
  return true;
}

int settings_editbox_right_offset(int pane_w, int width) {
  int row_w = pane_w - SETTINGS_ROW_W_PAD;
  int right = SETTINGS_ROW_X + row_w - SETTINGS_ROW_EDGE_PAD;
  int base = SETTINGS_ROW_X + SETTINGS_EDITBOX_BASE_X;
  int offset = right - width - base;
  return offset > 0 ? offset : 0;
}

static int settings_editbox_auto_offset(const SettingsBlock *b, int pane_w) {
  int total = 0;
  for (int i = 0; i < b->field_count; i++)
    total += b->fields[i].width + (i ? SETTINGS_EDITBOX_FIELD_GAP : 0);
  return settings_editbox_right_offset(pane_w, total);
}

int settings_entry_count(void) { return settings_entry_total; }

void settings_scroll_to_block(const char *entry_name, int block_index) {
  if (!entry_name || block_index < 0)
    return;
  SettingsEntry *entry = NULL;
  for (int i = 0; i < settings_entry_total; i++) {
    if (strcmp(settings_entries[i].name, entry_name) == 0) {
      entry = &settings_entries[i];
      break;
    }
  }
  if (!entry || block_index >= entry->block_count)
    return;
  int view_h = settings_view_height(0);
  int max_scroll =
      SETTINGS_ROW_LEAD + entry->block_count * SETTINGS_ROW_STEP - view_h;
  if (max_scroll < 0)
    max_scroll = 0;
  int want = SETTINGS_ROW_START + block_index * SETTINGS_ROW_STEP -
             SETTINGS_VIEW_TOP - SETTINGS_ROW_STEP;
  if (want < 0)
    want = 0;
  if (want > max_scroll)
    want = max_scroll;
  entry->scroll_y = want;
}

const SettingsEntrySpec *settings_entry_at(int index) {
  if (index < 0 || index >= settings_entry_total)
    return NULL;
  return &settings_entries[index].pub;
}

void settings_registry_purge_owner(const char *path_a, const char *path_b) {
  if (!path_a && !path_b)
    return;
  for (int i = 0; i < settings_entry_total; i++) {
    search_index_destroy(settings_entries[i].search);
    settings_entries[i].search = NULL;
  }
  int out = 0;
  for (int i = 0; i < settings_entry_total; i++) {
    if (owner_module_matches(settings_entries[i].owner, path_a, path_b))
      continue;
    if (out != i) {
      settings_entries[out] = settings_entries[i];
      settings_entries[out].pub.name = settings_entries[out].name;
      settings_entries[out].pub.icon = settings_entries[out].icon;
      settings_entries[out].pub.desc = settings_entries[out].desc;
    }
    out++;
  }
  for (int i = out; i < settings_entry_total; i++)
    memset(&settings_entries[i], 0, sizeof(settings_entries[i]));
  settings_entry_total = out;
  s_focus_ctx_count = 0;
}

static SettingsEntry *s_dispatch_entry = NULL;
static int s_dispatch_index = -1;
static int s_dispatch_option = -1;

static SettingsBlock *dispatch_block(void) {
  if (!s_dispatch_entry || s_dispatch_index < 0)
    return NULL;
  return &s_dispatch_entry->blocks[s_dispatch_index];
}

static void persist_block_value(const SettingsEntry *e,
                                const SettingsBlock *b, const char *value) {
  char key[SETTINGS_CONFIG_KEY_MAX];
  snprintf(key, sizeof(key), "%s.%s", e->name, b->title);
  settings_config_set(key, value);
}

void settings_dispatch_widget_click(void) {
  SettingsEntry *e = s_dispatch_entry;
  SettingsBlock *b = dispatch_block();
  if (!b)
    return;
  if (b->type == SETTINGS_BLOCK_DROPDOWN) {
    if (s_dispatch_option < 0) {
      bool open = !b->open;
      for (int i = 0; i < e->block_count; i++)
        if (e->blocks[i].type == SETTINGS_BLOCK_DROPDOWN)
          e->blocks[i].open = false;
      b->open = open;
      if (open) {
        b->dropdown_scroll_y = 0;
        int view_h = settings_view_height(0);
        int max_scroll =
            SETTINGS_ROW_LEAD + e->block_count * SETTINGS_ROW_STEP - view_h;
        if (max_scroll < 0)
          max_scroll = 0;
        int want_scroll = SETTINGS_ROW_START +
                          s_dispatch_index * SETTINGS_ROW_STEP -
                          SETTINGS_VIEW_TOP;
        if (want_scroll < 0)
          want_scroll = 0;
        if (want_scroll > max_scroll)
          want_scroll = max_scroll;
        e->scroll_y = want_scroll;
      }
      return;
    }
    const char **options = NULL;
    int count = dropdown_options(b, &options);
    if (s_dispatch_option >= count)
      return;
    b->selected = s_dispatch_option;
    b->open = false;
    const char *option =
        options && options[b->selected] ? options[b->selected] : NULL;
    if (option)
      persist_block_value(e, b, option);
    if (b->on_select)
      b->on_select(option, b->selected, b->ud);
    return;
  }
  if (b->type == SETTINGS_BLOCK_CHECKBOX) {
    bool checked = checkbox_is_checked(b->title, PLACE_CHECKBOX);
    b->initial = checked;
    persist_block_value(e, b, checked ? "true" : "false");
  }
  if (b->on_click)
    b->on_click();
}

void settings_dispatch_focus_click(void) {
  FocusBlockCtx *ctx = focus_ctx_find(focus_name());
  if (!ctx) {
    log_error("focus: no settings context for '%s'",
              focus_name() ? focus_name() : "(null)");
    return;
  }
  SettingsEntry *saved_entry = s_dispatch_entry;
  int saved_index = s_dispatch_index;
  int saved_option = s_dispatch_option;
  s_dispatch_entry = ctx->entry;
  s_dispatch_index = ctx->index;
  s_dispatch_option = ctx->option;
  settings_dispatch_widget_click();
  s_dispatch_entry = saved_entry;
  s_dispatch_index = saved_index;
  s_dispatch_option = saved_option;
}

void settings_dispatch_focus_change(float value) {
  FocusBlockCtx *ctx = focus_ctx_find(focus_name());
  if (!ctx) {
    log_error("focus: no settings context for '%s'",
              focus_name() ? focus_name() : "(null)");
    return;
  }
  SettingsEntry *saved_entry = s_dispatch_entry;
  int saved_index = s_dispatch_index;
  s_dispatch_entry = ctx->entry;
  s_dispatch_index = ctx->index;
  settings_dispatch_slider_change(value);
  s_dispatch_entry = saved_entry;
  s_dispatch_index = saved_index;
}

static void dispatch_widget_hover(void) {
  SettingsBlock *b = dispatch_block();
  if (!b)
    return;
  if (b->on_hover)
    b->on_hover();
}

void settings_dispatch_slider_change(float value) {
  SettingsEntry *e = s_dispatch_entry;
  SettingsBlock *b = dispatch_block();
  if (!b)
    return;
  b->value = value;
  {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", value);
    persist_block_value(e, b, buf);
  }
  if (b->on_change)
    b->on_change();
}

static void settings_entry_builtin_render(SettingsEntry *entry, void *ud) {
  (void)ud;
  if (!entry)
    return;
  bool capturing = search_index_current() != NULL;
  char search_name[320];
  snprintf(search_name, sizeof(search_name), "settings-search:%s", entry->name);
  if (!capturing) {
    const char *live = settings_editbox_text(search_name);
    if (live && strcmp(live, entry->search_filter) != 0)
      snprintf(entry->search_filter, sizeof(entry->search_filter), "%s", live);
  }
  bool scoped_search =
      !capturing && !entry->search_capturing && entry->search_filter[0];
  bool block_match[SETTINGS_MAX_BLOCKS];
  memset(block_match, 0, sizeof(block_match));
  int match_count = entry->block_count;
  if (scoped_search) {
    if (!entry->search)
      entry->search = search_index_create();
    if (strcmp(entry->search_filter, entry->last_search_filter) != 0) {
      snprintf(entry->last_search_filter, sizeof(entry->last_search_filter),
               "%s", entry->search_filter);
      entry->scroll_y = 0;
    }
    search_index_reindex(entry->search, settings_entry_capture_render, entry);
    int hits = search_index_query(entry->search, entry->search_filter);
    match_count = 0;
    for (int h = 0; h < hits; h++) {
      const SearchHit *hit = search_index_hit_at(entry->search, h);
      if (!hit || hit->source_id < 0 || hit->source_id >= entry->block_count)
        continue;
      if (!block_match[hit->source_id]) {
        block_match[hit->source_id] = true;
        match_count++;
      }
    }
  }
  int pane_w = PromptW - draw_state_get()->off_x;
  if (pane_w < 240)
    pane_w = 240;
  int cursor = SETTINGS_ROW_START;
  SettingsBlock *open_block = NULL;
  SettingsEntry *open_entry = NULL;
  int open_index = -1;
  int open_x = 0, open_trigger_y = 0, open_w = 0;
  const char **open_options = NULL;
  int open_count = 0;

  settings_draw_rect(.x = -7, .y = -10, .w = pane_w + 24, .h = 220,
               .color = settings_color_lerp(COLOR_BG, RGB(0, 0, 0), 0.42f),
               .filled = true, .rounded = false);

  if (entry->icon[0])
    settings_draw_image(.path = entry->icon, .x = pane_w / 2 - 64, .y = 10, .w = 128,
                  .h = 128);
  if (entry->desc[0])
    settings_draw_text_centered(.x = 30, .y = 138, .w = pane_w - 60, .h = 36,
                          .text = entry->desc,
                          .color =
                              color_lerp(COLOR_BG, RGB(255, 255, 255), 0.48f));
  settings_draw_rect(.x=5,.y=218,.w=200,.h=28,.color=settings_color_lerp(COLOR_BG,RGB(0,0,0),0.5f),.filled=true,.radius=9);
  settings_place_editbox(.name = search_name, .x = 5, .y = 218, .w = 200,
                   .h = 28, .font_size = 14, .caret_h = 16,
                   .buffer = entry->search_filter,
                   .buffer_size = sizeof(entry->search_filter),
                   .placeholder = "Search this section", .no_tab = false);
  settings_draw_line(.x1 = -7, .y1 = 210, .x2 = pane_w + 17, .y2 = 210,
               .color = COLOR_BORDER);

  if (!capturing && s_settings_scroll_entry != entry) {
    s_settings_scroll_entry = entry;
    entry->scroll_y = 0;
  }
  int base_off_x = 0, base_off_y = 0;
  get_draw_offset(&base_off_x, &base_off_y);
  bool scrolled = false;
  int view_top = SETTINGS_VIEW_TOP;
  if (!capturing) {
    int view_h = settings_view_height(base_off_y);
    char scroll_name[300];
    snprintf(scroll_name, sizeof(scroll_name), "settings:%s", entry->name);
    ScrollAreaSpec scroll_spec = {
        .name = scroll_name,
        .x = 0,
        .y = SETTINGS_VIEW_TOP,
        .w = pane_w,
        .h = view_h,
        .content_w = 0,
        .content_h = SETTINGS_ROW_LEAD + match_count * SETTINGS_ROW_STEP,
        .off_y = &entry->scroll_y,
        .wheel_px = SETTINGS_ROW_STEP,
        .owner = entry->owner,
    };
    scrolled = scroll_area_begin(&scroll_spec);
    view_top = SETTINGS_VIEW_TOP + (scrolled ? entry->scroll_y : 0);
  }

  for (int i = 0; i < entry->block_count; i++) {
    SettingsBlock *b = &entry->blocks[i];
    if (scoped_search && !block_match[i])
      continue;
    if (!capturing && cursor < view_top) {
      cursor += SETTINGS_ROW_STEP;
      continue;
    }
    if (!capturing && cursor >= PromptH - draw_state_get()->off_y)
      break;
    if (capturing)
      search_index_push_context(NULL, i, b->title);
    s_dispatch_entry = entry;
    s_dispatch_index = i;
    focus_ctx_remember(b->title, entry, i, -1);
    int x = SETTINGS_ROW_X, y = cursor, w = pane_w - SETTINGS_ROW_W_PAD;
    settings_draw_rect(.x = x, .y = y, .w = w, .h = 60,
                 .color = color_lerp(COLOR_BG, RGB(205, 205, 205), 0.05f),
                 .filled = true, .radius = 6);
    if (b->title[0])
      settings_draw_text(.text = b->title, .x = x + 10, .y = y + 8,
                   .color = color_lerp(COLOR_BG, RGB(255, 255, 255), 0.82f),
                   .align = "LEFT");
    if (b->sub[0])
      settings_draw_text(.text = b->sub, .x = x + 10, .y = y + 30,
                   .color = color_lerp(COLOR_BG, RGB(255, 255, 255), 0.48f),
                   .align = "LEFT");
    int right = x + w - SETTINGS_ROW_EDGE_PAD;
    if (b->type == SETTINGS_BLOCK_CHECKBOX) {
      settings_place_checkbox(.name = b->title, .x = right - 32, .y = y + 14, .w = 32,
                        .h = 32, .checked = b->initial,
                        .on_click = settings_dispatch_widget_click,
                        .on_hover = dispatch_widget_hover);
    } else if (b->type == SETTINGS_BLOCK_BUTTON) {
      int bw = settings_text_width(b->label, FontSize) + 28;
      if (bw < 64)
        bw = 64;
      settings_place_button(.name = b->title, .x = right - bw, .y = y + 16, .w = bw,
                      .h = 28, .text = b->label,
                      .on_click = settings_dispatch_widget_click,
                      .on_hover = dispatch_widget_hover);
    } else if (b->type == SETTINGS_BLOCK_SLIDER) {
      float range = b->max_value - b->min_value;
      float step = range >= 10.0f ? 1.0f : range / 100.0f;
      if (step <= 0.0f)
        step = 1.0f;
      settings_place_slider(.name = b->title, .x = right - 180, .y = y + 20, .w = 180,
                      .h = 20, .min_value = b->min_value,
                      .max_value = b->max_value, .step = step,
                      .value = b->value, .on_change = settings_dispatch_slider_change,
                      .on_hover = dispatch_widget_hover);
    } else if (b->type == SETTINGS_BLOCK_EDITBOX) {
      int offset = b->x_offset;
      if (offset == SETTINGS_EDITBOX_X_OFFSET_AUTO)
        offset = settings_editbox_auto_offset(b, pane_w);
      int ex = x + SETTINGS_EDITBOX_BASE_X + offset;
      const int gap = SETTINGS_EDITBOX_FIELD_GAP;
      for (int fi = 0; fi < b->field_count; fi++) {
        SettingsEditboxFieldBlock *f = &b->fields[fi];
        int field_w = f->width;
        if (ex + field_w > right)
          field_w = right - ex;
        if (field_w < 32)
          field_w = 32;
        settings_draw_rect(.x = ex - 3, .y = y + 13, .w = field_w + 6, .h = 34,
                     .color = color_lerp(COLOR_BG, RGB(0, 0, 0), 0.28f),
                     .filled = true, .radius = 6);
        settings_place_editbox(.name = f->key, .x = ex, .y = y + 16, .w = field_w,
                         .h = 28, .buffer = f->buffer,
                         .buffer_size = f->buffer_size,
                         .placeholder = f->label, .no_tab = true);
        ex += field_w + gap;
        const char *current = settings_editbox_text(f->key);
        if (!current)
          current = f->buffer;
        if (strcmp(current, f->last_value) != 0) {
          safe_copy(f->buffer, f->buffer_size, current);
          settings_editbox_field_copy(f->last_value, sizeof(f->last_value),
                                      current, strlen(current) + 1);
          if (b->on_edit_change)
            b->on_edit_change(f->name, current);
        }
      }
    } else if (b->type == SETTINGS_BLOCK_DROPDOWN) {
      const char **options = NULL;
      int count = dropdown_options(b, &options);
      if (capturing) {
        for (int oi = 0; oi < count; oi++)
          search_index_add_text(NULL, options ? options[oi] : NULL,
                                SEARCH_KIND_DROPDOWN, b->title, NULL);
      }
      if (b->selected >= count)
        b->selected = count > 0 ? 0 : -1;
      const char *label = "Select...";
      if (b->selected >= 0 && options && options[b->selected])
        label = options[b->selected];
      int bw = settings_text_width(label, FontSize) + 40;
      if (bw < 96)
        bw = 96;
      if (bw > w)
        bw = w;
      settings_place_button(.name = b->title, .x = right - bw, .y = y + 16, .w = bw,
                      .h = 28, .text = (char *)label,
                      .on_click = settings_dispatch_widget_click,
                      .on_hover = dispatch_widget_hover);
      if (b->open && count > 0) {
        open_block = b;
        open_entry = entry;
        open_index = i;
        open_options = options;
        open_count = count;
        open_x = right - bw + draw_state_get()->off_x - base_off_x;
        open_trigger_y = y + 16 + draw_state_get()->off_y - base_off_y;
        open_w = bw;
      }
    }
    if (capturing)
      search_index_pop_context(NULL);
    cursor += SETTINGS_ROW_STEP;
  }

  if (scrolled)
    scroll_area_end();

  if (open_block && open_w > 0) {
    const int row_h = SETTINGS_DROPDOWN_OPT_H + SETTINGS_DROPDOWN_OPT_GAP;
    int panel_x = open_x - 3;
    int panel_y = open_trigger_y + 28 + 4;
    int bottom = PromptH - draw_state_get()->off_y - 4;
    int room = bottom - panel_y - 4;
    int visible = open_count;
    if (visible > SETTINGS_DROPDOWN_MAX_VISIBLE)
      visible = SETTINGS_DROPDOWN_MAX_VISIBLE;
    if (room / row_h < visible)
      visible = room / row_h;
    if (visible > 0) {
      int max_opt_w = 0;
      for (int i = 0; i < open_count; i++) {
        int w = dropdown_option_width(open_options ? open_options[i] : NULL);
        if (w > max_opt_w)
          max_opt_w = w;
      }
      int avail_w = PromptW - draw_state_get()->off_x - panel_x - 6;
      if (avail_w < SETTINGS_DROPDOWN_OPT_MIN_W + 6)
        avail_w = SETTINGS_DROPDOWN_OPT_MIN_W + 6;
      if (max_opt_w > avail_w)
        max_opt_w = avail_w;
      int panel_w = max_opt_w + 6;
      int panel_h = visible * row_h + 4;
      if (panel_y + panel_h > bottom)
        panel_y = bottom - panel_h;

      settings_draw_rect(.x = panel_x, .y = panel_y, .w = panel_w, .h = panel_h,
                   .color = color_lerp(COLOR_BG, RGB(0, 0, 0), 0.22f),
                   .filled = true, .radius = 6);

      char dropdown_scroll_name[512];
      snprintf(dropdown_scroll_name, sizeof(dropdown_scroll_name),
               "settings:dropdown:%s:%s", open_entry->name, open_block->title);
      ScrollAreaSpec dropdown_scroll = {
          .name = dropdown_scroll_name,
          .x = panel_x + 2,
          .y = panel_y + 2,
          .w = panel_w - 4,
          .h = visible * row_h,
          .content_w = 0,
          .content_h = open_count * row_h,
          .off_y = &open_block->dropdown_scroll_y,
          .wheel_px = row_h,
          .owner = open_entry->owner,
      };
      bool dropdown_scrolled = scroll_area_begin(&dropdown_scroll);
      checkbox_frame_begin();
      int top = dropdown_scrolled ? open_block->dropdown_scroll_y : 0;
      int first = top / row_h;
      int last = first + visible;
      if (last > open_count)
        last = open_count;
      for (int i = first; i < last; i++) {
        const char *text =
            (open_options && open_options[i]) ? open_options[i] : "";
        int opt_w = dropdown_option_width(text);
        if (opt_w > panel_w - 8)
          opt_w = panel_w - 8;
        int y = panel_y + 4 + i * row_h;
        char opt_name[256];
        snprintf(opt_name, sizeof(opt_name), "%s##%d", open_block->title, i);
        if (i == open_block->selected)
          settings_draw_rect(.x = panel_x + 4, .y = y, .w = opt_w,
                       .h = SETTINGS_DROPDOWN_OPT_H,
                       .color = settings_color_lerp(COLOR_BG, RGB(255, 255, 255), 0.10f),
                       .filled = true, .radius = 4);
        s_dispatch_entry = open_entry;
        s_dispatch_index = open_index;
        s_dispatch_option = i;
        focus_ctx_remember(opt_name, open_entry, open_index, i);
        settings_place_button(.name = opt_name, .x = panel_x + 4, .y = y, .w = opt_w,
                        .h = SETTINGS_DROPDOWN_OPT_H, .text = (char *)text,
                        .on_click = settings_dispatch_widget_click,
                        .on_hover = dispatch_widget_hover);
      }
      scroll_area_end();
    }
  }
  s_dispatch_entry = NULL;
  s_dispatch_index = -1;
  s_dispatch_option = -1;
}
