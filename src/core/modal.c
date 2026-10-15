#include "core/context.h"
#include "core/modal.h"
#include "core/modal.h"
#include "config.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/context.h"
#include "core/vim.h"
#include "helpers/helpers_ui.h"

static const char* g_mode_names[] = { "Normal", "Insert", "Visual" };
int last_index = 0;
char* selection_feature = "";

const char* Vim_ModeName(EditorMode mode) {
    if ((int)mode < 0 || (int)mode > 2) return g_mode_names[0];
    return g_mode_names[mode];
}

static EditorMode ActiveMode(void) {
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e) return e->mode;
    }
    return FlintContext_Get()->vim_mode;
}

static void clamp_cursor_for_mode(EditorMode mode) {
    if (mode != MODE_NORMAL) return;
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) return;
    char text[1024];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    if (len <= 0) return;
    if (Vim_GetCursor(ctx) > len - 1) Vim_SetCursor(ctx, len - 1);
}

void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    FlintContext* ctx = FlintContext_Get();
    if (ctx) { ctx->vim_mode = mode; ctx->vim_mode_str = g_vim_mode; }
    clamp_cursor_for_mode(mode);
    WindowManager* wm = WindowManager_Get();
    bool in_main = !wm || wm->popup_count <= 1;

    if (!lb) {
        lb = Vim_GetListBox(ctx);
    }

    if (in_main && IsPopupActive(WH_NULL) && lb) {
        if (strcmp(g_vim_mode, "Insert") == 0) {
            if (RememberLastIndex) { last_index = lb->selected_index; if (ctx) ctx->window.last_index = last_index; }
            UI_SetSelection(lb, FlintContext_Get()->window.hListBox, -1);
        } else if (strcmp(g_vim_mode, "Normal") == 0) {
            if (last_index < lb->total_matches && RememberLastIndex) {
                UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, last_index);
            } else {
                UI_SetSelection(lb, FlintContext_Get()->window.hListBox, 0);
            }
        }
    }

#if defined(_WIN32)
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) {
        InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), NULL, TRUE);
    }
    UpdateTextCaret();
#endif
}
void SetMode(EditorMode mode) {
    WindowManager* wm = WindowManager_Get();
    Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    if (top && top->type == POPUP_EDIT) {
        WMEditPopupData* e = (WMEditPopupData*)top;
        if (e) {
            e->mode = mode;
            clamp_cursor_for_mode(mode);
            UpdateTextCaret();
            return;
        }
    }
    SetModeWithListBox(mode, NULL);
}

bool IsNormalMode(void) {
    return ActiveMode() == MODE_NORMAL;
}

bool IsInsertMode(void) {
    return ActiveMode() == MODE_INSERT;
}

bool IsVisualMode(void) {
    return ActiveMode() == MODE_VISUAL;
}

ColorRef Modal_CursorColor(void) {
    return ActiveMode() == MODE_VISUAL ? COLOR_VISUAL_CURSOR : COLOR_CURSOR;
}