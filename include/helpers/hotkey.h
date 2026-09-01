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
    HotkeyCallback hotkey_callback;
    HotkeyCondition hotkey_condition;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);
extern EditorMode g_last_mode;
bool AlwaysEnabled(void);
bool AlwaysDisabled(void);
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                   int pressed_vk, int modifiers,
                   const char* current_title);
bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count);
HotkeyBinding convert_into_hotkey_binding(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows);

static inline bool fl_hotkey_match(int pressed_vk, int pressed_mod, int bind_vk, int bind_mod) {
    return pressed_vk == bind_vk && pressed_mod == bind_mod;
}
static inline bool fl_exe_matches(const char* bind_exe, const char* activeExe) {
    if (!bind_exe || !activeExe) return false;
    if (strcmp(bind_exe, "*") == 0) return true;
#if defined(_WIN32)
    size_t nlen = strlen(bind_exe);
    size_t hlen = strlen(activeExe);
    if (nlen == 0) return true;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = activeExe[i+j];
            char b = bind_exe[j];
            if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
            if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
            if (a != b) break;
        }
        if (j == nlen) return true;
    }
    return false;
#else
    return strstr(activeExe, bind_exe) != NULL;
#endif
}

#endif
