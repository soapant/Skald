#include "helpers/helpers_text.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_misc.h"
#include "core/owner_registry.h"
#include "core/remap_manager.h"
#include "core/types/keys.h"
#include "core/types/window.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "deps/stb_ds.h"
#include <string.h>
#include <stdlib.h>

#define MAX_SRC_VKS 8
#define REMAP_MAX_WINDOWS 8

typedef struct {
  int src_mods;
  int src_vks[MAX_SRC_VKS];
  int src_vk_count;
  int dst_mods;
  int dst_vk;
  bool whitelist;
  char *windows[REMAP_MAX_WINDOWS];
  int window_count;
  OwnerTag tag;
} RemapBinding;

static RemapBinding *remaps;
#define remap_count arrlenu(remaps)

static void *remap_mutex = NULL;

static void remap_lock(void) {
  remap_mutex = lazy_mutex_get(&remap_mutex);
  if (remap_mutex)
    Platform_MutexLock(remap_mutex);
}

static void remap_unlock(void) {
  if (remap_mutex)
    Platform_MutexUnlock(remap_mutex);
}

static int phys_vk[256] = {0};
static int virt_vk[256] = {0};
static unsigned long long key_seq[256] = {0};
static unsigned long long seq_counter = 0;

static bool *scratch_before;
static bool *scratch_after;
static bool *scratch_allowed;
static size_t scratch_cap = 0;

static bool remap_scratch_reserve(size_t n) {
  if (n <= scratch_cap)
    return true;
  bool *p = realloc(scratch_before, n * sizeof(*p));
  if (!p)
    return false;
  scratch_before = p;
  p = realloc(scratch_after, n * sizeof(*p));
  if (!p)
    return false;
  scratch_after = p;
  p = realloc(scratch_allowed, n * sizeof(*p));
  if (!p)
    return false;
  scratch_allowed = p;
  scratch_cap = n;
  return true;
}

bool remap_has_bindings(void) {
  remap_lock();
  bool has = remap_count > 0;
  remap_unlock();
  return has;
}

static bool remap_window_eq(const char *a, const char *b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return strcmp(a, b) == 0;
}

static bool remap_register_internal_locked(const char *src, const char *dst,
                                           bool whitelist,
                                           const char *const *windows,
                                           void *caller) {
  if (!src || !dst)
    return false;
  RemapBinding b = {0};
  b.whitelist = whitelist;
  owner_tag_set_for_func(&b.tag, caller, 0);
  if (windows) {
    for (int i = 0; windows[i] != NULL && b.window_count < REMAP_MAX_WINDOWS;
         i++) {
      if (!windows[i][0])
        continue;
      char *copy = strdup(windows[i]);
      if (!copy)
        continue;
      b.windows[b.window_count++] = copy;
    }
  }
  int tmp_vks[MAX_SRC_VKS];
  int tmp_count = 0;
  int tmp_mods = 0;
  remap_parse_triggers(src, &tmp_mods, tmp_vks, &tmp_count);
  if (tmp_count == 0 && tmp_mods == 0) {
    log_warn("remap '%s' -> '%s': no trigger keys parsed, refusing", src, dst);
    for (int k = 0; k < b.window_count; k++)
      free(b.windows[k]);
    owner_tag_clear(&b.tag);
    return false;
  }
  b.src_mods = tmp_mods;
  b.src_vk_count = tmp_count;
  for (int i = 0; i < tmp_count; i++)
    b.src_vks[i] = tmp_vks[i];

  int dst_mods = 0;
  int dst_vks[MAX_SRC_VKS];
  int dst_count = 0;
  remap_parse_triggers(dst, &dst_mods, dst_vks, &dst_count);
  if (dst_count == 0 && dst_mods == 0) {
    log_warn("remap '%s' -> '%s': no destination keys parsed, refusing", src,
             dst);
    for (int k = 0; k < b.window_count; k++)
      free(b.windows[k]);
    owner_tag_clear(&b.tag);
    return false;
  }
  b.dst_mods = dst_mods;
  if (dst_count > 0)
    b.dst_vk = dst_vks[0];
  else
    b.dst_vk = 0;
  for (size_t i = 0; i < remap_count; i++) {
    const RemapBinding *e = &remaps[i];
    if (e->src_mods == b.src_mods && e->dst_mods == b.dst_mods &&
        e->dst_vk == b.dst_vk && e->src_vk_count == b.src_vk_count &&
        e->whitelist == b.whitelist && e->window_count == b.window_count &&
        (e->tag.owner == b.tag.owner ||
          (e->tag.owner && b.tag.owner &&
           strcmp(e->tag.owner, b.tag.owner) == 0))) {
      bool same = true;
      for (int k = 0; k < b.src_vk_count; k++)
        if (e->src_vks[k] != b.src_vks[k]) {
          same = false;
          break;
        }
      for (int k = 0; k < b.window_count && same; k++)
        if (!remap_window_eq(e->windows[k], b.windows[k]))
          same = false;
      if (same) {
        log_warn("remap duplicate ignored src='%s' dst='%s' total=%d", src, dst,
                 remap_count);
        for (int k = 0; k < b.window_count; k++)
          free(b.windows[k]);
        owner_tag_clear(&b.tag);
        return true;
      }
    }
  }
  arrput(remaps, b);
  return true;
}

bool remap_register_internal(const char *src, const char *dst, bool whitelist,
                             const char *const *windows, void *caller) {
  remap_lock();
  bool ok = remap_register_internal_locked(src, dst, whitelist, windows, caller);
  remap_unlock();
  return ok;
}

void remap_update_phys(int vk, int value) {
  if (vk < 0 || vk >= 256)
    return;
  if (value == 2)
    return;
  bool was_down = phys_vk[vk];
  phys_vk[vk] = (value != 0);
  if (value != 0 && !was_down) {
    seq_counter++;
    key_seq[vk] = seq_counter;
  } else if (value == 0) {
    key_seq[vk] = 0;
  }
}

static void emit_tracked(int vk, int down) {
  if (!Platform_EmitKey(vk, down)) {
    log_error("remap: emit failed vk=%d down=%d", vk, down);
    return;
  }
  virt_vk[vk] = down ? 1 : 0;
}

void remap_reset(void) {
  for (int i = 0; i < 256; i++) {
    if (virt_vk[i])
      emit_tracked(i, 0);
    phys_vk[i] = 0;
    key_seq[i] = 0;
  }
  seq_counter = 0;
}

bool remap_check_strict_order(int trigger_vk, int mods) {
  if (trigger_vk < 0 || trigger_vk >= 256)
    return true;
  unsigned long long trig_seq = key_seq[trigger_vk];
  if (trig_seq == 0)
    return false;

  if (mods & HOTKEY_MOD_CTRL) {
    unsigned long long mseq = 0;
    if (key_seq[VK_LCONTROL])
      mseq = key_seq[VK_LCONTROL];
    if (key_seq[VK_RCONTROL] && key_seq[VK_RCONTROL] > mseq)
      mseq = key_seq[VK_RCONTROL];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_ALT) {
    unsigned long long mseq = 0;
    if (key_seq[VK_LMENU])
      mseq = key_seq[VK_LMENU];
    if (key_seq[VK_RMENU] && key_seq[VK_RMENU] > mseq)
      mseq = key_seq[VK_RMENU];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_SHIFT) {
    unsigned long long mseq = 0;
    if (key_seq[VK_LSHIFT])
      mseq = key_seq[VK_LSHIFT];
    if (key_seq[VK_RSHIFT] && key_seq[VK_RSHIFT] > mseq)
      mseq = key_seq[VK_RSHIFT];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_SUPER) {
    unsigned long long mseq = 0;
    if (key_seq[VK_LWIN])
      mseq = key_seq[VK_LWIN];
    if (key_seq[VK_RWIN] && key_seq[VK_RWIN] > mseq)
      mseq = key_seq[VK_RWIN];
    if (mseq > trig_seq)
      return false;
  }
  return true;
}

int remap_current_mods(void) {
  int m = 0;
  if (phys_vk[VK_LCONTROL] || phys_vk[VK_RCONTROL] || phys_vk[VK_CONTROL])
    m |= HOTKEY_MOD_CTRL;
  if (phys_vk[VK_LMENU] || phys_vk[VK_RMENU] || phys_vk[VK_MENU])
    m |= HOTKEY_MOD_ALT;
  if (phys_vk[VK_LSHIFT] || phys_vk[VK_RSHIFT] || phys_vk[VK_SHIFT])
    m |= HOTKEY_MOD_SHIFT;
  if (phys_vk[VK_LWIN] || phys_vk[VK_RWIN])
    m |= HOTKEY_MOD_SUPER;
  return m;
}

bool remap_is_virt_held(int vk) {
  if (vk < 0 || vk >= 256)
    return false;
  return virt_vk[vk];
}

static bool binding_active(RemapBinding *b) {
  if (b->src_mods == 0 && b->src_vk_count == 0)
    return false;
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (!(phys_vk[VK_LCONTROL] || phys_vk[VK_RCONTROL] || phys_vk[VK_CONTROL]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (!(phys_vk[VK_LMENU] || phys_vk[VK_RMENU] || phys_vk[VK_MENU]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (!(phys_vk[VK_LSHIFT] || phys_vk[VK_RSHIFT] || phys_vk[VK_SHIFT]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (!(phys_vk[VK_LWIN] || phys_vk[VK_RWIN]))
      return false;
  }
  for (int i = 0; i < b->src_vk_count; i++) {
    int vk = b->src_vks[i];
    if (vk < 0 || vk >= 256)
      return false;
    if (!phys_vk[vk])
      return false;
  }
  return true;
}

static void remap_fire_binding(RemapBinding *b, int trigger_vk) {
  for (int k = 0; k < b->src_vk_count; k++) {
    int sv = b->src_vks[k];
    if (sv != trigger_vk) {
      emit_tracked(sv, 0);
    }
  }
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (phys_vk[VK_LCONTROL]) {
      emit_tracked(VK_LCONTROL, 0);
    }
    if (phys_vk[VK_RCONTROL]) {
      emit_tracked(VK_RCONTROL, 0);
    }
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (phys_vk[VK_LMENU]) {
      emit_tracked(VK_LMENU, 0);
    }
    if (phys_vk[VK_RMENU]) {
      emit_tracked(VK_RMENU, 0);
    }
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (phys_vk[VK_LSHIFT]) {
      emit_tracked(VK_LSHIFT, 0);
    }
    if (phys_vk[VK_RSHIFT]) {
      emit_tracked(VK_RSHIFT, 0);
    }
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (phys_vk[VK_LWIN]) {
      emit_tracked(VK_LWIN, 0);
    }
    if (phys_vk[VK_RWIN]) {
      emit_tracked(VK_RWIN, 0);
    }
  }
  if (b->dst_mods & HOTKEY_MOD_CTRL) {
    emit_tracked(VK_LCONTROL, 1);
  }
  if (b->dst_mods & HOTKEY_MOD_SHIFT) {
    emit_tracked(VK_LSHIFT, 1);
  }
  if (b->dst_mods & HOTKEY_MOD_ALT) {
    emit_tracked(VK_LMENU, 1);
  }
  if (b->dst_mods & HOTKEY_MOD_SUPER) {
    emit_tracked(VK_LWIN, 1);
  }
  if (b->dst_vk) {
    emit_tracked(b->dst_vk, 1);
  }
}

static void remap_release_binding(RemapBinding *b, bool restore_src) {
  if (b->dst_vk && virt_vk[b->dst_vk]) {
    emit_tracked(b->dst_vk, 0);
  }
  if ((b->dst_mods & HOTKEY_MOD_CTRL) && virt_vk[VK_LCONTROL]) {
    emit_tracked(VK_LCONTROL, 0);
  }
  if ((b->dst_mods & HOTKEY_MOD_SHIFT) && virt_vk[VK_LSHIFT]) {
    emit_tracked(VK_LSHIFT, 0);
  }
  if ((b->dst_mods & HOTKEY_MOD_ALT) && virt_vk[VK_LMENU]) {
    emit_tracked(VK_LMENU, 0);
  }
  if ((b->dst_mods & HOTKEY_MOD_SUPER) && virt_vk[VK_LWIN]) {
    emit_tracked(VK_LWIN, 0);
  }
  if (!restore_src)
    return;
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (phys_vk[VK_LCONTROL]) {
      emit_tracked(VK_LCONTROL, 1);
    }
    if (phys_vk[VK_RCONTROL]) {
      emit_tracked(VK_RCONTROL, 1);
    }
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (phys_vk[VK_LMENU]) {
      emit_tracked(VK_LMENU, 1);
    }
    if (phys_vk[VK_RMENU]) {
      emit_tracked(VK_RMENU, 1);
    }
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (phys_vk[VK_LSHIFT]) {
      emit_tracked(VK_LSHIFT, 1);
    }
    if (phys_vk[VK_RSHIFT]) {
      emit_tracked(VK_RSHIFT, 1);
    }
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (phys_vk[VK_LWIN]) {
      emit_tracked(VK_LWIN, 1);
    }
    if (phys_vk[VK_RWIN]) {
      emit_tracked(VK_RWIN, 1);
    }
  }
}

static void remap_repeat_binding(RemapBinding *b) {
  if (b->dst_vk) {
    Platform_EmitKey(b->dst_vk, 1);
  }
}

void remap_send_combo(const char *combo) {
  if (!combo || !combo[0])
    return;
  int mods = 0;
  int vks[MAX_SRC_VKS];
  int count = 0;
  remap_parse_triggers(combo, &mods, vks, &count);
  if (count <= 0)
    return;
  RemapBinding b = {0};
  b.src_mods = remap_current_mods();
  b.dst_mods = mods;
  b.dst_vk = vks[0];
  remap_lock();
  remap_fire_binding(&b, 0);
  remap_release_binding(&b, true);
  remap_unlock();
}

static bool remap_handle_vk_locked(int vk, int value) {
  if (vk < 0 || vk >= 256)
    return false;
  if (value == 2)
    return false;
  if (!remap_scratch_reserve(remap_count)) {
    remap_update_phys(vk, value);
    return false;
  }
  bool *active_before = scratch_before;
  bool *active_after = scratch_after;
  bool *allowed = scratch_allowed;
  memset(active_before, 0, remap_count * sizeof(*active_before));
  memset(active_after, 0, remap_count * sizeof(*active_after));
  memset(allowed, 1, remap_count * sizeof(*allowed));
  for (size_t i = 0; i < remap_count; i++)
    active_before[i] = binding_active(&remaps[i]);

  remap_update_phys(vk, value);

  for (size_t i = 0; i < remap_count; i++)
    active_after[i] = binding_active(&remaps[i]);

  bool need_title = false;
  for (size_t i = 0; i < remap_count; i++) {
    if (remaps[i].window_count > 0) {
      need_title = true;
      break;
    }
  }
  const char *title = NULL;
  if (need_title)
    title = Platform_GetActiveWindowTitle();
  for (size_t i = 0; i < remap_count; i++) {
    if (remaps[i].window_count > 0)
      allowed[i] =
          window_list_allows(remaps[i].window_count, remaps[i].whitelist,
                             remaps[i].windows, title);
  }

  for (size_t i = 0; i < remap_count; i++) {
    RemapBinding *b = &remaps[i];
    bool before = active_before[i];
    bool after = active_after[i];
    if (!before && after && allowed[i]) {
      remap_fire_binding(b, vk);
    } else if (before && !after) {
      remap_release_binding(b, allowed[i]);
    } else if (before && after && allowed[i]) {
      remap_repeat_binding(b);
    }
  }

  bool swallow = false;
  for (size_t i = 0; i < remap_count; i++) {
    const RemapBinding *b = &remaps[i];
    bool before = active_before[i] && allowed[i];
    bool after = active_after[i] && allowed[i];
    for (int k = 0; k < b->src_vk_count; k++)
      if (b->src_vks[k] == vk) {
        if (before || after)
          swallow = true;
      }
    if (b->src_mods & HOTKEY_MOD_CTRL &&
        (vk == VK_LCONTROL || vk == VK_RCONTROL ||
         vk == VK_CONTROL)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_ALT &&
        (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_SHIFT &&
        (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_SUPER &&
        (vk == VK_LWIN || vk == VK_RWIN)) {
      if (before || after)
        swallow = true;
    }
    if (after && !before) {
      if (b->src_vk_count > 0 && b->src_vks[b->src_vk_count - 1] == vk)
        swallow = true;
    }
  }
  return swallow;
}

bool remap_handle_vk(int vk, int value) {
  remap_lock();
  bool swallow = remap_handle_vk_locked(vk, value);
  remap_unlock();
  return swallow;
}

bool remap_should_swallow_vk(int vk, int value) {
  return remap_handle_vk(vk, value);
}

static void remap_destroy_cb(void *element) {
  RemapBinding *b = (RemapBinding *)element;
  remap_release_binding(b, true);
  for (int k = 0; k < b->window_count; k++)
    free(b->windows[k]);
  owner_tag_clear(&b->tag);
}

void remap_purge_owner(const char *path_a, const char *path_b) {
  remap_lock();
  owner_purge_array(remaps, sizeof(RemapBinding), offsetof(RemapBinding, tag),
                    path_a, path_b, 0, remap_destroy_cb);
  remap_unlock();
}
