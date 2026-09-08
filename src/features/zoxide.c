#include "core/flapi.h"
#include "core/context.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include <stdlib.h>
#include <string.h>


static void edit_confirm(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, data->refresh_cb);
    }
    free(data);
}
static void do_edit(const char* selection, ListBoxInfo* lb, const char* filepath, RefreshCallback refresh) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    if (!data) return;
    data->filepath = filepath;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = FlintContext_Get()->window.hListBox;
    data->hEditControl = FlintContext_Get()->window.hEditControl;
    data->refresh_cb = refresh;
    Popup* popup = EditPopup_Create(wm, selection, "Edit Zoxide", 960, 540, edit_confirm, data);
    WindowManager_PushPopup(wm, popup);
}

const char* TerminalZoxideFilePath = "data/TerminalZoxide.txt";
static void zoxide_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(TerminalZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static void zoxide_edit(const char* selection, ListBoxInfo* lb) { do_edit(selection, lb, TerminalZoxideFilePath, zoxide_refresh); }
static bool zoxide_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        fl_launch_terminal_at_path(path, NULL);
        if (!UI_IsListBoxSelected(lb)) fl_append_to_file(TerminalZoxideFilePath, selection);
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
        if (!UI_IsListBoxSelected(lb)) fl_append_to_file(TextEditorZoxideFilePath, selection);
    }
    return true;
}
static void text_editor_edit(const char* selection, ListBoxInfo* lb) { do_edit(selection, lb, TextEditorZoxideFilePath, text_editor_refresh); }
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
        if (!UI_IsListBoxSelected(lb)) fl_append_to_file(YaziZoxideFilePath, selection);
    }
    return true;
}
static void yazi_edit(const char* selection, ListBoxInfo* lb) { do_edit(selection, lb, YaziZoxideFilePath, yazi_refresh); }
static void TriggerYaziZoxide(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(YaziZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📂 Yazi", .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = yazi_refresh, .enter = yazi_enter, .edit = yazi_edit);
    fl_free_file_list(items, n);
}

const char* FileManagerZoxideFilePath = "data/FileManagerZoxide.txt";
static void FileManager_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, n, NULL);
    fl_free_file_list(items, n);
}
static bool FileManager_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (fl_launch_file_manager_at_path(path,NULL)) {
        if (!UI_IsListBoxSelected(lb)) fl_append_to_file(FileManagerZoxideFilePath, selection);
        free(path);
    }
    return true;
}
static void FileManager_edit(const char* selection, ListBoxInfo* lb) { do_edit(selection, lb, FileManagerZoxideFilePath, FileManager_refresh); }
static void TriggerFileManager(void) {
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(FileManagerZoxideFilePath, items, MAX_FILE_LINES);
    PROMPT(.title = "📁 FileManager", .items = items, .count = n,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = FileManager_refresh, .enter = FileManager_enter, .edit = FileManager_edit);
    fl_free_file_list(items, n);
}

static void Testing(void) {
  fl_paste_from_clipboard();
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+W", TriggerZoxide);
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide);
    // fl_register_hotkey("Alt+Q", TriggerYaziZoxide);
    fl_register_hotkey("Alt+R",TriggerFileManager);
    fl_register_hotkey("Alt+Shift+V",Testing);
}
