#include "core/listbox_hover.h"
#include "deps/log.h"
#include <stdlib.h>
#include "core/context.h"
#include "core/timer.h"
#include "core/window_manager.h"
#include "config.h"
#include "platform/draw.h"
#include "platform/input_state.h"
#include "platform/ui.h"

static bool g_mouse_valid = false;
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_pending_scroll = 0;
static bool g_focus_by_keyboard = false;
static bool g_mouse_moved = false;
static int g_list_top = 0;
static int g_item_height = 0;
static int g_visible_count = 0;
static bool g_pub_set = false;
static int g_pub_x = 0;
static int g_pub_y = 0;

void ListBox_FocusByKeyboard(void) {
    g_focus_by_keyboard = true;
    g_mouse_moved = false;
}

void ListBox_FocusByMouse(void) {
    g_focus_by_keyboard = false;
}

void ListBox_PublishMousePos(int x, int y) {
    if (g_pub_set && (g_pub_x != x || g_pub_y != y)) g_mouse_moved = true;
    g_pub_x = x;
    g_pub_y = y;
    g_pub_set = true;
    g_mouse_x = x;
    g_mouse_y = y;
    g_mouse_valid = true;
}

void ListBox_InvalidateMousePos(void) {
    g_mouse_valid = false;
    g_mouse_moved = false;
}

bool ListBox_ClientMousePos(int* out_x, int* out_y) {
    PROFILE_START(_pt);
    if (!g_mouse_valid) {
        if (!Platform_GetMousePos(&g_mouse_x, &g_mouse_y)) {
            g_mouse_valid = false;
            if (out_x) *out_x = 0;
            if (out_y) *out_y = 0;
            PROFILE_END_VAR(_pt, "ListBox_ClientMousePos");
            return false;
        }
        g_mouse_valid = true;
    }
    int mx = g_mouse_x;
    int my = g_mouse_y;
    int wx = 0, wy = 0, dx = 0, dy = 0;
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    bool have_origin = Platform_WindowToRoot(FlintContext_Get()->window.hPopupWnd, &wx, &wy);
    if (!have_origin && top) have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
    if (have_origin) {
        mx -= wx;
        my -= wy;
    }
    Platform_GetDrawOffset(&dx, &dy);
    mx -= dx;
    my -= dy;
    int pw = 0, ph = 0;
    if (top) {
        if (top->type == POPUP_fl_window && top->user_data) {
            PromptState* s = (PromptState*)top->user_data;
            pw = s->width;
            ph = s->height;
        } else if (top->type == POPUP_EDIT) {
            WMEditPopupData* e = (WMEditPopupData*)top;
            pw = e->w;
            ph = e->h;
        }
    }
    bool inside = (pw <= 0 || ph <= 0) ||
                  (mx >= 0 && my >= 0 && mx < pw && my < ph);
    if (out_x) *out_x = mx;
    if (out_y) *out_y = my;
    if (!inside) {
        PROFILE_END_VAR(_pt, "ListBox_ClientMousePos");
        return false;
    }
    PROFILE_END_VAR(_pt, "ListBox_ClientMousePos");
    return true;
}

int ListBox_RowAtY(const ListBoxInfo* lb, int list_top, int item_height,
                   int top_index, int visible_count, int mouse_y) {
    PROFILE_START(_pt);
    if (!lb || item_height <= 0 || visible_count <= 0) {
        PROFILE_END_VAR(_pt, "ListBox_RowAtY");
        return -1;
    }
    int rel = mouse_y - list_top;
    if (rel < 0) { PROFILE_END_VAR(_pt, "ListBox_RowAtY"); return -1; }
    int row = rel / item_height;
    if (row >= visible_count) { PROFILE_END_VAR(_pt, "ListBox_RowAtY"); return -1; }
    int idx = top_index + row;
    if (idx < 0 || idx >= lb->populated_count) { PROFILE_END_VAR(_pt, "ListBox_RowAtY"); return -1; }
    { PROFILE_END_VAR(_pt, "ListBox_RowAtY"); return idx; }
}

bool ListBox_HoverFocus(ListBoxInfo* lb, int list_top, int item_height,
                        int top_index, int visible_count) {
    PROFILE_START(_pt);
    if (!lb || lb->populated_count <= 0) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    if (!g_mouse_moved) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    g_mouse_moved = false;
    ListBox_FocusByMouse();
    int mx = 0, my = 0;
    if (!ListBox_ClientMousePos(&mx, &my)) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    int idx = ListBox_RowAtY(lb, list_top, item_height, top_index, visible_count, my);
    if (idx < 0 || idx == lb->selected_index) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, idx);
    PROFILE_END_VAR(_pt, "ListBox_HoverFocus");
    return true;
}

void ListBox_QueueScroll(int rows) {
    g_pending_scroll += rows;
}

int ListBox_TakeScroll(void) {
    int rows = g_pending_scroll;
    g_pending_scroll = 0;
    return rows;
}

void ListBox_ScrollBy(ListBoxInfo* lb, int rows, int visible_count) {
    PROFILE_START(_pt);
    if (!lb || rows == 0) { PROFILE_END_VAR(_pt, "ListBox_ScrollBy"); return; }
    int max_off = lb->populated_count - visible_count;
    if (max_off < 0) max_off = 0;
    int off = lb->view_offset + rows;
    if (off < 0) off = 0;
    if (off > max_off) off = max_off;
    int delta = off - lb->view_offset;
    if (delta == 0) { PROFILE_END_VAR(_pt, "ListBox_ScrollBy"); return; }
    int old_sel = lb->selected_index;
    lb->view_offset = off;
    if (!g_focus_by_keyboard && old_sel >= 0) {
        int sel = old_sel + delta;
        if (sel < 0) sel = 0;
        if (sel >= lb->populated_count) sel = lb->populated_count - 1;
        lb->selected_index = sel;
        UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, sel, false);
    }
    UI_LB_SetTopIndex(FlintContext_Get()->window.hListBox, off);
    PROFILE_END_VAR(_pt, "ListBox_ScrollBy");
}

void ListBox_ApplyPendingScroll(ListBoxInfo* lb, int visible_count) {
    int rows = ListBox_TakeScroll();
    if (rows == 0) return;
    ListBox_ScrollBy(lb, rows, visible_count);
}

int ListBox_ScrollTopFor(int index, int visible_count, int count) {
    if (visible_count <= 0) visible_count = 16;
    int new_top = index - visible_count / 2;
    if (new_top < 0) new_top = 0;
    if (new_top + visible_count > count) new_top = count - visible_count;
    if (new_top < 0) new_top = 0;
    return new_top;
}

void ListBox_ScrollToSelection(ListBoxInfo* lb) {
    if (!lb || lb->selected_index < 0 || lb->populated_count <= 0) return;
    int visible = UI_LB_GetVisibleCount();
    if (visible <= 0) visible = 16;
    int old_top = UI_LB_GetViewTop(lb);
    int new_top = ListBox_ScrollTopFor(lb->selected_index, visible, lb->populated_count);
    if (new_top != old_top) UI_LB_ScrollToTop(lb, new_top);
}

void ListBox_SetGeometry(int list_top, int item_height, int visible_count) {
    g_list_top = list_top;
    g_item_height = item_height;
    g_visible_count = visible_count;
}

void ListBox_GetGeometry(int* out_list_top, int* out_item_height, int* out_visible_count) {
    if (out_list_top) *out_list_top = g_list_top;
    if (out_item_height) *out_item_height = g_item_height;
    if (out_visible_count) *out_visible_count = g_visible_count;
}

void ListBox_NavigateUp(ListBoxInfo* lb, WindowHandle hListBox) {
    if (!lb) return;
    int sel = lb->selected_index;
    if (sel >= 0) sel--;
    if (sel == lb->selected_index) return;
    UI_NavigateTo(lb, hListBox, sel);
}

void ListBox_NavigateDown(ListBoxInfo* lb, WindowHandle hListBox) {
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

void ListBox_CommandExecute(ListBoxInfo* lb, const char* string) {
    if (!lb || !string) return;
    int num = atoi(string);
    UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, num - 1);
    ListBox_ScrollToSelection(lb);
}
