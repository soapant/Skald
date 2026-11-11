#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include <stdbool.h>
#include "helpers/helpers_hotkey.h"
#include "core/types/keys.h"

#define HOTKEY_STRING_MAX_LEN 64

bool HotkeyString_Parse(const char *str, HotkeyKey *out);
bool HotkeyString_Format(HotkeyKey key, char *out, int out_size);

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool hotkey_register_internal(HotkeyBinding binding);
bool hotkey_unregister_internal(int vk_code, int modifiers);
int hotkey_count(void);
const HotkeyBinding *hotkey_get(int index);
bool hotkey_is_taken(int vk_code, int modifiers);
const HotkeyBinding *hotkey_find_by_callback(HotkeyCallback cb);
const HotkeyBinding *hotkey_find_by_ud(void *ud);
bool hotkey_process(int vk_code, int modifiers);
void hotkey_drain_pending(void);
int hotkey_wake_fd(void);
void hotkey_set_suspended(bool suspended);
bool hotkey_has_mouse_binding(void);
bool hotkey_mouse_button_bound(int btn);

#endif
