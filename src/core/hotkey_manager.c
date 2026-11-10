#include "helpers/helpers_text.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#if !defined(_WIN32)
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include "core/types/window.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#include "core/hotkey_string.h"
#include "core/remap_manager.h"
#include "deps/log.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_owner.h"
#include "config_values.h"
#include "core/types/keys.h"

static HotkeyBinding *g_hotkeys = NULL;
static int g_hotkey_count = 0;
static int g_hotkey_capacity = 0;
static bool g_hotkeys_suspended = false;

void hotkey_set_suspended(bool suspended) { g_hotkeys_suspended = suspended; }

#define HOTKEY_PENDING_MAX 64
static HotkeyCallback g_pending_q[HOTKEY_PENDING_MAX];
static HotkeyCallbackUd g_pending_cb_ud[HOTKEY_PENDING_MAX];
static void *g_pending_ud[HOTKEY_PENDING_MAX];
static int g_pending_vk[HOTKEY_PENDING_MAX];
static int g_pending_mods[HOTKEY_PENDING_MAX];
static char g_pending_owner[HOTKEY_PENDING_MAX][OWNER_PATH_MAX];
static int g_pending_head = 0;
static int g_pending_tail = 0;
static void *g_q_mutex = NULL;

static int g_wake_fd = -1;
int hotkey_wake_fd(void) {
  if (g_wake_fd < 0) {
#if defined(_WIN32)
    g_wake_fd = 0;
#else
    g_wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
#endif
  }
  return g_wake_fd;
}

static void hotkey_wake(void) {
  if (g_wake_fd < 0)
    return;
#if defined(_WIN32)
  return;
#else
  unsigned long long one = 1;
  ssize_t wr = write(g_wake_fd, &one, sizeof(one));
  (void)wr;
#endif
}

static void hotkey_queue_init(void) {
  if (!g_q_mutex)
    g_q_mutex = Platform_MutexCreate();
  hotkey_wake_fd();
}
#define HOTKEY_Q_LOCK()                                                        \
  do {                                                                         \
    hotkey_queue_init();                                                       \
    Platform_MutexLock(g_q_mutex);                                             \
  } while (0)
#define HOTKEY_Q_UNLOCK() Platform_MutexUnlock(g_q_mutex)
static void hotkey_enqueue(HotkeyCallback fn, HotkeyCallbackUd fn_ud,
                           void *ud, const char *owner, int vk, int mods) {
  if (!fn && !fn_ud)
    return;
  HOTKEY_Q_LOCK();
  int nxt = (g_pending_tail + 1) % HOTKEY_PENDING_MAX;
  if (nxt != g_pending_head) {
    g_pending_q[g_pending_tail] = fn;
    g_pending_cb_ud[g_pending_tail] = fn_ud;
    g_pending_ud[g_pending_tail] = ud;
    g_pending_vk[g_pending_tail] = vk;
    g_pending_mods[g_pending_tail] = mods;
    snprintf(g_pending_owner[g_pending_tail], OWNER_PATH_MAX, "%s",
             owner ? owner : "");
    g_pending_tail = nxt;
    hotkey_wake();
  }
  HOTKEY_Q_UNLOCK();
}
#define HOTKEY_HOLD_MAX 16
typedef struct {
  int vk;
  int modifiers;
  unsigned long long next_fire_ms;
  bool active;
} HotkeyHoldSlot;
static HotkeyHoldSlot g_hold_slots[HOTKEY_HOLD_MAX];

static unsigned long long hotkey_now_ms(void) {
  return (unsigned long long)Platform_GetWallClockMs();
}

static int hotkey_repeat_interval_ms(void) {
  int ms = 50;
  if (ms < HOTKEY_HOLD_MIN_INTERVAL_MS)
    ms = HOTKEY_HOLD_MIN_INTERVAL_MS;
  return ms;
}

static int hotkey_hold_delay_ms(void) {
  int ms = HotkeyRepeatDelayMs;
  if (ms < 0)
    ms = 0;
  return ms;
}

static void hotkey_hold_end(int vk) {
  if (!vk)
    return;
  HOTKEY_Q_LOCK();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!g_hold_slots[i].active)
      continue;
    bool match = false;
    if (g_hold_slots[i].vk == vk) {
      match = true;
    } else {
      int mods = g_hold_slots[i].modifiers;
      if ((mods & HOTKEY_MOD_CTRL) &&
          (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL))
        match = true;
      else if ((mods & HOTKEY_MOD_ALT) &&
               (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU))
        match = true;
      else if ((mods & HOTKEY_MOD_SHIFT) &&
               (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT))
        match = true;
      else if ((mods & HOTKEY_MOD_SUPER) && (vk == VK_LWIN || vk == VK_RWIN))
        match = true;
    }
    if (match) {
      g_hold_slots[i].active = false;
    }
  }
  HOTKEY_Q_UNLOCK();
}

void hotkey_key_up(int vk_code) {
  if (!vk_code)
    return;
  hotkey_hold_end(vk_code);
}

static void hotkey_hold_begin(int vk, int modifiers) {
  int interval_ms = hotkey_hold_delay_ms();
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
    if (!g_hold_slots[i].active)
      continue;
    if ((long long)(now - g_hold_slots[i].next_fire_ms) < 0)
      continue;

    int vk = g_hold_slots[i].vk;
    int mods = g_hold_slots[i].modifiers;
    HotkeyBinding *b = NULL;
    for (int k = 0; k < g_hotkey_count; k++) {
      if (g_hotkeys[k].vk_code == vk && g_hotkeys[k].modifiers == mods &&
          g_hotkeys[k].hold) {
        b = &g_hotkeys[k];
        break;
      }
    }
    if (!b) {
      g_hold_slots[i].active = false;
      continue;
    }
    g_hold_slots[i].next_fire_ms =
        now + (unsigned long long)hotkey_repeat_interval_ms();
    hotkey_enqueue(b->hotkey_callback, b->hotkey_callback_ud,
                   b->user_ud, b->owner, vk, mods);
  }
}

void hotkey_drain_pending(void) {
  if (g_hotkeys_suspended)
    return;
  hotkey_tick_holds();
  for (;;) {
    HotkeyCallback fn = NULL;
    HotkeyCallbackUd fn_ud = NULL;
    void *ud = NULL;
    HOTKEY_Q_LOCK();
    if (g_pending_head != g_pending_tail) {
      fn = g_pending_q[g_pending_head];
      fn_ud = g_pending_cb_ud[g_pending_head];
      ud = g_pending_ud[g_pending_head];
      g_pending_head = (g_pending_head + 1) % HOTKEY_PENDING_MAX;
    }
    HOTKEY_Q_UNLOCK();
    if (!fn && !fn_ud)
      break;
    if (fn_ud)
      fn_ud(ud);
    else if (ud)
      ((HotkeyCallbackUd)fn)(ud); /* legacy raw sk_register casts */
    else
      fn();
  }
}

bool hotkey_has_mouse_binding(void) {
  for (int i = 0; i < g_hotkey_count; i++) {
    int vk = g_hotkeys[i].vk_code;
    if (vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + MOUSE_BTN_MAX - 1)
      return true;
  }
  return false;
}

bool hotkey_mouse_button_bound(int btn) {
  if (btn <= 0 || btn > MOUSE_BTN_MAX)
    return false;
  int vk = MOUSE_VK(btn);
  for (int i = 0; i < g_hotkey_count; i++) {
    if (g_hotkeys[i].vk_code != vk || g_hotkeys[i].modifiers != 0)
      continue;
    HotkeyBinding *b = &g_hotkeys[i];
    if (b->whitelist) {
      if (!b->blacklist || b->blacklist_count == 0)
        continue;
      const char *title = Platform_GetActiveWindowAppId();
      bool ok = false;
      for (int j = 0; j < b->blacklist_count; j++) {
        if (b->blacklist[j][0] == '\0')
          continue;
        if (title &&
            String_ContainsCI(title, b->blacklist[j])) {
          ok = true;
          break;
        }
      }
      if (!ok)
        continue;
    } else {
      if (b->blacklist && b->blacklist_count > 0) {
        const char *title = Platform_GetActiveWindowAppId();
        bool blocked = false;
        for (int j = 0; j < b->blacklist_count; j++) {
          if (b->blacklist[j][0] == '\0')
            continue;
          if (title &&
              String_ContainsCI(title, b->blacklist[j])) {
            blocked = true;
            break;
          }
        }
        if (blocked)
          continue;
      }
    }
    return true;
  }
  return false;
}

int hotkey_count(void) { return g_hotkey_count; }

const HotkeyBinding *hotkey_get(int index) {
  if (index < 0 || index >= g_hotkey_count)
    return NULL;
  return &g_hotkeys[index];
}

const HotkeyBinding *hotkey_find_by_ud(void *ud) {
  if (!ud)
    return NULL;
  for (int i = 0; i < g_hotkey_count; i++)
    if (g_hotkeys[i].user_ud == ud)
      return &g_hotkeys[i];
  return NULL;
}

const HotkeyBinding *hotkey_find_by_callback(HotkeyCallback cb) {
  if (!cb)
    return NULL;
  for (int i = 0; i < g_hotkey_count; i++)
    if (g_hotkeys[i].hotkey_callback == cb)
      return &g_hotkeys[i];
  return NULL;
}

bool hotkey_is_taken(int vk_code, int modifiers) {
  for (int i = 0; i < g_hotkey_count; i++)
    if (g_hotkeys[i].vk_code == vk_code && g_hotkeys[i].modifiers == modifiers)
      return true;
  return false;
}

bool hotkey_register_internal(HotkeyBinding binding) {
  char owner[OWNER_PATH_MAX];
  Owner_ResolveForFunc((void *)(uintptr_t)binding.hotkey_callback, owner,
                       sizeof(owner));
  if (!owner[0])
    Owner_ResolveForFunc((void *)(uintptr_t)binding.hotkey_callback_ud, owner,
                         sizeof(owner));
  if (!owner[0])
    Owner_ResolveForFunc((void *)(uintptr_t)binding.hotkey_condition, owner,
                         sizeof(owner));
  snprintf(binding.owner, sizeof(binding.owner), "%s", owner);

  for (int i = 0; i < g_hotkey_count; i++) {
    if (g_hotkeys[i].vk_code == binding.vk_code &&
        g_hotkeys[i].modifiers == binding.modifiers) {
      g_hotkeys[i] = binding;
      return true;
    }
  }
  if (g_hotkey_count >= g_hotkey_capacity) {
    int new_cap = g_hotkey_capacity == 0 ? 16 : g_hotkey_capacity * 2;
    HotkeyBinding *new_arr =
        realloc(g_hotkeys, new_cap * sizeof(HotkeyBinding));
    if (!new_arr)
      return false;
    g_hotkeys = new_arr;
    g_hotkey_capacity = new_cap;
  }
  g_hotkeys[g_hotkey_count++] = binding;
  return true;
}

bool hotkey_unregister_internal(int vk_code, int modifiers) {
  int n = 0;
  bool found = false;
  for (int i = 0; i < g_hotkey_count; i++) {
    if (g_hotkeys[i].vk_code == vk_code &&
        g_hotkeys[i].modifiers == modifiers) {
      found = true;
      continue;
    }
    g_hotkeys[n++] = g_hotkeys[i];
  }
  g_hotkey_count = n;
  return found;
}

void hotkey_purge_owner(const char *path_a, const char *path_b) {
  if ((!path_a || !path_a[0]) && (!path_b || !path_b[0]))
    return;

  int n = 0;
  for (int i = 0; i < g_hotkey_count; i++) {
    if (Owner_ModuleMatches(g_hotkeys[i].owner, path_a, path_b)) {
      log_info("hotkey vk=0x%X mods=%d purged (owner unloaded)",
               g_hotkeys[i].vk_code, g_hotkeys[i].modifiers);
      continue;
    }
    g_hotkeys[n++] = g_hotkeys[i];
  }
  int dropped = g_hotkey_count - n;
  g_hotkey_count = n;
  if (dropped > 0)
    log_info("hotkey purged %d binding%s", dropped, dropped == 1 ? "" : "s");

  HOTKEY_Q_LOCK();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!g_hold_slots[i].active)
      continue;
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
      g_pending_cb_ud[qn] = NULL;
      g_pending_ud[qn] = NULL;
    }
    qn = (qn + 1) % HOTKEY_PENDING_MAX;
  }
  HOTKEY_Q_UNLOCK();
}

bool hotkey_process(int vk_code, int modifiers) {
  if (g_hotkeys_suspended)
    return false;
  int eff_modifiers = modifiers;
  if (vk_code == VK_LSHIFT || vk_code == VK_RSHIFT || vk_code == VK_SHIFT)
    eff_modifiers &= ~HOTKEY_MOD_SHIFT;
  else if (vk_code == VK_LCONTROL || vk_code == VK_RCONTROL ||
           vk_code == VK_CONTROL)
    eff_modifiers &= ~HOTKEY_MOD_CTRL;
  else if (vk_code == VK_LMENU || vk_code == VK_RMENU || vk_code == VK_MENU)
    eff_modifiers &= ~HOTKEY_MOD_ALT;
  else if (vk_code == VK_LWIN || vk_code == VK_RWIN)
    eff_modifiers &= ~HOTKEY_MOD_SUPER;
  for (int i = 0; i < g_hotkey_count; i++) {
    HotkeyBinding *b = &g_hotkeys[i];

    if (b->vk_code != vk_code) {
      continue;
    }
    int b_eff = b->modifiers;
    if (b->vk_code == VK_LSHIFT || b->vk_code == VK_RSHIFT ||
        b->vk_code == VK_SHIFT)
      b_eff &= ~HOTKEY_MOD_SHIFT;
    else if (b->vk_code == VK_LCONTROL || b->vk_code == VK_RCONTROL ||
             b->vk_code == VK_CONTROL)
      b_eff &= ~HOTKEY_MOD_CTRL;
    else if (b->vk_code == VK_LMENU || b->vk_code == VK_RMENU ||
             b->vk_code == VK_MENU)
      b_eff &= ~HOTKEY_MOD_ALT;
    else if (b->vk_code == VK_LWIN || b->vk_code == VK_RWIN)
      b_eff &= ~HOTKEY_MOD_SUPER;
    if (b_eff != eff_modifiers) {
      continue;
    }
    if (b_eff != 0 && !remap_check_strict_order(vk_code, b_eff)) {
      continue;
    }

    if (b->hotkey_condition) {
      FeatureLoader_Lock();
      bool cond_pass = b->hotkey_condition();
      FeatureLoader_Unlock();
      if (!cond_pass) {
        continue;
      }
    }

    const char *title = NULL;
    bool title_fetched = false;

    if (!b->whitelist) {
      if (b->blacklist && b->blacklist_count > 0) {
        bool blacklisted = false;
        for (int j = 0; j < b->blacklist_count; j++) {
          if (b->blacklist[j][0] == '\0') {
            continue;
          }
          if (!title_fetched) {
            title = Platform_GetActiveWindowAppId();
            title_fetched = true;
          }
          bool match =
              String_ContainsCI(title, b->blacklist[j]);

          if (match) {
            blacklisted = true;
            break;
          }
        }
        if (blacklisted) {
          continue;
        }
      }
    } else {
      if (!b->blacklist || b->blacklist_count == 0) {
        continue;
      }

      bool whitelisted = false;
      for (int j = 0; j < b->blacklist_count; j++) {
        if (b->blacklist[j][0] == '\0') {
          continue;
        }
        if (!title_fetched) {
          title = Platform_GetActiveWindowAppId();
          title_fetched = true;
        }
        bool match =
            String_ContainsCI(title, b->blacklist[j]);

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
    bool swallow = true;
    if (b->should_swallow) {
      if (FeatureLoader_TryLock()) {
        swallow = b->should_swallow();
        FeatureLoader_Unlock();
      } else {
        swallow = false;
      }
    }
    hotkey_enqueue(cb, b->hotkey_callback_ud, b->user_ud, b->owner,
                   vk_code, eff_modifiers);
    char buf[2556];
    char hotkey_buf[2422];
    HotkeyString_Format((HotkeyKey){.vk_code = vk_code, .mods = eff_modifiers},
                        hotkey_buf, sizeof(hotkey_buf));
    if (MessageAfterEachHotkey) {
      snprintf(buf, sizeof(buf), "Ran Hotkey %s", hotkey_buf);
      DisplayMessageSlot(buf, BOTTOM_CENTER, 400, 400, 20, TimeOutForMessages,
                         COLOR_TEXT, NULL, NULL);
    }
    log_info("hotkey_process Hotkey %s Matches", hotkey_buf);
    if (b->hold)
      hotkey_hold_begin(vk_code, b->modifiers);
    return swallow;
  }
  return false;
}
