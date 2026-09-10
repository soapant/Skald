#include "core/flapi.h"

const char* TerminalZoxideFilePath = "data/TerminalZoxide.txt";
static void zoxide_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static void zoxide_edit(const char* selection, ListBoxInfo* lb) {
    fl_edit_text_popup(selection, "Edit Zoxide", 960, 540, TerminalZoxideFilePath, lb, zoxide_refresh);
}
static bool zoxide_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        fl_launch_terminal_at_path(path, NULL);
        if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(TerminalZoxideFilePath, selection);
    }
    return true;
}
static void TriggerZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "⌨️ Zoxide", .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = zoxide_refresh, .enter = zoxide_enter, .edit = zoxide_edit);
    fl_free_file_list(items, n);
}

const char* TextEditorZoxideFilePath = "data/NvimZoxide.txt";
static void text_editor_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TextEditorZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool text_editor_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        char cmd_args[512];
        snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"", UsedTextEditor, path);
        fl_launch_terminal_at_path(path, cmd_args);
        if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(TextEditorZoxideFilePath, selection);
    }
    return true;
}
static void text_editor_edit(const char* selection, ListBoxInfo* lb) {
    fl_edit_text_popup(selection, "Edit TextEditor", 960, 540, TextEditorZoxideFilePath, lb, text_editor_refresh);
}
static void TriggerTextEditorZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TextEditorZoxideFilePath, items, MAX_FILE_LINES);
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"📂 %s",UsedTextEditor);
    PROMPT(.title = buffer, .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = text_editor_refresh, .enter = text_editor_enter, .edit = text_editor_edit);
    fl_free_file_list(items, n);
}

const char* YaziZoxideFilePath = "data/YaziZoxide.txt";
static void yazi_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(YaziZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool yazi_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        char cmd_args[512];
        snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
        fl_launch_terminal_at_path(path, cmd_args);
        if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(YaziZoxideFilePath, selection);
    }
    return true;
}
static void yazi_edit(const char* selection, ListBoxInfo* lb) { 
  fl_edit_text_popup(selection, "Edit Yazi", 960, 540, YaziZoxideFilePath, lb, yazi_refresh);
}
static void TriggerYaziZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(YaziZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📂 Yazi", .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = yazi_refresh, .enter = yazi_enter, .edit = yazi_edit);
    fl_free_file_list(items, n);
}

const char* FileManagerZoxideFilePath = "data/FileManagerZoxide.txt";
static void file_manager_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool file_manager_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (fl_launch_file_manager_at_path(path,NULL)) {
        if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(FileManagerZoxideFilePath, selection);
        free(path);
    }
    return true;
}
static void file_manager_edit(const char* selection, ListBoxInfo* lb) { 
  fl_edit_text_popup(selection, "Edit FileManager", 960, 540, FileManagerZoxideFilePath, lb, file_manager_refresh);
}
static void TriggerFileManager(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📁 FileManager", .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = file_manager_refresh, .enter = file_manager_enter, .edit = file_manager_edit);
    fl_free_file_list(items, n);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+W", TriggerZoxide);
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide);

    fl_register_hotkey("Alt+R",TriggerFileManager);
    fl_register_feature("Zoxide", TriggerZoxide);
    fl_register_feature("Zoxide Text Editor", TriggerTextEditorZoxide);
    fl_register_feature("File Manager", TriggerFileManager);
}