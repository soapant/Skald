#include <windows.h>
#include "core/context.h"
#include "platform/win32/popup_helpers_win32.h"

#include "core/timer.h"
bool only_once = true;
bool rainbow_init = false;
int g_last_selected_index = -1;
int g_pending_navigate = -1;

extern PromptState* Platform_GetPromptStateForHWND(HWND hwnd);

ListBoxInfo* GetLBFromPopup(HWND hPopup) {
    
    if (hPopup && IsWindow(hPopup)) {
        PromptState* state = Platform_GetPromptStateForHWND(hPopup);
        if (state) {
            return &state->listbox;
        }
        return (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
    }
  return NULL;
}
