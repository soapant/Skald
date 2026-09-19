#include "core/flapi.h"

const char* TerminalZoxideFilePath = "TerminalZoxide.txt";
static void zoxide_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static void zoxide_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection, .title = "Edit Zoxide", .x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TerminalZoxideFilePath, idx, edited);
    free(edited);
    zoxide_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static bool zoxide_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
    fl_launch_terminal_at_path(path, NULL);
    if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(TerminalZoxideFilePath, selection);
    free(path);
    return true;
}
static void TriggerZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "⌨️ Zoxide", .items = items,
           .refresh = zoxide_refresh, .enter = zoxide_enter, .edit = zoxide_edit);
    fl_free_file_list(items, n);
}

const char* TextEditorZoxideFilePath = "NvimZoxide.txt";
static void text_editor_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TextEditorZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool text_editor_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
    char cmd_args[512];
    snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"", UsedTextEditor, path);
    fl_launch_terminal_at_path(path, cmd_args);
    if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(TextEditorZoxideFilePath, selection);
    free(path);
    return true;
}
static void text_editor_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection, .title = "Edit TextEditor", .x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TextEditorZoxideFilePath, idx, edited);
    free(edited);
    text_editor_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerTextEditorZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TextEditorZoxideFilePath, items, MAX_FILE_LINES);
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"📂 %s",UsedTextEditor);
    PROMPT(.title = buffer, .items = items,
           .refresh = text_editor_refresh, .enter = text_editor_enter, .edit = text_editor_edit);
    fl_free_file_list(items, n);
}

const char* YaziZoxideFilePath = "YaziZoxide.txt";
static void yazi_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(YaziZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool yazi_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
    char cmd_args[512];
    snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
    fl_launch_terminal_at_path(path, cmd_args);
    if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(YaziZoxideFilePath, selection);
    free(path);
    return true;
}
static void yazi_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection, .title = "Edit Yazi", .x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(YaziZoxideFilePath, idx, edited);
    free(edited);
    yazi_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerYaziZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(YaziZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📂 Yazi", .items = items,
           .refresh = yazi_refresh, .enter = yazi_enter, .edit = yazi_edit);
    fl_free_file_list(items, n);
}

const char* FileManagerZoxideFilePath = "FileManagerZoxide.txt";
static void file_manager_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool file_manager_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
    if (fl_launch_file_manager_at_path(path,NULL)) {
        if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(FileManagerZoxideFilePath, selection);
    }
    free(path);
    return true;
}
static void file_manager_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.x = 960, .y = 540, .lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(FileManagerZoxideFilePath, idx, edited);
    free(edited);
    file_manager_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerFileManager(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📁 FileManager", .items = items,
           .refresh = file_manager_refresh, .enter = file_manager_enter, .edit = file_manager_edit);
    fl_free_file_list(items, n);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+W", TriggerZoxide);
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide);
    fl_register_hotkey("Alt+R",TriggerFileManager,"Fortnite");

    fl_register_feature("Zoxide", TriggerZoxide);
    fl_register_feature("Zoxide Text Editor", TriggerTextEditorZoxide);
    fl_register_feature("File Manager", TriggerFileManager);
}
