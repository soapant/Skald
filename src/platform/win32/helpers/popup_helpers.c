#include <windows.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/types.h"
#include "platform/win32/popup_helpers.h"

bool only_once = true;
bool rainbow_init = false;
int g_last_selected_index = -1;
int g_pending_navigate = -1;

ListBoxInfo* GetLBFromPopup(HWND hPopup) {
  if (hPopup && IsWindow(hPopup)) return (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
  return NULL;
}
ListBoxInfo* GetLBFromChild(HWND hChild) {
  HWND hParent = GetParent(hChild);
  if (hParent && IsWindow(hParent)) return GetLBFromPopup(hParent);
  
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) return GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  return NULL;
}
ListBoxInfo* GetLBForPopupWindow(HWND hPopup) { return GetLBFromPopup(hPopup); }

void RequestInitialNavigation(int index) {
  g_pending_navigate = index;
}
