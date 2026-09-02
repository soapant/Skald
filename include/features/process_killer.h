#ifndef PROCESS_KILLER_H
#define PROCESS_KILLER_H

#include "core/flapi.h"


const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam"};

#define MAX_PK_APPS 256

char G_PK_LIST[MAX_PK_APPS][128];
int G_PK_COUNT = 0;

void collect_running_processes() {
    G_PK_COUNT = fl_collect_running_processes(G_PK_LIST, MAX_PK_APPS);
}

void BulkKillProcesses(const char* window_titles[], int count) {
    for (int i = 0; i < count; i++) {
        if (!window_titles[i] || !*window_titles[i]) continue;
        fl_terminate_process_by_name(window_titles[i]);
    }
}

void bulk_kill_bkp(void) {
    BulkKillProcesses(bkp_windows, ARRAY_LEN(bkp_windows));
}
void pk_enter_callback(const char* selection, ListBoxInfo* lb) {
      fl_terminate_process_by_name(selection);
}

void pk_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char** items = (const char**)malloc(G_PK_COUNT * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < G_PK_COUNT; i++) {
        items[i] = G_PK_LIST[i];
    }
    PopulateListBox(lb, hListBox, filter, items, G_PK_COUNT, NULL);
    free(items);
}
void TriggerProcessKiller(void) {
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
