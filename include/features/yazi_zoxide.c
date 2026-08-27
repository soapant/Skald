#include "common.h"
#include "features/yazi_zoxide.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/zoxide_helpers.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_yazi_commands = NULL;
static int g_cached_yazi_count = 0;

static void RebuildYaziCache(void) {
    ZoxideHelper_RebuildCache(YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count);
}


static void yazi_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, hListBox, lb, YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count);
}


static void yazi_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = Platform_QueryZoxidePath(selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
#if defined(_WIN32)
          char terminal[512];
          snprintf(terminal, sizeof(terminal), "%s.exe", UsedTerminal);
          ShellExecuteA(NULL, "open", terminal, cmd_args, NULL, SW_SHOWNORMAL);
#else
          Platform_LaunchTerminal(path, cmd_args);
#endif
          ZoxideHelper_AppendNewEntry(YaziZoxideFile, selection, lb, &g_cached_yazi_commands, &g_cached_yazi_count);
          free(path);
      }
}

static void yazi_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, YaziZoxideFile, &g_cached_yazi_commands, &g_cached_yazi_count, yazi_refresh_callback);
}

void TriggerYaziZoxide(void) {
    if (!g_cached_yazi_commands) RebuildYaziCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_yazi_commands, &g_cached_yazi_count, NULL);
    PROMPT(.title = "📂 Yazi", .items = list, .count = g_cached_yazi_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = yazi_refresh_callback,
           .enter = yazi_enter_callback,
           .edit = yazi_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}
