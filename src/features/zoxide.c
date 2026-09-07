#include "core/flapi.h"

static void Zox_RebuildCache(const char* filepath, char*** cached_out, int* count_out) {
    if (!cached_out || !count_out) return;
    if (*cached_out) {
        for (int i = 0; i < *count_out; i++) free((*cached_out)[i]);
        free(*cached_out);
        *cached_out = NULL;
        *count_out = 0;
    }
    if (!filepath) return;
    const char* tmp[MAX_FILE_LINES];
    int n = fl_load_file_list(filepath, tmp, MAX_FILE_LINES);
    if (n <= 0) return;
    char** new_cache = (char**)malloc((size_t)n * sizeof(char*));
    if (!new_cache) { fl_free_file_list(tmp, n); return; }
    for (int i = 0; i < n; i++) new_cache[i] = (char*)tmp[i];
    *cached_out = new_cache;
    *count_out = n;
}
static void Zox_Refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb, const char* filepath, char*** cached, int* count) {
    if (!cached || !count) return;
    if (!*cached) Zox_RebuildCache(filepath, cached, count);
    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)*cached, *count, NULL);
}
static void zox_edit_confirm(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, data->refresh_cb);
    }
    free(data);
}
static void Zox_Edit(const char* selection, ListBoxInfo* lb, const char* filepath, char*** cached, int* count, RefreshCallback refresh) {
    (void)cached; (void)count;
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
    Popup* popup = EditPopup_Create(wm, selection, "Edit Zoxide", 960, 540, zox_edit_confirm, data);
    WindowManager_PushPopup(wm, popup);
}
static void Zox_Append(const char* filepath, const char* selection, ListBoxInfo* lb, char*** cached, int* count) {
    if (!filepath || !selection) return;
    if (UI_IsListBoxSelected(lb)) return;
    if (fl_append_to_file(filepath, selection)) Zox_RebuildCache(filepath, cached, count);
}
static const char** Zox_Prepare(const char* filepath, char*** cached, int* count, void (*rebuild)(void)) {
    (void)filepath;
    if (!cached || !count) return NULL;
    if (!*cached && rebuild) rebuild();
    const char** list = NULL;
    if (*count > 0) {
        list = (const char**)malloc((size_t)*count * sizeof(char*));
        if (!list) return NULL;
        for (int i = 0; i < *count; i++) list[i] = (*cached)[i];
    } else {
        list = (const char**)malloc(1);
    }
    return list;
}
static void Zox_Cleanup(const char** list) { if (list) free((void*)list); }

const char* TerminalZoxideFilePath = "data/TerminalZoxide.txt";
static char** cached_zoxide_commands = NULL;
static int cached_zoxide_count = 0;
static void RebuildZoxideCache(void) { Zox_RebuildCache(TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count); }
static void zoxide_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) { Zox_Refresh(filter, hListBox, lb, TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count); }
static void zoxide_edit(const char* selection, ListBoxInfo* lb) { Zox_Edit(selection, lb, TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count, zoxide_refresh); }
static bool zoxide_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        fl_launch_terminal_at_path(path, NULL);
        Zox_Append(TerminalZoxideFilePath, selection, lb, &cached_zoxide_commands, &cached_zoxide_count);
    }
    return true;
}
static void TriggerZoxide(void) {
    if (!cached_zoxide_commands) RebuildZoxideCache();
    const char** list = Zox_Prepare(TerminalZoxideFilePath, &cached_zoxide_commands, &cached_zoxide_count, NULL);
    PROMPT(.title = "⌨️ Zoxide", .items = list, .count = cached_zoxide_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = zoxide_refresh, .enter = zoxide_enter, .edit = zoxide_edit);
    Zox_Cleanup(list);
}

const char* TextEditorZoxideFilePath = "data/NvimZoxide.txt";
static char** cached_text_editor_commands = NULL;
static int cached_text_editor_count = 0;
static void RebuildTextEditorCache(void) { Zox_RebuildCache(TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count); }
static void text_editor_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) { Zox_Refresh(filter, hListBox, lb, TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count); }
static bool text_editor_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        char cmd_args[512];
        snprintf(cmd_args, sizeof(cmd_args), "-e %s \"%s\"", UsedTextEditor, path);
        fl_launch_terminal_at_path(path, cmd_args);
        Zox_Append(TextEditorZoxideFilePath, selection, lb, &cached_text_editor_commands, &cached_text_editor_count);
    }
    return true;
}
static void text_editor_edit(const char* selection, ListBoxInfo* lb) { Zox_Edit(selection, lb, TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count, text_editor_refresh); }
static void TriggerTextEditorZoxide(void) {
    if (!cached_text_editor_commands) RebuildTextEditorCache();
    const char** list = Zox_Prepare(TextEditorZoxideFilePath, &cached_text_editor_commands, &cached_text_editor_count, NULL);
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "📂 %s", UsedTextEditor);
    PROMPT(.title = buffer, .items = list, .count = cached_text_editor_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = text_editor_refresh, .enter = text_editor_enter, .edit = text_editor_edit);
    Zox_Cleanup(list);
}

const char* YaziZoxideFilePath = "data/YaziZoxide.txt";
static char** cached_yazi_commands = NULL;
static int cached_yazi_count = 0;
static void RebuildYaziCache(void) { Zox_RebuildCache(YaziZoxideFilePath, &cached_yazi_commands, &cached_yazi_count); }
static void yazi_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) { Zox_Refresh(filter, hListBox, lb, YaziZoxideFilePath, &cached_yazi_commands, &cached_yazi_count); }
static bool yazi_enter(const char* selection, ListBoxInfo* lb) {
    char* path = fl_get_zoxide_match((char*)selection);
    if (path) {
        char cmd_args[512];
        snprintf(cmd_args, sizeof(cmd_args), "-e yazi \"%s\"", path);
        fl_launch_terminal_at_path(path, cmd_args);
        Zox_Append(YaziZoxideFilePath, selection, lb, &cached_yazi_commands, &cached_yazi_count);
    }
    return true;
}
static void yazi_edit(const char* selection, ListBoxInfo* lb) { Zox_Edit(selection, lb, YaziZoxideFilePath, &cached_yazi_commands, &cached_yazi_count, yazi_refresh); }
static void TriggerYaziZoxide(void) {
    if (!cached_yazi_commands) RebuildYaziCache();
    const char** list = Zox_Prepare(YaziZoxideFilePath, &cached_yazi_commands, &cached_yazi_count, NULL);
    PROMPT(.title = "📂 Yazi", .items = list, .count = cached_yazi_count,
           .height = 920, .width = 650, .item_height = 32,
           .refresh = yazi_refresh, .enter = yazi_enter, .edit = yazi_edit);
    Zox_Cleanup(list);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+W", TriggerZoxide);
    fl_register_hotkey("Alt+I", TriggerTextEditorZoxide);
    // fl_register_hotkey("Alt+Q", TriggerYaziZoxide);
}
