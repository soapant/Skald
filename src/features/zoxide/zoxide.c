#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>

char *get_zoxide_match(char *query) {
  char clean_query[512];
  strncpy(clean_query, query, sizeof(clean_query) - 1);
  clean_query[sizeof(clean_query) - 1] = '\0';
  clean_query[strcspn(clean_query, "\r\n")] = '\0';

  const char *real_home = fl_resolve_user_path();

  char command[1024];
#if defined(_WIN32)
  snprintf(command, sizeof(command),
           "cmd.exe /c \"set \"HOME=%s\" & set "
           "\"_ZO_DATA_DIR=%s\\AppData\\Local\\zoxide\" & zoxide query -- "
           "\"%s\" 2>&1\"",
           real_home, real_home, clean_query);
#elif defined(__linux__)
  snprintf(command, sizeof(command),
           "export HOME=\"%s\"; "
           "export _ZO_DATA_DIR=\"%s/.local/share/zoxide\"; "
           "export PATH=\"%s/.local/bin:%s/.cargo/bin:/usr/local/bin:$PATH\"; "
           "zoxide query -- \"%s\" 2>&1",
           real_home, real_home, real_home, real_home, clean_query);
#endif

  char *out = fl_run_terminal_command(command);
  if (!out) {
    return NULL;
  }
  out[strcspn(out, "\r\n")] = '\0';

  if (strstr(out, "no match found") || strlen(out) == 0) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Query {%s} Doesnt Have A Zoxide Path",
             query);
    DisplayMessageSlot(buffer, TOP_CENTER_RELATIVE, 0, 0, 0, TimeOutForMessages,
                       COLOR_ERROR, NULL, NULL);
    free(out);
    return NULL;
  }
  return out;
}

const char *TerminalZoxideFilePath = "TerminalZoxide.txt";
static void zoxide_refresh(PromptState *state) {
  FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
  fl_populate_listbox(state->listbox,state->editbox->filter, (const char **)files.items, files.count);
  fl_free_file_list(&files);
}
static void zoxide_edit(PromptState *state) {
  int idx = -1;
  char *edited = fl_edit_text_popup(.initial_text = state->listbox->selected_text, .lb = state->listbox,
                                    .out_index = &idx);
  if (!edited)
    return;
  if (idx >= 0)
    fl_replace_line_in_file(TerminalZoxideFilePath, idx, edited);
  free(edited);
  zoxide_refresh(state);
}
static bool zoxide_enter(PromptState *state) {
  char* text = state->listbox->selected_text ? state->listbox->selected_text : state->editbox->filter;
  if (!fl_is_selected(state->listbox)) {
      fl_append_unique_line(TerminalZoxideFilePath, text);
  }
  char* path = get_zoxide_match(text);
  fl_launch_terminal_at_path(path, NULL);
  free(path);
  return true;
}
static void TriggerZoxide(void) {
  FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
  fl_window(.title = "⌨️ Zoxide", .items = (const char **)files.items,
            .refresh = zoxide_refresh, .enter = zoxide_enter,
            .edit = zoxide_edit);
  fl_free_file_list(&files);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey(.hotkey = "Alt+W", .cb = TriggerZoxide);

  fl_add_to_cc(.display_name = "Zoxide", .on_action = TriggerZoxide,
               .icon = "/folder.png");
}
