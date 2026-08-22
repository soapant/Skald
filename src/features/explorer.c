#include "common.h"
#include "features/explorer.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_explorer_commands = NULL;
static int g_cached_explorer_count = 0;


static void RebuildExplorerCache(void) {
    if (g_cached_explorer_commands) {
        for (int i = 0; i < g_cached_explorer_count; i++) {
            free(g_cached_explorer_commands[i]);
        }
        free(g_cached_explorer_commands);
        g_cached_explorer_commands = NULL;
        g_cached_explorer_count = 0;
    }
    
    if (!ExplorerZoxideFile || ExplorerZoxideFile->fileSize == 0) {
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(ExplorerZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_explorer_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_explorer_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_explorer_commands[g_cached_explorer_count++] = cmd;
        }
    }
    
    free(lines);
}

static void explorer_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    PopulateListBox(lb, hListBox, filter, (const char**)g_cached_explorer_commands, g_cached_explorer_count, NULL);
}

static void explorer_enter_callback(const char* selection, ListBoxInfo* lb) {
          char* path = Platform_QueryZoxidePath(selection);
          if (path) {
#if defined(_WIN32)
              ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
#else
        const char* real_user = getenv("SUDO_USER");
        if (!real_user) real_user = "root";

        const char* display = getenv("DISPLAY");
        if (!display) display = ":0";
        
        const char* xauthority = getenv("XAUTHORITY");
        if (!xauthority) xauthority = "";

        char runtime_dir[256];
        const char* env_runtime = getenv("XDG_RUNTIME_DIR");
        if (env_runtime) {
            snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
        } else {
            snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
        }

        char cmd_open[1024];
        snprintf(cmd_open, sizeof(cmd_open), 
                 "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s xdg-open \"%s\" &", 
                 real_user, display, xauthority, runtime_dir, path);
        
        system(cmd_open);
#endif
              if (ExplorerZoxideFile->fileSize > 0) {
                  char lastChar = Platform_GetLastChar(ExplorerZoxideFile);
                    if (lastChar != '\n') {
                      Platform_FileAppend(ExplorerZoxideFile, "\n");
                  }
              }

              if (!UI_IsListBoxSelected(lb)) {
                Platform_FileAppend(ExplorerZoxideFile, selection);
                Platform_FileAppend(ExplorerZoxideFile, "\n");
                RebuildExplorerCache();
              }
          }
}

static void explorer_edit_callback(const char* selection, ListBoxInfo* lb) {
    char* NewexplorerQuery = Platform_EditTextPopup(selection,960,540,1000,300);
    
    if (NewexplorerQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index,NewexplorerQuery);
    }
    
    Platform_ReplaceLineInMappedFile(ExplorerZoxideFile,lb->selected_index,NewexplorerQuery);
    
    RebuildExplorerCache();
    
    explorer_refresh_callback("", hListBox, lb);
}

void TriggerExplorer(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_explorer_commands) {
        RebuildExplorerCache();
    }
    
    const char** list = (const char**)malloc(g_cached_explorer_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_explorer_count; i++) {
        list[i] = g_cached_explorer_commands[i];
    }
    
    ImportantPromptUser(list, "📁 Explorer", g_cached_explorer_count, 920, 650, 32, 
               explorer_refresh_callback, 
               explorer_enter_callback, 
               explorer_edit_callback,NULL);
    
    free(list);
}
