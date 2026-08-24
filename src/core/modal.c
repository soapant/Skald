#include "common.h"
#include "core/modal.h"
#include "config.h"
#include "core/common_ui.h"
#include "core/vim_view.h"

EditorMode g_current_mode = MODE_INSERT;
bool g_vim_mode_enabled = true;
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
    // If an isolated VimView is active, delegate to it for per-window isolation
    VimView* active = VimView_GetActive();
    if (active) {
        VimView_SetMode(active, mode);
        // VimView_SetMode already synced globals and handled listbox selection
        // Still need to handle in_main lb resolve if caller passed NULL and active has lb
        if (!lb && active->has_listbox) lb = &active->lb;
        // Invalidate is handled inside VimView_SetMode path for globals, but keep Win32 rect
#if defined(_WIN32)
        if (WH_TO_HWND(hPopupWnd) && IsWindow(WH_TO_HWND(hPopupWnd))) {
            RECT rect;
            GetClientRect(WH_TO_HWND(hPopupWnd), &rect);
            RECT titleRect = {0, 0, rect.right, 40};
            InvalidateRect(WH_TO_HWND(hPopupWnd), &titleRect, FALSE);
        }
#endif
        return;
    }
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
    VimView* active = VimView_GetActive();
    if (active) { VimView_SetMode(active, mode); return; }
    SetModeWithListBox(mode, NULL);
}

EditorMode GetMode(void) {
    VimView* active = VimView_GetActive();
    if (active) return active->mode;
    return g_current_mode;
}
bool IsNormalMode(void) {
    VimView* active = VimView_GetActive();
    if (active) return active->mode == MODE_NORMAL;
    return g_current_mode == MODE_NORMAL;
}
bool IsInsertMode(void) {
    VimView* active = VimView_GetActive();
    if (active) return active->mode == MODE_INSERT;
    return g_current_mode == MODE_INSERT;
}
bool IsVisualMode(void) {
    VimView* active = VimView_GetActive();
    if (active) return active->mode == MODE_VISUAL;
    return g_current_mode == MODE_VISUAL;
}
