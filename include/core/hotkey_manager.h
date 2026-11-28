#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include <stdbool.h>
#include "helpers/helpers_hotkey.h"
#include "core/types/keys.h"

#define HOTKEY_STRING_MAX_LEN 64

bool hotkey_string_parse(const char *str, HotkeyKey *out);
bool hotkey_string_format(HotkeyKey key, char *out, int out_size);
char *hotkey_binding_format(const HotkeyBinding *b);

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

void hotkey_register_internal(HotkeyBinding binding);
bool hotkey_unregister_internal(int vk, int modifiers);
int hotkey_count(void);
const HotkeyBinding *hotkey_get(int index);
bool hotkey_is_taken(int vk, int modifiers);
const HotkeyBinding *hotkey_find_by_callback(void (*cb)(void));
const HotkeyBinding *hotkey_find_by_ud(const void *ud);
bool hotkey_process(int vk, int modifiers);
bool hotkey_execute(int vk, int modifiers);
void hotkey_drain_pending(void);
int hotkey_wake_fd(void);
void hotkey_set_suspended(bool suspended);
bool hotkey_has_mouse_binding(void);
bool hotkey_has_keyboard_binding(void);
bool hotkey_mouse_button_bound(int btn);

#endif
