#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "core/common_ui.h"

EditorMode g_current_mode = MODE_INSERT;
static const char* g_mode_names[] = { "NORMAL", "INSERT", "VISUAL" };
const char* g_vim_mode = "INSERT";
int last_index = 0;
char* selection_feature = "";

#if defined(_WIN32)
static ListBoxInfo* GetActiveListBoxForModal(void) {
  if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
    return (ListBoxInfo*)GetWindowLongPtrA(WH_TO_HWND(hPopupWnd), GWLP_USERDATA);
  }
  return NULL;
}
#endif

void SetModeWithListBox(EditorMode mode, ListBoxInfo* lb) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    bool in_main = WH_TO_HWND(g_hSearchPopup) == NULL && WH_TO_HWND(g_hEditPopup) == NULL;

#if defined(_WIN32)
    if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
        RECT rect;
        GetClientRect(WH_TO_HWND(hPopupWnd), &rect);
        RECT titleRect = {0, 0, rect.right, 40};
        InvalidateRect(WH_TO_HWND(hPopupWnd), &titleRect, FALSE);
    }
#endif

    // Resolve lb if not provided
    if (!lb) {
#if defined(_WIN32)
        lb = GetActiveListBoxForModal();
#else
        extern PromptState* g_currentPromptState;
        if (g_currentPromptState && g_currentPromptState->has_listbox) lb = &g_currentPromptState->listbox;
        else lb = NULL;
#endif
    }

    if (in_main && IsPopupActive(hPopupWnd) && lb) {
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

    if (WH_TO_HWND(g_ActiveEditControl) != NULL) {
        UpdateTextCaret(g_ActiveEditControl);
    }
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
