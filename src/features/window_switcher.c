#include "core/flapi.h"

void window_switcher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    Fl_WindowList list_of_titles = fl_get_all_window_titles();
    Fl_WindowList list_of_names = fl_get_all_window_names();

    int count = list_of_titles.count;
    if (list_of_names.count < count) count = list_of_names.count;
    const char** items = NULL;
    if (count > 0) items = (const char**)malloc(count * sizeof(char*));

    for (int i = 0; i < count; i++) {
        char* buffer = malloc(256);
        snprintf(buffer, 256, "%s {%s}",
                 list_of_titles.windows[i],
                 list_of_names.windows[i]);
        items[i] = buffer;
    }

    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, count, NULL);
    fl_free_list(items,count);
    fl_free_window_list(&list_of_titles);
    fl_free_window_list(&list_of_names);
}

bool window_switcher_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
      char* title = fl_string_cut_before(selection,'{');
      fl_focus_window_by_title(title);
      free(title);
    }
    return true;
}

void TriggerWindowSwitcher(void) {
  PROMPT(.title = "🎵 WindowSwitcher",
           .refresh = window_switcher_refresh_callback,
           .enter = window_switcher_enter_callback);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+Space",TriggerWindowSwitcher);
}
