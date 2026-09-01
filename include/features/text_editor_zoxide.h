#ifndef TEXT_EDITOR_ZOXIDE_H
#define TEXT_EDITOR_ZOXIDE_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"

#define FILE_TEXT_EDITOR_ZOXIDE 1
#define TEXT_EDITOR_ZOXIDEFILE G_FILE_ENTRIES[FILE_TEXT_EDITOR_ZOXIDE].mf

static char** g_cached_text_editor_commands = NULL;
static int g_cached_text_editor_count = 0;

static void Rebuildtext_editorCache(void) {
    ZoxideHelper_RebuildCache(TEXT_EDITOR_ZOXIDEFILE, &g_cached_text_editor_commands, &g_cached_text_editor_count);
}


static void text_editor_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TEXT_EDITOR_ZOXIDEFILE, &g_cached_text_editor_commands, &g_cached_text_editor_count);
}


static void text_editor_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = fl_get_zoxide_match(selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"",UsedTextEditor,path);
          fl_launch_terminal_at_path(path, cmd_args);
          ZoxideHelper_AppendNewEntry(TEXT_EDITOR_ZOXIDEFILE, selection, lb, &g_cached_text_editor_commands, &g_cached_text_editor_count);
          free(path);
      }
}

static void text_editor_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TEXT_EDITOR_ZOXIDEFILE, &g_cached_text_editor_commands, &g_cached_text_editor_count, text_editor_refresh_callback);
}

void TriggerTextEditorZoxide(void) {
    if (!g_cached_text_editor_commands) Rebuildtext_editorCache();
    const char** list = ZoxideHelper_PrepareTriggerList(NULL, &g_cached_text_editor_commands, &g_cached_text_editor_count, NULL);
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"📂 %s",UsedTextEditor);
    PROMPT(.title = buffer, .items = list, .count = g_cached_text_editor_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = text_editor_refresh_callback,
           .enter = text_editor_enter_callback,
           .edit = text_editor_edit_callback);
    ZoxideHelper_CleanupTriggerList(list);
}

static inline void text_editor_zoxide_init(void) {
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide, NULL, NULL);
}

#endif
