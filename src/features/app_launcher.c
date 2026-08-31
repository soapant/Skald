#include "core/flapi.h"

const char* ListOfShitToIgnore[] = {
  "node_modules",
  ".git",
  "bin",
  "dll",
  "gtnh 2.8",
  ".h",
  "yazi",
  "obj",
  "build"
};

const char* ListOfProgramPaths[20] = {
#if defined(_WIN32)
    "{APPDATA}/Microsoft/Windows/Start Menu",
    "{PROGRAMDATA}/Microsoft/Windows/Start Menu/Programs",
    "{DEV}",
    "{MISC}",
    "{SystemRoot}/System32/",             
    "shell:AppsFolder",
#else
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/home/diggy/.local/share/applications",
    "/usr/bin",
    "/home/diggy/.local/bin",
    "{DEV}",
    "{MISC}",
#endif
};

static ListOfFiles s_apps = {0};

void initialize_app_cache(void) {
#if defined(_WIN32)
    char* allowed[] = {"lnk", "exe", "url", NULL};
#else
    char* allowed[] = {"desktop", "sh", "lnk", "exe", "url", NULL};
#endif
    for (int i = 0; i < sizeof(ListOfProgramPaths) / sizeof(ListOfProgramPaths[0]); i++) {
        if (!ListOfProgramPaths[i] || ListOfProgramPaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfProgramPaths[i], allowed, &s_apps, (char**)ListOfShitToIgnore);
    }
    
#if defined(_WIN32)
    for (int shortcut_idx = 0; shortcut_idx < SYSTEM_SHORTCUT_COUNT; shortcut_idx++) {
        if (s_apps.count >= MAX_APPS) break;
        
        bool found = false;
        for (size_t app_idx = 0; app_idx < s_apps.count; app_idx++) {
            if (StrStrIA(s_apps.items[app_idx].path, SystemShortcuts[shortcut_idx].path) != NULL) {
                strncpy(s_apps.items[app_idx].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[app_idx].name) - 1);
                s_apps.items[app_idx].name[sizeof(s_apps.items[app_idx].name) - 1] = '\0';
                found = true;
                break;
            }
        }
        if (!found) {
            if (s_apps.count >= s_apps.capacity) {
                s_apps.capacity = s_apps.capacity == 0 ? 10 : s_apps.capacity * 2;
                s_apps.items = realloc(s_apps.items, s_apps.capacity * sizeof(FileItem));
            }
            strncpy(s_apps.items[s_apps.count].name, SystemShortcuts[shortcut_idx].name, sizeof(s_apps.items[s_apps.count].name) - 1);
            s_apps.items[s_apps.count].name[sizeof(s_apps.items[s_apps.count].name) - 1] = '\0';
            
            strncpy(s_apps.items[s_apps.count].path, SystemShortcuts[shortcut_idx].path, sizeof(s_apps.items[s_apps.count].path) - 1);
            s_apps.items[s_apps.count].path[sizeof(s_apps.items[s_apps.count].path) - 1] = '\0';
            
            s_apps.count++;
        }
    }

#endif

}
void launch_app_by_name(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
#if defined(_WIN32)
        if (StrCmpIA(s_apps.items[i].name, display_name) == 0) {
#else
        if (strcasecmp(s_apps.items[i].name, display_name) == 0) {
#endif
            Platform_LaunchApp(s_apps.items[i].path);
            return;
        }
    }
}

static void launch_app_as_admin(const char* display_name) {
    for (size_t i = 0; i < s_apps.count; i++) {
#if defined(_WIN32)
        if (StrCmpIA(s_apps.items[i].name, display_name) == 0) {
#else
        if (strcasecmp(s_apps.items[i].name, display_name) == 0) {
#endif
            Platform_LaunchAppAsAdmin(s_apps.items[i].path);
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
    const char* display = Platform_GetSessionDisplay();
    const char* xauthority = Platform_GetSessionXAuthority();
    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "%s", Platform_GetRuntimeDir());
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

    const char** names = (const char**)malloc(s_apps.count * sizeof(char*));
    if (!names) return;
    for (size_t i = 0; i < s_apps.count; i++) {
        names[i] = s_apps.items[i].name;
    }
    PopulateListBox(lb, hListBox, filter, names, (int)s_apps.count, NULL);
    free(names);
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
        if (true_index >= 0 && (size_t)true_index < s_apps.count) {
            text = s_apps.items[true_index].path;
        }
        break;
      }
    }
    fl_copy_into_clipboard(text);
}

void TriggerAppLauncher(void) {
    const char** initial_items = (const char**)malloc(s_apps.count * sizeof(char*));
    if (!initial_items) return;
    
    for (size_t i = 0; i < s_apps.count; i++) {
        initial_items[i] = s_apps.items[i].name;
    }

    PROMPT(.title = "🚀 Launcher", .items = initial_items, .count = (int)s_apps.count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = applauncher_refresh_callback,
           .enter = applauncher_enter_callback,
           .ctrl_enter = applauncher_ctrlenter_callback,
           .copy = applauncher_copy_callback);
    free(initial_items);
}
