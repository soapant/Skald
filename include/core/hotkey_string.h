#ifndef CORE_HOTKEY_STRING_H
#define CORE_HOTKEY_STRING_H

#include <stdbool.h>
#include "core/types/keys.h"

#define HOTKEY_STRING_MAX_LEN 64

bool HotkeyString_Parse(const char* str, HotkeyKey* out);
bool HotkeyString_Format(HotkeyKey key, char* out, int out_size);

#endif
