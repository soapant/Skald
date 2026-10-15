#include "config.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#include "deps/log.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_owner.h"
#include <stdint.h>
#include <stdio.h>
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
static char g_pending_owner[HOTKEY_PENDING_MAX][OWNER_PATH_MAX];
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
static void hotkey_enqueue(HotkeyCallback fn, bool is_toggle, const char* owner) {
    if (!fn) return;
    HOTKEY_Q_LOCK();
    int nxt = (g_pending_tail + 1) % HOTKEY_PENDING_MAX;
    if (nxt != g_pending_head) {
        g_pending_q[g_pending_tail] = fn;
        g_pending_toggle[g_pending_tail] = is_toggle;
        snprintf(g_pending_owner[g_pending_tail], OWNER_PATH_MAX, "%s", owner ? owner : "");
        g_pending_tail = nxt;
    }
    HOTKEY_Q_UNLOCK();
}
#define HOTKEY_HOLD_MAX 16
static bool hotkey_toggle_blocked(HotkeyBinding* b);
typedef struct {
    int vk;
    int modifiers;
    unsigned long long next_fire_ms;
    bool active;
} HotkeyHoldSlot;
static HotkeyHoldSlot g_hold_slots[HOTKEY_HOLD_MAX];

static unsigned long long hotkey_now_ms(void) {
#if defined(_WIN32)
    return (unsigned long long)GetTickCount64();
#endif
}

static int hotkey_repeat_interval_ms(void) {
    int ms = HotkeyRepeatDelayMs;
    if (ms < HOTKEY_HOLD_MIN_INTERVAL_MS) ms = HOTKEY_HOLD_MIN_INTERVAL_MS;
    return ms;
}

static void hotkey_hold_end(int vk) {
    HOTKEY_Q_LOCK();
    for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
        if (g_hold_slots[i].active && g_hold_slots[i].vk == vk) g_hold_slots[i].active = false;
    }
    HOTKEY_Q_UNLOCK();
}

void hotkey_key_up(int vk_code) {
    if (!vk_code) return;
    hotkey_hold_end(vk_code);
}

static void hotkey_hold_begin(int vk, int modifiers) {
    int interval_ms = hotkey_repeat_interval_ms();
    unsigned long long now = hotkey_now_ms();
    HOTKEY_Q_LOCK();
    for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
        if (g_hold_slots[i].active && g_hold_slots[i].vk == vk) {
            g_hold_slots[i].modifiers = modifiers;
            g_hold_slots[i].next_fire_ms = now + (unsigned long long)interval_ms;
            HOTKEY_Q_UNLOCK();
            return;
        }
    }
    for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
        if (!g_hold_slots[i].active) {
            g_hold_slots[i].active = true;
            g_hold_slots[i].vk = vk;
            g_hold_slots[i].modifiers = modifiers;
            g_hold_slots[i].next_fire_ms = now + (unsigned long long)interval_ms;
            HOTKEY_Q_UNLOCK();
            return;
        }
    }
    HOTKEY_Q_UNLOCK();
}

static void hotkey_tick_holds(void) {
    unsigned long long now = hotkey_now_ms();
    for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
        if (!g_hold_slots[i].active) continue;
        if ((long long)(now - g_hold_slots[i].next_fire_ms) < 0) continue;

        int vk = g_hold_slots[i].vk;
        int mods = g_hold_slots[i].modifiers;
        HotkeyBinding* b = NULL;
        for (int k = 0; k < g_hotkey_count; k++) {
            if (g_hotkeys[k].vk_code == vk && g_hotkeys[k].modifiers == mods && g_hotkeys[k].hold) {
                b = &g_hotkeys[k];
                break;
            }
        }
        if (!b) {
            g_hold_slots[i].active = false;
            continue;
        }
        if (hotkey_toggle_blocked(b)) continue;

        g_hold_slots[i].next_fire_ms = now + (unsigned long long)hotkey_repeat_interval_ms();
        hotkey_enqueue(b->hotkey_callback, b->is_feature_toggle, b->owner);
    }
}

void hotkey_drain_pending(void) {
    hotkey_tick_holds();
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

bool hotkey_has_mouse_binding(void) {
    for (int i = 0; i < g_hotkey_count; i++) {
        int vk = g_hotkeys[i].vk_code;
        if (vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + FL_MOUSE_BTN_MAX - 1) return true;
    }
    return false;
}

bool hotkey_mouse_button_bound(int btn) {
    if (btn <= 0 || btn > FL_MOUSE_BTN_MAX) return false;
    int vk = FL_MOUSE_VK(btn);
    for (int i = 0; i < g_hotkey_count; i++) {
        if (g_hotkeys[i].vk_code == vk && g_hotkeys[i].modifiers == 0) return true;
    }
    return false;
}

static bool hotkey_toggle_blocked(HotkeyBinding* b) {
    if (!b || !b->is_feature_toggle || AllowOtherFeatureTogglesInFeature) return false;
    WindowManager* wm = WindowManager_Get();
    return wm && wm->popup_count > 0;
}

bool hotkey_register_internal(HotkeyBinding binding) {
    char owner[OWNER_PATH_MAX];
    Owner_ResolveForFunc((void*)(uintptr_t)binding.hotkey_callback, owner, sizeof(owner));
    if (!owner[0]) Owner_ResolveForFunc((void*)(uintptr_t)binding.hotkey_condition, owner, sizeof(owner));
    snprintf(binding.owner, sizeof(binding.owner), "%s", owner);

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

void hotkey_purge_owner(const char* path_a, const char* path_b) {
    if ((!path_a || !path_a[0]) && (!path_b || !path_b[0])) return;

    int n = 0;
    for (int i = 0; i < g_hotkey_count; i++) {
        if (Owner_ModuleMatches(g_hotkeys[i].owner, path_a, path_b)) {
            log_info("hotkey vk=0x%X mods=%d purged (owner unloaded)", g_hotkeys[i].vk_code, g_hotkeys[i].modifiers);
            continue;
        }
        g_hotkeys[n++] = g_hotkeys[i];
    }
    int dropped = g_hotkey_count - n;
    g_hotkey_count = n;
    if (dropped > 0) log_info("hotkey purged %d binding%s", dropped, dropped == 1 ? "" : "s");

    HOTKEY_Q_LOCK();
    for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
        if (!g_hold_slots[i].active) continue;
        for (int k = 0; k < g_hotkey_count; k++) {
            if (g_hotkeys[k].vk_code == g_hold_slots[i].vk &&
                g_hotkeys[k].modifiers == g_hold_slots[i].modifiers &&
                Owner_ModuleMatches(g_hotkeys[k].owner, path_a, path_b)) {
                g_hold_slots[i].active = false;
                break;
            }
        }
    }
    int qn = g_pending_head;
    while (qn != g_pending_tail) {
        if (Owner_ModuleMatches(g_pending_owner[qn], path_a, path_b)) {
            log_info("hotkey pending entry dropped (owner unloaded)");
            g_pending_q[qn] = NULL;
        }
        qn = (qn + 1) % HOTKEY_PENDING_MAX;
    }
    HOTKEY_Q_UNLOCK();
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
        hotkey_enqueue(cb, b->is_feature_toggle, b->owner);
        if (b->hold) hotkey_hold_begin(vk_code, b->modifiers);
        return true;
    }



















    
    return false;
}
