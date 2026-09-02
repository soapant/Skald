#ifndef PROCESS_KILLER_H
#define PROCESS_KILLER_H

#include "core/flapi.h"

typedef struct {
    int vk_code;
    int modifiers;
    const char** window_titles;  
    int window_count;
} BulkKillProcessBinding;

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#endif

static const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam"};

#define MAX_PK_APPS 256

static char G_PK_LIST[MAX_PK_APPS][128];
static int G_PK_COUNT = 0;

static void collect_running_processes() {
#if defined(_WIN32)
    G_PK_COUNT = 0;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            bool exists = false;
            for (int i = 0; i < G_PK_COUNT; i++) {
                if (StrCmpIA(G_PK_LIST[i], pe.szExeFile) == 0) {
                    exists = true;
                    break;
                }
            }

            if (!exists && G_PK_COUNT < MAX_PK_APPS) {
                strncpy(G_PK_LIST[G_PK_COUNT], pe.szExeFile, 127);
                G_PK_LIST[G_PK_COUNT][127] = '\0';
                G_PK_COUNT++;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
#else
    G_PK_COUNT = 0;
    DIR* proc = opendir("/proc");
    if (!proc) return;
    struct dirent* entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        char comm_path[512];
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        FILE* f = fopen(comm_path, "r");
        if (!f) continue;
        char exe[256];
        if (!fgets(exe, sizeof(exe), f)) { fclose(f); continue; }
        fclose(f);
        size_t len = strlen(exe);
        while (len>0 && (exe[len-1]=='\n' || exe[len-1]=='\r')) exe[--len]='\0';
        if (len==0) continue;
        {
            char stat_path[512];
            snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);
            FILE* fstat = fopen(stat_path, "r");
            if (fstat) {
                char stat_buf[1024];
                if (fgets(stat_buf, sizeof(stat_buf), fstat)) {
                    char* rparen = strrchr(stat_buf, ')');
                    if (rparen) {
                        int ppid = 0;
                        if (sscanf(rparen+2, "%*c %d", &ppid)==1 && ppid==2) {
                            fclose(fstat);
                            continue;
                        }
                    }
                }
                fclose(fstat);
            }
        }
        bool exists=false;
        for (int i=0;i<G_PK_COUNT;i++) if (strcasecmp(G_PK_LIST[i], exe)==0) {exists=true;break;}
        if (!exists && G_PK_COUNT < MAX_PK_APPS) {
            strncpy(G_PK_LIST[G_PK_COUNT], exe, 127);
            G_PK_LIST[G_PK_COUNT][127]='\0';
            G_PK_COUNT++;
        }
    }
    closedir(proc);
#endif
}

static void run_hidden_command(const char* cmd) {
#if defined(_WIN32)
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    char cmd_copy[1024];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    if (CreateProcessA(NULL, cmd_copy, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);  // Wait for it to complete
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        WinExec(cmd, SW_HIDE);
    }
#else
    if (!cmd) return;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s >/dev/null 2>&1", cmd);
    system(buf);
#endif
}

static void terminate_process_by_name(const char* exe_name) {
#if defined(_WIN32)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", exe_name);
    run_hidden_command(cmd);
#else
    if (!exe_name || !*exe_name) return;
    char *ext = strrchr(exe_name, '.');
    if (ext && strcmp(ext, ".exe") == 0) {
        *ext = '\0';
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pkill -f \"%s\"", exe_name);
    run_hidden_command(cmd);
    snprintf(cmd, sizeof(cmd), "killall \"%s\"", exe_name);
    run_hidden_command(cmd);
#endif
}

static void BulkKillProcesses(const char* window_titles[], int count) {
#if defined(_WIN32)
    for (int i = 0; i < count; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", window_titles[i]);
        run_hidden_command(cmd);
    }
#else
    for (int i = 0; i < count; i++) {
        if (!window_titles[i] || !*window_titles[i]) continue;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "pkill -f \"%s\"", window_titles[i]);
        run_hidden_command(cmd);
    }
#endif
}

static void bulk_kill_bkp(void) {
    BulkKillProcesses(bkp_windows, ARRAY_LEN(bkp_windows));
}
static void pk_enter_callback(const char* selection, ListBoxInfo* lb) {
      terminate_process_by_name(selection);
}

static void pk_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char** items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < G_PK_COUNT; i++) {
        items[i] = G_PK_LIST[i];
    }
    PopulateListBox(lb, hListBox, filter, items, G_PK_COUNT, NULL);
    free(items);
}
static void TriggerProcessKiller(void) {
    collect_running_processes();

    const char** list_items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!list_items) {
        return;
    }

    for (int i = 0; i < G_PK_COUNT; i++) {
        list_items[i] = G_PK_LIST[i];
    }

    PROMPT(.title = "💀 ProcessKiller", .items = list_items, .count = G_PK_COUNT,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = pk_refresh_callback,
           .enter = pk_enter_callback);
    free(list_items);
}

static inline void process_killer_init(void) {
    fl_register_hotkey("Alt+U", TriggerProcessKiller, NULL, NULL);
    fl_register_hotkey("Alt+Ctrl+P", bulk_kill_bkp, NULL, NULL);
}

#endif
