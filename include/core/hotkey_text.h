#ifndef CORE_HOTKEY_TEXT_H
#define CORE_HOTKEY_TEXT_H

#include <stdbool.h>
#include "core/window_manager.h"

bool HotkeyText_ShouldSwallow(const PopupKeyEvent *key);
bool HotkeyText_IsPrintable(const PopupKeyEvent *key);

#endif
