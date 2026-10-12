#include "core/flapi.h"

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
    fl_register_hotkey("Alt+D", TriggerSettings);
    fl_register_feature("Settings", TriggerSettings);
}