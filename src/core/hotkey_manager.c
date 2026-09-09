#include "helpers.h"
#include <stdlib.h>
#include <string.h>
#include "core/platform.h"

static HotkeyBinding* g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;

typedef struct {
    int from_vk;
    int from_mod;
    int to_vk;
    int to_mod;
} KeyMapping;
static KeyMapping* g_mappings = NULL;
static int g_mapping_count = 0;
static int g_mapping_capacity = 0;

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

bool hotkey_remap_register(int from_vk, int from_mod, int to_vk, int to_mod) {
    if (g_mapping_count >= g_mapping_capacity) {
        int new_cap = g_mapping_capacity == 0 ? 8 : g_mapping_capacity * 2;
        KeyMapping* new_arr = realloc(g_mappings, new_cap * sizeof(KeyMapping));
        if (!new_arr) return false;
        g_mappings = new_arr;
        g_mapping_capacity = new_cap;
    }
    g_mappings[g_mapping_count++] = (KeyMapping){from_vk, from_mod, to_vk, to_mod};
    return true;
}

bool hotkey_remap_process(int vk_code, int modifiers, int value) {
    if (value != 1) return false;
    for (int i = 0; i < g_mapping_count; i++) {
        KeyMapping* m = &g_mappings[i];
        if (m->from_vk == vk_code && m->from_mod == modifiers) {
            Platform_EmitHotkey(m->to_vk, m->to_mod);
            return true;
        }
    }
    return false;
}

bool hotkey_process(int vk_code, int modifiers) {
    char* title = Platform_GetActiveWindowTitle();
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
//             if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
//                 KillTimer(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd), 999);
//                 DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
//                 FlintContext_Get()->window.hPopupWnd = WH_NULL;
//             }
// #else
//             if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
//                 selection_complete = true;
//             }
// #endif
//         } else {
//           if (IsPopupActive(FlintContext_Get()->window.hPopupWnd)) {
//             return false;
//           }
//         }
    
    return false;
}
