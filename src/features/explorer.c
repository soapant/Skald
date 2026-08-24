#include "common.h"
#include "features/explorer.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/zoxide_helpers.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

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
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    if (!g_cached_explorer_commands) RebuildExplorerCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_explorer_commands, &g_cached_explorer_count, NULL);
    ImportantPromptUser(list, "📁 Explorer", g_cached_explorer_count, 920, 650, 32, 
               explorer_refresh_callback, 
               explorer_enter_callback, 
               explorer_edit_callback,NULL);
    ZoxideHelper_CleanupTriggerList(list);
}
