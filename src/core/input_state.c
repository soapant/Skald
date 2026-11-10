#include "helpers/helpers_input_state.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform/platform.h"
#include "core/types/keys.h"
#include "core/timer.h"
#include "helpers/helpers_owner.h"

static bool g_down[256] = {0};
static bool g_pressed[256] = {0};
static bool g_released[256] = {0};
static void (*g_cb)(int vk, bool down, int mods, void *ud) = NULL;
static void *g_ud = NULL;
static char g_cb_owner[OWNER_PATH_MAX] = {0};
static char g_char_queue[32] = {0};
static int g_char_head = 0, g_char_tail = 0;

#define SK_INHIBIT_MOD_COUNT 11
static const int kInhibitMods[SK_INHIBIT_MOD_COUNT] = {
    VK_SHIFT,  VK_CONTROL,  VK_MENU,     VK_LWIN,  VK_RWIN, VK_LSHIFT,
    VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU};

void InputState_InhibitMods(void) {
  for (int i = 0; i < SK_INHIBIT_MOD_COUNT; i++) {
    int nv = kInhibitMods[i];
    if (nv >= 0 && nv < 256)
      g_down[nv] = false;
  }
}

static int norm_vk(int vk) {
  if (vk >= 'a' && vk <= 'z')
    return vk - 'a' + 'A';
  return vk;
}
void InputState_Update(int vk, bool down, int mods, int char_hint) {
  int nv = norm_vk(vk);
  if (nv < 0 || nv >= 256)
    return;
  bool was = g_down[nv];
  if (down) {
    g_down[nv] = true;
    g_pressed[nv] = true;
    g_released[nv] = false;
    char ch = (char)char_hint;
    if (ch) {
      int nxt = (g_char_tail + 1) % 32;
      if (nxt != g_char_head) {
        g_char_queue[g_char_tail] = ch;
        g_char_tail = nxt;
      }
    }
  } else {
    if (was) {
      g_down[nv] = false;
      g_released[nv] = true;
    }
    if (!was)
      g_released[nv] = false;
  }
  if (g_cb)
    g_cb(nv, down, mods, g_ud);
}

bool Input_IsKeyDown(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  return g_down[vk];
}

bool Input_WasKeyPressed(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  bool v = g_pressed[vk];
  g_pressed[vk] = false;
  return v;
}

bool Input_WasKeyReleased(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  bool v = g_released[vk];
  g_released[vk] = false;
  return v;
}

void Input_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud),
                             void *ud) {
  g_cb = cb;
  g_ud = ud;
  g_cb_owner[0] = '\0';
  if (cb)
    Owner_ResolveForFunc((void *)cb, g_cb_owner, sizeof(g_cb_owner));
}

void KeyCallback_PurgeOwner(const char *path_a, const char *path_b) {
  if (g_cb && Owner_ModuleMatches(g_cb_owner, path_a, path_b)) {
    g_cb = NULL;
    g_ud = NULL;
    g_cb_owner[0] = '\0';
  }
}

void InputState_Clear(void) {
  for (int i = 0; i < 256; i++) {
    g_down[i] = false;
    g_pressed[i] = false;
    g_released[i] = false;
  }
  g_char_head = g_char_tail = 0;
}

void InputState_ClearQueue(void) { g_char_head = g_char_tail = 0; }
int Input_GetPressedKey(void) {
  for (int i = 0; i < 256; i++) {
    if (g_pressed[i]) {
      if (i >= 32 && i <= 126) {
        g_pressed[i] = false;
        continue;
      }
      g_pressed[i] = false;
      return i;
    }
  }
  return 0;
}
char Input_GetPressedChar(void) {
  if (g_char_head != g_char_tail) {
    char c = g_char_queue[g_char_head];
    g_char_head = (g_char_head + 1) % 32;
    return c;
  }
  return 0;
}

#define GLOBAL_KEY_RING 512

static PlatformKeyEvent g_gkeys[GLOBAL_KEY_RING];
static int g_gkey_head = 0, g_gkey_tail = 0;
static unsigned long g_gkey_dropped = 0;
static void *g_gkey_mutex = NULL;
static void gkey_lock_init(void) {
  if (!g_gkey_mutex)
    g_gkey_mutex = Platform_MutexCreate();
}
#define GKEY_LOCK()                                                            \
  do {                                                                         \
    gkey_lock_init();                                                          \
    Platform_MutexLock(g_gkey_mutex);                                          \
  } while (0)
#define GKEY_UNLOCK() Platform_MutexUnlock(g_gkey_mutex)

void GlobalKey_Clear(void) {
  GKEY_LOCK();
  g_gkey_head = g_gkey_tail = 0;
  g_gkey_dropped = 0;
  GKEY_UNLOCK();
}

static unsigned long long input_now_us(void) {
  int64_t ticks = TimerNowTicks();
  int64_t freq = TimerTickFrequency();
  if (freq <= 0)
    freq = 1;
  return (unsigned long long)((ticks * 1000000LL) / freq);
}

void GlobalKey_Push(const PlatformKeyEvent *ev) {
  if (!ev)
    return;
  PlatformKeyEvent stamped = *ev;
  stamped.t_us = input_now_us();
  GKEY_LOCK();
  int nxt = (g_gkey_tail + 1) % GLOBAL_KEY_RING;
  if (nxt == g_gkey_head) {
    g_gkey_dropped++;
    GKEY_UNLOCK();
    return;
  }
  g_gkeys[g_gkey_tail] = stamped;
  g_gkey_tail = nxt;
  GKEY_UNLOCK();
}

bool GlobalKey_Pop(PlatformKeyEvent *out) {
  if (!out)
    return false;
  bool got = false;
  GKEY_LOCK();
  if (g_gkey_head != g_gkey_tail) {
    *out = g_gkeys[g_gkey_head];
    g_gkey_head = (g_gkey_head + 1) % GLOBAL_KEY_RING;
    got = true;
  }
  GKEY_UNLOCK();
  return got;
}

int GlobalKey_Pending(void) {
  GKEY_LOCK();
  int n = g_gkey_tail - g_gkey_head;
  if (n < 0)
    n += GLOBAL_KEY_RING;
  GKEY_UNLOCK();
  return n;
}

unsigned long GlobalKey_Dropped(void) {
  GKEY_LOCK();
  unsigned long d = g_gkey_dropped;
  GKEY_UNLOCK();
  return d;
}

#define GLOBAL_MOUSE_RING 256

static PlatformMouseEvent g_gmice[GLOBAL_MOUSE_RING];
static int g_gmouse_head = 0, g_gmouse_tail = 0;
static unsigned long g_gmouse_dropped = 0;
static bool g_mbtn_down[MOUSE_BTN_MAX] = {0};
static void *g_gmouse_mutex = NULL;
static void gmouse_lock_init(void) {
  if (!g_gmouse_mutex)
    g_gmouse_mutex = Platform_MutexCreate();
}
#define GMOUSE_LOCK()                                                          \
  do {                                                                         \
    gmouse_lock_init();                                                        \
    Platform_MutexLock(g_gmouse_mutex);                                        \
  } while (0)
#define GMOUSE_UNLOCK() Platform_MutexUnlock(g_gmouse_mutex)

int GlobalMouse_Full(void) {
  int next = (g_gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  return next == g_gmouse_head;
}

void GlobalMouse_Push(const PlatformMouseEvent *ev) {
  if (!ev)
    return;
  if (GlobalMouse_Full())
    return;
  PlatformMouseEvent stamped = *ev;
  stamped.t_us = input_now_us();
  GMOUSE_LOCK();
  int nxt = (g_gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  if (nxt == g_gmouse_head) {
    g_gmouse_dropped++;
    GMOUSE_UNLOCK();
    return;
  }
  g_gmice[g_gmouse_tail] = stamped;
  g_gmouse_tail = nxt;
  GMOUSE_UNLOCK();
}

bool GlobalMouse_Pop(PlatformMouseEvent *out) {
  if (!out)
    return false;
  bool got = false;
  GMOUSE_LOCK();
  if (g_gmouse_head != g_gmouse_tail) {
    *out = g_gmice[g_gmouse_head];
    g_gmouse_head = (g_gmouse_head + 1) % GLOBAL_MOUSE_RING;
    got = true;
  }
  GMOUSE_UNLOCK();
  return got;
}

void GlobalMouse_Clear(void) {
  GMOUSE_LOCK();
  g_gmouse_head = g_gmouse_tail = 0;
  g_gmouse_dropped = 0;
  GMOUSE_UNLOCK();
}

int GlobalMouse_Pending(void) {
  GMOUSE_LOCK();
  int n = g_gmouse_tail - g_gmouse_head;
  if (n < 0)
    n += GLOBAL_MOUSE_RING;
  GMOUSE_UNLOCK();
  return n;
}

unsigned long GlobalMouse_Dropped(void) {
  GMOUSE_LOCK();
  unsigned long d = g_gmouse_dropped;
  GMOUSE_UNLOCK();
  return d;
}

void Input_UpdateMouseButton(int button, bool down) {
  if (button < 0 ||
      button >= (int)(sizeof(g_mbtn_down) / sizeof(g_mbtn_down[0])))
    return;
  GMOUSE_LOCK();
  g_mbtn_down[button] = down;
  GMOUSE_UNLOCK();
}

bool Input_MouseStateDown(int button) {
  if (button < 0 ||
      button >= (int)(sizeof(g_mbtn_down) / sizeof(g_mbtn_down[0])))
    return false;
  GMOUSE_LOCK();
  bool d = g_mbtn_down[button];
  GMOUSE_UNLOCK();
  return d;
}
