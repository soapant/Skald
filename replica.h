#ifndef REPLICA_CONTROL_H
#define REPLICA_CONTROL_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "window_switcher.h"

#define REPLICA_WINDOW_CLASS "ReplicaWindowClass"

static WindowList g_replica_windows = {0};
static HWND g_replica_selected_hwnd = NULL;

static HWND FindReplicaWindow(void) {
    return FindWindowA(REPLICA_WINDOW_CLASS, NULL);
}

static void LaunchReplicaWithHWND(HWND target) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %p", ReplicaPath, target);
    
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("[REPLICA] Launched: %s\n", cmd);
    } else {
        printf("[REPLICA] Failed to launch: %s (Error: %lu)\n", ReplicaPath, GetLastError());
    }
}


static void SendTargetToReplica(HWND target) {
    HWND hReplica = FindWindowA("ReplicaWindowClass", NULL);
    if (hReplica) {
        SendMessage(hReplica, WM_USER + 1, (WPARAM)target, 0);
        printf("[REPLICA] Sent new target HWND: %p\n", target);
    } else {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "D:\\Projects\\Replica\\replica.exe %p", target);
        system(cmd);
    }
}


static void replica_refresh_callback(const char* filter, HWND hListBox) {
    if (!hListBox) return;
    
    WindowUtils_GetAll(&g_replica_windows);
    
    int count = 0;
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strlen(g_replica_windows.items[i].title) > 0) count++;
    }
    
    const char** titles = (const char**)malloc(count * sizeof(char*));
    int idx = 0;
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strlen(g_replica_windows.items[i].title) > 0) {
            titles[idx] = g_replica_windows.items[i].title;
            idx++;
        }
    }
    
    PopulateListBox(hListBox, filter, titles, count, NULL);
    free(titles);
}


static void replica_enter_callback(const char* selection) {
    g_replica_selected_hwnd = NULL;
    
    if (!selection) return;
    
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strcmp(g_replica_windows.items[i].title, selection) == 0) {
            g_replica_selected_hwnd = g_replica_windows.items[i].hwnd;
            return;
        }
    }
}

static void TriggerReplica(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    if (!ReplicaAvailable) return;
    
    g_replica_selected_hwnd = NULL;
    
    WindowUtils_GetAll(&g_replica_windows);
    
    int count = 0;
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strlen(g_replica_windows.items[i].title) > 0) count++;
    }
    
    if (count == 0) {
        printf("[REPLICA] No windows found\n");
        return;
    }
    
    const char** titles = (const char**)malloc(count * sizeof(char*));
    int idx = 0;
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strlen(g_replica_windows.items[i].title) > 0) {
            titles[idx] = g_replica_windows.items[i].title;
            idx++;
        }
    }
    
    ImportantPromptUser(
        titles,
        "🔍 Replica",
        count,
        920, 650, 32,
        replica_refresh_callback,
        replica_enter_callback,
        NULL,
        NULL
    );
    
    free(titles);
    
    if (g_replica_selected_hwnd) {
        SendTargetToReplica(g_replica_selected_hwnd);
    }
}

#endif
