#include "core/flapi.h"
#include "config.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void delete_app_refresh(const char* filter, ListBoxInfo* lb) {

    char* output = fl_run_terminal_command("pacman -Qe");
    if (!output) return;
    FlStringList lines = fl_string_split(output, "\n");
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
    if (!UI_IsListBoxSelected(lb)) {fflush(stdout); return true; }
    const char* probe[] = { "pacman", "-Rns", "--noconfirm", selection, NULL };
    int rc = 0;
    char* output = fl_run_argv(probe, NULL, &rc);
    if (!output) return true;
    bool has_root = fl_string_contains(output, "root");
    bool needs_root = rc != 0 || has_root;
    free(output);
    if (!needs_root) { fflush(stdout); return true; }
    char* pass = fl_popup(.placeholder = "Input Sudo Password");
    const char* argv[] = { "sudo", "-S", "-k", "pacman", "-Rns", "--noconfirm", selection, NULL };
    int src = 0;
    char* res = fl_run_argv(argv, pass, &src);
    memset(pass, 0, strlen(pass));
    free(pass);
    if (res) free(res);
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
