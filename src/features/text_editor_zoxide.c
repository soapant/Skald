#include "features/api.h"
#include "core/flapi.h"
#include "core/zoxide_helpers.h"

const char* TextEditorZoxideFilePath = "data/NvimZoxide.txt";

char** cached_text_editor_commands = NULL;
int cached_text_editor_count = 0;

void Rebuildtext_editorCache(void) {
    ZoxideHelper_RebuildCache(TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count);
}


void text_editor_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count);
}


void text_editor_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = fl_get_zoxide_match((char*)selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"",UsedTextEditor,path);
          fl_launch_terminal_at_path(path, cmd_args);
          ZoxideHelper_AppendNewEntry(TextEditorZoxideFilePath, selection, lb, &cached_text_editor_commands, &cached_text_editor_count);
          free(path);
      }
}

void text_editor_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count, text_editor_refresh_callback);
}

void TriggerTextEditorZoxide(void) {
    if (!cached_text_editor_commands) Rebuildtext_editorCache();
    const char** list = ZoxideHelper_PrepareTriggerList(TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count, NULL);
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"📂 %s",UsedTextEditor);
    PROMPT(.title = buffer, .items = list, .count = cached_text_editor_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = text_editor_refresh_callback,
           .enter = text_editor_enter_callback,
           .edit = text_editor_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

static inline void text_editor_zoxide_init(void) {
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide);
}

FLINT_FEATURE_INIT {
    text_editor_zoxide_init();
}
