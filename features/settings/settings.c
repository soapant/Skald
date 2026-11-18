#include "core/skapi.h"
#include <stdio.h>

static void settings_refresh(PromptState *state) {}

static bool settings_enter(PromptState *state) { return true; }

static bool settings_ctrl_enter(PromptState *state) { return false; }

static void settings_tab(PromptState *state) {}

static void settings_render(PromptState *state) {
  sk_draw_text(.text="ADAWJDKWJAKd",.angle=130.0,.x=100,.y=100,.fontsize=30);
}

static void trigger_settings(void) {
  sk_window(.title = "Settings", .refresh = settings_refresh,
            .enter = settings_enter, .ctrl_enter = settings_ctrl_enter,
            .tab = settings_tab, .render = settings_render);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+D", .cb = trigger_settings);
  sk_add_feature(.display_name = "Settings", .on_action = trigger_settings,
                 .icon = "/settings.png");
}
