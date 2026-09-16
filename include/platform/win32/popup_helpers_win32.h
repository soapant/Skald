#ifndef POPUP_HELPERS_H
#define POPUP_HELPERS_H

#include <windows.h>
#include <stdbool.h>
#include "core/types.h"

#define WM_PREVIEW_SELECTION (WM_APP + 1)

ListBoxInfo* GetLBFromPopup(HWND hPopup);
ListBoxInfo* GetLBFromChild(HWND hChild);
ListBoxInfo* GetLBForPopupWindow(HWND hPopup);

extern bool only_once;
extern bool rainbow_init;
extern int g_last_selected_index;
extern int g_pending_navigate;

void RequestInitialNavigation(int index);

#endif
