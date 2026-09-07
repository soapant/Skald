#include "core/flapi.h"
#include "core/zoxide_helpers.h"

const char* TerminalZoxideFilePath = "data/TerminalZoxide.txt";

char** cached_zoxide_commands = NULL;
int cached_zoxide_count = 0;

void RebuildZoxideCache(void) {
    ZoxideHelper_RebuildCache(TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count);
}

void zoxide_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, FlintContext_Get()->window.hListBox, lb, TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count);
}

void zoxide_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count, zoxide_refresh_callback);
}

bool zoxide_enter_callback(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match(selection);
    if (path) {
        fl_launch_terminal_at_path(path, NULL);
        ZoxideHelper_AppendNewEntry(TerminalZoxideFilePath, selection, lb, &cached_zoxide_commands, &cached_zoxide_count);
    }
    return true;
}

void TriggerZoxide(void) {
    if (!cached_zoxide_commands) RebuildZoxideCache();
    const char** list = ZoxideHelper_PrepareTriggerList(TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count, NULL);

    PROMPT(.title = "⌨️ Zoxide", .items = list, .count = cached_zoxide_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = zoxide_refresh_callback,
           .enter = zoxide_enter_callback,
           .edit = zoxide_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+W", TriggerZoxide);
}
