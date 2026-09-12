#include "core/context.h"
#include "core/modal.h"
#include "core/modal.h"
#include "config.h"
#include "core/common_ui.h"
#include "core/window_manager.h"
#include "core/context.h"

static const char* g_mode_names[] = { "Normal", "Insert", "Visual" };
int last_index = 0;
char* selection_feature = "";

static ListBoxInfo* GetActiveListBoxFromTopPopup(void) {
    WindowManager* wm = WindowManager_Get();
    if (!wm) return NULL;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top || !top->user_data) return NULL;
    if (top->type != POPUP_PROMPT) return NULL;
    PromptState* state = (PromptState*)top->user_data;
    if (state->has_listbox) return &state->listbox;
    return NULL;
}

void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    FlintContext* ctx = FlintContext_Get();
    if (ctx) { ctx->vim_mode = mode; ctx->vim_mode_str = g_vim_mode; }
    WindowManager* wm = WindowManager_Get();
    bool in_main = wm ? wm->popup_count == 1 : true;

    if (!lb) {
        lb = GetActiveListBoxFromTopPopup();
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
    if (WH_TO_HWND(FlintContext_Get()->window.g_ActiveEditControl) != NULL) {
        UpdateTextCaret(FlintContext_Get()->window.g_ActiveEditControl);
    }
#endif
}

void SetMode(EditorMode mode) {
    SetModeWithListBox(mode, NULL);
}

EditorMode GetMode(void) {
    return g_current_mode;
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
