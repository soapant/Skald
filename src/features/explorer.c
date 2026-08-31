#include "core/flapi.h"
#include "core/zoxide_helpers.h"

#define FILE_EXPLORER_ZOXIDE 4
#define ExplorerZoxideFile G_FILE_ENTRIES[FILE_EXPLORER_ZOXIDE].mf

static char** g_cached_explorer_commands = NULL;
static int g_cached_explorer_count = 0;

static void RebuildExplorerCache(void) {
    ZoxideHelper_RebuildCache(ExplorerZoxideFile, &g_cached_explorer_commands, &g_cached_explorer_count);
}

static void explorer_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, hListBox, lb, ExplorerZoxideFile, &g_cached_explorer_commands, &g_cached_explorer_count);
}

static void explorer_enter_callback(const char* selection, ListBoxInfo* lb) {
          char* path = Platform_QueryZoxidePath(selection);
          if (path) {
#if defined(_WIN32)
              ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
#else
              Platform_ShellExecuteWithVerb(path, "open", true);
#endif
              ZoxideHelper_AppendNewEntry(ExplorerZoxideFile, selection, lb, &g_cached_explorer_commands, &g_cached_explorer_count);
              free(path);
          }
}

static void explorer_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, ExplorerZoxideFile, &g_cached_explorer_commands, &g_cached_explorer_count, explorer_refresh_callback);
}

void TriggerExplorer(void) {
    RebuildExplorerCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_explorer_commands, &g_cached_explorer_count, NULL);
    PROMPT(.title = "📁 Explorer", .items = list, .count = g_cached_explorer_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = explorer_refresh_callback,
           .enter = explorer_enter_callback,
           .edit = explorer_edit_callback);

    ZoxideHelper_CleanupTriggerList(list);
}

void explorer_init(void) {
    fl_register_hotkey("Alt+R", TriggerExplorer, NULL, NULL);
}
