#include "core/flapi.h"
#include "core/zoxide_helpers.h"

#define FILE_TERMINAL_ZOXIDE 3
#define TerminalZoxideFile G_FILE_ENTRIES[FILE_TERMINAL_ZOXIDE].mf

static char** g_cached_zoxide_commands = NULL;
static int g_cached_zoxide_count = 0;

static void RebuildZoxideCache(void) {
    ZoxideHelper_RebuildCache(TerminalZoxideFile, &g_cached_zoxide_commands, &g_cached_zoxide_count);
}

static void zoxide_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TerminalZoxideFile, &g_cached_zoxide_commands, &g_cached_zoxide_count);
}

static void zoxide_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TerminalZoxideFile, &g_cached_zoxide_commands, &g_cached_zoxide_count, zoxide_refresh_callback);
}

static void zoxide_enter_callback(const char* selection, ListBoxInfo* lb) {
        char* path = Platform_QueryZoxidePath(selection);
        if (path) {
            Platform_LaunchTerminal(path, NULL);
            ZoxideHelper_AppendNewEntry(TerminalZoxideFile, selection, lb, &g_cached_zoxide_commands, &g_cached_zoxide_count);
            free(path);
        }
}

void TriggerZoxide(void) {
    if (!g_cached_zoxide_commands) RebuildZoxideCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_zoxide_commands, &g_cached_zoxide_count, NULL);

    PROMPT(.title = "⌨️ Zoxide", .items = list, .count = g_cached_zoxide_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = zoxide_refresh_callback,
           .enter = zoxide_enter_callback,
           .edit = zoxide_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}
