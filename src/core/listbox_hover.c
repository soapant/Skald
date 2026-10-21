#include "core/listbox_hover.h"
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

void ListBox_PublishMousePos(int x, int y) {
    g_mouse_x = x;
    g_mouse_y = y;
    g_mouse_valid = true;
}

void ListBox_InvalidateMousePos(void) {
    g_mouse_valid = false;
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
    if (top && Platform_WindowToRoot(top->handle, &wx, &wy)) {
        mx -= wx;
        my -= wy;
    }
    Platform_GetDrawOffset(&dx, &dy);
    mx -= dx;
    my -= dy;
    if (out_x) *out_x = mx;
    if (out_y) *out_y = my;
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
    int mx = 0, my = 0;
    if (!ListBox_ClientMousePos(&mx, &my)) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    int idx = ListBox_RowAtY(lb, list_top, item_height, top_index, visible_count, my);
    if (idx < 0) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
    if (idx == lb->selected_index) { PROFILE_END_VAR(_pt, "ListBox_HoverFocus"); return false; }
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
    if (off == lb->view_offset) { PROFILE_END_VAR(_pt, "ListBox_ScrollBy"); return; }
    lb->view_offset = off;
    UI_LB_SetTopIndex(FlintContext_Get()->window.hListBox, off);
    PROFILE_END_VAR(_pt, "ListBox_ScrollBy");
}

void ListBox_ApplyPendingScroll(ListBoxInfo* lb, int visible_count) {
    int rows = ListBox_TakeScroll();
    if (rows == 0) return;
    ListBox_ScrollBy(lb, rows, visible_count);
}
