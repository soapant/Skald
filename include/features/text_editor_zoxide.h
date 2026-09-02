#ifndef TEXT_EDITOR_ZOXIDE_H
#define TEXT_EDITOR_ZOXIDE_H

#include "core/flapi.h"
#include "core/zoxide_helpers.h"
#include "core/platform.h"

static const char* TextEditorZoxideFilePath = "data/NvimZoxide.txt";
static MappedFile TextEditorZoxideFileData = {0};
static bool TextEditorZoxideFileInit = false;
static MappedFile* GetTextEditorZoxideFile(void) {
    if (!TextEditorZoxideFileInit) {
#if !defined(_WIN32)
        TextEditorZoxideFileData.fd = -1;
#endif
        Platform_FileInit(TextEditorZoxideFilePath, &TextEditorZoxideFileData);
        TextEditorZoxideFileInit = true;
    }
    return &TextEditorZoxideFileData;
}
#define TextEditorZoxideFile GetTextEditorZoxideFile()


static char** g_cached_text_editor_commands = NULL;
static int g_cached_text_editor_count = 0;

static void Rebuildtext_editorCache(void) {
    ZoxideHelper_RebuildCache(TextEditorZoxideFile, &g_cached_text_editor_commands, &g_cached_text_editor_count);
}


static void text_editor_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    ZoxideHelper_Refresh(filter, hListBox, lb, TextEditorZoxideFile, &g_cached_text_editor_commands, &g_cached_text_editor_count);
}


static void text_editor_enter_callback(const char* selection, ListBoxInfo* lb) {
      char* path = fl_get_zoxide_match(selection);
      if (path) {
          char cmd_args[512];
          snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"",UsedTextEditor,path);
          fl_launch_terminal_at_path(path, cmd_args);
          ZoxideHelper_AppendNewEntry(TextEditorZoxideFile, selection, lb, &g_cached_text_editor_commands, &g_cached_text_editor_count);
          free(path);
      }
}

static void text_editor_edit_callback(const char* selection, ListBoxInfo* lb) {
    ZoxideHelper_Edit(selection, lb, TextEditorZoxideFile, &g_cached_text_editor_commands, &g_cached_text_editor_count, text_editor_refresh_callback);
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
