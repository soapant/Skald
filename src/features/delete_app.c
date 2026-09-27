#include "core/flapi.h"
#include "config.h"
#include "core/context.h"
#include "helpers/helpers_text.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void delete_app_refresh(const char* filter, ListBoxInfo* lb) {

    char* output = fl_run_terminal_command("pacman -Qe");
    if (!output) return;
    StringList lines = String_Split(output, "\n");
    const char** names = (const char**)malloc(lines.count * sizeof(char*));
    for (size_t i = 0; i < lines.count; i++) {
      char* space = strchr(lines.items[i], ' ');
      if (space) {
        *space = '\0';
      }
      names[i] = lines.items[i];
    }
    PopulateListBox(lb, filter, names,lines.count, NULL);
    free(names);
}

bool delete_app_enter(const char* selection, ListBoxInfo* lb) {
    if (!UI_IsListBoxSelected(lb)) return false;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "pacman -Rns %s", selection);
    char* output = fl_run_terminal_command(cmd);
    if (fl_string_contains(output,"root")) {
        char* pass = fl_popup(.title="Input Sudo Password");
        printf("pass=%s \n",pass);
    }

    return true;
}

bool delete_app_ctrl_enter(const char* selection, ListBoxInfo* lb) {
    return false;
}

void TriggerDeleteApp(void) {
    PROMPT(.title = "Delete App",
           .refresh = delete_app_refresh,
           .enter = delete_app_enter,
           .ctrl_enter = delete_app_ctrl_enter);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+Shift+N", TriggerDeleteApp);
    fl_register_feature("Delete App", TriggerDeleteApp);
}
