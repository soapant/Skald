#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
bool IsPopupActive(WindowHandle hPopupWnd) {
      
    (void)hPopupWnd;
    WindowManager* wm = WindowManager_Get();
    return wm && wm->popup_count > 0;
}

void UI_SetSelection(ListBoxInfo* lb, int index) {
    lb->selected_index = index;
    return;
}

void UI_SetHorizontalExtent(int width) {
}

void UI_LB_SetCount(ListBoxInfo* lb,int count) {
    if (lb) lb->populated_count = count;
}

void UI_LB_BeginUpdate() {
}

void UI_LB_EndUpdate() {
}

void UI_RequestPopupRepaint(void) {
    
    
}

void UI_LegacySyncRepaint(void) {
    
    
}

void UI_LB_SetItemHeight(int height) {
}

void UI_LB_SetCaretIndex(int index, bool scroll) {
}

void UI_LB_SetTopIndex(int index) {
}

int UI_LB_GetTopIndex(ListBoxInfo* lb) {
  return 0;
}

int UI_LB_GetViewTop(ListBoxInfo* lb) {
        
    return lb ? lb->view_offset : 0;
}

static int g_x11_lb_visible;

int UI_LB_GetVisibleCount(void) {
        
return g_x11_lb_visible;
}

void UI_LB_SetVisibleCount(int count) {
        
g_x11_lb_visible = count;
}

void UI_LB_ScrollToTop(ListBoxInfo* lb, int top_index) {
        
if (!lb) {  return ; }
    if (top_index < 0) top_index = 0;
    if (g_x11_lb_visible > 0 && top_index + g_x11_lb_visible > lb->populated_count)
        top_index = lb->populated_count - g_x11_lb_visible;
    if (top_index < 0) top_index = 0;
    lb->view_offset = top_index;
}

int UI_LB_GetItemHeight() {
      
    return 0;
}

void UI_NavigateTo(ListBoxInfo* lb,int index) {
        
if (!lb) {  return ; }
    int count = lb->populated_count;
    if (count <= 0) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(-1, false);
        return;
    }
    if (index >= count) index = count - 1;
    if (index == -1) {
        lb->selected_index = -1;
        UI_LB_SetCaretIndex(-1, false);
        return;
    }
    int old_sel = lb->selected_index;
    ListBox_FocusByKeyboard();
    if (old_sel == index) {  return ; }
    lb->selected_index = index;
    UI_LB_SetCaretIndex(index, false);
    ListBox_ScrollToSelection(lb);
}

void UI_NavigateUp(ListBoxInfo* lb) {
        
    ListBox_NavigateUp(lb);
    
}

void UI_NavigateDown(ListBoxInfo* lb) {
    
    ListBox_NavigateDown(lb);
    
}

void UI_Edit_SetSel(uint32_t start, uint32_t end) {
        
(void)start; (void)end;
}

void UI_Edit_GetSel(uint32_t* start, uint32_t* end) {
        
if (start) *start = 0;
    if (end) *end = 0;
}

void UI_GetCurrentTextEdit(char* buffer, size_t cap) {
        
if (!buffer || cap == 0) {  return ; }
    buffer[0] = '\0';
    WindowManager* wm = WindowManager_Get();
    if (!wm) {  return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) {  return ; }
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
        
if (!text) text = "";
    WindowManager* wm = WindowManager_Get();
    if (!wm) {  return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) {  return ; }
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

int UI_GetCursorPosition(void) {
        
WindowManager* wm = WindowManager_Get();
    if (!wm) return 0;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) return 0;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        
        return (int)prompt->cursor;
    }
    if (top->type == POPUP_EDIT) {
        WMEditPopupData* ed = (WMEditPopupData*)top;
        if (ed) return (int)ed->cursor;
    }
    
    return 0;
}

void UI_SetCursorPosition(int position) {
        
WindowManager* wm = WindowManager_Get();
    if (!wm) {  return ; }
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top) {  return ; }
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

void UI_RedrawNow(ListBoxInfo* lb) {
        
    Prompt_RedrawNow(lb);
    
}

void UI_Redraw(ListBoxInfo* lb) {
        
        UI_RedrawNow(lb);
}

void UI_SyncFilterFromEdit(FlintContext* ctx) {
        
if (!ctx) {  return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) {  return ; }
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
        
if (!ctx) {  return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) {  return ; }
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
        
if (!ctx) return NULL;
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) return NULL;
    if (top->type == POPUP_PROMPT && top->user_data) {
        PromptState* prompt = (PromptState*)top->user_data;
        return prompt->has_listbox ? &prompt->listbox : NULL;
    }
    return NULL;
}

void UI_SyncEditCursor(FlintContext* ctx, int pos) {
        
if (!ctx) {  return ; }
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (!top) {  return ; }
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
        
(void)ctx;
    return false;
}
