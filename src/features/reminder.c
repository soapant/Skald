#include "core/flapi.h"

void reminder_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
}

bool reminder_enter(const char* selection, ListBoxInfo* lb) {
    return true;
}

bool reminder_ctrl_enter(const char* selection, ListBoxInfo* lb) {
    return false;
}

void reminder_copy(const char* selection, int true_index, int type) {
}

void reminder_tab(ListBoxInfo* lb) {
}

void reminder_render(PromptState *state) {

}

void TriggerReminder(void) {
    PROMPT(.title = "Reminder",
           .refresh = reminder_refresh,
           .enter = reminder_enter,
           .ctrl_enter = reminder_ctrl_enter,
           .copy = reminder_copy,
           .tab = reminder_tab,
           .render = reminder_render);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+S", TriggerReminder);
    fl_register_feature("Reminder", TriggerReminder);
}
