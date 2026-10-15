#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

#include "core/timer.h"
bool IsPopupActive(WindowHandle hPopupWnd) {
        PROFILE_START(_pt);
(void)hPopupWnd;
    WindowManager* wm = WindowManager_Get();
    { PROFILE_END_VAR(_pt, "IsPopupActive"); return wm && wm->popup_count > 0; }
}

void UI_SetSelection(ListBoxInfo* lb, WindowHandle hList, int index) {
        PROFILE_START(_pt);
(void)hList;
    if (!lb) { PROFILE_END_VAR(_pt, "UI_SetSelection"); return ; }
    lb->selected_index = index;
}

void UI_SetHorizontalExtent(WindowHandle hList, int width) {
        PROFILE_START(_pt);
(void)hList; (void)width;
}

void UI_LB_SetCount(ListBoxInfo* lb, WindowHandle hList, int count) {
        PROFILE_START(_pt);
(void)hList;
    if (lb) lb->populated_count = count;
}

void UI_LB_SetItemHeight(WindowHandle hList, int height) {
        PROFILE_START(_pt);
(void)hList; (void)height;
}

void UI_LB_SetCaretIndex(WindowHandle hList, int index, bool scroll) {
        PROFILE_START(_pt);
(void)hList; (void)index; (void)scroll;
}

void UI_LB_SetTopIndex(WindowHandle hList, int index) {
        PROFILE_START(_pt);
(void)hList; (void)index;
}

int UI_LB_GetTopIndex(WindowHandle hList) {
        PROFILE_START(_pt);
(void)hList;
    { PROFILE_END_VAR(_pt, "UI_LB_GetTopIndex"); return 0; }
}

static int g_x11_lb_visible;

int UI_LB_GetVisibleCount(void) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "UI_LB_GetVisibleCount"); return g_x11_lb_visible; }
}

void UI_LB_SetVisibleCount(int count) {
        PROFILE_START(_pt);
g_x11_lb_visible = count;
}

void UI_LB_ScrollToTop(ListBoxInfo* lb, int top_index) {
        PROFILE_START(_pt);
if (!lb) { PROFILE_END_VAR(_pt, "UI_LB_ScrollToTop"); return ; }
    if (top_index < 0) top_index = 0;
    if (g_x11_lb_visible > 0 && top_index + g_x11_lb_visible > lb->populated_count)
        top_index = lb->populated_count - g_x11_lb_visible;
    if (top_index < 0) top_index = 0;
    lb->view_offset = top_index;
}

void UI_LB_SetFont(WindowHandle hList, void* hFont) {
        PROFILE_START(_pt);
(void)hList; (void)hFont;
}

int UI_LB_GetItemHeight(WindowHandle hList) {
        PROFILE_START(_pt);
(void)hList;
    { PROFILE_END_VAR(_pt, "UI_LB_GetItemHeight"); return 0; }
}

void UI_NavigateTo(ListBoxInfo* lb, WindowHandle hListBox, int index) {
        PROFILE_START(_pt);
if (!lb) { PROFILE_END_VAR(_pt, "UI_NavigateTo"); return ; }
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
    if (old_sel == index) { PROFILE_END_VAR(_pt, "UI_NavigateTo"); return ; }
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
        PROFILE_START(_pt);
if (!lb) { PROFILE_END_VAR(_pt, "UI_NavigateUp"); return ; }
    int sel = lb->selected_index;
    if (sel >= 0) sel--;
    if (sel == lb->selected_index) { PROFILE_END_VAR(_pt, "UI_NavigateUp"); return ; }
    UI_NavigateTo(lb, hListBox, sel);
}

void UI_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox) {
        PROFILE_START(_pt);
if (!lb) { PROFILE_END_VAR(_pt, "UI_NavigateDown"); return ; }
    int count = lb->populated_count;
    if (count <= 0) { PROFILE_END_VAR(_pt, "UI_NavigateDown"); return ; }
    int sel = lb->selected_index;
    if (sel < 0) sel = -1;
    if (sel >= count - 1) sel = count - 1;
    else sel++;
    if (sel == lb->selected_index) { PROFILE_END_VAR(_pt, "UI_NavigateDown"); return ; }
    UI_NavigateTo(lb, hListBox, sel);
}

void UI_Edit_SetSel(uint32_t start, uint32_t end) {
        PROFILE_START(_pt);
(void)start; (void)end;
}

void UI_Edit_GetSel(uint32_t* start, uint32_t* end) {
        PROFILE_START(_pt);
if (start) *start = 0;
    if (end) *end = 0;
}

void UI_GetCurrentTextEdit(char* buffer, size_t cap) {
        PROFILE_START(_pt);
if (!buffer || cap == 0) { PROFILE_END_VAR(_pt, "UI_GetCurrentTextEdit"); return ; }
    buffer[0] = '\0';
    WindowManager* wm = WindowManager_Get();
    if (!wm) { PROFILE_END_VAR(_pt, "UI_GetCurrentTextEdit"); return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(_pt, "UI_GetCurrentTextEdit"); return ; }
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

void UI_SetText(const char* text) {
        PROFILE_START(_pt);
if (!text) text = "";
    WindowManager* wm = WindowManager_Get();
    if (!wm) { PROFILE_END_VAR(_pt, "UI_SetText"); return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(_pt, "UI_SetText"); return ; }
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

void UI_SetCursorPosition(int position) {
        PROFILE_START(_pt);
WindowManager* wm = WindowManager_Get();
    if (!wm) { PROFILE_END_VAR(_pt, "UI_SetCursorPosition"); return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) { PROFILE_END_VAR(_pt, "UI_SetCursorPosition"); return ; }
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
        PROFILE_START(_pt);
FlintContext* _c = FlintContext_Get();
    if (_c->prompt_callbacks.refresh != NULL) {
        char buffer[MaxFeatures];
        UI_GetCurrentTextEdit(buffer, sizeof(buffer));
        _c->prompt_callbacks.refresh(buffer, lb);
    }
}

void UI_Redraw(ListBoxInfo* lb, WindowHandle hListBox) {
        PROFILE_START(_pt);
UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
}

void UI_SyncFilterFromEdit(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_SyncFilterFromEdit"); return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) { PROFILE_END_VAR(_pt, "UI_SyncFilterFromEdit"); return ; }
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        strncpy(ctx->window.filter, prompt->filter, sizeof(ctx->window.filter) - 1);
        ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
        ctx->window.filter_cursor = prompt->cursor;
        return;
    }
    if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed && ed->text) {
            strncpy(ctx->window.filter, ed->text, sizeof(ctx->window.filter) - 1);
            ctx->window.filter[sizeof(ctx->window.filter) - 1] = '\0';
            ctx->window.filter_cursor = ed->cursor;
        }
    }
}

void UI_ApplyFilterToEdit(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_ApplyFilterToEdit"); return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) { PROFILE_END_VAR(_pt, "UI_ApplyFilterToEdit"); return ; }
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        strncpy(prompt->filter, ctx->window.filter, sizeof(prompt->filter) - 1);
        prompt->filter[sizeof(prompt->filter) - 1] = '\0';
        return;
    }
    if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed && ed->text && ed->text_capacity > 1) {
            strncpy(ed->text, ctx->window.filter, ed->text_capacity - 1);
            ed->text[ed->text_capacity - 1] = '\0';
        }
    }
}

ListBoxInfo* UI_GetPopupListBox(FlintContext* ctx) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_GetPopupListBox"); return NULL; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) { PROFILE_END_VAR(_pt, "UI_GetPopupListBox"); return NULL; }
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        return prompt->has_listbox ? &prompt->listbox : NULL;
    }
    { PROFILE_END_VAR(_pt, "UI_GetPopupListBox"); return NULL; }
}

void UI_SyncEditCursor(FlintContext* ctx, int pos) {
        PROFILE_START(_pt);
if (!ctx) { PROFILE_END_VAR(_pt, "UI_SyncEditCursor"); return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) { PROFILE_END_VAR(_pt, "UI_SyncEditCursor"); return ; }
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        int len = (int)strlen(prompt->filter);
        if (pos < 0) pos = 0;
        if (pos > len) pos = len;
        prompt->cursor = pos;
    }
    else if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (!ed || !ed->text) return;
        int len = (int)strlen(ed->text);
        if (pos < 0) pos = 0;
        if (pos > len) pos = len;
        ed->cursor = pos;
    }
    UI_SetCursorPosition(pos);
}

bool UI_ApplyBackspaceToPopup(FlintContext* ctx) {
        PROFILE_START(_pt);
(void)ctx;
    { PROFILE_END_VAR(_pt, "UI_ApplyBackspaceToPopup"); return false; }
}
