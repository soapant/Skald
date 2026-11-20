#include "core/scroll_area.h"

#include <stdio.h>
#include <string.h>

#include "core/context.h"
#include "core/window_manager.h"
#include "helpers/helpers_ui.h"
#include "platform/platform.h"

#define SCROLL_AREA_MAX 32
#define SCROLL_DRAW_STACK_MAX 32
#define SCROLL_STALE_LIMIT 256
#define SCROLL_DEFAULT_WHEEL_PX 48

typedef struct {
  bool used;
  char name[128];
  int *off_x;
  int *off_y;
  int local_off_x;
  int local_off_y;
  int root_x;
  int root_y;
  int w;
  int h;
  int content_w;
  int content_h;
  int wheel_px;
  char owner[OWNER_PATH_MAX];
  unsigned long seq;
} ScrollSlot;

typedef struct {
  int slot;
  int base_off_x;
  int base_off_y;
} ScrollDrawFrame;

static ScrollSlot g_slots[SCROLL_AREA_MAX];
static ScrollDrawFrame g_draw_stack[SCROLL_DRAW_STACK_MAX];
static int g_draw_depth = 0;
static unsigned long g_seq = 0;
static void *g_scroll_mutex = NULL;

static void scroll_lock_init(void) {
  if (!g_scroll_mutex)
    g_scroll_mutex = Platform_MutexCreate();
}

#define SCROLL_LOCK()                                                          \
  do {                                                                         \
    scroll_lock_init();                                                        \
    Platform_MutexLock(g_scroll_mutex);                                        \
  } while (0)
#define SCROLL_UNLOCK() Platform_MutexUnlock(g_scroll_mutex)

static int clamp_scroll(int value, int content, int view) {
  int max = content - view;
  if (max < 0)
    max = 0;
  if (value < 0)
    return 0;
  if (value > max)
    return max;
  return value;
}

static void scroll_purge_stale(void) {
  unsigned long cutoff =
      g_seq > SCROLL_STALE_LIMIT ? g_seq - SCROLL_STALE_LIMIT : 0;
  for (int i = 0; i < SCROLL_AREA_MAX; i++) {
    if (g_slots[i].used && g_slots[i].seq < cutoff)
      memset(&g_slots[i], 0, sizeof(g_slots[i]));
  }
}

static int scroll_slot_find(const ScrollAreaSpec *spec) {
  for (int i = 0; i < SCROLL_AREA_MAX; i++) {
    ScrollSlot *slot = &g_slots[i];
    if (!slot->used)
      continue;
    if (spec->name && spec->name[0]) {
      if (strcmp(slot->name, spec->name) == 0)
        return i;
    } else if (slot->off_x == spec->off_x && slot->off_y == spec->off_y) {
      return i;
    }
  }
  return -1;
}

static int scroll_slot_alloc(void) {
  for (int i = 0; i < SCROLL_AREA_MAX; i++) {
    if (!g_slots[i].used)
      return i;
  }
  int oldest = 0;
  unsigned long oldest_seq = g_slots[0].seq;
  for (int i = 1; i < SCROLL_AREA_MAX; i++) {
    if (g_slots[i].seq < oldest_seq) {
      oldest = i;
      oldest_seq = g_slots[i].seq;
    }
  }
  return oldest;
}

static void scroll_area_window_origin(int *out_x, int *out_y) {
  SkaldContext *ctx = SkaldContext_Get();
  if (ctx && Platform_WindowToRoot(ctx->window.hPopupWnd, out_x, out_y))
    return;
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && Platform_WindowToRoot(top->handle, out_x, out_y))
    return;
  *out_x = 0;
  *out_y = 0;
}

bool scroll_area_begin(const ScrollAreaSpec *spec) {
  if (!spec || spec->w <= 0 || spec->h <= 0)
    return false;
  if (g_draw_depth >= SCROLL_DRAW_STACK_MAX)
    return false;

  int base_off_x = 0, base_off_y = 0;
  get_draw_offset(&base_off_x, &base_off_y);
  int origin_x = 0, origin_y = 0;
  scroll_area_window_origin(&origin_x, &origin_y);

  int off_x = spec->off_x ? *spec->off_x : 0;
  int off_y = spec->off_y ? *spec->off_y : 0;
  off_x = clamp_scroll(off_x, spec->content_w, spec->w);
  off_y = clamp_scroll(off_y, spec->content_h, spec->h);
  if (spec->off_x)
    *spec->off_x = off_x;
  if (spec->off_y)
    *spec->off_y = off_y;

  SCROLL_LOCK();
  scroll_purge_stale();
  int index = scroll_slot_find(spec);
  if (index < 0)
    index = scroll_slot_alloc();
  ScrollSlot *slot = &g_slots[index];

  memset(slot, 0, sizeof(*slot));
  slot->used = true;
  if (spec->name && spec->name[0])
    snprintf(slot->name, sizeof(slot->name), "%s", spec->name);
  slot->local_off_x = off_x;
  slot->local_off_y = off_y;
  slot->off_x = spec->off_x ? spec->off_x : &slot->local_off_x;
  slot->off_y = spec->off_y ? spec->off_y : &slot->local_off_y;
  slot->root_x = spec->x + base_off_x + origin_x;
  slot->root_y = spec->y + base_off_y + origin_y;
  slot->w = spec->w;
  slot->h = spec->h;
  slot->content_w = spec->content_w;
  slot->content_h = spec->content_h;
  slot->wheel_px =
      spec->wheel_px > 0 ? spec->wheel_px : SCROLL_DEFAULT_WHEEL_PX;
  if (spec->owner)
    snprintf(slot->owner, sizeof(slot->owner), "%s", spec->owner);
  slot->seq = ++g_seq;

  g_draw_stack[g_draw_depth].slot = index;
  g_draw_stack[g_draw_depth].base_off_x = base_off_x;
  g_draw_stack[g_draw_depth].base_off_y = base_off_y;
  g_draw_depth++;
  SCROLL_UNLOCK();

  set_draw_offset(base_off_x - off_x, base_off_y - off_y);
  return true;
}

void scroll_area_end(void) {
  int base_off_x = 0, base_off_y = 0;
  bool restore = false;
  SCROLL_LOCK();
  if (g_draw_depth > 0) {
    g_draw_depth--;
    base_off_x = g_draw_stack[g_draw_depth].base_off_x;
    base_off_y = g_draw_stack[g_draw_depth].base_off_y;
    g_draw_stack[g_draw_depth].slot = -1;
    restore = true;
  }
  SCROLL_UNLOCK();
  if (restore)
    set_draw_offset(base_off_x, base_off_y);
}

bool scroll_area_dispatch_wheel(int root_x, int root_y, int dx, int dy) {
  if (dx == 0 && dy == 0)
    return false;

  bool consumed = false;
  SCROLL_LOCK();
  int best = -1;
  unsigned long best_seq = 0;
  for (int i = 0; i < SCROLL_AREA_MAX; i++) {
    ScrollSlot *slot = &g_slots[i];
    if (!slot->used || slot->seq <= best_seq)
      continue;
    if (root_x < slot->root_x || root_y < slot->root_y)
      continue;
    if (root_x >= slot->root_x + slot->w || root_y >= slot->root_y + slot->h)
      continue;
    best = i;
    best_seq = slot->seq;
  }
  if (best >= 0) {
    ScrollSlot *slot = &g_slots[best];
    int off_x = slot->off_x ? *slot->off_x : 0;
    int off_y = slot->off_y ? *slot->off_y : 0;
    off_x = clamp_scroll(off_x + dx * slot->wheel_px, slot->content_w, slot->w);
    off_y = clamp_scroll(off_y + dy * slot->wheel_px, slot->content_h, slot->h);
    if (slot->off_x)
      *slot->off_x = off_x;
    if (slot->off_y)
      *slot->off_y = off_y;
    consumed = true;
  }
  SCROLL_UNLOCK();
  return consumed;
}

void scroll_area_purge_owner(const char *path_a, const char *path_b) {
  SCROLL_LOCK();
  for (int i = 0; i < SCROLL_AREA_MAX; i++) {
    ScrollSlot *slot = &g_slots[i];
    if (!slot->used)
      continue;
    if (owner_module_matches(slot->owner, path_a, path_b))
      memset(slot, 0, sizeof(*slot));
  }
  SCROLL_UNLOCK();
}
