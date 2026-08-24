#include "common.h"
#include "features/zoxide.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/zoxide_helpers.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_zoxide_commands = NULL;
static int g_cached_zoxide_count = 0;

static void RebuildZoxideCache(void) {
    printf("[ZOXIDE] Rebuild fileSize=%zu path=%s\n", TerminalZoxideFile ? TerminalZoxideFile->fileSize : 0, TerminalZoxideFile ? "ok" : "null");
    if (!TerminalZoxideFile || TerminalZoxideFile->fileSize == 0) {
        if (g_cached_zoxide_commands) {
            for (int i = 0; i < g_cached_zoxide_count; i++) free(g_cached_zoxide_commands[i]);
            free(g_cached_zoxide_commands);
            g_cached_zoxide_commands = NULL;
            g_cached_zoxide_count = 0;
        }
        printf("[ZOXIDE] empty, return\n");
        return;
    }
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
        printf("Got Path %s \n",path);
        if (path) {
#if defined(_WIN32)
            char terminal[512];
            snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
            ShellExecuteA(NULL, "open", terminal, NULL, path, SW_SHOWNORMAL);
#else
            Platform_LaunchTerminal(path, NULL);
#endif
            ZoxideHelper_AppendNewEntry(TerminalZoxideFile, selection, lb, &g_cached_zoxide_commands, &g_cached_zoxide_count);
            free(path);
        }
}

void TriggerZoxide(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    if (!g_cached_zoxide_commands) RebuildZoxideCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_zoxide_commands, &g_cached_zoxide_count, NULL);
    ImportantPromptUser(list, "⌨️ Zoxide", g_cached_zoxide_count,920,650, 32, 
               zoxide_refresh_callback, 
               zoxide_enter_callback, 
               zoxide_edit_callback,NULL);
    ZoxideHelper_CleanupTriggerList(list);
}
