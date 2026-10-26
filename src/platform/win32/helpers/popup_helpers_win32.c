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
    PROFILE_START(_pt);
    if (hPopup && IsWindow(hPopup)) {
        PromptState* state = Platform_GetPromptStateForHWND(hPopup);
        if (state) {
            { PROFILE_END_VAR(_pt, "GetLBFromPopup"); return &state->listbox; }
        }
        { PROFILE_END_VAR(_pt, "GetLBFromPopup"); return (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA); }
    }
  { PROFILE_END_VAR(_pt, "GetLBFromPopup"); return NULL; }
}
