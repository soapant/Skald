#include <windows.h>
#include <gdiplus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deps/log.h"
#include "config.h"
#include "core/context.h"
#include "core/vim.h"
#include "platform/win32/win32_state.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/win32/common_ui_win32.h"

bool IsPopupActive(WindowHandle hPopupWnd) {
    (void)hPopupWnd;
    return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
}

void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
    if (!lb) return;
    int old_sel = lb->selected_index;
    if (old_sel == index) return;
    lb->selected_index = index;
    RECT invalidRect = {0};
    bool hasInvalid = false;
    RECT itemRect = {0, 0, 0, 0};
    if (old_sel >= 0 && old_sel < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect) != LB_ERR) {
            invalidRect = itemRect;
            hasInvalid = true;
        }
    }
    if (index >= 0 && index < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, index, (LPARAM)&itemRect) != LB_ERR) {
            if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; }
            else UnionRect(&invalidRect, &invalidRect, &itemRect);
        }
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hList))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hList), &invalidRect, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void UI_SetHorizontalExtent(WindowHandle hList, int width) {
    SendMessageA(WH_TO_HWND(hList), LB_SETHORIZONTALEXTENT, width, 0);
}

void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
    if (lb) lb->populated_count = count;
    SendMessageA(WH_TO_HWND(hList), LB_SETCOUNT, count, 0);
}

void UI_LB_SetItemHeight(WindowHandle hList, int height) {
    SendMessageA(WH_TO_HWND(hList), LB_SETITEMHEIGHT, 0, height);
}

void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
    SendMessageA(WH_TO_HWND(hList), LB_SETCARETINDEX, index, scroll);
}

void UI_LB_SetTopIndex(WindowHandle hList, int index) {
    SendMessageA(WH_TO_HWND(hList), LB_SETTOPINDEX, index, 0);
}

int UI_LB_GetTopIndex(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTOPINDEX, 0, 0);
}

void UI_LB_SetFont(WindowHandle hList, void* hFont) {
    SendMessageA(WH_TO_HWND(hList), WM_SETFONT, (WPARAM)hFont, FALSE);
}

int UI_LB_GetItemHeight(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETITEMHEIGHT, 0, 0);
}

void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
    if (!lb) return;
    int count = lb->populated_count;
    if (count <= 0) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(hListBox, -1, false);
        if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
            HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
            if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        }
        return;
    }
    if (index >= count) index = count - 1;
    if (index == -1) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(hListBox, -1, false);
        if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
            HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
            if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        }
        return;
    }
    int old_sel = lb->selected_index;
    if (old_sel == index) return;
    int oldTop = UI_LB_GetTopIndex(hListBox);
    lb->selected_index = index;
    UI_LB_SetCaretIndex(hListBox, index, false);
    int itemHeight = UI_LB_GetItemHeight(hListBox);
    RECT rect;
    GetClientRect(WH_TO_HWND(hListBox), &rect);
    int visibleCount = (itemHeight > 0) ? (rect.bottom / itemHeight) : 16;
    if (visibleCount < 1) visibleCount = 1;
    int newTop = oldTop;
    if (index <= oldTop + PaddingForScrolling) newTop = index - PaddingForScrolling;
    else if (index >= oldTop + visibleCount - PaddingForScrolling) newTop = index - visibleCount + PaddingForScrolling + 1;
    if (newTop < 0) newTop = 0;
    if (newTop + visibleCount > count) newTop = count - visibleCount;
    if (newTop < 0) newTop = 0;
    if (newTop != oldTop) {
        SendMessageA(WH_TO_HWND(hListBox), WM_SETREDRAW, FALSE, 0);
        UI_LB_SetTopIndex(hListBox, newTop);
        SendMessageA(WH_TO_HWND(hListBox), WM_SETREDRAW, TRUE, 0);
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (newTop != oldTop) {
        RedrawWindow(WH_TO_HWND(hListBox), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        return;
    }
    RECT invalidRect = {0};
    bool hasInvalid = false;
    RECT itemRect;
    if (old_sel >= 0 && old_sel != index && old_sel < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect) != LB_ERR) {
            invalidRect = itemRect;
            hasInvalid = true;
        }
    }
    if (index >= 0 && index < lb->populated_count) {
        if ((int)SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, index, (LPARAM)&itemRect) != LB_ERR) {
            if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; } else UnionRect(&invalidRect, &invalidRect, &itemRect);
        }
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hListBox), &invalidRect, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void UI_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox) {
    if (!lb) return;
    int sel = lb->selected_index;
    if (sel >= 0) sel--;
    if (sel == lb->selected_index) return;
    UI_NavigateTo(lb, hListBox, sel);
}

void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox) {
    if (!lb) return;
    int count = lb->populated_count;
    if (count <= 0) return;
    int sel = lb->selected_index;
    if (sel < 0) sel = -1;
    if (sel >= count - 1) sel = count - 1;
    else sel++;
    if (sel == lb->selected_index) return;
    UI_NavigateTo(lb, hListBox, sel);
}

int UI_LB_GetTextLen(WindowHandle hList) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXTLEN, 0, 0);
}

int UI_LB_GetText(WindowHandle hList, int index, char* buffer) {
    return (int)SendMessageA(WH_TO_HWND(hList), LB_GETTEXT, index, (LPARAM)buffer);
}

void UI_Edit_SetFont(WindowHandle hEdit, HFONT hFont) {
    SendMessageA(WH_TO_HWND(hEdit), WM_SETFONT, (WPARAM)hFont, FALSE);
}

HFONT UI_GetFont(WindowHandle hWnd) {
    return (HFONT)SendMessageA(WH_TO_HWND(hWnd), WM_GETFONT, 0, 0);
}

void UI_Edit_UpdateUIState(WindowHandle hEdit, unsigned int action, unsigned int uiFlags) {
    SendMessageA(WH_TO_HWND(hEdit), WM_UPDATEUISTATE, MAKEWPARAM(action, uiFlags), 0);
}

void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end) {
    SendMessageA(WH_TO_HWND(hEdit), EM_SETSEL, start, end);
}

void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
    SendMessageA(WH_TO_HWND(hEdit), EM_GETSEL, (WPARAM)start, (LPARAM)end);
}

void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap) {
    GetWindowTextA(WH_TO_HWND(edit), buffer, (int)cap);
}

void UI_SetText(WindowHandle hEdit, char* text) {
    SetWindowTextA(WH_TO_HWND(hEdit), text);
}

void UI_SetCursorPosition(WindowHandle ctrl, int position) {
    UI_Edit_SetSel(ctrl, position, position);
}

PromptState *g_win32_prompt_state = NULL;
void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
    FlintContext* _c = FlintContext_Get();
    if (_c->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, buffer, sizeof(buffer));
        if (g_win32_prompt_state) {
            strncpy(g_win32_prompt_state->filter, buffer, sizeof(g_win32_prompt_state->filter) - 1);
            g_win32_prompt_state->filter[sizeof(g_win32_prompt_state->filter) - 1] = '\0';
            HWND hEdit = WH_TO_HWND(FlintContext_Get()->window.hEditControl);
            if (hEdit && IsWindow(hEdit)) {
                DWORD cs = 0, ce = 0;
                SendMessageA(hEdit, EM_GETSEL, (WPARAM)&cs, (LPARAM)&ce);
                g_win32_prompt_state->cursor = (size_t)cs;
            }
        }
        _c->prompt_callbacks.refresh(buffer, FlintContext_Get()->window.hListBox, lb);
    }
}

static bool g_toggle_key_pending = false;

void UI_FlushPendingInput(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) g_toggle_key_pending = false;
    }
}

void UI_PreparePopupPump(void) {
    g_toggle_key_pending = true;
    UI_FlushPendingInput();
}

bool UI_ShouldSkipPopupKey(const void* msg) {
    const MSG* m = (const MSG*)msg;
    if (!g_toggle_key_pending) return false;
    if (m->message == WM_KEYDOWN || m->message == WM_SYSKEYDOWN || m->message == WM_KEYUP || m->message == WM_SYSKEYUP || m->message == WM_CHAR || m->message == WM_SYSCHAR) {
        g_toggle_key_pending = false;
        return true;
    }
    return false;
}

void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
    if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
        SetTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), UI_DEBOUNCE_TIMER_ID, UI_DEBOUNCE_MS, NULL);
        return;
    }
    UI_RedrawNow(lb, hListBox);
}

void UI_GetParent(WindowHandle hwnd) {
    SendMessageA(GetParent(WH_TO_HWND(hwnd)), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)WH_TO_HWND(FlintContext_Get()->window.hEditControl));
}

char* Platform_SearchPopup(void) { log_error("operation failed, returning error"); return NULL; log_info("Platform_SearchPopup completed");
}

#include <gdiplus.h>

PromptState* Platform_GetWin32PromptState(void) { return g_win32_prompt_state; }

static void win_command_confirm(const char* text, void* user_data) {
  extern void HandleCommandExecution(ListBoxInfo* lb, char* string);
  ListBoxInfo* lb = (ListBoxInfo*)user_data;
  if (!text) return;
  char* dup = _strdup(text);
  if (!dup) return;
  HandleCommandExecution(lb, dup);
  free(dup);
}


void UI_SyncFilterFromEdit(FlintContext* ctx) {
    if (!ctx) return;
    if (ctx->prompt_callbacks.render != NULL) {
        PromptState* ws = g_win32_prompt_state;
        if (ws) {
            strncpy(ctx->window.filter, ws->filter, sizeof(ctx->window.filter) - 1);
            ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
            ctx->window.filter_cursor = (int)ws->cursor;
            return;
        }
    }
    HWND hEdit = WH_TO_HWND(ctx->window.hActiveEditControl);
    if (!hEdit) hEdit = WH_TO_HWND(ctx->window.hEditControl);
    if (!hEdit || !IsWindow(hEdit)) return;
    char buf[sizeof(ctx->window.filter)];
    GetWindowTextA(hEdit, buf, (int)sizeof(buf));
    strncpy(ctx->window.filter, buf, sizeof(ctx->window.filter) - 1);
    ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
    DWORD se = 0, ee = 0;
    SendMessageA(hEdit, EM_GETSEL, (WPARAM)&se, (LPARAM)&ee);
    ctx->window.filter_cursor = (int)se;
}

void UI_ApplyFilterToEdit(FlintContext* ctx) {
    if (!ctx) return;
    PromptState* ws = g_win32_prompt_state;
    if (ws) {
        strncpy(ws->filter, ctx->window.filter, sizeof(ws->filter)-1);
        ws->filter[sizeof(ws->filter)-1] = '\0';
        ws->cursor = ctx->window.filter_cursor;
    }
    HWND hEdit = WH_TO_HWND(ctx->window.hEditControl);
    if (!hEdit) hEdit = WH_TO_HWND(ctx->window.hActiveEditControl);
    if (!hEdit || !IsWindow(hEdit)) return;
    int pos = ctx->window.filter_cursor;
    int len = (int)strlen(ctx->window.filter);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    SetWindowTextA(hEdit, ctx->window.filter);
    SendMessageA(hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
    if (WH_TO_HWND(ctx->window.hActiveEditControl) &&
        WH_TO_HWND(ctx->window.hActiveEditControl) != hEdit) {
        HWND hActive = WH_TO_HWND(ctx->window.hActiveEditControl);
        SetWindowTextA(hActive, ctx->window.filter);
        SendMessageA(hActive, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
    }
    UpdateTextCaret(WH_FROM_HWND(hEdit));
    if (ctx->prompt_callbacks.render != NULL) {
        HWND hPopup = WH_TO_HWND(ctx->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
    }
}

ListBoxInfo* UI_GetPopupListBox(FlintContext* ctx) {
    (void)ctx;
    PromptState* ws = g_win32_prompt_state;
    if (ws && ws->has_listbox) return &ws->listbox;
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hPopup && IsWindow(hPopup)) {
        ListBoxInfo* lb = (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
        if (lb) return lb;
    }
    return NULL;
}

void UI_SyncEditCursor(FlintContext* ctx, int pos) {
    if (!ctx) return;
    HWND hEdit = WH_TO_HWND(ctx->window.hEditControl);
    if (!hEdit) hEdit = WH_TO_HWND(ctx->window.hActiveEditControl);
    if (hEdit) {
        SendMessageA(hEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)pos);
        UpdateTextCaret(WH_FROM_HWND(hEdit));
    }
    if (ctx->prompt_callbacks.render != NULL) {
        HWND hPopup = WH_TO_HWND(ctx->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
    }
}

bool UI_ApplyBackspaceToPopup(FlintContext* ctx) {
    if (!ctx) return false;
    PromptState* ws = g_win32_prompt_state;
    if (!ws) return false;
    int len = (int)strlen(ctx->window.filter);
    int cur = ctx->window.filter_cursor;
    if (cur <= 0 || len <= 0) return false;
    int del = cur - 1;
    memmove(ctx->window.filter + del, ctx->window.filter + del + 1, len - del);
    ctx->window.filter_cursor = del;
    ws->cursor = (size_t)del;
    strncpy(ws->filter, ctx->window.filter, sizeof(ws->filter)-1);
    ws->filter[sizeof(ws->filter)-1] = '\0';
    if (ctx->prompt_callbacks.refresh) {
        ListBoxInfo* lb = Vim_GetListBox(ctx);
        ctx->prompt_callbacks.refresh(ctx->window.filter, ctx->window.hListBox, lb);
    }
    UI_ApplyFilterToEdit(ctx);
    return true;
}
