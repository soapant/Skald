#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

bool IsPopupActive(WindowHandle hPopupWnd) {
    (void)hPopupWnd;
    WindowManager* wm = WindowManager_Get();
    return wm && wm->popup_count > 0;
}

void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
    (void)hList;
    if (!lb) return;
    lb->selected_index = index;
}

void UI_SetHorizontalExtent(WindowHandle hList, int width) {
    (void)hList; (void)width;
}

void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
    (void)hList;
    if (lb) lb->populated_count = count;
}

void UI_LB_SetItemHeight(WindowHandle hList, int height) {
    (void)hList; (void)height;
}

void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
    (void)hList; (void)index; (void)scroll;
}

void UI_LB_SetTopIndex(WindowHandle hList, int index) {
    (void)hList; (void)index;
}

int UI_LB_GetTopIndex(WindowHandle hList) {
    (void)hList;
    return 0;
}

void UI_LB_SetFont(WindowHandle hList, void* hFont) {
    (void)hList; (void)hFont;
}

int UI_LB_GetItemHeight(WindowHandle hList) {
    (void)hList;
    return 0;
}

void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
    if (!lb) return;
    int count = lb->populated_count;
    if (count <= 0) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, -1, false);
        return;
    }
    if (index >= count) index = count - 1;
    if (index == -1) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, -1, false);
        return;
    }
    int old_sel = lb->selected_index;
    if (old_sel == index) return;
    int oldTop = UI_LB_GetTopIndex(FlintContext_Get()->window.hListBox);
    lb->selected_index = index;
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, index, false);
    int visibleCount = 16;
    int newTop = oldTop;
    if (index <= oldTop + PaddingForScrolling) newTop = index - PaddingForScrolling;
    else if (index >= oldTop + visibleCount - PaddingForScrolling) newTop = index - visibleCount + PaddingForScrolling + 1;
    if (newTop < 0) newTop = 0;
    if (newTop + visibleCount > count) newTop = count - visibleCount;
    if (newTop < 0) newTop = 0;
    if (newTop != oldTop) UI_LB_SetTopIndex(FlintContext_Get()->window.hListBox, newTop);
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

void UI_Edit_SetSel(WindowHandle hEdit, uint32_t start, uint32_t end) {
    (void)hEdit; (void)start; (void)end;
}

void UI_Edit_GetSel(WindowHandle hEdit, uint32_t* start, uint32_t* end) {
    (void)hEdit;
    if (start) *start = 0;
    if (end) *end = 0;
}

void UI_GetCurrentTextEdit(WindowHandle edit, char* buffer, size_t cap) {
    (void)edit;
    if (!buffer || cap == 0) return;
    buffer[0] = '\0';
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        snprintf(buffer, cap, "%s", prompt->filter);
        buffer[cap-1]='\0';
    }
    else if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed && ed->text) snprintf(buffer, cap, "%s", ed->text);
        buffer[cap-1]='\0';
    }
}

void UI_SetText(WindowHandle hEdit, char* text) {
    (void)hEdit;
    if (!text) text = "";
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        strncpy(prompt->filter, text, sizeof(prompt->filter)-1);
        prompt->filter[sizeof(prompt->filter)-1]='\0';
        prompt->cursor = (int)strlen(prompt->filter);
    }
    else if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed && ed->text) {
            strncpy(ed->text, text, ed->text_capacity-1);
            ed->text[ed->text_capacity-1]='\0';
            ed->cursor = (int)strlen(ed->text);
        }
    }
}

void UI_SetCursorPosition(WindowHandle ctrl, int position) {
    (void)ctrl;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        int len = (int)strlen(prompt->filter);
        if (position < 0) position = 0;
        if (position > len) position = len;
        prompt->cursor = position;
    }
    else if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (!ed || !ed->text) return;
        int len = (int)strlen(ed->text);
        if (position < 0) position = 0;
        if (position > len) position = len;
        ed->cursor = position;
    }
}

void UI_RedrawNow(ListBoxInfo* lb, WindowHandle hListBox) {
    FlintContext* _c = FlintContext_Get();
    if (_c->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, buffer, sizeof(buffer));
        _c->prompt_callbacks.refresh(buffer, lb);
    }
}

void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
    UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
}

void UI_GetParent(WindowHandle hwnd) {
    (void)hwnd;
}

void UI_SyncFilterFromEdit(FlintContext* ctx) {
    (void)ctx;
}

void UI_ApplyFilterToEdit(FlintContext* ctx) {
    (void)ctx;
}

ListBoxInfo* UI_GetPopupListBox(FlintContext* ctx) {
    (void)ctx;
    return NULL;
}

void UI_SyncEditCursor(FlintContext* ctx, int pos) {
    (void)ctx;
    (void)pos;
}

bool UI_ApplyBackspaceToPopup(FlintContext* ctx) {
    (void)ctx;
    return false;
}
