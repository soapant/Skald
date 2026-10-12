#include <windows.h>
#include "core/context.h"
#include "platform/win32/popup_helpers_win32.h"

#include "core/timer.h"
bool only_once = true;
bool rainbow_init = false;
int g_last_selected_index = -1;
int g_pending_navigate = -1;

ListBoxInfo* GetLBFromPopup(HWND hPopup) {
    PROFILE_START(_pt);
if (hPopup && IsWindow(hPopup)) { PROFILE_END_VAR(_pt, "GetLBFromPopup"); return (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA); }
  { PROFILE_END_VAR(_pt, "GetLBFromPopup"); return NULL; }
}
ListBoxInfo* GetLBFromChild(HWND hChild) {
    PROFILE_START(_pt);
HWND hParent = GetParent(hChild);
  if (hParent && IsWindow(hParent)) { PROFILE_END_VAR(_pt, "GetLBFromChild"); return GetLBFromPopup(hParent); }
  
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) { PROFILE_END_VAR(_pt, "GetLBFromChild"); return GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd)); }
  { PROFILE_END_VAR(_pt, "GetLBFromChild"); return NULL; }
}
ListBoxInfo* GetLBForPopupWindow(HWND hPopup) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "GetLBForPopupWindow"); return GetLBFromPopup(hPopup); } }

void RequestInitialNavigation(int index) {
    PROFILE_START(_pt);
g_pending_navigate = index;
}
