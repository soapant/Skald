#include "core/skapi.h"
#include <stdio.h>

void settings_refresh(PromptState *state) {}

bool settings_enter(PromptState *state) { return true; }

bool settings_ctrl_enter(PromptState *state) { return false; }

void settings_tab(PromptState *state) {}

void settings_render(PromptState *state) {
  sk_place_button(.name = "Checkbox", .x = state->width / 2,
                  .y = state->height / 2, .text = "Button", .fontsize = 20,
                  .bg_color = RGB(100, 100, 0));
}

void TriggerSettings(void) {
  sk_window(.title = "Settings", .refresh = settings_refresh,
            .enter = settings_enter, .ctrl_enter = settings_ctrl_enter,
            .tab = settings_tab, .render = settings_render);
}

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Alt+D", .cb = TriggerSettings);
  sk_add_to_cc(.display_name = "Settings", .on_action = TriggerSettings,
               .icon = "/settings.png");
}
