#include "core/flapi.h"

char* games_windows[] = {"Brawlhalla", "Fortnite", "RocketLeague", "Factorio", "Minecraft"};
char* browsers_windows[] = {"Zen", "Chrome"};
char* coding_windows[] = {"Neovide", "vim"};
char* terminal_windows[] = {"alacritty", "pwsh", "foot"};

void FocusGameWindows(void) {
    for (int i = 0; i < ARRAY_LEN(games_windows); i++) {
        if (fl_focus_window_by_title(games_windows[i])) return;
    }
}

void FocusBrowserWindows(void) {
    for (int i = 0; i < ARRAY_LEN(browsers_windows); i++) {
        if (fl_focus_window_by_title(browsers_windows[i])) return;
    }
}

void FocusCodingWindows(void) {
    for (int i = 0; i < ARRAY_LEN(coding_windows); i++) {
        if (fl_focus_window_by_title(coding_windows[i])) return;
    }
}

void FocusTerminalWindows(void) {
    for (int i = 0; i < ARRAY_LEN(terminal_windows); i++) {
        if (fl_focus_window_by_title(terminal_windows[i])) return;
    }
}



void window_switcher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    WindowList list = fl_get_all_active_windows();
    
    const char** items = (const char**)malloc(list.count * sizeof(char*));
    for (size_t i = 0; i < list.count; i++) {
        items[i] = list.windows[i];
    }
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items,list.count, NULL);
    free(items);
}

bool window_switcher_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
      fl_focus_window_by_title(selection);
    } 
    return true;
}

void TriggerWindowSwitcher(void) {
  WindowList list = fl_get_all_active_windows();
  PROMPT(.title = "🎵 WindowSwitcher", .items = (const char**)list.windows,
           .refresh = window_switcher_refresh_callback,
           .enter = window_switcher_enter_callback);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+Space",TriggerWindowSwitcher);
}
