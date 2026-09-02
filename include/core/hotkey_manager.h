#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include "helpers/hotkey.h"

bool hotkey_register_internal(HotkeyBinding binding);
bool hotkey_process(int vk_code, int modifiers, const char* title);

#endif
