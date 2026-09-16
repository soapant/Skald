#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_file.h"
#include "helpers/helpers_ui.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#include "core/platform.h"

static HotkeyBinding* g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;

#if !defined(_WIN32)
static HotkeyCallback g_pending_q[64];
static int g_pending_head = 0;
static int g_pending_tail = 0;
static pthread_mutex_t g_q_mtx = PTHREAD_MUTEX_INITIALIZER;
static void hotkey_enqueue(HotkeyCallback fn) {
    pthread_mutex_lock(&g_q_mtx);
    int nxt = (g_pending_tail + 1) % 64;
    if (nxt != g_pending_head) { g_pending_q[g_pending_tail] = fn; g_pending_tail = nxt; }
    pthread_mutex_unlock(&g_q_mtx);
}
void hotkey_drain_pending(void) {
    while (1) {
        HotkeyCallback fn = NULL;
        pthread_mutex_lock(&g_q_mtx);
        if (g_pending_head != g_pending_tail) { fn = g_pending_q[g_pending_head]; g_pending_head = (g_pending_head + 1) % 64; }
        pthread_mutex_unlock(&g_q_mtx);
        if (!fn) break;
        fn();
    }
}
#else
void hotkey_drain_pending(void) {}
#endif

bool hotkey_register_internal(HotkeyBinding binding) {
    for (int i=0;i<g_hotkey_count;i++) {
        if (g_hotkeys[i].vk_code == binding.vk_code && g_hotkeys[i].modifiers == binding.modifiers) {
            g_hotkeys[i] = binding;
            return true;
        }
    }
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

bool hotkey_process(int vk_code, int modifiers) {
    char* title = Platform_GetActiveWindowTitle();
    int eff_modifiers = modifiers;
    if (vk_code == VK_LSHIFT || vk_code == VK_RSHIFT || vk_code == VK_SHIFT) eff_modifiers &= ~HOTKEY_MOD_SHIFT;
    else if (vk_code == VK_LCONTROL || vk_code == VK_RCONTROL || vk_code == VK_CONTROL) eff_modifiers &= ~HOTKEY_MOD_CTRL;
    else if (vk_code == VK_LMENU || vk_code == VK_RMENU || vk_code == VK_MENU) eff_modifiers &= ~HOTKEY_MOD_ALT;
    else if (vk_code == VK_LWIN || vk_code == VK_RWIN) eff_modifiers &= ~HOTKEY_MOD_SUPER;
    for (int i = 0; i < g_hotkey_count; i++) {
        HotkeyBinding* b = &g_hotkeys[i];
        
        if (b->vk_code != vk_code) {
            continue;
        }
        int b_eff = b->modifiers;
        if (b->vk_code == VK_LSHIFT || b->vk_code == VK_RSHIFT || b->vk_code == VK_SHIFT) b_eff &= ~HOTKEY_MOD_SHIFT;
        else if (b->vk_code == VK_LCONTROL || b->vk_code == VK_RCONTROL || b->vk_code == VK_CONTROL) b_eff &= ~HOTKEY_MOD_CTRL;
        else if (b->vk_code == VK_LMENU || b->vk_code == VK_RMENU || b->vk_code == VK_MENU) b_eff &= ~HOTKEY_MOD_ALT;
        else if (b->vk_code == VK_LWIN || b->vk_code == VK_RWIN) b_eff &= ~HOTKEY_MOD_SUPER;
        if (b_eff != eff_modifiers) {
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

        HotkeyCallback cb = b->hotkey_callback;
#if !defined(_WIN32)
        hotkey_enqueue(cb);
#else
        cb();
#endif
        return true; 
    }



















    
    return false;
}
