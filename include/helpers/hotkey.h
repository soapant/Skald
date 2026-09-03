#ifndef HELPERS_HOTKEY_H
#define HELPERS_HOTKEY_H

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "core/modal.h"
#include "core/types.h"

typedef void (*HotkeyCallback)(void);
typedef bool (*HotkeyCondition)(void);

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    int whitelist;
    HotkeyCallback hotkey_callback;
    HotkeyCondition hotkey_condition;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);
bool AlwaysEnabled(void);
bool AlwaysDisabled(void);
bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count);
HotkeyBinding convert_into_hotkey_binding(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows,bool whitelist);


#endif
