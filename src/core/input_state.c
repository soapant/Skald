#include "helpers/helpers_input_state.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform/platform.h"
#include "core/types/keys.h"
#include "core/timer.h"
#include "helpers/helpers_owner.h"

static bool down_map[256] = {0};
static bool pressed[256] = {0};
static bool released[256] = {0};
static void (*key_cb)(int vk, bool down, int mods, void *ud) = NULL;
static void *key_ud = NULL;
static char cb_owner[OWNER_PATH_MAX] = {0};
static char char_queue[32] = {0};
static int char_head = 0, char_tail = 0;

#define SK_INHIBIT_MOD_COUNT 11
static const int kInhibitMods[SK_INHIBIT_MOD_COUNT] = {
    VK_SHIFT,  VK_CONTROL,  VK_MENU,     VK_LWIN,  VK_RWIN, VK_LSHIFT,
    VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU};

void InputState_InhibitMods(void) {
  for (int i = 0; i < SK_INHIBIT_MOD_COUNT; i++) {
    int nv = kInhibitMods[i];
    if (nv >= 0 && nv < 256)
      down_map[nv] = false;
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
  bool was = down_map[nv];
  if (down) {
    down_map[nv] = true;
    pressed[nv] = true;
    released[nv] = false;
    char ch = (char)char_hint;
    if (ch) {
      int nxt = (char_tail + 1) % 32;
      if (nxt != char_head) {
        char_queue[char_tail] = ch;
        char_tail = nxt;
      }
    }
  } else {
    if (was) {
      down_map[nv] = false;
      released[nv] = true;
    }
    if (!was)
      released[nv] = false;
  }
  if (key_cb)
    key_cb(nv, down, mods, key_ud);
}

bool Input_IsKeyDown(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  return down_map[vk];
}

bool Input_WasKeyPressed(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  bool v = pressed[vk];
  pressed[vk] = false;
  return v;
}

bool Input_WasKeyReleased(int vk) {
  vk = norm_vk(vk);
  if (vk < 0 || vk >= 256)
    return false;
  bool v = released[vk];
  released[vk] = false;
  return v;
}

void Input_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud),
                          void *ud) {
  key_cb = cb;
  key_ud = ud;
  cb_owner[0] = '\0';
  if (key_cb)
    Owner_ResolveForFunc((void *)key_cb, cb_owner, sizeof(cb_owner));
}

void KeyCallback_PurgeOwner(const char *path_a, const char *path_b) {
  if (key_cb && Owner_ModuleMatches(cb_owner, path_a, path_b)) {
    key_cb = NULL;
    key_ud = NULL;
    cb_owner[0] = '\0';
  }
}

void InputState_Clear(void) {
  for (int i = 0; i < 256; i++) {
    down_map[i] = false;
    pressed[i] = false;
    released[i] = false;
  }
  char_head = char_tail = 0;
}

void InputState_ClearQueue(void) { char_head = char_tail = 0; }
int Input_GetPressedKey(void) {
  for (int i = 0; i < 256; i++) {
    if (pressed[i]) {
      if (i >= 32 && i <= 126) {
        pressed[i] = false;
        continue;
      }
      pressed[i] = false;
      return i;
    }
  }
  return 0;
}
char Input_GetPressedChar(void) {
  if (char_head != char_tail) {
    char c = char_queue[char_head];
    char_head = (char_head + 1) % 32;
    return c;
  }
  return 0;
}

#define GLOBAL_KEY_RING 512

static PlatformKeyEvent gkeys[GLOBAL_KEY_RING];
static int gkey_head = 0, gkey_tail = 0;
static unsigned long gkey_dropped = 0;
static void *gkey_mutex = NULL;
static void gkey_lock_init(void) {
  if (!gkey_mutex)
    gkey_mutex = Platform_MutexCreate();
}
#define GKEY_LOCK()                                                            \
  do {                                                                         \
    gkey_lock_init();                                                          \
    Platform_MutexLock(gkey_mutex);                                            \
  } while (0)
#define GKEY_UNLOCK() Platform_MutexUnlock(gkey_mutex)

void GlobalKey_Clear(void) {
  GKEY_LOCK();
  gkey_head = gkey_tail = 0;
  gkey_dropped = 0;
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
  int nxt = (gkey_tail + 1) % GLOBAL_KEY_RING;
  if (nxt == gkey_head) {
    gkey_dropped++;
    GKEY_UNLOCK();
    return;
  }
  gkeys[gkey_tail] = stamped;
  gkey_tail = nxt;
  GKEY_UNLOCK();
}

bool GlobalKey_Pop(PlatformKeyEvent *out) {
  if (!out)
    return false;
  bool got = false;
  GKEY_LOCK();
  if (gkey_head != gkey_tail) {
    *out = gkeys[gkey_head];
    gkey_head = (gkey_head + 1) % GLOBAL_KEY_RING;
    got = true;
  }
  GKEY_UNLOCK();
  return got;
}

int GlobalKey_Pending(void) {
  GKEY_LOCK();
  int n = gkey_tail - gkey_head;
  if (n < 0)
    n += GLOBAL_KEY_RING;
  GKEY_UNLOCK();
  return n;
}

unsigned long GlobalKey_Dropped(void) {
  GKEY_LOCK();
  unsigned long d = gkey_dropped;
  GKEY_UNLOCK();
  return d;
}

#define GLOBAL_MOUSE_RING 256

static PlatformMouseEvent gmice[GLOBAL_MOUSE_RING];
static int gmouse_head = 0, gmouse_tail = 0;
static unsigned long gmouse_dropped = 0;
static bool mbtn_down[MOUSE_BTN_MAX] = {0};
static void *gmouse_mutex = NULL;
static void gmouse_lock_init(void) {
  if (!gmouse_mutex)
    gmouse_mutex = Platform_MutexCreate();
}
#define GMOUSE_LOCK()                                                          \
  do {                                                                         \
    gmouse_lock_init();                                                        \
    Platform_MutexLock(gmouse_mutex);                                          \
  } while (0)
#define GMOUSE_UNLOCK() Platform_MutexUnlock(gmouse_mutex)

int GlobalMouse_Full(void) {
  int next = (gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  return next == gmouse_head;
}

void GlobalMouse_Push(const PlatformMouseEvent *ev) {
  if (!ev)
    return;
  if (GlobalMouse_Full())
    return;
  PlatformMouseEvent stamped = *ev;
  stamped.t_us = input_now_us();
  GMOUSE_LOCK();
  int nxt = (gmouse_tail + 1) % GLOBAL_MOUSE_RING;
  if (nxt == gmouse_head) {
    gmouse_dropped++;
    GMOUSE_UNLOCK();
    return;
  }
  gmice[gmouse_tail] = stamped;
  gmouse_tail = nxt;
  GMOUSE_UNLOCK();
}

bool GlobalMouse_Pop(PlatformMouseEvent *out) {
  if (!out)
    return false;
  bool got = false;
  GMOUSE_LOCK();
  if (gmouse_head != gmouse_tail) {
    *out = gmice[gmouse_head];
    gmouse_head = (gmouse_head + 1) % GLOBAL_MOUSE_RING;
    got = true;
  }
  GMOUSE_UNLOCK();
  return got;
}

void GlobalMouse_Clear(void) {
  GMOUSE_LOCK();
  gmouse_head = gmouse_tail = 0;
  gmouse_dropped = 0;
  GMOUSE_UNLOCK();
}

int GlobalMouse_Pending(void) {
  GMOUSE_LOCK();
  int n = gmouse_tail - gmouse_head;
  if (n < 0)
    n += GLOBAL_MOUSE_RING;
  GMOUSE_UNLOCK();
  return n;
}

unsigned long GlobalMouse_Dropped(void) {
  GMOUSE_LOCK();
  unsigned long d = gmouse_dropped;
  GMOUSE_UNLOCK();
  return d;
}

void Input_UpdateMouseButton(int button, bool down) {
  if (button < 0 || button >= (int)(sizeof(mbtn_down) / sizeof(mbtn_down[0])))
    return;
  GMOUSE_LOCK();
  mbtn_down[button] = down;
  GMOUSE_UNLOCK();
}

bool Input_MouseStateDown(int button) {
  if (button < 0 || button >= (int)(sizeof(mbtn_down) / sizeof(mbtn_down[0])))
    return false;
  GMOUSE_LOCK();
  bool d = mbtn_down[button];
  GMOUSE_UNLOCK();
  return d;
}
