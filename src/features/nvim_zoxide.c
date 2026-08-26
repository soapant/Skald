#include "common.h"
#include "features/nvim_zoxide.h"
#include "helpers.h"
#include "core/common_ui.h"
#include "core/zoxide_helpers.h"
#include "platform/platform.h"
#include <ctype.h>
#if defined(_WIN32)
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

static char** g_cached_nvim_commands = NULL;
static int g_cached_nvim_count = 0;
char* TextEditorName;
bool Initialized = false;

static void InitTextEditor() {
    
    char expanded_path[MAX_PATH];
    if (!Platform_ExpandPath(TextEditorPath, expanded_path, sizeof(expanded_path))) return;
    
    const char* last_slash = strrchr(expanded_path, '\\');
    const char* last_slash2 = strrchr(expanded_path, '/');
    if (last_slash2 && (!last_slash || last_slash2 > last_slash)) last_slash = last_slash2;

    const char* start = (last_slash != NULL) ? (last_slash + 1) : expanded_path;

    const char* dot = strrchr(start, '.');
    size_t len;
    if (dot && strcmp(dot, ".exe") == 0) len = dot - start;
    else len = strlen(start);

    static char app_name[64];
    
    snprintf(app_name, sizeof(app_name), "%.*s", (int)len, start);
    
    TextEditorName = app_name;
    
    if (TextEditorName[0]) TextEditorName[0] = toupper(TextEditorName[0]);
    
    Initialized = true;
          
}
static void LaunchNvim(const char* path) {
    if (!path) {
        printf("[NVIM] ERROR: path is NULL\n");
        return;
    }
    char expanded_path[MAX_PATH];
    if (!Platform_ExpandPath(TextEditorPath, expanded_path, sizeof(expanded_path))) return;
#if defined(_WIN32)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s \"%s\"", expanded_path, path);
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        ShellExecuteA(NULL, "open", TextEditorName, path, NULL, SW_SHOWNORMAL);
    }
#else
    // Use Platform_LaunchTerminal with working dir = path's dir and extra = -e nvim
    // Extract dir from path if it's a file
    char dir[MAX_PATH];
    strncpy(dir, path, sizeof(dir)-1); dir[sizeof(dir)-1]='\0';
    char* slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else strncpy(dir, path, sizeof(dir)-1);
    char extra[1024];
    snprintf(extra, sizeof(extra), "-e %s \"%s\"", expanded_path, path);
    // Leverage Platform_LaunchTerminal for daemonized, nohup-aware launch
    if (!Platform_LaunchTerminal(dir[0]?dir:path, extra)){
        // fallback to shell
        char expanded_cmd[2048];
        snprintf(expanded_cmd, sizeof(expanded_cmd), "%s -e %s \"%s\"", UsedTerminal, expanded_path, path);
        const char* real_user = Platform_GetRealUser();
        const char* disp=Platform_GetSessionDisplay();
        const char* xa=Platform_GetSessionXAuthority();
        char rt[256]; snprintf(rt,sizeof(rt),"%s",Platform_GetRuntimeDir());
        pid_t pid=fork();
        if(pid==0){
            setsid(); pid_t p2=fork(); if(p2<0) _exit(1); if(p2>0) _exit(0);
            int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
            if(Platform_IsRunningAsRoot()){
                char de[256], xe[512], re[512];
                snprintf(de,sizeof(de),"DISPLAY=%s",disp); snprintf(xe,sizeof(xe),"XAUTHORITY=%s",xa); snprintf(re,sizeof(re),"XDG_RUNTIME_DIR=%s",rt);
                char* const av[]={"sudo","-u",(char*)real_user,"env",de,xe,re,"sh","-c",expanded_cmd,NULL};
                execvp(av[0],av);
            } else {
                setenv("DISPLAY",disp,1); setenv("XAUTHORITY",xa,1); setenv("XDG_RUNTIME_DIR",rt,1);
                execl("/bin/sh","sh","-c",expanded_cmd,(char*)NULL);
            }
            _exit(127);
        } else if(pid>0){ int st=0; waitpid(pid,&st,0); }
    }
#endif
}

static void RebuildNvimCache(void) {
    ZoxideHelper_RebuildCache(NvimZoxideFile, &g_cached_nvim_commands, &g_cached_nvim_count);
}

static void neovide_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    ZoxideHelper_Refresh(filter, hListBox, lb, NvimZoxideFile, &g_cached_nvim_commands, &g_cached_nvim_count);
}


static void neovide_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = Platform_QueryZoxidePath(selection);
      if (path) {
          LaunchNvim(path);
          ZoxideHelper_AppendNewEntry(NvimZoxideFile, selection, lb, &g_cached_nvim_commands, &g_cached_nvim_count);
          free(path);
      }
}
static void neovide_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, NvimZoxideFile, &g_cached_nvim_commands, &g_cached_nvim_count, neovide_refresh_callback);
}

void TriggerNvimZoxide(void) {
    if (!g_cached_nvim_commands) RebuildNvimCache();
    if (!Initialized) InitTextEditor();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_nvim_commands, &g_cached_nvim_count, NULL);
    char titleBuffer[256];
    snprintf(titleBuffer,sizeof(titleBuffer),"📝 %s",TextEditorName);
    PROMPT(.title = titleBuffer, .items = list, .count = g_cached_nvim_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = neovide_refresh_callback,
           .enter = neovide_enter_callback,
           .edit = neovide_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}
