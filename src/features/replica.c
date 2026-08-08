#include "common.h"
#include "features/replica.h"
#include "features/window_switcher.h"
#include "core/common_ui.h"

static WindowList g_replica_windows = {0};
static HWND g_replica_selected_hwnd = NULL;

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
    
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strcmp(g_replica_windows.items[i].title, selection) == 0) {
            g_replica_selected_hwnd = g_replica_windows.items[i].hwnd;
            return;
        }
    }
}

void TriggerReplica(void) {
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
