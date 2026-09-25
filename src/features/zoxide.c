#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include <stdio.h>
#include <stdlib.h>


const char* TextEditor = "nvim";

const char* TerminalZoxideFilePath = "TerminalZoxide.txt";
static void zoxide_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
}
static void zoxide_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
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
    FlStringList files = fl_load_file_list(TerminalZoxideFilePath);
    PROMPT(.title = "⌨️ Zoxide", .items = (const char**)files.items,
           .refresh = zoxide_refresh, .enter = zoxide_enter, .edit = zoxide_edit);
    fl_free_file_list(&files);
}

const char* TextEditorZoxideFilePath = "NvimZoxide.txt";
static void text_editor_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(TextEditorZoxideFilePath);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
}
static bool text_editor_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (!path) return false;
     char cmd_args[512];
     snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"", TextEditor, path);
     fl_launch_terminal_at_path(path, cmd_args);
    if (!UI_IsListBoxSelected(lb)) fl_append_unique_line(TextEditorZoxideFilePath, selection);
    free(path);
    return true;
}
static void text_editor_edit(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TextEditorZoxideFilePath, idx, edited);
    free(edited);
    text_editor_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerTextEditorZoxide(void) {
    FlStringList files = fl_load_file_list(TextEditorZoxideFilePath);
    PROMPT(.title = "TextEditor", .items = (const char**)files.items,
           .refresh = text_editor_refresh, .enter = text_editor_enter, .edit = text_editor_edit);
    fl_free_file_list(&files);
}

const char* YaziZoxideFilePath = "YaziZoxide.txt";
static void yazi_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(YaziZoxideFilePath);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
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
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(YaziZoxideFilePath, idx, edited);
    free(edited);
    yazi_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerYaziZoxide(void) {
    FlStringList files = fl_load_file_list(YaziZoxideFilePath);
    PROMPT(.title = "📂 Yazi", .items = (const char**)files.items,
           .refresh = yazi_refresh, .enter = yazi_enter, .edit = yazi_edit);
    fl_free_file_list(&files);
}

const char* FileManagerZoxideFilePath = "FileManagerZoxide.txt";
static void file_manager_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(FileManagerZoxideFilePath);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
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
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(FileManagerZoxideFilePath, idx, edited);
    free(edited);
    file_manager_refresh("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}
static void TriggerFileManager(void) {
    FlStringList files = fl_load_file_list(FileManagerZoxideFilePath);
    PROMPT(.title = "📁 FileManager", .items = (const char**)files.items,
           .refresh = file_manager_refresh, .enter = file_manager_enter, .edit = file_manager_edit);
    fl_free_file_list(&files);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+W", TriggerZoxide, "Fortnite");
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide, "Fortnite");
    fl_register_hotkey("Alt+R",TriggerFileManager,"Fortnite");

    fl_register_feature("Zoxide", TriggerZoxide);
    fl_register_feature("Zoxide Text Editor", TriggerTextEditorZoxide);
    fl_register_feature("File Manager", TriggerFileManager);
}
