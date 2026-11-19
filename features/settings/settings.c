#include "core/skapi.h"
#include <stdio.h>
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
  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = settings_names, .count = settings_name_count);
}

static bool settings_enter(PromptState *state) { return true; }

static bool settings_ctrl_enter(PromptState *state) { return false; }

static void settings_tab(PromptState *state) {}

static void settings_list(PromptState *state) {
  if (!sk_listbox_begin(.lb = state->lb,
                        .x = SETTINGS_EB_X, .y = SETTINGS_LIST_Y,
                        .w = SETTINGS_SIDEBAR_W,
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
    sk_draw_text(.text = ctx.text, .x = ctx.x + 36,
                 .y = row_y + 3,
                 .align = "LEFT", .color = COLOR_TEXT, .font_size = 15);
  }
  sk_listbox_end();
}

static void settings_render(PromptState *state) {
  sk_draw_rect(.w = SETTINGS_SIDEBAR_W, .h = state->height,
               .color = sk_color_lerp(COLOR_BG, SK_BLACK, 0.82f),
               .filled = true, .rounded = false);

  sk_place_editbox(.x = SETTINGS_EB_X, .y = SETTINGS_EB_Y,
                   .w = SETTINGS_SIDEBAR_W, .h = SETTINGS_EB_H, .font_size = 16,.caret_h=19);

  sk_draw_line(.x1 = SETTINGS_EB_X, .y1 = SETTINGS_SEP_Y,
               .x2 = SETTINGS_EB_X + SETTINGS_SIDEBAR_W - 2, .y2 = SETTINGS_SEP_Y,
               .color = COLOR_BORDER);

  sk_draw_line(.x1 = SETTINGS_SIDEBAR_W, .x2 = SETTINGS_SIDEBAR_W,
               .y2 = state->height, .color = COLOR_BORDER);


  settings_list(state);

  if (state->lb && state->lb->selected_index >= 0 &&
      state->lb->selected_index < state->lb->item_count) {
    const SkSettingsEntrySpec *entry =
        settings_entry_by_name(state->lb->selected_text);
    if (entry && entry->render) {
      int saved_x = 0, saved_y = 0;
      sk_get_draw_offset(&saved_x, &saved_y);
      sk_set_draw_offset(SETTINGS_SIDEBAR_W + 8, 8);
      /* pub is the first member of SkSettingsEntry */
      entry->render((SkSettingsEntry *)entry, entry->ud);
      sk_set_draw_offset(saved_x, saved_y);
    }
  }
}

static void trigger_settings(void) {
  sk_window(.title = "Settings", .placeholder = "Search settings",
            .refresh = settings_refresh, .enter = settings_enter,
            .ctrl_enter = settings_ctrl_enter, .tab = settings_tab,
            .render = settings_render, .no_separators = true);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = (char *)SettingsHotkey, .cb = trigger_settings);
  sk_add_feature(.display_name = "Settings", .on_action = trigger_settings,
                 .icon = "/settings.png");

  SkSettingsEntry *ql =
      sk_add_settings_entry(.name = "Quicklinks", .icon = "/quicklinks.png",
                            .desc = "Add Quicklinks for your most commonly "
                                    "opened websites.");
  sk_settings_entry_add_checkbox(ql,
                                 .title = "Add Quicklink",
                                 .sub = "Add a quicklink to appear in cc");

  sk_add_settings_entry(.name = "Config", .icon = "/settings.png",
                        .desc = "Coming soon.");
  sk_add_settings_entry(.name = "Snippets", .icon = "/snippets.png",
                        .desc = "Coming soon.");
  sk_add_settings_entry(.name = "Hotkeys", .icon = "/hotkeys.png",
                        .desc = "Coming soon.");
  sk_add_settings_entry(.name = "Remaps", .icon = "/remaps.png",
                        .desc = "Coming soon.");
}
