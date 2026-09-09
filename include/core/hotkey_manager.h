#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include "helpers/hotkey.h"

bool hotkey_register_internal(HotkeyBinding binding);
bool hotkey_process(int vk_code, int modifiers);
bool hotkey_remap_register(int from_vk, int from_mod, int to_vk, int to_mod);
bool hotkey_remap_process(int vk_code, int modifiers, int value);

#endif
