#include "core/flapi.h"
#include <stdio.h>

void settings_refresh(const char* filter, ListBoxInfo* lb) {
}

bool settings_enter(const char* selection, ListBoxInfo* lb) {
    return true;
}

bool settings_ctrl_enter(const char* selection, ListBoxInfo* lb) {
    return false;
}

void settings_copy(const char* selection, int true_index, int type) {
}

void settings_tab(ListBoxInfo* lb) {
}

void settings_render(PromptState *state) {
    static int scale = 1;
    if (fl_is_key_pressed('L') && fl_ctrl_pressed()) scale *= 2;
    if (fl_is_key_pressed('H') && fl_ctrl_pressed()) scale /= 2;
    fl_place_checkbox(.name = "Checkbox",.x=state->width / 2,.y=state->height / 2,.scale=scale,.checked = true);
}

void TriggerSettings(void) {
    PROMPT(.title = "Settings",
           .refresh = settings_refresh,
           .enter = settings_enter,
           .ctrl_enter = settings_ctrl_enter,
           .copy = settings_copy,
           .tab = settings_tab,
           .render = settings_render);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+D", TriggerSettings,"Fortnite");
    fl_register_feature("Settings", TriggerSettings);
}
