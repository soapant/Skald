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
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "deps/log.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_owner.h"
#include "config_values.h"
#include "core/types/keys.h"

static HotkeyBinding *hotkeys = NULL;
static int hotkey_total = 0;
static int hotkey_capacity = 0;
static bool hotkeys_suspended = false;

static void hotkey_binding_free_blacklist(HotkeyBinding *b) {
  if (!b)
    return;
  for (int i = 0; i < b->blacklist_count; i++)
    free(b->blacklist[i]);
  free(b->blacklist);
  b->blacklist = NULL;
  b->blacklist_count = 0;
}

void hotkey_set_suspended(bool suspended) { hotkeys_suspended = suspended; }

#define HOTKEY_PENDING_MAX 64
typedef struct {
  HotkeyCallback fn;
  HotkeyCallbackUd fn_ud;
  void *ud;
  char owner[OWNER_PATH_MAX];
} PendingHotkey;
static PendingHotkey pending_hotkeys[HOTKEY_PENDING_MAX];
static int pending_head = 0;
static int pending_tail = 0;
static void *q_mutex = NULL;
static unsigned long pending_dropped = 0;

static int wake_fd = -1;
int hotkey_wake_fd(void) {
#if defined(_WIN32)
  return -1;
#else
  if (wake_fd < 0)
    wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  return wake_fd;
#endif
}

static void hotkey_wake(void) {
  if (wake_fd < 0)
    return;
#if defined(_WIN32)
  return;
#else
  unsigned long long one = 1;
  ssize_t wr = write(wake_fd, &one, sizeof(one));
  (void)wr;
#endif
}

static void hotkey_queue_init(void) {
  if (!q_mutex)
    q_mutex = Platform_MutexCreate();
  hotkey_wake_fd();
}
#define HOTKEY_Q_LOCK()                                                        \
  do {                                                                         \
    hotkey_queue_init();                                                       \
    Platform_MutexLock(q_mutex);                                               \
  } while (0)
#define HOTKEY_Q_UNLOCK() Platform_MutexUnlock(q_mutex)
static void hotkey_enqueue(HotkeyCallback fn, HotkeyCallbackUd fn_ud, void *ud,
                           const char *owner) {
  if (!fn && !fn_ud)
    return;
  HOTKEY_Q_LOCK();
  int nxt = (pending_tail + 1) % HOTKEY_PENDING_MAX;
  if (nxt != pending_head) {
    PendingHotkey *slot = &pending_hotkeys[pending_tail];
    slot->fn = fn;
    slot->fn_ud = fn_ud;
    slot->ud = ud;
    snprintf(slot->owner, sizeof(slot->owner), "%s", owner ? owner : "");
    pending_tail = nxt;
    hotkey_wake();
  } else {
    pending_dropped++;
    if (pending_dropped == 1 || pending_dropped % 100 == 0)
      log_warn("hotkey pending queue full (%d), dropped %lu event(s) so far",
               HOTKEY_PENDING_MAX, pending_dropped);
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
static HotkeyHoldSlot hold_slots[HOTKEY_HOLD_MAX];
static unsigned long hold_dropped = 0;

static unsigned long long hotkey_now_ms(void) {
  return (unsigned long long)Platform_GetWallClockMs();
}

static int hotkey_repeat_interval_ms(void) { return 50; }

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
    if (!hold_slots[i].active)
      continue;
    bool match = false;
    if (hold_slots[i].vk == vk) {
      match = true;
    } else {
      int mods = hold_slots[i].modifiers;
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
      hold_slots[i].active = false;
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
    if (hold_slots[i].active && hold_slots[i].vk == vk) {
      hold_slots[i].modifiers = modifiers;
      hold_slots[i].next_fire_ms = now + (unsigned long long)interval_ms;
      HOTKEY_Q_UNLOCK();
      return;
    }
  }
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!hold_slots[i].active) {
      hold_slots[i].active = true;
      hold_slots[i].vk = vk;
      hold_slots[i].modifiers = modifiers;
      hold_slots[i].next_fire_ms = now + (unsigned long long)interval_ms;
      HOTKEY_Q_UNLOCK();
      return;
    }
  }
  hold_dropped++;
  if (hold_dropped == 1 || hold_dropped % 100 == 0)
    log_warn("hotkey hold queue full (%d), dropped %lu hold(s) so far",
             HOTKEY_HOLD_MAX, hold_dropped);
  HOTKEY_Q_UNLOCK();
}

static bool hotkey_binding_allowed(const HotkeyBinding *b, int vk_code,
                                   int modifiers);

static void hotkey_tick_holds(void) {
  unsigned long long now = hotkey_now_ms();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!hold_slots[i].active)
      continue;
    if ((long long)(now - hold_slots[i].next_fire_ms) < 0)
      continue;

    int vk = hold_slots[i].vk;
    int mods = hold_slots[i].modifiers;
    HotkeyBinding *b = NULL;
    for (int k = 0; k < hotkey_total; k++) {
      if (hotkeys[k].vk_code == vk && hotkeys[k].modifiers == mods &&
          hotkeys[k].hold) {
        b = &hotkeys[k];
        break;
      }
    }
    if (!b) {
      hold_slots[i].active = false;
      continue;
    }
    if (!hotkey_binding_allowed(b, vk, mods)) {
      hold_slots[i].active = false;
      continue;
    }
    hold_slots[i].next_fire_ms =
        now + (unsigned long long)hotkey_repeat_interval_ms();
    hotkey_enqueue(b->hotkey_callback, b->hotkey_callback_ud, b->user_ud,
                   b->owner);
  }
}

void hotkey_drain_pending(void) {
  if (hotkeys_suspended)
    return;
  hotkey_tick_holds();
  for (;;) {
    PendingHotkey job = {0};
    bool have = false;
    HOTKEY_Q_LOCK();
    if (pending_head != pending_tail) {
      job = pending_hotkeys[pending_head];
      pending_head = (pending_head + 1) % HOTKEY_PENDING_MAX;
      have = true;
    }
    HOTKEY_Q_UNLOCK();
    if (!have)
      break;
    if (!job.fn && !job.fn_ud)
      continue;
    if (job.fn_ud)
      job.fn_ud(job.ud);
    else
      job.fn();
  }
}

bool hotkey_has_mouse_binding(void) {
  for (int i = 0; i < hotkey_total; i++) {
    int vk = hotkeys[i].vk_code;
    if (vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + MOUSE_BTN_MAX - 1)
      return true;
  }
  return false;
}

static bool hotkey_blacklist_match(const HotkeyBinding *b, const char *title) {
  if (!b->blacklist || b->blacklist_count == 0)
    return false;
  for (int j = 0; j < b->blacklist_count; j++) {
    if (b->blacklist[j][0] == '\0')
      continue;
    if (title && string_contains_ci(title, b->blacklist[j]))
      return true;
  }
  return false;
}

bool hotkey_mouse_button_bound(int btn) {
  if (btn <= 0 || btn > MOUSE_BTN_MAX)
    return false;
  int vk = MOUSE_VK(btn);
  for (int i = 0; i < hotkey_total; i++) {
    if (hotkeys[i].vk_code != vk || hotkeys[i].modifiers != 0)
      continue;
    const HotkeyBinding *b = &hotkeys[i];
    if (b->whitelist) {
      if (!b->blacklist || b->blacklist_count == 0)
        continue;
      const char *title = Platform_GetActiveWindowId();
      if (!hotkey_blacklist_match(b, title))
        continue;
    } else {
      if (b->blacklist && b->blacklist_count > 0) {
        const char *title = Platform_GetActiveWindowId();
        if (hotkey_blacklist_match(b, title))
          continue;
      }
    }
    return true;
  }
  return false;
}

int hotkey_count(void) { return hotkey_total; }

const HotkeyBinding *hotkey_get(int index) {
  if (index < 0 || index >= hotkey_total)
    return NULL;
  return &hotkeys[index];
}

const HotkeyBinding *hotkey_find_by_ud(const void *ud) {
  if (!ud)
    return NULL;
  for (int i = 0; i < hotkey_total; i++)
    if (hotkeys[i].user_ud == ud)
      return &hotkeys[i];
  return NULL;
}

const HotkeyBinding *hotkey_find_by_callback(HotkeyCallback cb) {
  if (!cb)
    return NULL;
  for (int i = 0; i < hotkey_total; i++)
    if (hotkeys[i].hotkey_callback == cb)
      return &hotkeys[i];
  return NULL;
}

bool hotkey_is_taken(int vk_code, int modifiers) {
  for (int i = 0; i < hotkey_total; i++)
    if (hotkeys[i].vk_code == vk_code && hotkeys[i].modifiers == modifiers)
      return true;
  return false;
}

bool hotkey_register_internal(HotkeyBinding binding) {
  char owner[OWNER_PATH_MAX];
  owner_resolve_for_func((void *)(uintptr_t)binding.hotkey_callback, owner,
                         sizeof(owner));
  if (!owner[0])
    owner_resolve_for_func((void *)(uintptr_t)binding.hotkey_callback_ud, owner,
                           sizeof(owner));
  if (!owner[0])
    owner_resolve_for_func((void *)(uintptr_t)binding.hotkey_condition, owner,
                           sizeof(owner));
  snprintf(binding.owner, sizeof(binding.owner), "%s", owner);

  for (int i = 0; i < hotkey_total; i++) {
    if (hotkeys[i].vk_code == binding.vk_code &&
        hotkeys[i].modifiers == binding.modifiers) {
      hotkey_binding_free_blacklist(&hotkeys[i]);
      hotkeys[i] = binding;
      return true;
    }
  }
  if (hotkey_total >= hotkey_capacity) {
    int new_cap = hotkey_capacity == 0 ? 16 : hotkey_capacity * 2;
    HotkeyBinding *new_arr = realloc(hotkeys, new_cap * sizeof(HotkeyBinding));
    if (!new_arr)
      return false;
    hotkeys = new_arr;
    hotkey_capacity = new_cap;
  }
  hotkeys[hotkey_total++] = binding;
  return true;
}

bool hotkey_unregister_internal(int vk_code, int modifiers) {
  int n = 0;
  bool found = false;
  for (int i = 0; i < hotkey_total; i++) {
    if (hotkeys[i].vk_code == vk_code && hotkeys[i].modifiers == modifiers) {
      hotkey_binding_free_blacklist(&hotkeys[i]);
      found = true;
      continue;
    }
    hotkeys[n++] = hotkeys[i];
  }
  hotkey_total = n;
  return found;
}

void hotkey_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;

  int n = 0;
  for (int i = 0; i < hotkey_total; i++) {
    if (owner_module_matches(hotkeys[i].owner, path_a, path_b)) {
      log_info("hotkey vk=0x%X mods=%d purged (owner unloaded)",
               hotkeys[i].vk_code, hotkeys[i].modifiers);
      hotkey_binding_free_blacklist(&hotkeys[i]);
      continue;
    }
    hotkeys[n++] = hotkeys[i];
  }
  int dropped = hotkey_total - n;
  hotkey_total = n;
  if (dropped > 0)
    log_info("hotkey purged %d binding%s", dropped, dropped == 1 ? "" : "s");

  HOTKEY_Q_LOCK();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!hold_slots[i].active)
      continue;
    for (int k = 0; k < hotkey_total; k++) {
      if (hotkeys[k].vk_code == hold_slots[i].vk &&
          hotkeys[k].modifiers == hold_slots[i].modifiers &&
          owner_module_matches(hotkeys[k].owner, path_a, path_b)) {
        hold_slots[i].active = false;
        break;
      }
    }
  }
  int qn = pending_head;
  while (qn != pending_tail) {
    if (owner_module_matches(pending_hotkeys[qn].owner, path_a, path_b)) {
      log_info("hotkey pending entry dropped (owner unloaded)");
      pending_hotkeys[qn].fn = NULL;
      pending_hotkeys[qn].fn_ud = NULL;
      pending_hotkeys[qn].ud = NULL;
      pending_hotkeys[qn].owner[0] = '\0';
    }
    qn = (qn + 1) % HOTKEY_PENDING_MAX;
  }
  HOTKEY_Q_UNLOCK();
}

static int hotkey_strip_self_mod(int vk_code, int mods) {
  int eff = mods;
  if (vk_code == VK_LSHIFT || vk_code == VK_RSHIFT || vk_code == VK_SHIFT)
    eff &= ~HOTKEY_MOD_SHIFT;
  else if (vk_code == VK_LCONTROL || vk_code == VK_RCONTROL ||
           vk_code == VK_CONTROL)
    eff &= ~HOTKEY_MOD_CTRL;
  else if (vk_code == VK_LMENU || vk_code == VK_RMENU || vk_code == VK_MENU)
    eff &= ~HOTKEY_MOD_ALT;
  else if (vk_code == VK_LWIN || vk_code == VK_RWIN)
    eff &= ~HOTKEY_MOD_SUPER;
  return eff;
}

static bool hotkey_binding_allowed(const HotkeyBinding *b, int vk_code,
                                   int modifiers) {
  int eff_modifiers = hotkey_strip_self_mod(vk_code, modifiers);
  int b_eff = hotkey_strip_self_mod(b->vk_code, b->modifiers);
  if (b_eff != eff_modifiers)
    return false;
  if (b_eff != 0 && !remap_check_strict_order(vk_code, b_eff))
    return false;

  if (b->hotkey_condition) {
    feature_loader_lock();
    bool cond_pass = b->hotkey_condition();
    feature_loader_unlock();
    if (!cond_pass)
      return false;
  }

  const char *title = NULL;
  bool title_fetched = false;

  if (!b->whitelist) {
    if (b->blacklist && b->blacklist_count > 0) {
      if (!title_fetched) {
        title = Platform_GetActiveWindowId();
        title_fetched = true;
      }
      if (hotkey_blacklist_match(b, title))
        return false;
    }
    return true;
  }

  if (!b->blacklist || b->blacklist_count == 0)
    return false;
  if (!title_fetched) {
    title = Platform_GetActiveWindowId();
    title_fetched = true;
  }
  return hotkey_blacklist_match(b, title);
}

bool hotkey_process(int vk_code, int modifiers) {
  if (hotkeys_suspended)
    return false;
  int eff_modifiers = hotkey_strip_self_mod(vk_code, modifiers);
  for (int i = 0; i < hotkey_total; i++) {
    HotkeyBinding *b = &hotkeys[i];

    if (b->vk_code != vk_code) {
      continue;
    }
    if (!hotkey_binding_allowed(b, vk_code, eff_modifiers)) {
      continue;
    }

    HotkeyCallback cb = b->hotkey_callback;
    bool swallow = true;
    if (b->should_swallow) {
      if (feature_loader_try_lock()) {
        swallow = b->should_swallow();
        feature_loader_unlock();
      } else {
        swallow = false;
      }
    }
    hotkey_enqueue(cb, b->hotkey_callback_ud, b->user_ud, b->owner);
    char buf[2556];
    char hotkey_buf[2422];
    hotkey_string_format((HotkeyKey){.vk_code = vk_code, .mods = eff_modifiers},
                         hotkey_buf, sizeof(hotkey_buf));
    if (MessageAfterEachHotkey) {
      snprintf(buf, sizeof(buf), "Ran Hotkey %s", hotkey_buf);
      display_message_slot(buf, BOTTOM_CENTER, 400, 400, 20, TimeOutForMessages,
                           COLOR_TEXT, NULL, NULL);
    }
    log_info("hotkey_process Hotkey %s Matches", hotkey_buf);
    if (b->hold)
      hotkey_hold_begin(vk_code, b->modifiers);
    return swallow;
  }
  return false;
}
