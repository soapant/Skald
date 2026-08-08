#include "common.h"
#include "features/yazi_zoxide.h"
#include "config.h"
#include "helpers.h"
#include "core/common_ui.h"

static char** g_cached_yazi_commands = NULL;
static int g_cached_yazi_count = 0;

static void RebuildYaziCache(void) {
    if (g_cached_yazi_commands) {
        for (int i = 0; i < g_cached_yazi_count; i++) {
            free(g_cached_yazi_commands[i]);
        }
        free(g_cached_yazi_commands);
        g_cached_yazi_commands = NULL;
        g_cached_yazi_count = 0;
    }
    
    if (!YaziZoxideFile || YaziZoxideFile->fileSize == 0) {
        return;
    }
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(YaziZoxideFile, &lineCount);
    if (!lines) return;
    
    g_cached_yazi_commands = (char**)malloc(lineCount * sizeof(char*));
    g_cached_yazi_count = 0;
    
    for (size_t i = 0; i < lineCount; i++) {
        size_t len = lines[i].length;
        while (len > 0 && lines[i].ptr[len-1] == '\r') len--;
        
        if (len > 0) {
            char* cmd = (char*)malloc(len + 1);
            memcpy(cmd, lines[i].ptr, len);
            cmd[len] = '\0';
            g_cached_yazi_commands[g_cached_yazi_count++] = cmd;
        }
    }
    
    free(lines);
}


static void yazi_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    if (!g_cached_yazi_commands) {
        RebuildYaziCache();
    }
    
    PopulateListBox(hListBox, filter, (const char**)g_cached_yazi_commands, g_cached_yazi_count, NULL);
}


static void yazi_enter_callback(const char* selection) {
      char* path = QueryZoxidePath(selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
          char terminal[512];
          snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
          ShellExecuteA(NULL, "open", terminal, cmd_args, NULL, SW_SHOWNORMAL);
          if (YaziZoxideFile->fileSize > 0) {
              char lastChar = YaziZoxideFile->pBuffer[YaziZoxideFile->fileSize - 1];
              if (lastChar != '\n') {
                  FileAppend(YaziZoxideFile, "\n");
              }
          }
          if (!UI_IsListBoxSelected()) {
              FileAppend(YaziZoxideFile, selection);
              FileAppend(YaziZoxideFile, "\n");
              RebuildYaziCache();
          }
      }
    
}
static void yazi_edit_callback(const char* selection) {
    char* NewyaziQuery = EditTextPopup(selection,960,540,1000,300);
    
    if (NewyaziQuery != NULL) {
        WriteToCacheIndex(g_SelectedIndex,NewyaziQuery);
    }
    
    ReplaceLineInMappedFile(YaziZoxideFile,g_SelectedIndex,NewyaziQuery);
    
    RebuildYaziCache();
    
    yazi_refresh_callback("",hListBox);
}

void TriggerYaziZoxide(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    if (!g_cached_yazi_commands) {
        RebuildYaziCache();
    }
    
    const char** list = (const char**)malloc(g_cached_yazi_count * sizeof(char*));
    if (!list) return;
    
    for (int i = 0; i < g_cached_yazi_count; i++) {
        list[i] = g_cached_yazi_commands[i];
    }
    
    ImportantPromptUser(list, "📂 Yazi", g_cached_yazi_count, 920, 650, 32, 
               yazi_refresh_callback, 
               yazi_enter_callback, 
               yazi_edit_callback,NULL);
    
    free(list);
}
