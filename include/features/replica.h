#ifndef REPLICA_H
#define REPLICA_H

#include "common.h"
#include "core/flapi.h"
#include "features/replica.h"
#include "features/window_switcher.h"
#include "core/common_ui.h"

static WindowList g_replica_windows = {0};
static WindowHandle g_replica_selected_hwnd = WH_NULL;


static void SendTargetToReplica(WindowHandle target) {
#if defined(_WIN32)
    HWND hReplica = FindWindowA("ReplicaWindowClass", NULL);
    if (hReplica) {
        SendMessage(hReplica, WM_USER + 1, (WPARAM)WH_TO_HWND(target), 0);
        printf("[REPLICA] Sent new target HWND: %p\n", WH_TO_HWND(target));
    } else {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "D:\\Projects\\Replica\\replica.exe %p", WH_TO_HWND(target));
        system(cmd);
    }
#else
    (void)target;
#endif
}


static void replica_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    if (!lb) return;
    
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
    
    PopulateListBox(lb, hListBox, filter, titles, count, NULL);
    free(titles);
}


static void replica_enter_callback(const char* selection, ListBoxInfo* lb) {
    g_replica_selected_hwnd = WH_NULL;
    
    for (int i = 0; i < g_replica_windows.count; i++) {
        if (strcmp(g_replica_windows.items[i].title, selection) == 0) {
            g_replica_selected_hwnd = g_replica_windows.items[i].hwnd;
            return;
        }
    }
}

void TriggerReplica(void) {
    g_replica_selected_hwnd = WH_NULL;
    
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
    
    PROMPT(.title = "🔍 Replica", .items = titles, .count = count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = replica_refresh_callback,
           .enter = replica_enter_callback);
    
    free(titles);
    
    if (WH_TO_HWND(g_replica_selected_hwnd) != NULL) {
        SendTargetToReplica(g_replica_selected_hwnd);
    }
}

static inline void replica_init(void) {
    if (!ReplicaAvailable) return;
    fl_register_hotkey("Alt+Shift+R", TriggerReplica, NULL, NULL);
}

#endif
