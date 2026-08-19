#include "common.h"
#include "features/process_killer.h"
#if defined(_WIN32)
#include <tlhelp32.h>
#endif
#include "core/common_ui.h"

static char G_PK_LIST[MAX_PK_APPS][128];
static int G_PK_COUNT = 0;

static bool is_system_noise(const char* exe) {
    const char* noise[] = {
        "[system process]", "system", "idle", "svchost.exe", "lsass.exe", 
        "services.exe", "wininit.exe", "csrss.exe", "smss.exe", 
        "winlogon.exe", "spoolsv.exe", "dasHost.exe", "conhost.exe",
        "sihost.exe", "RuntimeBroker.exe", "ctfmon.exe", "SearchHost.exe",
        "SearchIndexer.exe", "wmiprvse.exe", "dllhost.exe", "audiodg.exe",
        "taskhostw.exe", "dwm.exe", "fontdrvhost.exe", "Registry","TextInputHost.exe"
    };
    for (size_t i = 0; i < sizeof(noise) / sizeof(noise[0]); i++) {
#if defined(_WIN32)
        if (StrCmpIA(exe, noise[i]) == 0) return true;
#else
        if (strcasecmp(exe, noise[i]) == 0) return true;
#endif
    }
    return false;
}

static void collect_running_processes() {
#if defined(_WIN32)
    G_PK_COUNT = 0;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (!is_system_noise(pe.szExeFile)) {
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
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
#else
    G_PK_COUNT = 0;
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
    (void)cmd;
#endif
}

void terminate_process_by_name(const char* exe_name) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", exe_name);
    run_hidden_command(cmd);
}

static void BulkKillProcesses(const char* window_titles[], int count) {
    for (int i = 0; i < count; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", window_titles[i]);
        run_hidden_command(cmd);
    }
}

bool HandleBulkKillProcess(int vk_code, int modifiers, const char* current_title) {
    for (int i = 0; i < BULKKILLPROCESS_COUNT; i++) {
        const BulkKillProcessBinding* binding = &G_BULK_KILL_PROCESS[i];
        if (binding->vk_code == vk_code && binding->modifiers == modifiers) {
            BulkKillProcesses(binding->window_titles, binding->window_count);
            return true;
        }
    }
    return false;
}
static void pk_enter_callback(const char* selection) {
      terminate_process_by_name(selection);
}

static void pk_refresh_callback(const char* filter, WindowHandle hListBox) {
    const char** items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < G_PK_COUNT; i++) {
        items[i] = G_PK_LIST[i];
    }
    PopulateListBox(hListBox, filter, items, G_PK_COUNT, NULL);
    free(items);
}

void TriggerProcessKiller(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    collect_running_processes();

    const char** list_items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!list_items) {
        return;
    }

    for (int i = 0; i < G_PK_COUNT; i++) {
        list_items[i] = G_PK_LIST[i];
    }

    ImportantPromptUser(list_items,"💀 ProcessKiller", G_PK_COUNT, 920, 650,32,
                        pk_refresh_callback,        
                        pk_enter_callback,          
                        NULL,NULL);
    free(list_items);
}
