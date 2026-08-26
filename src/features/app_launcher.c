#include "common.h"
#include "features/app_launcher.h"
#include "features/file_search.h"
#include "helpers.h"
#include "core/common_ui.h"
#if defined(_WIN32)
#include <shlwapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#endif

void initialize_app_cache(void) {
    G_APP_COUNT = 0;
    
    for (int i = 0; i < sizeof(ListOfProgramPaths) / sizeof(ListOfProgramPaths[0]); i++) {
        scan_directory_recursive(ListOfProgramPaths[i]);
    }
    
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (G_APP_COUNT >= MAX_APPS) break;
        
        bool found = false;
        for (int app_idx = 0; app_idx < G_APP_COUNT; app_idx++) {
            if (StrStrIA(G_APPS[app_idx].path, SystemShortcuts[shortcut_idx].path) != NULL) {
                strncpy(G_APPS[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(G_APPS[app_idx].name) - 1);
                G_APPS[app_idx].name[sizeof(G_APPS[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            strncpy(G_APPS[G_APP_COUNT].name, SystemShortcuts[shortcut_idx].name, sizeof(G_APPS[G_APP_COUNT].name) - 1);
            G_APPS[G_APP_COUNT].name[sizeof(G_APPS[G_APP_COUNT].name) - 1] = '\0';
            
            strncpy(G_APPS[G_APP_COUNT].path, SystemShortcuts[shortcut_idx].path, sizeof(G_APPS[G_APP_COUNT].path) - 1);
            G_APPS[G_APP_COUNT].path[sizeof(G_APPS[G_APP_COUNT].path) - 1] = '\0';
            
            G_APP_COUNT++;
        }
    }

#endif

}
void launch_app_by_name(const char* display_name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
#if defined(_WIN32)
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
#else
        if (strcasecmp(G_APPS[i].name, display_name) == 0) {
#endif
            Platform_LaunchApp(G_APPS[i].path);
            return;
        }
    }
}

static void launch_app_as_admin(const char* display_name) {
    for (int i = 0; i < G_APP_COUNT; i++) {
#if defined(_WIN32)
        if (StrCmpIA(G_APPS[i].name, display_name) == 0) {
#else
        if (strcasecmp(G_APPS[i].name, display_name) == 0) {
#endif
            Platform_LaunchAppAsAdmin(G_APPS[i].path);
            return;
        }
    }
}

void launch_exe_with_args(const char* exe_path, const char* args) {
    char cmd[MAX_PATH * 2 + 512];

    char expanded_path[MAX_PATH];
    if (!Platform_ExpandPath(exe_path, expanded_path, sizeof(expanded_path))) return;
    exe_path = expanded_path;

    if (args != NULL && strlen(args) > 0 && strcmp(args, "0") != 0) {
        snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe_path, args);
    } else {
        snprintf(cmd, sizeof(cmd), "\"%s\"", exe_path);
    }
    
#if defined(_WIN32)
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#else
    // Use GetRealUser logic inline to handle `sudo true; nohup ... &` where SUDO_USER not set for nohup child
    const char* real_user = NULL;
    {
        const char* u=getenv("SUDO_USER");
        if(u && u[0] && strcmp(u,"root")!=0) real_user=u;
        else {
            u=getenv("USER");
            if(u && u[0]) real_user=u;
            else {
                u=getenv("LOGNAME");
                if(u && u[0]) real_user=u;
                else {
                    struct passwd* pw=getpwuid(getuid());
                    real_user = (pw && pw->pw_name) ? pw->pw_name : "diggy";
                }
            }
        }
    }
    const char* display = getenv("DISPLAY");
    if (!display) display = ":0";
    const char* xauthority = getenv("XAUTHORITY");
    if (!xauthority) xauthority = "";
    char runtime_dir[256];
    const char* env_runtime = getenv("XDG_RUNTIME_DIR");
    if (env_runtime) snprintf(runtime_dir, sizeof(runtime_dir), "%s", env_runtime);
    else snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/1000");
    // daemon double-fork so app outlives launcher - handle both sudo and nohup cases
    char disp_env[256], xauth_env[512], runtime_env[512];
    snprintf(disp_env, sizeof(disp_env), "DISPLAY=%s", display);
    snprintf(xauth_env, sizeof(xauth_env), "XAUTHORITY=%s", xauthority);
    snprintf(runtime_env, sizeof(runtime_env), "XDG_RUNTIME_DIR=%s", runtime_dir);
    pid_t pid = fork();
    if (pid==0){
        setsid();
        pid_t pid2=fork();
        if(pid2<0) _exit(1);
        if(pid2>0) _exit(0);
        int fd=open("/dev/null",O_RDWR); if(fd>=0){dup2(fd,0);dup2(fd,1);dup2(fd,2); if(fd>2) close(fd);}
        if(getuid()==0){
            char* const argv[] = {"sudo","-u",(char*)real_user,"env",disp_env,xauth_env,runtime_env,"sh","-c",cmd,NULL};
            execvp(argv[0], argv);
        } else {
            setenv("DISPLAY",display,1); setenv("XAUTHORITY",xauthority,1); setenv("XDG_RUNTIME_DIR",runtime_dir,1);
            execl("/bin/sh","sh","-c",cmd,(char*)NULL);
        }
        _exit(127);
    } else if(pid>0){
        int st=0; waitpid(pid,&st,0);
    }
#endif
}

static void applauncher_enter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_by_name(selection);
}

static void applauncher_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    launch_app_as_admin(selection);
    selection_complete = true;
}

static void applauncher_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;

    const char* names[MAX_APPS];
    int count = (G_APP_COUNT < MAX_APPS) ? G_APP_COUNT : MAX_APPS;
    
    for (int i = 0; i < count; i++) {
        names[i] = G_APPS[i].name;
    }
    PopulateListBox(lb, hListBox, filter, names, count, NULL);
}
static void applauncher_copy_callback(const char* selection,int true_index,int type) {
    char* text = (char*)selection;
    switch(type) {
      case 0: {
        int start = strlen(text);
        for (int i = start; i > 0 ; i--) {
          if (text[i] == '\\') {
            for (int j = i; j < start; ++j){
                text[j] = '\0';
            }
            break;
          }
        }
        break;
      }
      case 1: {
        if (true_index >= 0 && true_index < G_APP_COUNT) {
            text = G_APPS[true_index].path;
        }
        break;
      }
    }
    Platform_CopyToClipboard(text);
}

void TriggerAppLauncher(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char** initial_items = (const char**)malloc(G_APP_COUNT * sizeof(char*));
    if (!initial_items) return;
    
    for (int i = 0; i < G_APP_COUNT; i++) {
        initial_items[i] = G_APPS[i].name;
    }

    PROMPT(.title = "🚀 Launcher", .items = initial_items, .count = G_APP_COUNT,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback);
    free(initial_items);
}
