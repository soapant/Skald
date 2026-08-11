#include "common.h"
#include "core/modal.h"
#include "core/common_ui.h"

EditorMode g_current_mode = MODE_INSERT;
bool g_vim_mode_enabled = true;
static const char* g_mode_names[] = { "NORMAL", "INSERT", "VISUAL" };
const char* g_vim_mode = "INSERT";
int last_index = 0;
char* selection_feature = "";

void SetMode(EditorMode mode) {
    g_current_mode = mode;
    g_vim_mode = g_mode_names[mode];
    bool in_main = !g_hSearchPopup && !g_hEditPopup;
    draw_background = true;

    if (hPopupWnd && IsWindow(hPopupWnd)) {
        RECT rect;
        GetClientRect(hPopupWnd, &rect);
        RECT titleRect = {0, 0, rect.right, 40};
        InvalidateRect(hPopupWnd, &titleRect, FALSE);
    }

    if (in_main) {
        if (strcmp(g_vim_mode,"INSERT") == 0 ) {
          if (RememberLastIndex) last_index = g_SelectedIndex;
          UI_SetSelection(hListBox,-1, g_SelectedIndex);
        } else if (strcmp(g_vim_mode,"NORMAL") == 0 ) {
          if (RememberLastIndex > g_TotalMatches && RememberLastIndex && last_index >= 0) {
            printf("LAST INDEX %i \n",last_index);
            UI_NavigateTo(last_index);
          } else {
            UI_SetSelection(hListBox,0,g_SelectedIndex);
          }
        }
    }

    if (g_ActiveEditControl) {
        UpdateTextCaret(g_ActiveEditControl);
    }

}

EditorMode GetMode(void) { return g_current_mode; }
bool IsNormalMode(void) { return g_current_mode == MODE_NORMAL; }
bool IsInsertMode(void) { return g_current_mode == MODE_INSERT; }
bool IsVisualMode(void) { return g_current_mode == MODE_VISUAL; }
