#ifndef PROCESS_KILLER_H
#define PROCESS_KILLER_H

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include "config.h"
#include <stdbool.h>
#include <shlwapi.h>
#include "types.h"

#define MAX_PK_APPS 256

static char G_PK_LIST[MAX_PK_APPS][128];
static int G_PK_COUNT = 0;

extern char* PromptQuickSwitcher(const char* window_list[], int count, int height, int width, 
                                 RefreshCallback refresh_callback, 
                                 EnterCallback enter_callback,
                                 CtrlEnterCallback ctrl_enter_callback,
                                 TabCallback tab_callback);

// Programmatically filter out core protected Windows OS components to save list space
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
        if (StrCmpIA(exe, noise[i]) == 0) return true;
    }
    return false;
}

// Scans the raw NT kernel process table directly
static void collect_running_processes() {
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
}

// Helper function to run a command without showing a window
static void run_hidden_command(const char* cmd) {
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    // Create a modifiable copy of the command
    char cmd_copy[1024];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    if (CreateProcessA(NULL, cmd_copy, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);  // Wait for it to complete
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        // Fallback to WinExec if CreateProcess fails
        WinExec(cmd, SW_HIDE);
    }
}

static void terminate_process_by_name(const char* exe_name) {
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

static bool HandleBulkKillProcess(int vk_code, int modifiers, const char* current_title) {
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
    if (selection != NULL && strlen(selection) > 0) {
        terminate_process_by_name(selection);
    }
}

static void pk_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
    
    if (filter == NULL || strlen(filter) == 0) {
        for (int i = 0; i < G_PK_COUNT; i++) {
            SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_PK_LIST[i]);
        }
    } else {
        for (int i = 0; i < G_PK_COUNT; i++) {
            if (StrStrIA(G_PK_LIST[i], filter) != NULL) {
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)G_PK_LIST[i]);
            }
        }
    }
    SendMessageA(hListBox, LB_SETCURSEL, 0, 0);
}

void TriggerProcessKiller(HWND hPopupWnd) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) return;

    collect_running_processes();

    const char** list_items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!list_items) {
        return;
    }

    for (int i = 0; i < G_PK_COUNT; i++) {
        list_items[i] = G_PK_LIST[i];
    }

    // Open your UI window with callbacks
    PromptQuickSwitcher(list_items, G_PK_COUNT, 220, 250, 
                        pk_refresh_callback,        
                        pk_enter_callback,          
                        NULL,                      
                        NULL);                     
    free(list_items);
}

#endif
