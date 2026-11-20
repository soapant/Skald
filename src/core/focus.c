#include "core/focus.h"

#include <stdio.h>
#include <string.h>

#include "deps/log.h"

#define FOCUS_MAX 1024
#define FOCUS_NAME_MAX 256
#define FOCUS_LISTBOX_NAME "__listbox__"

typedef struct {
  char name[FOCUS_NAME_MAX];
  FocusKind kind;
  int x, y, w, h;
  void *owner;
  FocusActivateFn activate;
  FocusAdjustFn adjust;
} FocusEntry;

static FocusEntry s_entries[FOCUS_MAX];
static int s_count = 0;
static int s_current = -1;

static char s_saved_name[FOCUS_NAME_MAX];
static FocusKind s_saved_kind = FOCUS_KIND_NONE;
static bool s_saved_seen = false;

static bool focus_is_widget_kind(FocusKind kind) {
  return kind == FOCUS_KIND_BUTTON || kind == FOCUS_KIND_CHECKBOX ||
         kind == FOCUS_KIND_SLIDER || kind == FOCUS_KIND_CUSTOM;
}

static bool focus_is_selectable(FocusKind kind) {
  return focus_is_widget_kind(kind) || kind == FOCUS_KIND_LISTBOX;
}

static int focus_find(const char *name) {
  if (!name || !name[0])
    return -1;
  for (int i = 0; i < s_count; i++) {
    if (strcmp(s_entries[i].name, name) == 0)
      return i;
  }
  return -1;
}

void focus_frame_begin(bool include_listbox, int lb_x, int lb_y, int lb_w,
                       int lb_h) {
  s_count = 0;
  s_current = -1;
  s_saved_seen = false;
  if (include_listbox) {
    focus_register(FOCUS_LISTBOX_NAME, FOCUS_KIND_LISTBOX, lb_x, lb_y, lb_w,
                   lb_h, NULL, NULL, NULL);
    if (!s_saved_name[0] && s_saved_kind != FOCUS_KIND_EDITBOX)
      focus_select(FOCUS_LISTBOX_NAME);
  }
}

void focus_frame_end(void) {
  if (focus_is_widget_kind(s_saved_kind) && !s_saved_seen) {
    log_warn("focus: dropped stale widget '%s' (kind=%d)", s_saved_name,
             (int)s_saved_kind);
    focus_clear();
  }
}

void focus_register(const char *name, FocusKind kind, int x, int y, int w,
                    int h, void *owner, FocusActivateFn activate,
                    FocusAdjustFn adjust) {
  if (!name || !name[0] || s_count >= FOCUS_MAX)
    return;
  int idx = focus_find(name);
  if (idx >= 0) {
    s_current = idx;
    return;
  }
  FocusEntry *e = &s_entries[s_count];
  snprintf(e->name, sizeof(e->name), "%s", name);
  e->kind = kind;
  e->x = x;
  e->y = y;
  e->w = w;
  e->h = h;
  e->owner = owner;
  e->activate = activate;
  e->adjust = adjust;
  idx = s_count++;
  if (s_saved_name[0] && strcmp(s_saved_name, name) == 0 &&
      s_saved_kind == kind) {
    s_current = idx;
    s_saved_seen = true;
  }
}

bool focus_is(const char *name) {
  if (s_current < 0 || !name)
    return false;
  return strcmp(s_entries[s_current].name, name) == 0;
}

bool focus_has_entries(void) { return s_count > 0; }

FocusKind focus_kind(void) {
  if (s_current >= 0)
    return s_entries[s_current].kind;
  return s_saved_kind;
}

const char *focus_name(void) {
  if (s_current >= 0)
    return s_entries[s_current].name;
  return s_saved_name[0] ? s_saved_name : NULL;
}

bool focus_select(const char *name) {
  int idx = focus_find(name);
  if (idx < 0) {
    log_warn("focus: select failed for '%s'", name ? name : "(null)");
    return false;
  }
  s_current = idx;
  snprintf(s_saved_name, sizeof(s_saved_name), "%s", s_entries[idx].name);
  s_saved_kind = s_entries[idx].kind;
  s_saved_seen = true;
  log_info("focus: select '%s' (kind=%d)", s_saved_name, (int)s_saved_kind);
  return true;
}

bool focus_set_kind(FocusKind kind) {
  if (kind == FOCUS_KIND_NONE) {
    focus_clear();
    return true;
  }
  for (int i = 0; i < s_count; i++) {
    if (s_entries[i].kind == kind && focus_is_selectable(s_entries[i].kind))
      return focus_select(s_entries[i].name);
  }
  focus_set_pseudo(kind);
  return true;
}

bool focus_move(int dir) {
  if (s_count <= 0) {
    log_warn("focus: move dir=%d with no registered widgets", dir);
    return false;
  }
  int start = s_current >= 0 ? s_current : (dir > 0 ? s_count - 1 : 0);
  for (int step = 1; step <= s_count; step++) {
    int idx = (start + dir * step) % s_count;
    if (idx < 0)
      idx += s_count;
    if (focus_is_selectable(s_entries[idx].kind)) {
      log_info("focus: move dir=%d -> '%s'", dir, s_entries[idx].name);
      return focus_select(s_entries[idx].name);
    }
  }
  log_warn("focus: move dir=%d found no widget entries", dir);
  return false;
}

bool focus_move_direction(FocusDirection dir) {
  if (s_current < 0 || s_count <= 0) {
    log_warn("focus: move direction=%d with no selection", (int)dir);
    return false;
  }
  FocusEntry *cur = &s_entries[s_current];
  int cx = cur->x + cur->w / 2;
  int cy = cur->y + cur->h / 2;
  int best = -1;
  long best_score = 0;
  for (int i = 0; i < s_count; i++) {
    if (i == s_current || !focus_is_selectable(s_entries[i].kind))
      continue;
    FocusEntry *cand = &s_entries[i];
    int nx = cand->x + cand->w / 2;
    int ny = cand->y + cand->h / 2;
    long forward = 0, lateral = 0;
    if (dir == FOCUS_RIGHT) {
      forward = nx - cx;
      lateral = ny - cy;
    } else if (dir == FOCUS_LEFT) {
      forward = cx - nx;
      lateral = cy - ny;
    } else if (dir == FOCUS_DOWN) {
      forward = ny - cy;
      lateral = nx - cx;
    } else {
      forward = cy - ny;
      lateral = cx - nx;
    }
    if (forward <= 0)
      continue;
    if (lateral < 0)
      lateral = -lateral;
    long score = lateral * 8 + forward;
    if (best < 0 || score < best_score) {
      best = i;
      best_score = score;
    }
  }
  if (best < 0) {
    log_warn("focus: no entry direction=%d from '%s'", (int)dir, cur->name);
    return false;
  }
  log_info("focus: move direction=%d -> '%s'", (int)dir,
           s_entries[best].name);
  return focus_select(s_entries[best].name);
}

bool focus_activate(void) {
  if (s_current < 0) {
    log_warn("focus: activate with no selection");
    return false;
  }
  FocusEntry *e = &s_entries[s_current];
  if (!e->activate) {
    log_error("focus: '%s' has no activate callback", e->name);
    return false;
  }
  log_info("focus: activate '%s'", e->name);
  e->activate(e->owner);
  return true;
}

bool focus_adjust(int dir) {
  if (s_current < 0) {
    log_warn("focus: adjust dir=%d with no selection", dir);
    return false;
  }
  FocusEntry *e = &s_entries[s_current];
  if (!e->adjust) {
    log_error("focus: '%s' has no adjust callback", e->name);
    return false;
  }
  log_info("focus: adjust '%s' dir=%d", e->name, dir);
  e->adjust(e->owner, dir);
  return true;
}

void focus_set_pseudo(FocusKind kind) {
  if (kind == FOCUS_KIND_LISTBOX && focus_find(FOCUS_LISTBOX_NAME) >= 0) {
    log_info("focus: select listbox ring entry");
    focus_select(FOCUS_LISTBOX_NAME);
    return;
  }
  log_info("focus: set pseudo kind=%d (was '%s' kind=%d)", (int)kind,
           s_saved_name[0] ? s_saved_name : "-", (int)s_saved_kind);
  s_current = -1;
  s_saved_name[0] = '\0';
  s_saved_kind = kind;
  s_saved_seen = false;
}

void focus_set_pseudo_passive(FocusKind kind) {
  if (kind == FOCUS_KIND_LISTBOX && focus_find(FOCUS_LISTBOX_NAME) >= 0) {
    focus_select(FOCUS_LISTBOX_NAME);
    return;
  }
  focus_set_pseudo(kind);
}

void focus_clear(void) {
  log_info("focus: clear (was '%s' kind=%d)",
           s_saved_name[0] ? s_saved_name : "-", (int)s_saved_kind);
  s_current = -1;
  s_saved_name[0] = '\0';
  s_saved_kind = FOCUS_KIND_NONE;
  s_saved_seen = false;
}
