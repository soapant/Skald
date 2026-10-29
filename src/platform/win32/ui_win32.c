#include <windows.h>
#include <gdiplus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deps/log.h"
#include "config.h"
#include "core/context.h"
#include "core/window_manager.h"
#include "core/vim.h"
#include "platform/win32/win32_state.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"

#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
bool IsPopupActive(WindowHandle hPopupWnd) {
        PROFILE_START(_pt);
(void)hPopupWnd;
    { PROFILE_END_VAR(_pt, "IsPopupActive"); return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd)); }
}

void UI_RequestPopupRepaint(void) {
        PROFILE_START(_pt);
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (!hPopup || !IsWindow(hPopup)) { PROFILE_END_VAR(_pt, "UI_RequestPopupRepaint"); return; }
    extern void win32_popup_request_repaint(HWND hPopup);
    win32_popup_request_repaint(hPopup);
    PROFILE_END_VAR(_pt, "UI_RequestPopupRepaint");
}

void UI_LegacySyncRepaint(void) {
        PROFILE_START(_pt);
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup)) {
        RedrawWindow(hPopup, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
    PROFILE_END_VAR(_pt, "UI_LegacySyncRepaint");
}

void UI_SetSelection(ListBoxInfo* lb, int index) {
    PROFILE_START(_pt);
    if (!lb) { PROFILE_END_VAR(_pt, "UI_SetSelection"); return ; }
    int old_sel = lb->selected_index;
    if (old_sel == index) { PROFILE_END_VAR(_pt, "UI_SetSelection"); return ; }
    lb->selected_index = index;
    
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_SetSelection");
}

void UI_SetHorizontalExtent(int width) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_SetHorizontalExtent");
}

static int g_lb_update_depth = 0;
static HWND g_lb_update_hwnd = NULL;

void UI_LB_BeginUpdate(void) {
    PROFILE_START(_pt);
    g_lb_update_depth++;
    PROFILE_END_VAR(_pt, "UI_LB_BeginUpdate");
}

void UI_LB_EndUpdate(void) {
    PROFILE_START(_pt);
    if (g_lb_update_depth > 0) g_lb_update_depth--;
    PROFILE_END_VAR(_pt, "UI_LB_EndUpdate");
}

void UI_LB_SetCount(ListBoxInfo* lb,int count) {
    PROFILE_START(_pt);
    if (lb) lb->populated_count = count;
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_LB_SetCount");
}

void UI_LB_SetItemHeight(int height) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_LB_SetItemHeight");
}

void UI_LB_SetCaretIndex(int index, bool scroll) {
    PROFILE_START(_pt);
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_LB_SetCaretIndex");
}

void UI_LB_SetTopIndex(int index) {
    PROFILE_START(_pt);
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_LB_SetTopIndex");
}

int UI_LB_GetTopIndex(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_LB_GetTopIndex");
    return lb ? lb->view_offset : 0;
}

int UI_LB_GetViewTop(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_LB_GetViewTop");
    return lb ? lb->view_offset : 0;
}

static int g_render_lb_visible = 0;

int UI_LB_GetVisibleCount(void) {
    PROFILE_START(_pt);
    if (FlintContext_Get()->prompt_callbacks.render != NULL && g_render_lb_visible > 0) { PROFILE_END_VAR(_pt, "UI_LB_GetVisibleCount"); return g_render_lb_visible; }
    int itemHeight = UI_LB_GetItemHeight();
    if (itemHeight <= 0) { PROFILE_END_VAR(_pt, "UI_LB_GetVisibleCount"); return 16; }
    RECT rect;
    int visible = rect.bottom / itemHeight;
    if (visible < 1) visible = 1;
    { PROFILE_END_VAR(_pt, "UI_LB_GetVisibleCount"); return visible; }
}

void UI_LB_SetVisibleCount(int count) {
        PROFILE_START(_pt);
    g_render_lb_visible = count;
}

void UI_LB_ScrollToTop(ListBoxInfo* lb, int top_index) {
    PROFILE_START(_pt);
    if (!lb) { PROFILE_END_VAR(_pt, "UI_LB_ScrollToTop"); return ; }
    int visible = UI_LB_GetVisibleCount();
    if (top_index < 0) top_index = 0;
    if (visible > 0 && top_index + visible > lb->populated_count) top_index = lb->populated_count - visible;
    if (top_index < 0) top_index = 0;
    lb->view_offset = top_index;
    
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_LB_ScrollToTop");
}

void UI_LB_SetFont(void* hFont) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_LB_SetFont");
}

int UI_LB_GetItemHeight(void) {
    PROFILE_START(_pt);
    PROFILE_END_VAR(_pt, "UI_LB_GetItemHeight");
    return 0;
}

void UI_NavigateTo(ListBoxInfo* lb, int index) {
    PROFILE_START(_pt);
    if (!lb) { PROFILE_END_VAR(_pt, "UI_NavigateTo"); return ; }
    int count = lb->populated_count;
    if (count <= 0) {
        lb->selected_index = -1;
        UI_RequestPopupRepaint();
        return;
    }
    if (index >= count) index = count - 1;
    if (index == -1) {
        lb->selected_index = -1;
        UI_RequestPopupRepaint();
        return;
    }
    int old_sel = lb->selected_index;
    if (old_sel == index) { PROFILE_END_VAR(_pt, "UI_NavigateTo"); return ; }
    lb->selected_index = index;
    ListBox_ScrollToSelection(lb);
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_NavigateTo");
}

void UI_NavigateUp(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    ListBox_NavigateUp(lb);
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_NavigateUp");
}

void UI_NavigateDown(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    ListBox_NavigateDown(lb);
    UI_RequestPopupRepaint();
    PROFILE_END_VAR(_pt, "UI_NavigateDown");
}

int UI_LB_GetTextLen(WindowHandle hList) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "UI_LB_GetTextLen"); return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXTLEN, 0, 0); }
}

int UI_LB_GetText(WindowHandle hList, int index, char* buffer) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "UI_LB_GetText"); return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXT, index, (LPARAM)buffer); }
}

void UI_Edit_SetFont(HFONT hFont) {
        PROFILE_START(_pt);
(void)hFont;
}

typedef struct {
    char* text;
    size_t capacity;
    int* cursor_i;
    size_t* cursor_z;
} Win32TextTarget;

static int win32_target_cursor(const Win32TextTarget* t) {
        PROFILE_START(_pt);
if (t->cursor_i) { PROFILE_END_VAR(_pt, "win32_target_cursor"); return *t->cursor_i; }
    if (t->cursor_z) { PROFILE_END_VAR(_pt, "win32_target_cursor"); return (int)*t->cursor_z; }
    { PROFILE_END_VAR(_pt, "win32_target_cursor"); return 0; }
}

static void win32_target_set_cursor(const Win32TextTarget* t, int value) {
        PROFILE_START(_pt);
if (t->cursor_i) { *t->cursor_i = value; return; }
    if (t->cursor_z) { *t->cursor_z = (size_t)value; return; }
}

static wchar_t s_pending_high_surrogate = 0;

static bool win32_resolve_text_target(Win32TextTarget* out) {
        PROFILE_START(_pt);
out->text = NULL;
    out->capacity = 0;
    out->cursor_i = NULL;
    out->cursor_z = NULL;
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed && ed->text) {
            out->text = ed->text;
            out->capacity = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
            out->cursor_i = &ed->cursor;
            return true;
        }
    }
    if (top && top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        out->text = prompt->filter;
        out->capacity = sizeof(prompt->filter);
        out->cursor_z = &prompt->cursor;
        return true;
    }
    if (g_win32_prompt_state) {
        out->text = g_win32_prompt_state->filter;
        out->capacity = sizeof(g_win32_prompt_state->filter);
        out->cursor_z = &g_win32_prompt_state->cursor;
        return true;
    }
    { PROFILE_END_VAR(_pt, "win32_resolve_text_target"); return false; }
}

void UI_Edit_SetSel(uint32_t start, uint32_t end) {
        PROFILE_START(_pt);
(void)end;
    UI_SetCursorPosition((int)start);
}

void UI_Edit_GetSel(uint32_t* start, uint32_t* end) {
        PROFILE_START(_pt);
Win32TextTarget t;
    uint32_t cursor = 0;
    if (win32_resolve_text_target(&t)) cursor = (uint32_t)win32_target_cursor(&t);
    if (start) *start = cursor;
    if (end) *end = cursor;
}

void UI_GetCurrentTextEdit(char* buffer, size_t cap) {
        PROFILE_START(_pt);
if (!buffer || cap == 0) { PROFILE_END_VAR(_pt, "UI_GetCurrentTextEdit"); return ; }
    buffer[0] = '\0';
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_GetCurrentTextEdit"); return ; }
    snprintf(buffer, cap, "%s", t.text);
    buffer[cap-1] = '\0';
}

void UI_SetText(const char* text) {
        PROFILE_START(_pt);
    if (!text) text = "";
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text || t.capacity == 0) { PROFILE_END_VAR(_pt, "UI_SetText"); return ; }
    strncpy(t.text, text, t.capacity - 1);
    t.text[t.capacity-1] = '\0';
    UI_SetCursorPosition((int)strlen(t.text));
    if (FlintContext_Get()->prompt_callbacks.render == NULL) {
        UI_RequestPopupRepaint();
        PROFILE_END_VAR(_pt, "UI_SetText");
        return;
    }
    HWND popup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (popup && IsWindow(popup)) UI_RequestPopupRepaint();
}

int UI_GetCursorPosition(void) {
    PROFILE_START(_pt);
Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_GetCursorPosition"); return 0; }
    int pos = win32_target_cursor(&t);
    PROFILE_END_VAR(_pt, "UI_GetCursorPosition");
    return pos;
}

void UI_SetCursorPosition(int position) {
        PROFILE_START(_pt);
s_pending_high_surrogate = 0;
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_SetCursorPosition"); return ; }
    int len = (int)strlen(t.text);
    if (position < 0) position = 0;
    if (position > len) position = len;
    win32_target_set_cursor(&t, position);
}

int UI_InsertText(const char* utf8, int len) {
        PROFILE_START(_pt);
Win32TextTarget t;
    if (!utf8 || len <= 0) { PROFILE_END_VAR(_pt, "UI_InsertText"); return 0; }
    if (!win32_resolve_text_target(&t) || !t.text || t.capacity == 0) { PROFILE_END_VAR(_pt, "UI_InsertText"); return 0; }
    size_t cur = (size_t)win32_target_cursor(&t);
    size_t have = strlen(t.text);
    if (cur > have) cur = have;
    if (have + (size_t)len + 1 > t.capacity) { PROFILE_END_VAR(_pt, "UI_InsertText"); return 0; }
    memmove(t.text + cur + len, t.text + cur, have - cur + 1);
    memcpy(t.text + cur, utf8, (size_t)len);
    win32_target_set_cursor(&t, (int)(cur + len));
    { PROFILE_END_VAR(_pt, "UI_InsertText"); return len; }
}

static int win32_prev_char_len(const char* text, size_t cur) {
        PROFILE_START(_pt);
if (cur == 0) { PROFILE_END_VAR(_pt, "win32_prev_char_len"); return 0; }
    size_t i = cur - 1;
    while (i > 0 && ((unsigned char)text[i] & 0xC0) == 0x80) i--;
    { PROFILE_END_VAR(_pt, "win32_prev_char_len"); return (int)(cur - i); }
}

static int win32_next_char_len(const char* text, size_t len, size_t cur) {
        PROFILE_START(_pt);
if (cur >= len) { PROFILE_END_VAR(_pt, "win32_next_char_len"); return 0; }
    size_t i = cur + 1;
    while (i < len && ((unsigned char)text[i] & 0xC0) == 0x80) i++;
    { PROFILE_END_VAR(_pt, "win32_next_char_len"); return (int)(i - cur); }
}

void UI_Backspace(void) {
        PROFILE_START(_pt);
s_pending_high_surrogate = 0;
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_Backspace"); return ; }
    size_t have = strlen(t.text);
    size_t cur = (size_t)win32_target_cursor(&t);
    if (cur > have) cur = have;
    int n = win32_prev_char_len(t.text, cur);
    if (n <= 0) { PROFILE_END_VAR(_pt, "UI_Backspace"); return ; }
    memmove(t.text + cur - n, t.text + cur, have - cur + 1);
    win32_target_set_cursor(&t, (int)(cur - n));
}

void UI_DeleteForward(void) {
        PROFILE_START(_pt);
s_pending_high_surrogate = 0;
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_DeleteForward"); return ; }
    size_t have = strlen(t.text);
    size_t cur = (size_t)win32_target_cursor(&t);
    if (cur > have) cur = have;
    int n = win32_next_char_len(t.text, have, cur);
    if (n <= 0) { PROFILE_END_VAR(_pt, "UI_DeleteForward"); return ; }
    memmove(t.text + cur, t.text + cur + n, have - cur - n + 1);
}

int UI_InsertChar(wchar_t wc) {
        PROFILE_START(_pt);
wchar_t pair[2];
    int pairlen = 0;
    if (s_pending_high_surrogate) {
        if (wc >= 0xDC00 && wc <= 0xDFFF) {
            pair[0] = s_pending_high_surrogate;
            pair[1] = wc;
            pairlen = 2;
            s_pending_high_surrogate = 0;
        } else {
            s_pending_high_surrogate = 0;
            return 0;
        }
    } else if (wc >= 0xD800 && wc <= 0xDBFF) {
        s_pending_high_surrogate = wc;
        return 0;
    } else {
        pair[0] = wc;
        pairlen = 1;
    }
    char utf8[8];
    int n = WideCharToMultiByte(CP_UTF8, 0, pair, pairlen, utf8, (int)sizeof(utf8), NULL, NULL);
    if (n <= 0) { PROFILE_END_VAR(_pt, "UI_InsertChar"); return 0; }
    { PROFILE_END_VAR(_pt, "UI_InsertChar"); return UI_InsertText(utf8, n); }
}

HFONT UI_GetFont(WindowHandle hWnd) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "UI_GetFont"); return (HFONT)SendMessageA(WH_TO_HWND(hWnd), WM_GETFONT, 0, 0); }
}

void UI_Edit_UpdateUIState(WindowHandle hEdit, unsigned int action, unsigned int uiFlags) {
        PROFILE_START(_pt);
SendMessageA(WH_TO_HWND(hEdit), WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}

PromptState *g_win32_prompt_state = NULL;
void UI_RedrawNow(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup)) {
        UI_RequestPopupRepaint();
    }
    PROFILE_END_VAR(_pt, "UI_RedrawNow");
}

static bool g_toggle_key_pending = false;
static int g_toggle_skip_vk = 0;

void UI_FlushPendingInput(void) {
        PROFILE_START(_pt);
    MSG msg;
    while (PeekMessageA(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) g_toggle_key_pending = false;
    }
}

void UI_PreparePopupPump(void) {
        PROFILE_START(_pt);
g_toggle_key_pending = true;
    extern int pending_hotkey_vk;
    extern int pending_hotkey_mod;
    g_toggle_skip_vk = pending_hotkey_vk;
    pending_hotkey_vk = 0;
    pending_hotkey_mod = 0;
    UI_FlushPendingInput();
}

bool UI_ShouldSkipPopupKey(const void* msg) {
        PROFILE_START(_pt);
const MSG* m = (const MSG*)msg;
    if (!g_toggle_key_pending) { PROFILE_END_VAR(_pt, "UI_ShouldSkipPopupKey"); return false; }
    if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN || m->message == WM_KEYUP || m->message == WM_SYSKEYUP || m->message == WM_CHAR || m->message == WM_SYSCHAR) {
        g_toggle_key_pending = false;
        if (g_toggle_skip_vk != 0 &&
            (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN || m->message == WM_KEYUP || m->message == WM_SYSKEYUP) &&
            (int)m->wParam == g_toggle_skip_vk) {
            g_toggle_skip_vk = 0;
            { PROFILE_END_VAR(_pt, "UI_ShouldSkipPopupKey"); return true; }
        }
        { PROFILE_END_VAR(_pt, "UI_ShouldSkipPopupKey"); return false; }
    }
    { PROFILE_END_VAR(_pt, "UI_ShouldSkipPopupKey"); return false; }
}

void UI_Redraw(ListBoxInfo* lb) {
        PROFILE_START(_pt);
    if (FlintContext_Get()->prompt_callbacks.render == NULL) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup) KillTimer(hPopup, UI_DEBOUNCE_TIMER_ID);
        UI_RedrawNow(lb);
        PROFILE_END_VAR(_pt, "UI_Redraw");
        return;
    }
    if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
        SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
        return;
    }
}

char* Platform_SearchPopup(void) {     PROFILE_START(_pt);
log_error("operation failed, returning error"); { PROFILE_END_VAR(_pt, "Platform_SearchPopup"); return NULL; } log_info("Platform_SearchPopup completed");
}

#include <gdiplus.h>

PromptState* Platform_GetPromptStateForHWND(HWND hwnd) {
    PROFILE_START(_pt);
    if (hwnd && IsWindow(hwnd)) {
        PromptState* st = (PromptState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
        if (st) { PROFILE_END_VAR(_pt, "Platform_GetPromptStateForHWND"); return st; }
        HWND parent = GetParent(hwnd);
        if (parent && IsWindow(parent)) {
            st = (PromptState*)GetWindowLongPtrA(parent, GWLP_USERDATA);
            if (st) { PROFILE_END_VAR(_pt, "Platform_GetPromptStateForHWND"); return st; }
        }
    }
    WindowManager* wm = WindowManager_Get();
    if (wm) {
        for (int i = wm->popup_count - 1; i >= 0; i--) {
            Popup* p = wm->popups[i];
            if (p && p->user_data && p->type == POPUP_PROMPT) {
                HWND ph = WH_TO_HWND(p->handle);
                if (hwnd && ph && (hwnd == ph || IsChild(ph, hwnd))) {
                    { PROFILE_END_VAR(_pt, "Platform_GetPromptStateForHWND"); return (PromptState*)p->user_data; }
                }
            }
        }
        Popup* top = WindowManager_GetTopPopup(wm);
        if (top && top->type == POPUP_PROMPT && top->user_data) {
            { PROFILE_END_VAR(_pt, "Platform_GetPromptStateForHWND"); return (PromptState*)top->user_data; }
        }
    }
    { PROFILE_END_VAR(_pt, "Platform_GetPromptStateForHWND"); return g_win32_prompt_state; }
}

PromptState* Platform_GetWin32PromptState(void) {     PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "Platform_GetWin32PromptState"); return Platform_GetPromptStateForHWND(GetFocus() ? GetFocus() : GetForegroundWindow()); }
}

void UI_SyncFilterFromEdit(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_SyncFilterFromEdit"); return ; }
    if (ctx->prompt_callbacks.render != NULL) {
        PromptState* ws = g_win32_prompt_state;
        if (ws) {
            strncpy(ctx->window.filter, ws->filter, sizeof(ctx->window.filter) - 1);
            ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
            ctx->window.filter_cursor = (int)ws->cursor;
            return;
        }
    }
    Win32TextTarget t;
    if (!win32_resolve_text_target(&t) || !t.text) { PROFILE_END_VAR(_pt, "UI_SyncFilterFromEdit"); return ; }
    strncpy(ctx->window.filter, t.text, sizeof(ctx->window.filter) - 1);
    ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
    ctx->window.filter_cursor = win32_target_cursor(&t);
}

void UI_ApplyFilterToEdit(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_ApplyFilterToEdit"); return ; }
    PromptState* ws = g_win32_prompt_state;
    if (ws) {
        strncpy(ws->filter, ctx->window.filter, sizeof(ws->filter)-1);
        ws->filter[sizeof(ws->filter)-1] = '\0';
        ws->cursor = ctx->window.filter_cursor;
    }
    int pos = ctx->window.filter_cursor;
    int len = (int)strlen(ctx->window.filter);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    UI_SetText(ctx->window.filter);
    UI_SetCursorPosition(pos);
    if (ctx->prompt_callbacks.render != NULL) {
        HWND hPopup = WH_TO_HWND(ctx->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) UI_RequestPopupRepaint();
    }
}

ListBoxInfo* UI_GetPopupListBox(FlintContext* ctx) {
        PROFILE_START(_pt);
(void)ctx;
    PromptState* ws = g_win32_prompt_state;
    if (ws && ws->has_listbox) { PROFILE_END_VAR(_pt, "UI_GetPopupListBox"); return &ws->listbox; }
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup)) {
        ListBoxInfo* lb = (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
        if (lb) return lb;
    }
    { PROFILE_END_VAR(_pt, "UI_GetPopupListBox"); return NULL; }
}

void UI_SyncEditCursor(FlintContext* ctx, int pos) {
      PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_SyncEditCursor"); return ; }
   PromptState* ws = g_win32_prompt_state;
   if (ws) {
       strncpy(ws->filter, ctx->window.filter, sizeof(ws->filter)-1);
       ws->filter[sizeof(ws->filter)-1] = '\0';
       int len = (int)strlen(ws->filter);
       if (pos < 0) pos = 0;
       if (pos > len) pos = len;
       ws->cursor = pos;
   }
    UI_SetCursorPosition(pos);
   if (ctx->prompt_callbacks.render != NULL) {
       HWND hPopup = WH_TO_HWND(ctx->window.hPopupWnd);
       if (hPopup && IsWindow(hPopup)) UI_RequestPopupRepaint();
   }
}

bool UI_ApplyBackspaceToPopup(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_ApplyBackspaceToPopup"); return false; }
    PromptState* ws = g_win32_prompt_state;
    if (!ws) { PROFILE_END_VAR(_pt, "UI_ApplyBackspaceToPopup"); return false; }
    int len = (int)strlen(ctx->window.filter);
    int cur = ctx->window.filter_cursor;
    if (cur <= 0 || len <= 0) { PROFILE_END_VAR(_pt, "UI_ApplyBackspaceToPopup"); return false; }
    int del = cur - 1;
    memmove(ctx->window.filter + del, ctx->window.filter + del + 1, len - del);
    ctx->window.filter_cursor = del;
    ws->cursor = (size_t)del;
    strncpy(ws->filter, ctx->window.filter, sizeof(ws->filter)-1);
    ws->filter[sizeof(ws->filter)-1] = '\0';
    if (ctx->prompt_callbacks.refresh) {
        ListBoxInfo* lb = Vim_GetListBox(ctx);
        ctx->prompt_callbacks.refresh(ctx->window.filter, lb);
    }
    UI_ApplyFilterToEdit(ctx);
    { PROFILE_END_VAR(_pt, "UI_ApplyBackspaceToPopup"); return true; }
}
