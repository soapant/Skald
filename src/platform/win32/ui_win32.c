#include "platform/ui.h"
#include "core/context.h"
#include <windows.h>
#include <string.h>

bool IsPopupActive(WindowHandle hPopupWnd) {
    (void)hPopupWnd;
    return WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
}

void* GetActivePopupUserData(void) {
    return NULL;
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
        SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
        invalidRect = itemRect;
        hasInvalid = true;
    }
    if (index >= 0 && index < lb->populated_count) {
        SendMessageA(WH_TO_HWND(hList), LB_GETITEMRECT, index, (LPARAM)&itemRect);
        if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; }
        else UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hList))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hList), &invalidRect, NULL, RDW_INVALIDATE);
}

int UI_GetCount(ListBoxInfo* lb, WindowHandle hList) {
    (void)hList;
    if (!lb) return 0;
    return lb->populated_count;
}

void UI_SetHorizontalExtent(WindowHandle hList, int width) {
    SendMessageA(WH_TO_HWND(hList), LB_SETHORIZONTALEXTENT, width, 0);
}

void UI_SetItemCount(ListBoxInfo* lb, WindowHandle hList, int count) {
    if (lb) lb->populated_count = count;
    SendMessageA(WH_TO_HWND(hList), LB_SETCOUNT, count, 0);
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
    RECT invalidRect = {0};
    bool hasInvalid = false;
    if (newTop != oldTop) { UI_LB_SetTopIndex(hListBox, newTop); hasInvalid = true; }
    RECT itemRect;
    if (old_sel >= 0 && old_sel != index && old_sel < lb->populated_count) {
        SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, old_sel, (LPARAM)&itemRect);
        if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; } else UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
    if (index >= 0 && index < lb->populated_count) {
        SendMessageA(WH_TO_HWND(hListBox), LB_GETITEMRECT, index, (LPARAM)&itemRect);
        if (!hasInvalid) { invalidRect = itemRect; hasInvalid = true; } else UnionRect(&invalidRect, &invalidRect, &itemRect);
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL || !IsWindowVisible(WH_TO_HWND(hListBox))) {
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) InvalidateRect(hPopup, NULL, FALSE);
        return;
    }
    if (hasInvalid) RedrawWindow(WH_TO_HWND(hListBox), &invalidRect, NULL, RDW_INVALIDATE);
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

void UI_Edit_KeyDown(WindowHandle hEdit, int vk) {
    SendMessageA(WH_TO_HWND(hEdit), WM_KEYDOWN, vk, 0);
}

void UI_Edit_KeyUp(WindowHandle hEdit, int vk) {
    SendMessageA(WH_TO_HWND(hEdit), WM_KEYUP, vk, 0);
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

void UI_GetCurrentCursorPosition(uint32_t *start, uint32_t *end) {
    if (start && end) UI_Edit_GetSel(FlintContext_Get()->window.g_ActiveEditControl, start, end);
}

void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
    FlintContext* _c = FlintContext_Get();
    if (_c->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, buffer, sizeof(buffer));
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

void UI_RedrawBackground(HDC hdc, RECT* rc, BOOL is_selected) {
    HBRUSH hBgBrush = CreateSolidBrush(is_selected ? COLOR_SELECTED_TEXT : COLOR_BG);
    FillRect(hdc, rc, hBgBrush);
    DeleteObject(hBgBrush);
}

void UI_GetParent(WindowHandle hwnd) {
    SendMessageA(GetParent(WH_TO_HWND(hwnd)), WM_COMMAND, MAKEWPARAM(IDC_INPUT_BOX, EN_CHANGE), (LPARAM)WH_TO_HWND(FlintContext_Get()->window.hEditControl));
}
