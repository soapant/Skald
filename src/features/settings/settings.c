#include "core/flapi.h"
#include <stdio.h>

void settings_refresh(const char *filter, ListBoxInfo *lb) {}

bool settings_enter(ListBoxInfo *lb) { return true; }

bool settings_ctrl_enter(ListBoxInfo *lb) { return false; }

void settings_tab(ListBoxInfo *lb) {}

void settings_render(PromptState *state) {
  fl_place_button(.name = "Checkbox", .x = state->width / 2,
                  .y = state->height / 2, .border = false, .text = "Button",
                  .fontsize = 20);
}

void TriggerSettings(void) {
  fl_window(.title = "Settings", .refresh = settings_refresh,
            .enter = settings_enter, .ctrl_enter = settings_ctrl_enter,
            .tab = settings_tab, .render = settings_render);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey(.hotkey = "Alt+D", .cb = TriggerSettings);
  fl_add_to_cc(.display_name = "Settings", .on_action = TriggerSettings,
               .icon = "/settings.png");
}
