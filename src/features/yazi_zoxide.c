#include "common.h"
#include "features/yazi_zoxide.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shellapi.h>
#endif

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
    Line* lines = Platform_FileGetLines(YaziZoxideFile, &lineCount);
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


static void yazi_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
    if (!g_cached_yazi_commands) {
        RebuildYaziCache();
    }
    
    PopulateListBox(lb, hListBox, filter, (const char**)g_cached_yazi_commands, g_cached_yazi_count, NULL);
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
          const char* real_user = getenv("SUDO_USER");
          if (!real_user) real_user = "diggy";

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

          const char* term = (UsedTerminal && strlen(UsedTerminal) > 0) ? UsedTerminal : "alacritty";

          char cmd_open[1024];
          snprintf(cmd_open, sizeof(cmd_open), 
                   "sudo -u %s DISPLAY=%s XAUTHORITY=%s XDG_RUNTIME_DIR=%s %s %s &", 
                   real_user, display, xauthority, runtime_dir, term, cmd_args);
          
          system(cmd_open);
#endif

          if (YaziZoxideFile->fileSize > 0) {
              char lastChar = Platform_GetLastChar(YaziZoxideFile);
              if (lastChar != '\n') {
                  Platform_FileAppend(YaziZoxideFile, "\n");
              }
          }
          if (!UI_IsListBoxSelected(lb)) {
              Platform_FileAppend(YaziZoxideFile, selection);
              Platform_FileAppend(YaziZoxideFile, "\n");
              RebuildYaziCache();
          }
      }
}

static void yazi_edit_callback(const char* selection, ListBoxInfo* lb) {
    char* NewyaziQuery = Platform_EditTextPopup(selection,960,540,1000,300);
    
    if (NewyaziQuery != NULL) {
        WriteToCacheIndex(lb, lb->selected_index,NewyaziQuery);
    }
    
    Platform_ReplaceLineInMappedFile(YaziZoxideFile,lb->selected_index,NewyaziQuery);
    
    RebuildYaziCache();
    
    yazi_refresh_callback("", hListBox, lb);
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
