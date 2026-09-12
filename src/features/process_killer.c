#include "core/flapi.h"

const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam","Wine"};

char PK_LIST[256][128];
int PK_COUNT = 0;

void collect_running_processes() {
    PK_COUNT = fl_collect_running_processes(PK_LIST, 256);
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
bool pk_enter_callback(const char* selection, ListBoxInfo* lb) {
      fl_terminate_process_by_name(selection);
    return true;
}

void pk_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char** items = (const char**)malloc(PK_COUNT * sizeof(char*));
    if (!items) return;
    for (int i = 0; i < PK_COUNT; i++) {
        items[i] = PK_LIST[i];
    }
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, PK_COUNT, NULL);
    free(items);
}
void TriggerProcessKiller(void) {
    collect_running_processes();

    const char** list_items = (const char**)malloc(PK_COUNT * sizeof(char*));
    if (!list_items) {
        return;
    }

    for (int i = 0; i < PK_COUNT; i++) {
        list_items[i] = PK_LIST[i];
    }

    PROMPT(.title = "💀 ProcessKiller", .items = list_items, .count = PK_COUNT,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = pk_refresh_callback,
           .enter = pk_enter_callback);
    free(list_items);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+U", TriggerProcessKiller);
    fl_register_hotkey("Alt+Shift+P", bulk_kill_bkp);

    fl_register_feature("Process Killer", TriggerProcessKiller);
}
