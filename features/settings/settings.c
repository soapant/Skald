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

static void settings_quicklinks_render(void *ud) {
   char* desc = "Add Quicklinks for your most commonly opened websites.";
   sk_draw_image(.path="/quicklinks.png",.x=(PromptW - SETTINGS_SIDEBAR_W) / 2 - 64,.y=10,.w=128,.h=128);
   sk_draw_text(.text=desc,.color=SK_GRAY,
                .x = 30,
                .y = 150
                );

  sk_draw_line(.x1 = -7, .y1 = 210,
               .x2 = PromptW, .y2 = 210,
               .color = COLOR_BORDER);

  sk_draw_rect(.x=10,.y=250,.w=700,.h=60,.color=SK_DARK_GRAY,.filled=true,.radius=6);
  sk_draw_text(.text = "Add Quicklink",.x=20,.y=250,.color=SK_LIGHT_GRAY);
  sk_draw_text(.text = "Blah Balh balhda bal ha ",.x=20,.y=275,.color=SK_GRAY);
  sk_place_checkbox(.name="blah",.x=650,.y=264,.w=32,.h=32);
}

static void settings_config_render(void *ud) {
  (void)ud;
}

static void settings_snippets_render(void *ud) {
  (void)ud;
}

static void settings_hotkeys_render(void *ud) {
  (void)ud;
}

static void settings_remaps_render(void *ud) {
  (void)ud;
}

static void settings_list(PromptState *state) {
  if (!sk_listbox_begin(.lb = state->lb, .item_height = SETTINGS_ROW_H,
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
                    .y = row_y + (row_h - 20) / 2, .w = 18, .h = 18);
    }
    sk_draw_text(.text = ctx.text, .x = ctx.x + 36,
                 .y = row_y + (row_h - sk_get_text_height(15)) / 2,
                 .align = "LEFT", .color = COLOR_TEXT, .font_size = 15);
  }
  sk_listbox_end();
}

static void settings_render(PromptState *state) {
  sk_draw_rect(.w = SETTINGS_SIDEBAR_W, .h = state->height,
               .color = sk_color_lerp(COLOR_BG, SK_BLACK, 0.32f),
               .filled = true, .rounded = false);

  sk_place_editbox(.x = SETTINGS_EB_X, .y = SETTINGS_EB_Y,
                   .w = SETTINGS_SIDEBAR_W, .h = SETTINGS_EB_H, .font_size = 16,.caret_h=19,.caret_y = SETTINGS_EB_H / 2);

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
      entry->render(entry->ud);
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

  sk_add_settings_entry(.name = "Quicklinks", .icon = "/quicklinks.png",
                        .render = settings_quicklinks_render);
  sk_add_settings_entry(.name = "Config", .icon = "/settings.png",
                        .render = settings_config_render);
  sk_add_settings_entry(.name = "Snippets", .icon = "/snippets.png",
                        .render = settings_snippets_render);
  sk_add_settings_entry(.name = "Hotkeys", .icon = "/hotkeys.png",
                        .render = settings_hotkeys_render);
  sk_add_settings_entry(.name = "Remaps", .icon = "/remaps.png",
                        .render = settings_remaps_render);
}
