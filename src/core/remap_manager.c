#include "core/remap_manager.h"
#include "core/remap_parse.h"
#include "core/types/keys.h"
#include "platform/platform.h"
#include "deps/log.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#define MAX_REMAPS 64
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
} RemapBinding;

static RemapBinding g_remaps[MAX_REMAPS];
static int g_remap_count = 0;

static int g_phys_vk[256] = {0};
static int g_virt_vk[256] = {0};
static unsigned long long g_key_seq[256] = {0};
static unsigned long long g_seq_counter = 0;

bool remap_has_bindings(void) { return g_remap_count > 0; }

static bool remap_window_eq(const char *a, const char *b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return strcmp(a, b) == 0;
}

static bool remap_window_allowed(RemapBinding *b, const char *title) {
  if (!b)
    return true;
  if (b->window_count == 0)
    return !b->whitelist;
  if (!b->whitelist) {
    for (int j = 0; j < b->window_count; j++) {
      if (b->windows[j] && b->windows[j][0] &&
          Platform_StringContainsCaseInsensitive(title, b->windows[j]))
        return false;
    }
    return true;
  }
  if (!title)
    return false;
  for (int j = 0; j < b->window_count; j++) {
    if (b->windows[j] && b->windows[j][0] &&
        Platform_StringContainsCaseInsensitive(title, b->windows[j]))
      return true;
  }
  return false;
}

bool remap_register_internal(const char *src, const char *dst, bool whitelist,
                             const char **windows) {
  if (!src || !dst)
    return false;
  if (g_remap_count >= MAX_REMAPS)
    return false;
  RemapBinding b = {0};
  b.whitelist = whitelist;
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
  Remap_ParseTriggers(src, &tmp_mods, tmp_vks, &tmp_count);
  b.src_mods = tmp_mods;
  b.src_vk_count = tmp_count;
  for (int i = 0; i < tmp_count; i++)
    b.src_vks[i] = tmp_vks[i];

  int dst_mods = 0;
  int dst_vks[MAX_SRC_VKS];
  int dst_count = 0;
  Remap_ParseTriggers(dst, &dst_mods, dst_vks, &dst_count);
  b.dst_mods = dst_mods;
  if (dst_count > 0)
    b.dst_vk = dst_vks[0];
  else
    b.dst_vk = 0;
  for (int i = 0; i < g_remap_count; i++) {
    RemapBinding *e = &g_remaps[i];
    if (e->src_mods == b.src_mods && e->dst_mods == b.dst_mods &&
        e->dst_vk == b.dst_vk && e->src_vk_count == b.src_vk_count &&
        e->whitelist == b.whitelist && e->window_count == b.window_count) {
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
                 g_remap_count);
        for (int k = 0; k < b.window_count; k++)
          free(b.windows[k]);
        return true;
      }
    }
  }
  g_remaps[g_remap_count++] = b;
  return true;
}

void remap_update_phys(int vk_code, int value) {
  if (vk_code < 0 || vk_code >= 256)
    return;
  if (value == 2)
    return;
  bool was_down = g_phys_vk[vk_code];
  g_phys_vk[vk_code] = (value != 0);
  if (value != 0 && !was_down) {
    g_seq_counter++;
    g_key_seq[vk_code] = g_seq_counter;
  } else if (value == 0) {
    g_key_seq[vk_code] = 0;
  }
}

void remap_reset(void) {
  for (int i = 0; i < 256; i++) {
    if (g_virt_vk[i]) {
      Platform_EmitKey(i, 0);
      g_virt_vk[i] = 0;
    }
    g_phys_vk[i] = 0;
    g_key_seq[i] = 0;
  }
  g_seq_counter = 0;
}

bool remap_check_strict_order(int trigger_vk, int mods) {
  if (trigger_vk < 0 || trigger_vk >= 256)
    return true;
  unsigned long long trig_seq = g_key_seq[trigger_vk];
  if (trig_seq == 0)
    return false;

  if (mods & HOTKEY_MOD_CTRL) {
    unsigned long long mseq = 0;
    if (g_key_seq[VK_LCONTROL])
      mseq = g_key_seq[VK_LCONTROL];
    if (g_key_seq[VK_RCONTROL] && g_key_seq[VK_RCONTROL] > mseq)
      mseq = g_key_seq[VK_RCONTROL];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_ALT) {
    unsigned long long mseq = 0;
    if (g_key_seq[VK_LMENU])
      mseq = g_key_seq[VK_LMENU];
    if (g_key_seq[VK_RMENU] && g_key_seq[VK_RMENU] > mseq)
      mseq = g_key_seq[VK_RMENU];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_SHIFT) {
    unsigned long long mseq = 0;
    if (g_key_seq[VK_LSHIFT])
      mseq = g_key_seq[VK_LSHIFT];
    if (g_key_seq[VK_RSHIFT] && g_key_seq[VK_RSHIFT] > mseq)
      mseq = g_key_seq[VK_RSHIFT];
    if (mseq > trig_seq)
      return false;
  }
  if (mods & HOTKEY_MOD_SUPER) {
    unsigned long long mseq = 0;
    if (g_key_seq[VK_LWIN])
      mseq = g_key_seq[VK_LWIN];
    if (g_key_seq[VK_RWIN] && g_key_seq[VK_RWIN] > mseq)
      mseq = g_key_seq[VK_RWIN];
    if (mseq > trig_seq)
      return false;
  }
  return true;
}

int remap_current_mods(void) {
  int m = 0;
  if (g_phys_vk[VK_LCONTROL] || g_phys_vk[VK_RCONTROL] || g_phys_vk[VK_CONTROL])
    m |= HOTKEY_MOD_CTRL;
  if (g_phys_vk[VK_LMENU] || g_phys_vk[VK_RMENU] || g_phys_vk[VK_MENU])
    m |= HOTKEY_MOD_ALT;
  if (g_phys_vk[VK_LSHIFT] || g_phys_vk[VK_RSHIFT] || g_phys_vk[VK_SHIFT])
    m |= HOTKEY_MOD_SHIFT;
  if (g_phys_vk[VK_LWIN] || g_phys_vk[VK_RWIN])
    m |= HOTKEY_MOD_SUPER;
  return m;
}

bool remap_is_virt_held(int vk) {
  if (vk < 0 || vk >= 256)
    return false;
  return g_virt_vk[vk];
}

static bool binding_active(RemapBinding *b) {
  if (b->src_mods == 0 && b->src_vk_count == 0)
    return false;
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (!(g_phys_vk[VK_LCONTROL] || g_phys_vk[VK_RCONTROL]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (!(g_phys_vk[VK_LMENU] || g_phys_vk[VK_RMENU]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (!(g_phys_vk[VK_LSHIFT] || g_phys_vk[VK_RSHIFT]))
      return false;
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (!(g_phys_vk[VK_LWIN] || g_phys_vk[VK_RWIN]))
      return false;
  }
  for (int i = 0; i < b->src_vk_count; i++) {
    int vk = b->src_vks[i];
    if (vk < 0 || vk >= 256)
      return false;
    if (!g_phys_vk[vk])
      return false;
  }
  return true;
}

static void remap_fire_binding(RemapBinding *b, int trigger_vk) {
  for (int k = 0; k < b->src_vk_count; k++) {
    int sv = b->src_vks[k];
    if (sv != trigger_vk) {
      Platform_EmitKey(sv, 0);
      g_virt_vk[sv] = 0;
    }
  }
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (g_phys_vk[VK_LCONTROL]) {
      Platform_EmitKey(VK_LCONTROL, 0);
      g_virt_vk[VK_LCONTROL] = 0;
    }
    if (g_phys_vk[VK_RCONTROL]) {
      Platform_EmitKey(VK_RCONTROL, 0);
      g_virt_vk[VK_RCONTROL] = 0;
    }
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (g_phys_vk[VK_LMENU]) {
      Platform_EmitKey(VK_LMENU, 0);
      g_virt_vk[VK_LMENU] = 0;
    }
    if (g_phys_vk[VK_RMENU]) {
      Platform_EmitKey(VK_RMENU, 0);
      g_virt_vk[VK_RMENU] = 0;
    }
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (g_phys_vk[VK_LSHIFT]) {
      Platform_EmitKey(VK_LSHIFT, 0);
      g_virt_vk[VK_LSHIFT] = 0;
    }
    if (g_phys_vk[VK_RSHIFT]) {
      Platform_EmitKey(VK_RSHIFT, 0);
      g_virt_vk[VK_RSHIFT] = 0;
    }
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (g_phys_vk[VK_LWIN]) {
      Platform_EmitKey(VK_LWIN, 0);
      g_virt_vk[VK_LWIN] = 0;
    }
    if (g_phys_vk[VK_RWIN]) {
      Platform_EmitKey(VK_RWIN, 0);
      g_virt_vk[VK_RWIN] = 0;
    }
  }
  if (b->dst_mods & HOTKEY_MOD_CTRL) {
    Platform_EmitKey(VK_LCONTROL, 1);
    g_virt_vk[VK_LCONTROL] = 1;
  }
  if (b->dst_mods & HOTKEY_MOD_SHIFT) {
    Platform_EmitKey(VK_LSHIFT, 1);
    g_virt_vk[VK_LSHIFT] = 1;
  }
  if (b->dst_mods & HOTKEY_MOD_ALT) {
    Platform_EmitKey(VK_LMENU, 1);
    g_virt_vk[VK_LMENU] = 1;
  }
  if (b->dst_mods & HOTKEY_MOD_SUPER) {
    Platform_EmitKey(VK_LWIN, 1);
    g_virt_vk[VK_LWIN] = 1;
  }
  if (b->dst_vk) {
    Platform_EmitKey(b->dst_vk, 1);
    g_virt_vk[b->dst_vk] = 1;
  }
}

static void remap_release_binding(RemapBinding *b, bool restore_src) {
  if (b->dst_vk && g_virt_vk[b->dst_vk]) {
    Platform_EmitKey(b->dst_vk, 0);
    g_virt_vk[b->dst_vk] = 0;
  }
  if ((b->dst_mods & HOTKEY_MOD_CTRL) && g_virt_vk[VK_LCONTROL]) {
    Platform_EmitKey(VK_LCONTROL, 0);
    g_virt_vk[VK_LCONTROL] = 0;
  }
  if ((b->dst_mods & HOTKEY_MOD_SHIFT) && g_virt_vk[VK_LSHIFT]) {
    Platform_EmitKey(VK_LSHIFT, 0);
    g_virt_vk[VK_LSHIFT] = 0;
  }
  if ((b->dst_mods & HOTKEY_MOD_ALT) && g_virt_vk[VK_LMENU]) {
    Platform_EmitKey(VK_LMENU, 0);
    g_virt_vk[VK_LMENU] = 0;
  }
  if ((b->dst_mods & HOTKEY_MOD_SUPER) && g_virt_vk[VK_LWIN]) {
    Platform_EmitKey(VK_LWIN, 0);
    g_virt_vk[VK_LWIN] = 0;
  }
  if (!restore_src)
    return;
  if (b->src_mods & HOTKEY_MOD_CTRL) {
    if (g_phys_vk[VK_LCONTROL]) {
      Platform_EmitKey(VK_LCONTROL, 1);
      g_virt_vk[VK_LCONTROL] = 1;
    }
    if (g_phys_vk[VK_RCONTROL]) {
      Platform_EmitKey(VK_RCONTROL, 1);
      g_virt_vk[VK_RCONTROL] = 1;
    }
  }
  if (b->src_mods & HOTKEY_MOD_ALT) {
    if (g_phys_vk[VK_LMENU]) {
      Platform_EmitKey(VK_LMENU, 1);
      g_virt_vk[VK_LMENU] = 1;
    }
    if (g_phys_vk[VK_RMENU]) {
      Platform_EmitKey(VK_RMENU, 1);
      g_virt_vk[VK_RMENU] = 1;
    }
  }
  if (b->src_mods & HOTKEY_MOD_SHIFT) {
    if (g_phys_vk[VK_LSHIFT]) {
      Platform_EmitKey(VK_LSHIFT, 1);
      g_virt_vk[VK_LSHIFT] = 1;
    }
    if (g_phys_vk[VK_RSHIFT]) {
      Platform_EmitKey(VK_RSHIFT, 1);
      g_virt_vk[VK_RSHIFT] = 1;
    }
  }
  if (b->src_mods & HOTKEY_MOD_SUPER) {
    if (g_phys_vk[VK_LWIN]) {
      Platform_EmitKey(VK_LWIN, 1);
      g_virt_vk[VK_LWIN] = 1;
    }
    if (g_phys_vk[VK_RWIN]) {
      Platform_EmitKey(VK_RWIN, 1);
      g_virt_vk[VK_RWIN] = 1;
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
  Remap_ParseTriggers(combo, &mods, vks, &count);
  if (count <= 0)
    return;
  RemapBinding b = {0};
  b.src_mods = remap_current_mods();
  b.dst_mods = mods;
  b.dst_vk = vks[0];
  remap_fire_binding(&b, 0);
  remap_release_binding(&b, true);
}

bool remap_handle_vk(int vk_code, int value) {
  if (vk_code < 0 || vk_code >= 256)
    return false;
  if (value == 2)
    return false;
  bool active_before[64] = {0};
  for (int i = 0; i < g_remap_count; i++)
    active_before[i] = binding_active(&g_remaps[i]);

  remap_update_phys(vk_code, value);

  bool active_after[64] = {0};
  for (int i = 0; i < g_remap_count; i++)
    active_after[i] = binding_active(&g_remaps[i]);

  bool allowed[64];
  for (int i = 0; i < g_remap_count; i++)
    allowed[i] = true;
  bool need_title = false;
  for (int i = 0; i < g_remap_count; i++) {
    if (g_remaps[i].window_count > 0 && (active_before[i] != active_after[i])) {
      need_title = true;
      break;
    }
  }
  const char *title = NULL;
  if (need_title)
    title = Platform_GetActiveWindowTitle();
  for (int i = 0; i < g_remap_count; i++) {
    if (g_remaps[i].window_count > 0)
      allowed[i] = remap_window_allowed(&g_remaps[i], title);
  }

  for (int i = 0; i < g_remap_count; i++) {
    RemapBinding *b = &g_remaps[i];
    bool before = active_before[i];
    bool after = active_after[i];
    if (!before && after && allowed[i]) {
      remap_fire_binding(b, vk_code);
    } else if (before && !after) {
      remap_release_binding(b, allowed[i]);
    } else if (before && after && allowed[i]) {
      remap_repeat_binding(b);
    }
  }

  bool swallow = false;
  for (int i = 0; i < g_remap_count; i++) {
    RemapBinding *b = &g_remaps[i];
    bool before = active_before[i] && allowed[i];
    bool after = active_after[i] && allowed[i];
    for (int k = 0; k < b->src_vk_count; k++)
      if (b->src_vks[k] == vk_code) {
        if (before || after)
          swallow = true;
      }
    if (b->src_mods & HOTKEY_MOD_CTRL &&
        (vk_code == VK_LCONTROL || vk_code == VK_RCONTROL ||
         vk_code == VK_CONTROL)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_ALT &&
        (vk_code == VK_LMENU || vk_code == VK_RMENU || vk_code == VK_MENU)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_SHIFT &&
        (vk_code == VK_LSHIFT || vk_code == VK_RSHIFT || vk_code == VK_SHIFT)) {
      if (before || after)
        swallow = true;
    }
    if (b->src_mods & HOTKEY_MOD_SUPER &&
        (vk_code == VK_LWIN || vk_code == VK_RWIN)) {
      if (before || after)
        swallow = true;
    }
    if (after && !before) {
      if (b->src_vk_count > 0 && b->src_vks[b->src_vk_count - 1] == vk_code)
        swallow = true;
    }
  }
  return swallow;
}

bool remap_should_swallow_vk(int vk_code, int value) {
  return remap_handle_vk(vk_code, value);
}
