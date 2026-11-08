#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include "helpers/helpers_hotkey.h"

bool hotkey_register_internal(HotkeyBinding binding);
bool hotkey_unregister_internal(int vk_code, int modifiers);
int hotkey_count(void);
const HotkeyBinding *hotkey_get(int index);
bool hotkey_is_taken(int vk_code, int modifiers);
bool hotkey_process(int vk_code, int modifiers);
void hotkey_drain_pending(void);
void hotkey_set_suspended(bool suspended);
bool hotkey_has_mouse_binding(void);
bool hotkey_mouse_button_bound(int btn);

#endif