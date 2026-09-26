#include "config.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#include "deps/log.h"
#include "helpers/helpers_hotkey.h"
#include <stdlib.h>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#include "platform/platform.h"

static HotkeyBinding* g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;

#define HOTKEY_PENDING_MAX 64
static HotkeyCallback g_pending_q[HOTKEY_PENDING_MAX];
static bool g_pending_toggle[HOTKEY_PENDING_MAX];
static int g_pending_head = 0;
static int g_pending_tail = 0;
#if !defined(_WIN32)
static pthread_mutex_t g_q_mtx = PTHREAD_MUTEX_INITIALIZER;
#define HOTKEY_Q_LOCK() pthread_mutex_lock(&g_q_mtx)
#define HOTKEY_Q_UNLOCK() pthread_mutex_unlock(&g_q_mtx)
#else
static CRITICAL_SECTION g_q_cs;
static LONG g_q_cs_ready = 0;
static void hotkey_queue_init(void) {
    if (InterlockedCompareExchange(&g_q_cs_ready, 1, 0) == 0) InitializeCriticalSection(&g_q_cs);
}
#define HOTKEY_Q_LOCK() do { hotkey_queue_init(); EnterCriticalSection(&g_q_cs); } while (0)
#define HOTKEY_Q_UNLOCK() do { hotkey_queue_init(); LeaveCriticalSection(&g_q_cs); } while (0)
#endif
static void hotkey_enqueue(HotkeyCallback fn, bool is_toggle) {
    if (!fn) return;
    HOTKEY_Q_LOCK();
    int nxt = (g_pending_tail + 1) % HOTKEY_PENDING_MAX;
    if (nxt != g_pending_head) {
        g_pending_q[g_pending_tail] = fn;
        g_pending_toggle[g_pending_tail] = is_toggle;
        g_pending_tail = nxt;
    }
    HOTKEY_Q_UNLOCK();
}
void hotkey_drain_pending(void) {
    for (;;) {
        HotkeyCallback fn = NULL;
        bool is_toggle = false;
        HOTKEY_Q_LOCK();
        if (g_pending_head != g_pending_tail) {
            fn = g_pending_q[g_pending_head];
            is_toggle = g_pending_toggle[g_pending_head];
            g_pending_head = (g_pending_head + 1) % HOTKEY_PENDING_MAX;
        }
        HOTKEY_Q_UNLOCK();
        if (!fn) break;
        if (is_toggle && !AllowOtherFeatureTogglesInFeature) {
            WindowManager* dwm = WindowManager_Get();
            if (dwm && dwm->popup_count > 0) continue;
        }
        log_info("hotkey drain run");
        FeatureLoader_Lock();
        fn();
        FeatureLoader_Unlock();
    }
}

void hotkey_mark_feature_toggles(HotkeyCallback cb) {
    if (!cb) return;
    for (int i = 0; i < g_hotkey_count; i++) {
        if (g_hotkeys[i].hotkey_callback == cb) g_hotkeys[i].is_feature_toggle = true;
    }
}

static bool hotkey_toggle_blocked(HotkeyBinding* b) {
    if (!b || !b->is_feature_toggle || AllowOtherFeatureTogglesInFeature) return false;
    WindowManager* wm = WindowManager_Get();
    return wm && wm->popup_count > 0;
}

bool hotkey_register_internal(HotkeyBinding binding) {
    for (int i=0;i<g_hotkey_count;i++) {
        if (g_hotkeys[i].vk_code == binding.vk_code && g_hotkeys[i].modifiers == binding.modifiers) {
            bool keep_toggle = g_hotkeys[i].is_feature_toggle || binding.is_feature_toggle;
            g_hotkeys[i] = binding;
            g_hotkeys[i].is_feature_toggle = keep_toggle;
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
            if (hotkey_toggle_blocked(b)) return true;
            FeatureLoader_Lock();
            bool cond_pass = b->hotkey_condition();
            FeatureLoader_Unlock();
            if (!cond_pass) {
                continue;
            }
        }

        const char* title = NULL;
        bool title_fetched = false;

        if (!b->whitelist) {
            if (b->blacklist && b->blacklist_count > 0) {
                bool blacklisted = false;
                for (int j = 0; j < b->blacklist_count; j++) {
                    if (b->blacklist[j][0] == '\0') {
                        continue;
                    }
                    if (!title_fetched) {
                        title = Platform_GetActiveWindowTitle();
                        title_fetched = true;
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
                if (!title_fetched) {
                    title = Platform_GetActiveWindowTitle();
                    title_fetched = true;
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
        bool blocked = hotkey_toggle_blocked(b);
        if (blocked) return true;
        log_info("hotkey match vk=0x%X mods=%d enqueue", vk_code, eff_modifiers);
        hotkey_enqueue(cb, b->is_feature_toggle);
        return true;
    }



















    
    return false;
}
