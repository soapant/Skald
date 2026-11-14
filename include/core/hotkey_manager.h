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

bool Hotkey_RegisterInternal(HotkeyBinding binding);
bool Hotkey_UnregisterInternal(int vk_code, int modifiers);
int Hotkey_Count(void);
const HotkeyBinding *Hotkey_Get(int index);
bool Hotkey_IsTaken(int vk_code, int modifiers);
const HotkeyBinding *Hotkey_FindByCallback(HotkeyCallback cb);
const HotkeyBinding *Hotkey_FindByUd(const void *ud);
bool Hotkey_Process(int vk_code, int modifiers);
void Hotkey_DrainPending(void);
int Hotkey_WakeFd(void);
void Hotkey_SetSuspended(bool suspended);
unsigned long Hotkey_PendingDropped(void);
bool Hotkey_HasMouseBinding(void);
bool Hotkey_MouseButtonBound(int btn);

#endif
