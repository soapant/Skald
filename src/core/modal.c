#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "core/common_ui.h"
#include "core/window_manager.h"

EditorMode g_current_mode = MODE_INSERT;
static const char* g_mode_names[] = { "NORMAL", "INSERT", "VISUAL" };
const char* g_vim_mode = "INSERT";
int last_index = 0;
char* selection_feature = "";

static ListBoxInfo* GetActiveListBoxFromTopPopup(void) {
    WindowManager* wm = WindowManager_Get();
    if (!wm) return NULL;
    Popup* top = WindowManager_GetTopPopup(wm);
    if (!top || !top->user_data) return NULL;
    PromptState* state = (PromptState*)top->user_data;
    if (state->has_listbox) return &state->listbox;
    return NULL;
}

void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    WindowManager* wm = WindowManager_Get();
    bool in_main = wm ? wm->popup_count == 1 : true;

    if (!lb) {
        lb = GetActiveListBoxFromTopPopup();
    }

    if (in_main && IsPopupActive(WH_NULL) && lb) {
        if (strcmp(g_vim_mode, "INSERT") == 0) {
            if (RememberLastIndex) last_index = lb->selected_index;
            UI_SetSelection(lb, hListBox, -1);
        } else if (strcmp(g_vim_mode, "NORMAL") == 0) {
            if (last_index < lb->total_matches && RememberLastIndex) {
                UI_NavigateTo(lb, hListBox, last_index);
            } else {
                UI_SetSelection(lb, hListBox, 0);
            }
        }
    }

#if defined(WIN_32)
    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UpdateTextCaret(g_ActiveEditControl);
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
