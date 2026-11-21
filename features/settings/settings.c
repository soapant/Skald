#include "core/skapi.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_SIDEBAR_W 170
#define SETTINGS_EB_X 0
#define SETTINGS_EB_Y 12
#define SETTINGS_EB_H 34
#define SETTINGS_SEP_Y (SETTINGS_EB_Y + SETTINGS_EB_H + 8)
#define SETTINGS_LIST_Y (SETTINGS_SEP_Y + 6)
#define SETTINGS_ROW_H 38
#define SETTINGS_MAX_NAMES 96

static const char *settings_names[SETTINGS_MAX_NAMES];
static int settings_name_count = 0;

static void settings_collect_names(void) {
  int n = sk_settings_entry_count();
  int c = 0;
  for (int i = 0; i < n && c < SETTINGS_MAX_NAMES; i++) {
    const SkSettingsEntrySpec *e = sk_settings_entry_at(i);
    if (e && e->name && e->name[0])
      settings_names[c++] = e->name;
  }
  settings_name_count = c;
}

static const SkSettingsEntrySpec *settings_entry_by_name(const char *name) {
  if (!name || !name[0])
    return NULL;
  int n = sk_settings_entry_count();
  for (int i = 0; i < n; i++) {
    const SkSettingsEntrySpec *e = sk_settings_entry_at(i);
    if (e && e->name && strcmp(e->name, name) == 0)
      return e;
  }
  return NULL;
}

static void settings_refresh(PromptState *state) {
  settings_collect_names();
  const char *filter =
      (state && state->default_eb) ? state->default_eb->filter : "";
  sk_populate_listbox(.lb = state->lb, .filter = filter,
                      .items = settings_names, .count = settings_name_count);
}

static bool settings_enter(PromptState *state) { return true; }

static bool settings_ctrl_enter(PromptState *state) { return false; }

static void settings_list(PromptState *state) {
  if (!sk_listbox_begin(.lb = state->lb, .x = SETTINGS_EB_X,
                        .y = SETTINGS_LIST_Y, .w = SETTINGS_SIDEBAR_W,
                        .h = state->height - SETTINGS_LIST_Y - 4))
    return;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx)) {
    const SkSettingsEntrySpec *entry = settings_entry_by_name(ctx.text);
    int row_y = ctx.y;
    int row_h = ctx.h;

    if (entry && entry->icon && entry->icon[0]) {
      sk_draw_image(.path = entry->icon, .x = ctx.x + 10,
                    .y = row_y + (row_h - 18) / 2, .w = 18, .h = 18);
    }
    sk_draw_text(.text = ctx.text, .x = ctx.x + 36, .y = row_y + 3,
                 .align = "LEFT", .color = COLOR_TEXT, .font_size = 15);
  }
  sk_listbox_end();
}

static void settings_render(PromptState *state) {
  sk_draw_rect(.w = SETTINGS_SIDEBAR_W, .h = state->height,
               .color = sk_color_lerp(COLOR_BG, SK_BLACK, 0.82f),
               .filled = true, .rounded = false);

  sk_place_editbox(.x = SETTINGS_EB_X, .y = SETTINGS_EB_Y,
                   .w = SETTINGS_SIDEBAR_W, .h = SETTINGS_EB_H, .font_size = 16,
                   .caret_h = 19);

  sk_draw_line(.x1 = SETTINGS_EB_X, .y1 = SETTINGS_SEP_Y,
               .x2 = SETTINGS_EB_X + SETTINGS_SIDEBAR_W - 2,
               .y2 = SETTINGS_SEP_Y, .color = COLOR_BORDER);

  sk_draw_line(.x1 = SETTINGS_SIDEBAR_W, .x2 = SETTINGS_SIDEBAR_W,
               .y2 = state->height, .color = COLOR_BORDER);

  settings_list(state);

  if (state->lb && state->lb->selected_index >= 0 &&
      state->lb->selected_index < state->lb->item_count) {
    const SkSettingsEntrySpec *entry =
        settings_entry_by_name(state->lb->selected_text);
    if (entry && entry->render) {
      int saved_x = 0, saved_y = 0;
      ski_get_draw_offset(&saved_x, &saved_y);
      ski_set_draw_offset(SETTINGS_SIDEBAR_W + 8, 8);
      entry->render((SkSettingsEntry *)entry, entry->ud);
      ski_set_draw_offset(saved_x, saved_y);
    }
  }
}

static void trigger_settings(void) {
  sk_window(.title = "Settings", .placeholder = "Search settings",
            .refresh = settings_refresh, .enter = settings_enter,
            .ctrl_enter = settings_ctrl_enter, .render = settings_render,
            .no_separators = true);
}

static void cfg_commit(void) {
  config_save();
  Platform_ResetUIScale();
  Platform_ApplyUIScale();
  Platform_PrewarmBackgroundPixmap();
  UI_RequestPopupRepaint();
}

static bool cfg_parse_int(const char *text, int *out) {
  if (!text || !out)
    return false;
  char *end = NULL;
  long value = strtol(text, &end, 10);
  if (!end || end == text || *end)
    return false;
  if (value < INT_MIN || value > INT_MAX)
    return false;
  *out = (int)value;
  return true;
}

static bool cfg_parse_float(const char *text, float *out) {
  if (!text || !out)
    return false;
  char *end = NULL;
  float value = strtof(text, &end);
  if (!end || end == text || *end)
    return false;
  *out = value;
  return true;
}

static int cfg_pane_w(void) { return PromptW - SETTINGS_SIDEBAR_W - 8; }

static int cfg_field_width(const char *text) {
  if (!text || !text[0])
    return 150;
  return sk_get_text_width(text, FontSize) + 34;
}

#define DEFINE_CFG_BOOL(cfg_name, cfg_desc)                                    \
  static void cfg_##cfg_name##_click(void) {                                   \
    char key[256];                                                             \
    snprintf(key, sizeof(key), "Config.%s", #cfg_name);                        \
    if (sk_config_has_setting(key)) {                                          \
      cfg_name = sk_config_get_setting_bool(key);                              \
      cfg_commit();                                                            \
    }                                                                          \
  }                                                                            \
  static void cfg_add_##cfg_name(SkSettingsEntry *entry) {                     \
    sk_settings_entry_add_checkbox(entry, .title = #cfg_name, .sub = cfg_desc,  \
                                   .initial = cfg_name,                        \
                                   .on_click = cfg_##cfg_name##_click);         \
  }

#define DEFINE_CFG_INT(cfg_name, cfg_desc)                                     \
  static char cfg_buf_##cfg_name[64];                                          \
  static void cfg_##cfg_name##_change(const char *field, const char *value) {   \
    (void)field;                                                               \
    int parsed = 0;                                                            \
    if (cfg_parse_int(value, &parsed))                                         \
      cfg_name = parsed;                                                       \
    cfg_commit();                                                              \
  }                                                                            \
  static void cfg_add_##cfg_name(SkSettingsEntry *entry) {                     \
    snprintf(cfg_buf_##cfg_name, sizeof(cfg_buf_##cfg_name), "%d", cfg_name);   \
    int width = cfg_field_width(cfg_buf_##cfg_name);                             \
    SkSettingsEditboxField fields[] = {{                                       \
        .name = #cfg_name, .label = "", .buffer = cfg_buf_##cfg_name,           \
        .buffer_size = sizeof(cfg_buf_##cfg_name), .width = width}};           \
    sk_settings_entry_add_editbox(entry, .title = #cfg_name, .sub = cfg_desc,   \
                                  .fields = fields, .field_count = 1,          \
                                   .x_offset = sk_settings_editbox_right_offset(cfg_pane_w(), width), \
                                   .on_change = cfg_##cfg_name##_change);        \
  }



#define DEFINE_CFG_FLOAT(cfg_name, cfg_desc)                                   \
  static char cfg_buf_##cfg_name[64];                                          \
  static void cfg_##cfg_name##_change(const char *field, const char *value) {   \
    (void)field;                                                               \
    float parsed = 0.0f;                                                       \
    if (cfg_parse_float(value, &parsed))                                       \
      cfg_name = parsed;                                                       \
    cfg_commit();                                                              \
  }                                                                            \
  static void cfg_add_##cfg_name(SkSettingsEntry *entry) {                     \
    snprintf(cfg_buf_##cfg_name, sizeof(cfg_buf_##cfg_name), "%.0f",            \
             (double)cfg_name);                                                \
    int width = cfg_field_width(cfg_buf_##cfg_name);                             \
    SkSettingsEditboxField fields[] = {{                                       \
        .name = #cfg_name, .label = "", .buffer = cfg_buf_##cfg_name,           \
        .buffer_size = sizeof(cfg_buf_##cfg_name), .width = width}};           \
    sk_settings_entry_add_editbox(entry, .title = #cfg_name, .sub = cfg_desc,   \
                                  .fields = fields, .field_count = 1,          \
                                   .x_offset = sk_settings_editbox_right_offset(cfg_pane_w(), width), \
                                   .on_change = cfg_##cfg_name##_change);        \
  }



#define DEFINE_CFG_STRING(cfg_name, cfg_desc)                                  \
  static char cfg_buf_##cfg_name[512];                                         \
  static void cfg_##cfg_name##_change(const char *field, const char *value) {   \
    (void)field;                                                               \
    cfg_name = cfg_buf_##cfg_name;                                             \
    cfg_commit();                                                              \
  }                                                                            \
  static void cfg_add_##cfg_name(SkSettingsEntry *entry) {                     \
    snprintf(cfg_buf_##cfg_name, sizeof(cfg_buf_##cfg_name), "%s",             \
             cfg_name ? cfg_name : "");                                        \
    int width = cfg_field_width(cfg_buf_##cfg_name);                             \
    SkSettingsEditboxField fields[] = {{                                       \
        .name = #cfg_name, .label = "", .buffer = cfg_buf_##cfg_name,           \
        .buffer_size = sizeof(cfg_buf_##cfg_name), .width = width}};           \
    sk_settings_entry_add_editbox(entry, .title = #cfg_name, .sub = cfg_desc,   \
                                  .fields = fields, .field_count = 1,          \
                                   .x_offset = sk_settings_editbox_right_offset(cfg_pane_w(), width), \
                                   .on_change = cfg_##cfg_name##_change);        \
  }



DEFINE_CFG_BOOL(Tray, "Show the skald tray icon.")
DEFINE_CFG_BOOL(RememberLastIndex, "Remember the last selected item.")
DEFINE_CFG_BOOL(MessageAfterEachHotkey, "Show a message after each hotkey.")
DEFINE_CFG_INT(TimeOutForMessages, "How long messages stay visible, in seconds.")
DEFINE_CFG_INT(HotkeyRepeatDelayMs,
               "Delay before held hotkeys repeat, in milliseconds.")
DEFINE_CFG_BOOL(DebugMode, "Enable verbose debug output.")
DEFINE_CFG_BOOL(RoundedCorners, "Use rounded window corners.")
DEFINE_CFG_INT(CornerRadius, "Radius used for rounded corners.")
DEFINE_CFG_INT(MessageScreenGap, "Gap between messages and the screen edge.")
DEFINE_CFG_FLOAT(WindowOpacity, "Window opacity from 0 to 255.")
DEFINE_CFG_STRING(UsedFont, "Font family used by the UI.")
DEFINE_CFG_INT(FontSize, "Base UI font size.")
DEFINE_CFG_INT(IconSize, "Base icon size.")
DEFINE_CFG_INT(WidgetGap, "Spacing between widgets.")
DEFINE_CFG_INT(PromptW, "Prompt window width.")
DEFINE_CFG_INT(PromptH, "Prompt window height.")
DEFINE_CFG_INT(PromptX, "Prompt X position; -1 centers.")
DEFINE_CFG_INT(PromptY, "Prompt Y position; -1 centers.")
DEFINE_CFG_STRING(UsedTerminal, "Terminal emulator used for commands.")
DEFINE_CFG_STRING(UsedShell, "Shell used for commands.")
DEFINE_CFG_STRING(UsedBrowser, "Preferred web browser.")
DEFINE_CFG_STRING(UsedSearchEngine, "Search engine used for web searches.")
DEFINE_CFG_STRING(FeaturePath, "Directory containing feature modules.")
DEFINE_CFG_STRING(IconPath, "Default icon asset path.")
DEFINE_CFG_STRING(ControlHotkey, "Hotkey that opens the main prompt.")
DEFINE_CFG_STRING(SettingsHotkey, "Hotkey that opens settings.")
DEFINE_CFG_STRING(ReloadFeaturesHotkey, "Hotkey that reloads features.")
DEFINE_CFG_STRING(ActionsHotkey, "Hotkey that opens actions.")
DEFINE_CFG_STRING(RegHotkey, "Hotkey for registering hotkeys.")
DEFINE_CFG_STRING(RegAliasHotkey, "Hotkey for registering aliases.")
DEFINE_CFG_STRING(BackgroundImage, "Background image path.")
DEFINE_CFG_BOOL(MessagesEnabled, "Enable popup messages.")
DEFINE_CFG_BOOL(EnableAutoPairing, "Automatically pair ' with ' ( with ) etc.")
DEFINE_CFG_BOOL(EnableBorders, "Draw window borders.")
DEFINE_CFG_BOOL(WindowTitleText, "Show window title text.")
DEFINE_CFG_BOOL(CloseOnLossOfFocus, "Close popups when focus is lost.")

#define CFG_THEME_MAX 64

static int cfg_theme_options(const char ***out_options) {
  static const char *options[CFG_THEME_MAX];
  int count = sk_theme_count();
  if (count > CFG_THEME_MAX)
    count = CFG_THEME_MAX;
  for (int i = 0; i < count; i++) {
    const char *name = sk_theme_name_at(i);
    options[i] = name ? name : "";
  }
  *out_options = options;
  return count;
}

static void cfg_theme_select(const char *option, int index, void *ud) {
  (void)index;
  (void)ud;
  if (option && sk_set_theme(option))
    UI_RequestPopupRepaint();
}

static void cfg_add_Theme(SkSettingsEntry *entry) {
  int initial = 0;
  int count = sk_theme_count();
  for (int i = 0; i < count; i++) {
    const char *name = sk_theme_name_at(i);
    if (name && Theme && strcmp(name, Theme) == 0) {
      initial = i;
      break;
    }
  }
  sk_settings_entry_add_dropdown(entry, .title = "Theme",
                                 .sub = "Active UI theme.",
                                 .refresh_options = cfg_theme_options,
                                 .initial = initial,
                                 .on_select = cfg_theme_select);
}

static void cfg_add_all(void) {
  SkSettingsEntry *entry =
      sk_add_settings_entry(.name = "Config", .icon = "/settings.png",
                            .desc = "Values stored in config/config.toml.");
  cfg_add_Tray(entry);
  cfg_add_RememberLastIndex(entry);
  cfg_add_MessageAfterEachHotkey(entry);
  cfg_add_TimeOutForMessages(entry);
  cfg_add_HotkeyRepeatDelayMs(entry);
  cfg_add_DebugMode(entry);
  cfg_add_RoundedCorners(entry);
  cfg_add_CornerRadius(entry);
  cfg_add_MessageScreenGap(entry);
  cfg_add_WindowOpacity(entry);
  cfg_add_UsedFont(entry);
  cfg_add_FontSize(entry);
  cfg_add_IconSize(entry);
  cfg_add_WidgetGap(entry);
  cfg_add_PromptW(entry);
  cfg_add_PromptH(entry);
  cfg_add_PromptX(entry);
  cfg_add_PromptY(entry);
  cfg_add_UsedTerminal(entry);
  cfg_add_UsedShell(entry);
  cfg_add_UsedBrowser(entry);
  cfg_add_UsedSearchEngine(entry);
  cfg_add_FeaturePath(entry);
  cfg_add_IconPath(entry);
  cfg_add_ControlHotkey(entry);
  cfg_add_SettingsHotkey(entry);
  cfg_add_ReloadFeaturesHotkey(entry);
  cfg_add_ActionsHotkey(entry);
  cfg_add_RegHotkey(entry);
  cfg_add_RegAliasHotkey(entry);
  cfg_add_Theme(entry);
  cfg_add_BackgroundImage(entry);
  cfg_add_MessagesEnabled(entry);
  cfg_add_EnableAutoPairing(entry);
  cfg_add_EnableBorders(entry);
  cfg_add_WindowTitleText(entry);
  cfg_add_CloseOnLossOfFocus(entry);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = (char *)SettingsHotkey, .cb = trigger_settings);
  sk_add_feature(.display_name = "Settings", .on_action = trigger_settings,
                  .icon = "/settings.png");
  cfg_add_all();

  static char ql_name[128];

  static char ql_url[512];
  SkSettingsEditboxField fields[] = {
      {.name = "quicklink_name",
       .label = "Name:",
       .buffer = ql_name,
       .buffer_size = sizeof(ql_name),
       .width = 100},
      {.name = "quicklink_url",
       .label = "Url:",
       .buffer = ql_url,
       .buffer_size = sizeof(ql_url),
       .width = 280},
  };

  SkSettingsEntry *ql =
      sk_add_settings_entry(.name = "Quicklinks", .icon = "/quicklinks.png",
                            .desc = "Add Quicklinks for your most commonly "
                                    "opened websites.");
  sk_settings_entry_add_editbox(
      ql, .title = "Add Quicklink", .sub = "Add a quicklink to appear in cc",
      .fields = fields, .field_count = 2, .x_offset = 0);

  sk_add_settings_entry(.name = "Snippets", .icon = "/snippets.png",

                        .desc = "Coming soon.");
  sk_add_settings_entry(.name = "Hotkeys", .icon = "/hotkeys.png",
                        .desc = "Coming soon.");
  sk_add_settings_entry(.name = "Remaps", .icon = "/remaps.png",
                        .desc = "Coming soon.");
}
