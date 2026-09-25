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

void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    FlintContext* ctx = FlintContext_Get();
    if (ctx) { ctx->vim_mode = mode; ctx->vim_mode_str = g_vim_mode; }
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
    if (WH_TO_HWND(FlintContext_Get()->window.hActiveEditControl) != NULL) {
        UpdateTextCaret(FlintContext_Get()->window.hActiveEditControl);
    }
#endif
}

void SetMode(EditorMode mode) {
    SetModeWithListBox(mode, NULL);
}

bool IsNormalMode(void) {
    return g_current_mode == MODE_NORMAL;
}
bool IsInsertMode(void) {
    return g_current_mode == MODE_INSERT;
}
bool IsVisualMode(void) {
    return g_current_mode == MODE_VISUAL;
}
