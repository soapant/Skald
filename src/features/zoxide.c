#include "common.h"
#include "features/zoxide.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

static char** g_cached_zoxide_commands = NULL;
static int g_cached_zoxide_count = 0;

static void RebuildZoxideCache(void) {
    if (g_cached_zoxide_commands) {
        for (int i = 0; i < g_cached_zoxide_count; i++) {
            free(g_cached_zoxide_commands[i]);
        }
        free(g_cached_zoxide_commands);
        g_cached_zoxide_commands = NULL;
        g_cached_zoxide_count = 0;
    }
    
    printf("[ZOXIDE] Rebuild fileSize=%zu path=%s\n", TerminalZoxideFile ? TerminalZoxideFile->fileSize : 0, TerminalZoxideFile ? "ok" : "null");
    if (!TerminalZoxideFile || TerminalZoxideFile->fileSize == 0) {
        printf("[ZOXIDE] empty, return\n");
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(TerminalZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_zoxide_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_zoxide_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_zoxide_commands[g_cached_zoxide_count++] = cmd;
        }
    }
    
    free(lines);
}

static void zoxide_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {

    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    PopulateListBox(lb, hListBox, filter, (const char**)g_cached_zoxide_commands, g_cached_zoxide_count, NULL);
}

static void zoxide_edit_callback(const char* selection, ListBoxInfo* lb) {
    char* NewZoxideQuery = Platform_EditTextPopup(selection,960,540,1000,300);
    
    if (NewZoxideQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index,NewZoxideQuery);
    }
    
    Platform_ReplaceLineInMappedFile(TerminalZoxideFile,lb->selected_index,NewZoxideQuery);
    
    RebuildZoxideCache();
    
    zoxide_refresh_callback("", hListBox, lb);
}



static void zoxide_enter_callback(const char* selection, ListBoxInfo* lb) {
        char* path = Platform_QueryZoxidePath(selection);
        printf("Got Path %s \n",path);
        if (path) {
            char terminal[512];
            snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
#if defined(_WIN32)
            ShellExecuteA(NULL, "open", terminal, NULL, path, SW_SHOWNORMAL);
#else
        const char* real_user = getenv("SUDO_USER");

        char cmd_open[1024];
        snprintf(cmd_open, sizeof(cmd_open), 
                 "sudo -u %s DISPLAY=%s XAUTHORITY=%s %s --working-directory=\"%s\" &", 
                 real_user,
                 getenv("DISPLAY") ? getenv("DISPLAY") : ":0",
                 getenv("XAUTHORITY") ? getenv("XAUTHORITY") : "",
                 UsedTerminal, 
                 path);
        
        system(cmd_open);
#endif
            if (TerminalZoxideFile->fileSize > 0) {
                  char lastChar = Platform_GetLastChar(TerminalZoxideFile);
                if (lastChar != '\n') {
                    Platform_FileAppend(TerminalZoxideFile, "\n");
                }
            }

            if (!UI_IsListBoxSelected(lb)) {
              Platform_FileAppend(TerminalZoxideFile, selection);
              Platform_FileAppend(TerminalZoxideFile, "\n");
              RebuildZoxideCache();
            }
        }
}

void TriggerZoxide(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_zoxide_commands) {
        RebuildZoxideCache();
    }
    
    const char** list = (const char**)malloc(g_cached_zoxide_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_zoxide_count; i++) {
        list[i] = g_cached_zoxide_commands[i];
    }
    
    ImportantPromptUser(list, "⌨️ Zoxide", g_cached_zoxide_count,920,650, 32, 
               zoxide_refresh_callback, 
               zoxide_enter_callback, 
               zoxide_edit_callback,NULL);
    
    free(list);
}
