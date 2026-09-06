#include "core/flapi.h"
#include "core/zoxide_helpers.h"

const char* ExplorerZoxideFilePath = "data/ExplorerZoxide.txt";

char** g_cached_explorer_commands = NULL;
int g_cached_explorer_count = 0;

void RebuildExplorerCache(void) {
    ZoxideHelper_RebuildCache(ExplorerZoxideFilePath, &g_cached_explorer_commands, &g_cached_explorer_count);
}

void explorer_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, hListBox, lb, ExplorerZoxideFilePath, &g_cached_explorer_commands, &g_cached_explorer_count);
}

void explorer_enter_callback(const char* selection, ListBoxInfo* lb) {
          char* path = fl_get_zoxide_match((char*)selection);
          if (fl_launch_file_manager_at_path(path,NULL)) {
              ZoxideHelper_AppendNewEntry(ExplorerZoxideFilePath, selection, lb, &g_cached_explorer_commands, &g_cached_explorer_count);
              free(path);
          }
}

void explorer_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, ExplorerZoxideFilePath, &g_cached_explorer_commands, &g_cached_explorer_count, explorer_refresh_callback);
}

void TriggerExplorer(void) {
    RebuildExplorerCache();
    const char** list = ZoxideHelper_PrepareTriggerList(ExplorerZoxideFilePath, &g_cached_explorer_commands, &g_cached_explorer_count, NULL);
    PROMPT(.title = "📁 Explorer", .items = list, .count = g_cached_explorer_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = explorer_refresh_callback,
           .enter = explorer_enter_callback,
           .edit = explorer_edit_callback);

    ZoxideHelper_CleanupTriggerList(list);
}

FLINT_FEATURE_INIT {
fl_register_hotkey("Alt+R", TriggerExplorer);
}
