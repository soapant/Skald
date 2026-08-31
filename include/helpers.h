#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "core/modal.h"
#include "core/types.h"
#include "core/hashmap.h"
#include "config.h"
#include "core/platform.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

typedef void (*HotkeyCallback)(void);
typedef bool (*HotkeyCondition)(void);

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    HotkeyCallback hotkey_callback;
    HotkeyCondition hotkey_condition;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);
extern EditorMode g_last_mode;
bool AlwaysEnabled(void);
bool AlwaysDisabled(void);
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                   int pressed_vk, int modifiers,
                   const char* current_title);

typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

typedef struct {
    char* item_name;
    int item_index;
    char* alias_name; 
} AliasInfo;

typedef struct {
    AliasInfo matches[1024];
    int count;
} AliasMatchList;

typedef struct {
    char* flag;
    int index;
} FoundFlag;

int ParseTimeToSeconds(const char* input);
typedef void (*ActionFn)(const char* parameter);
typedef bool (*ConditionFn)(const char* parameter);
typedef void (*TimerFn)(const char* parameter);
typedef struct { char* str; ActionFn    func; } ActionDef;
typedef struct { char* str; ConditionFn func; } ConditionDef;
typedef struct { char* str; TimerFn     func; } TimerDef;
typedef struct {
    char* name;
    ConditionFn func;
} ConditionParamDef;

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches);
char* GetFlagParameter(const char* text, int flag_index);
char* GetCleanText(const char* text);
bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count);

static inline bool fl_hotkey_match(int pressed_vk, int pressed_mod, int bind_vk, int bind_mod) {
    return pressed_vk == bind_vk && pressed_mod == bind_mod;
}
static inline bool fl_exe_matches(const char* bind_exe, const char* activeExe) {
    if (!bind_exe || !activeExe) return false;
    if (strcmp(bind_exe, "*") == 0) return true;
#if defined(_WIN32)
    size_t nlen = strlen(bind_exe);
    size_t hlen = strlen(activeExe);
    if (nlen == 0) return true;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = activeExe[i+j];
            char b = bind_exe[j];
            if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
            if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
            if (a != b) break;
        }
        if (j == nlen) return true;
    }
    return false;
#else
    return strstr(activeExe, bind_exe) != NULL;
#endif
}
static inline bool fl_check_hotkey_bindings(int vk, int mod, const char* title, const HotkeyBinding* bindings, int count) {
    for (int i = 0; i < count; i++) {
        const HotkeyBinding* b = &bindings[i];
        if (b->vk_code != vk || b->modifiers != mod) continue;
        if (b->hotkey_condition && !b->hotkey_condition()) continue;
        if (IsWindowBlacklisted(title, b->blacklist, b->blacklist_count)) continue;
        if (b->hotkey_callback) b->hotkey_callback();
        return true;
    }
    return false;
}
static inline int fl_find_app_hotkey_idx(int vk, int mod, const char* activeExe, const AppHotkey* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod && fl_exe_matches(arr[i].exe_name, activeExe)) return i;
    return -1;
}
static inline int fl_find_misc_hotkey_idx(int vk, int mod, const char* activeExe, const MiscHotkey* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod && fl_exe_matches(arr[i].exe_name, activeExe)) return i;
    return -1;
}
static inline int fl_find_quicklink_idx(int vk, int mod, const char* activeExe, const QuickLinkHotkey* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod && fl_exe_matches(arr[i].exe_name, activeExe)) return i;
    return -1;
}
static inline int fl_find_text_snippet_idx(int vk, int mod, const char* activeExe, const TextSnippet* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod && fl_exe_matches(arr[i].exe_name, activeExe)) return i;
    return -1;
}
static inline int fl_find_terminal_idx(int vk, int mod, const TerminalCommandHotkey* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod) return i;
    return -1;
}
static inline int fl_find_music_idx(int vk, int mod, const MusicPlayerBinding* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod) return i;
    return -1;
}
static inline int fl_find_video_idx(int vk, int mod, const VideoViewerBinding* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod) return i;
    return -1;
}
static inline int fl_find_bulk_idx(int vk, int mod, const BulkKillProcessBinding* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod) return i;
    return -1;
}
static inline int fl_find_focus_idx(int vk, int mod, const FocusSwitcherBinding* arr, int count) {
    for (int i = 0; i < count; i++) if (arr[i].vk_code == vk && arr[i].modifiers == mod) return i;
    return -1;
}

int UI_LookupTrueIndex(const char* name);
void SaveFeatureHistory(char* string);
int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text);
void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text);

typedef struct {
    const char* filepath;
    MappedFile* file;
    ListBoxInfo* lb;
    int selected_index;
    int true_index;
    char* new_text;
    bool restore_selection;
    WindowHandle hListBox;
    WindowHandle hEditControl;
    RefreshCallback refresh_cb;
} EditCallbackData;

int EditCallback_Handle(EditCallbackData* data, RefreshCallback refresh);
int ReadFile(char lines[100][512], const char* path);
void StripCheckboxes(char* str);
void WriteFile(char lines[100][512], int count, const char* path);
extern HashMap* history_indices;
bool GetFromHistory(ListBoxInfo* lb);
int convert_string_to_int(const char *str);
HotkeyBinding convert_into_hotkey_binding(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows);
void HandleCommandExecution(ListBoxInfo* lb, char* string);
const char* get_alias_for_item(const char* item);
unsigned long rgb_to_hex(unsigned long win_color);
void eval_window_title_color(PromptState * state,unsigned long *window_title_color);
void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int *xOffsetAlias);
unsigned short OpacityAlpha16(void);
#if !defined(_WIN32)
void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY,char* centered_to,int xOffset);
int get_offset_based_on_number(int index);
void Linux_XftAllocColor(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out);
unsigned long VimModeTextColor(void);
int Linux_UpdateEditScrollOffset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x);
void Linux_DrawEditCursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width);
const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size);
void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb, int idx, int y, int text_x);
bool CursorBack(size_t* cursor, size_t text_len);
bool CursorForward(size_t* cursor, size_t text_len);
pid_t GetWindowPid(Display* dpy, Window w);
void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen);
Window Linux_CreateWindowEx(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask);
Window Linux_CreatePromptWindow(Display* dpy, int x, int y, int w, int h);
Window Linux_CreateSimplePromptWindow(Display* dpy, int x, int y, int w, int h);
Window Linux_CreateEditPopupWindow(Display* dpy, int x, int y, int w, int h);
Window Linux_CreateDisplayMessageWindow(Display* dpy, int x, int y, int w, int h);
XftDraw* Linux_CreateXftDraw(Display* dpy, Window win);
#endif
#endif
