#ifndef HELPERS_HOTKEY_H
#define HELPERS_HOTKEY_H

#include <stdbool.h>
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
    bool is_feature_toggle;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);
bool AlwaysEnabled(void);
bool AlwaysDisabled(void);
HotkeyBinding convert_into_hotkey_binding(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows,bool whitelist);
void hotkey_mark_feature_toggles(HotkeyCallback cb);


#endif