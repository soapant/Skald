#include "helpers.h"
#include <stdlib.h>
#include <string.h>

static HotkeyBinding* g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;

bool hotkey_register_internal(HotkeyBinding binding) {
    if (g_hotkey_count >= g_hotkey_capacity) {
        int new_cap = g_hotkey_capacity == 0 ? 16 : g_hotkey_capacity * 2;
        HotkeyBinding* new_arr = realloc(g_hotkeys, new_cap * sizeof(HotkeyBinding));
        if (!new_arr) return false;
        g_hotkeys = new_arr;
        g_hotkey_capacity = new_cap;
    }
    g_hotkeys[g_hotkey_count++] = binding;
    return true;
}

bool hotkey_process(int vk_code, int modifiers, const char* title) {
    for (int i = 0; i < g_hotkey_count; i++) {
        HotkeyBinding* b = &g_hotkeys[i];
        
        if (b->vk_code != vk_code) {
            continue;
        }
        if (b->modifiers != modifiers) {
            continue;
        }

        if (b->hotkey_condition) {
            bool cond_pass = b->hotkey_condition();
            if (!cond_pass) {
                continue;
            }
        }

        if (!b->whitelist) {
            if (b->blacklist && b->blacklist_count > 0) {
                bool blacklisted = false;
                for (int j = 0; j < b->blacklist_count; j++) {
                    if (b->blacklist[j][0] == '\0') {
                        continue;
                    }
                    
                    bool match = Platform_StringContainsCaseInsensitive(title, b->blacklist[j]);
                    
                    if (match) {
                        blacklisted = true;
                        break;
                    }
                }
                if (blacklisted) {
                    continue; 
                }
            } else {
            }
        } 
        else {
            if (!b->blacklist || b->blacklist_count == 0) {
                continue;
            }

            bool whitelisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
                if (b->blacklist[j][0] == '\0') {
                    continue;
                }
                
                bool match = Platform_StringContainsCaseInsensitive(title, b->blacklist[j]);
                
                if (match) {
                    whitelisted = true;
                    break;
                } 
            }
            if (!whitelisted) {
                continue;
            }
        }

        b->hotkey_callback();
        return true; 
    }

    //         temprary need to implement
//         if (AllowOtherFeatureTogglesInFeature) {
// #if defined(_WIN32)
//             if (IsPopupActive(hPopupWnd)) {
//                 KillTimer(WH_TO_HWND(hPopupWnd), 999);
//                 DestroyWindow(WH_TO_HWND(hPopupWnd));
//                 hPopupWnd = WH_NULL;
//             }
// #else
//             if (IsPopupActive(hPopupWnd)) {
//                 selection_complete = true;
//             }
// #endif
//         } else {
//           if (IsPopupActive(hPopupWnd)) {
//             return false;
//           }
//         }
    
    return false;
}
