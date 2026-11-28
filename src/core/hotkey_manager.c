#include "helpers/helpers_text.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#if defined(__linux__)
#include <sys/eventfd.h>
#endif
#if !defined(_WIN32)
#include <unistd.h>
#include <fcntl.h>
#endif

#include "core/types/window.h"
#include "platform/platform.h"
#include "core/feature_loader.h"
#include "core/window_manager.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "core/owner_registry.h"
#include "core/widget_registry.h"
#include "deps/log.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_misc.h"
#include "config_values.h"
#include "core/types/keys.h"

static HotkeyBinding *hotkeys = NULL;
static atomic_bool hotkeys_suspended = false;

char *hotkey_binding_format(const HotkeyBinding *b) {
  if (!b)
    return NULL;
  char tmp[256];
  if (!hotkey_string_format((HotkeyKey){.vk = b->vk, .mods = b->modifiers}, tmp,
                            (int)sizeof(tmp)))
    return NULL;
  return strdup(tmp);
}

static void hotkey_binding_free_window_list(HotkeyBinding *b) {
  if (!b)
    return;
  for (int i = 0; i < b->window_count; i++)
    free(b->window_list[i]);
  free(b->window_list);
  b->window_list = NULL;
  b->window_count = 0;
}

static void hotkey_destroy_cb(void *element) {
  HotkeyBinding *b = (HotkeyBinding *)element;
  log_info("hotkey vk=0x%X mods=%d purged (owner unloaded)", b->vk,
           b->modifiers);
  hotkey_binding_free_window_list(b);
  owner_tag_clear(&b->tag);
}

void hotkey_set_suspended(bool suspended) {
  atomic_store_explicit(&hotkeys_suspended, suspended, memory_order_release);
}

static bool hotkey_is_suspended(void) {
  return atomic_load_explicit(&hotkeys_suspended, memory_order_acquire);
}

#define HOTKEY_PENDING_MAX 64

typedef struct {
  void (*fn)(void);
  HotkeyCallbackUd fn_ud;
  void *ud;
  OwnerTag tag;
} PendingHotkey;

static PendingHotkey pending_hotkeys[HOTKEY_PENDING_MAX];
static int pending_head = 0;
static int pending_tail = 0;
static void *q_mutex = NULL;
static unsigned long pending_dropped = 0;

#if defined(__linux__)
static int wake_fd = -1;
static void *wake_fd_mutex = NULL;
#endif

int hotkey_wake_fd(void) {
#if defined(__linux__)
  void *mutex = lazy_mutex_get(&wake_fd_mutex);
  if (mutex)
    Platform_MutexLock(mutex);
  if (wake_fd < 0)
    wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  int fd = wake_fd;
  if (mutex)
    Platform_MutexUnlock(mutex);
  return fd;
#else
  return -1;
#endif
}

static void hotkey_wake(void) {
#if defined(__linux__)
  int fd = hotkey_wake_fd();
  if (fd < 0)
    return;
  unsigned long long one = 1;
  ssize_t wr = write(fd, &one, sizeof(one));
  (void)wr;
#endif
}

static void hotkey_queue_init(void) {
  q_mutex = lazy_mutex_get(&q_mutex);
  hotkey_wake_fd();
}

#define HOTKEY_Q_LOCK()                                                        \
  do {                                                                         \
    hotkey_queue_init();                                                       \
    Platform_MutexLock(q_mutex);                                               \
  } while (0)
#define HOTKEY_Q_UNLOCK() Platform_MutexUnlock(q_mutex)

static void hotkey_enqueue(void (*fn)(void), HotkeyCallbackUd fn_ud, void *ud,
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
    owner_tag_set(&slot->tag, owner, 0);
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

void hotkey_key_up(int vk) {
  if (!vk)
    return;
  hotkey_hold_end(vk);
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

static bool hotkey_binding_allowed(const HotkeyBinding *b, int vk,
                                   int modifiers);

static void hotkey_tick_holds(void) {
  unsigned long long now = hotkey_now_ms();

  struct {
    int slot;
    int vk;
    int mods;
  } due[HOTKEY_HOLD_MAX];

  int due_count = 0;
  HOTKEY_Q_LOCK();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!hold_slots[i].active)
      continue;
    if ((long long)(now - hold_slots[i].next_fire_ms) < 0)
      continue;
    due[due_count].slot = i;
    due[due_count].vk = hold_slots[i].vk;
    due[due_count].mods = hold_slots[i].modifiers;
    due_count++;
    hold_slots[i].next_fire_ms =
        now + (unsigned long long)hotkey_repeat_interval_ms();
  }
  HOTKEY_Q_UNLOCK();

  for (int d = 0; d < due_count; d++) {
    int vk = due[d].vk;
    int mods = due[d].mods;
    HotkeyBinding *b = NULL;
    for (int k = 0; k < arrlen(hotkeys); k++) {
      if (hotkeys[k].vk == vk && hotkeys[k].modifiers == mods &&
          hotkeys[k].hold) {
        b = &hotkeys[k];
        break;
      }
    }
    if (!b || !hotkey_binding_allowed(b, vk, mods)) {
      HOTKEY_Q_LOCK();
      hold_slots[due[d].slot].active = false;
      HOTKEY_Q_UNLOCK();
      continue;
    }
    hotkey_enqueue(b->hotkey_callback, b->hotkey_callback_ud, b->user_ud,
                   b->tag.owner);
  }
}

void hotkey_drain_pending(void) {
  if (hotkey_is_suspended())
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
  for (int i = 0; i < arrlen(hotkeys); i++) {
    int vk = hotkeys[i].vk;
    if (vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + MOUSE_BTN_MAX - 1)
      return true;
  }
  return false;
}

bool hotkey_has_keyboard_binding(void) {
  for (int i = 0; i < arrlen(hotkeys); i++) {
    int vk = hotkeys[i].vk;
    if (vk >= VK_MOUSE_BASE && vk <= VK_MOUSE_BASE + MOUSE_BTN_MAX - 1)
      continue;
    return true;
  }
  return false;
}

static bool hotkey_window_list_match(const HotkeyBinding *b,
                                     const char *title) {
  if (!b->window_list || b->window_count == 0)
    return false;
  for (int j = 0; j < b->window_count; j++) {
    if (b->window_list[j][0] == '\0')
      continue;
    if (title && string_contains_ci(title, b->window_list[j]))
      return true;
  }
  return false;
}

bool hotkey_mouse_button_bound(int btn) {
  if (btn <= 0 || btn > MOUSE_BTN_MAX)
    return false;
  int vk = MOUSE_VK(btn);
  for (int i = 0; i < arrlen(hotkeys); i++) {
    if (hotkeys[i].vk != vk || hotkeys[i].modifiers != 0)
      continue;
    const HotkeyBinding *b = &hotkeys[i];
    if (b->whitelist) {
      if (!b->window_list || b->window_count == 0)
        continue;
      const char *title = Platform_GetActiveWindowId();
      if (!hotkey_window_list_match(b, title))
        continue;
    } else {
      if (b->window_list && b->window_count > 0) {
        const char *title = Platform_GetActiveWindowId();
        if (hotkey_window_list_match(b, title))
          continue;
      }
    }
    return true;
  }
  return false;
}

int hotkey_count(void) { return arrlen(hotkeys); }

const HotkeyBinding *hotkey_get(int index) {
  if (index < 0 || index >= arrlen(hotkeys))
    return NULL;
  return &hotkeys[index];
}

const HotkeyBinding *hotkey_find_by_ud(const void *ud) {
  if (!ud)
    return NULL;
  for (int i = 0; i < arrlen(hotkeys); i++)
    if (hotkeys[i].user_ud == ud)
      return &hotkeys[i];
  return NULL;
}

const HotkeyBinding *hotkey_find_by_callback(void (*cb)(void)) {
  if (!cb)
    return NULL;
  for (int i = 0; i < arrlen(hotkeys); i++)
    if (hotkeys[i].hotkey_callback == cb)
      return &hotkeys[i];
  return NULL;
}

bool hotkey_is_taken(int vk, int modifiers) {
  for (int i = 0; i < arrlen(hotkeys); i++)
    if (hotkeys[i].vk == vk && hotkeys[i].modifiers == modifiers)
      return true;
  return false;
}

void hotkey_register_internal(HotkeyBinding binding) {
  owner_tag_set_for_func(&binding.tag,
                         (void *)(uintptr_t)binding.hotkey_callback, 0);
  if (!binding.tag.owner)
    owner_tag_set_for_func(&binding.tag,
                           (void *)(uintptr_t)binding.hotkey_callback_ud, 0);
  if (!binding.tag.owner)
    owner_tag_set_for_func(&binding.tag,
                           (void *)(uintptr_t)binding.hotkey_condition, 0);

  for (int i = 0; i < arrlen(hotkeys); i++) {
    if (hotkeys[i].vk == binding.vk &&
        hotkeys[i].modifiers == binding.modifiers) {
      hotkey_binding_free_window_list(&hotkeys[i]);
      owner_tag_clear(&hotkeys[i].tag);
      hotkeys[i] = binding;
      return;
    }
  }
  arrput(hotkeys, binding);
}

bool hotkey_unregister_internal(int vk, int modifiers) {
  int n = 0;
  bool found = false;
  for (int i = 0; i < arrlen(hotkeys); i++) {
    if (hotkeys[i].vk == vk && hotkeys[i].modifiers == modifiers) {
      hotkey_binding_free_window_list(&hotkeys[i]);
      owner_tag_clear(&hotkeys[i].tag);
      found = true;
      continue;
    }
    hotkeys[n++] = hotkeys[i];
  }
  arrsetlen(hotkeys, n);
  return found;
}

void hotkey_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b))
    return;
  char *a = owner_strdup(path_a);
  char *b = owner_strdup(path_b);
  int dropped = (int)OWNER_PURGE_STRUCT(hotkeys, HotkeyBinding, a, b, 0,
                                        hotkey_destroy_cb);
  if (dropped > 0)
    log_info("hotkey purged %d binding%s", dropped, dropped == 1 ? "" : "s");
  HOTKEY_Q_LOCK();
  for (int i = 0; i < HOTKEY_HOLD_MAX; i++) {
    if (!hold_slots[i].active)
      continue;
    for (int k = 0; k < arrlen(hotkeys); k++) {
      if (hotkeys[k].vk == hold_slots[i].vk &&
          hotkeys[k].modifiers == hold_slots[i].modifiers &&
          owner_tag_matches(&hotkeys[k].tag, a, b)) {
        hold_slots[i].active = false;
        break;
      }
    }
  }
  int qn = pending_head;
  while (qn != pending_tail) {
    if (owner_tag_matches(&pending_hotkeys[qn].tag, a, b)) {
      log_info("hotkey pending entry dropped (owner unloaded)");
      pending_hotkeys[qn].fn = NULL;
      pending_hotkeys[qn].fn_ud = NULL;
      pending_hotkeys[qn].ud = NULL;
      owner_tag_clear(&pending_hotkeys[qn].tag);
    }
    qn = (qn + 1) % HOTKEY_PENDING_MAX;
  }
  HOTKEY_Q_UNLOCK();
  owner_strfree(&a);
  owner_strfree(&b);
}

static int hotkey_strip_self_mod(int vk, int mods) {
  int eff = mods;
  if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT)
    eff &= ~HOTKEY_MOD_SHIFT;
  else if (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL)
    eff &= ~HOTKEY_MOD_CTRL;
  else if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)
    eff &= ~HOTKEY_MOD_ALT;
  else if (vk == VK_LWIN || vk == VK_RWIN)
    eff &= ~HOTKEY_MOD_SUPER;
  return eff;
}

static bool hotkey_binding_allowed(const HotkeyBinding *b, int vk,
                                   int modifiers) {
  int eff_modifiers = hotkey_strip_self_mod(vk, modifiers);
  int b_eff = hotkey_strip_self_mod(b->vk, b->modifiers);
  if (b_eff != eff_modifiers)
    return false;
  if (b_eff != 0 && !remap_check_strict_order(vk, b_eff))
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
    if (b->window_list && b->window_count > 0) {
      if (!title_fetched) {
        title = Platform_GetActiveWindowId();
        title_fetched = true;
      }
      if (hotkey_window_list_match(b, title))
        return false;
    }
    return true;
  }

  if (!b->window_list || b->window_count == 0)
    return false;
  if (!title_fetched) {
    title = Platform_GetActiveWindowId();
    title_fetched = true;
  }
  return hotkey_window_list_match(b, title);
}

bool hotkey_process(int vk, int modifiers) {
  if (hotkey_is_suspended() || input_key_is_window_escape(vk) ||
      widget_key_capture_active())
    return false;
  int eff_modifiers = hotkey_strip_self_mod(vk, modifiers);
  for (int i = 0; i < arrlen(hotkeys); i++) {
    HotkeyBinding *b = &hotkeys[i];

    if (b->vk != vk) {
      continue;
    }
    if (!hotkey_binding_allowed(b, vk, eff_modifiers)) {
      continue;
    }

    void (*cb)(void) = b->hotkey_callback;
    bool swallow = true;
    if (b->should_swallow) {
      if (feature_loader_try_lock()) {
        swallow = b->should_swallow();
        feature_loader_unlock();
      } else {
        swallow = false;
      }
    }
    hotkey_enqueue(cb, b->hotkey_callback_ud, b->user_ud, b->tag.owner);
    char buf[2556];
    char hotkey_buf[2422];
    hotkey_string_format((HotkeyKey){.vk = vk, .mods = eff_modifiers},
                         hotkey_buf, sizeof(hotkey_buf));
    if (MessageAfterEachHotkey) {
      snprintf(buf, sizeof(buf), "Ran Hotkey %s", hotkey_buf);
      display_message_slot(&(MessageSlotSpec){.message = buf,
                                              .slot = BOTTOM_CENTER,
                                              .max_w = 400,
                                              .max_h = 400,
                                              .prompt_gap = 20,
                                              .timeout = TimeOutForMessages,
                                              .color = COLOR_TEXT});
    }
    log_info("hotkey_process Hotkey %s Matches", hotkey_buf);
    if (b->hold)
      hotkey_hold_begin(vk, b->modifiers);
    return swallow;
  }
  return false;
}

bool hotkey_execute(int vk, int modifiers) {
  int eff_modifiers = hotkey_strip_self_mod(vk, modifiers);
  for (int i = 0; i < arrlen(hotkeys); i++) {
    HotkeyBinding *b = &hotkeys[i];
    if (b->vk != vk)
      continue;
    if (hotkey_strip_self_mod(b->vk, b->modifiers) != eff_modifiers)
      continue;
    hotkey_enqueue(b->hotkey_callback, b->hotkey_callback_ud, b->user_ud,
                   b->tag.owner);
    return true;
  }
  return false;
}
