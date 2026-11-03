#ifndef CORE_EDIT_POPUP_KEYS_H
#define CORE_EDIT_POPUP_KEYS_H

#include <stdbool.h>
#include "core/window_manager.h"

bool EditPopup_HandleKey(WMEditPopupData *ed, const PopupKeyEvent *key);
bool EditPopup_SelectAll(WMEditPopupData *ed);
bool EditPopup_CopySelection(WMEditPopupData *ed);
bool EditPopup_DeleteSelection(WMEditPopupData *ed);
void EditPopup_InsertText(WMEditPopupData *ed, const char *text, int len);

#endif
