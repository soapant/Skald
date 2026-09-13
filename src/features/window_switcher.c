#include "core/flapi.h"

void window_switcher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    WindowList list = fl_get_all_window_titles();
    const char** items = (const char**)malloc(list.count * sizeof(char*));
    for (size_t i = 0; i < list.count; i++) items[i] = list.windows[i];
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items,list.count, NULL);
}
bool window_switcher_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) fl_focus_window_by_title(selection);
    return true;
}
void TriggerWindowSwitcher(void) {
  WindowList list = fl_get_all_window_titles();
  PROMPT(.title = "🎵 WindowSwitcher", .items = (const char**)list.windows,
           .refresh = window_switcher_refresh_callback,
           .enter = window_switcher_enter_callback);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+Space",TriggerWindowSwitcher);
}
