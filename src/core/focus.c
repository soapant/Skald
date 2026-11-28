#include "core/focus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deps/log.h"
#include "deps/stb_ds.h"

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

typedef struct {
  FocusEntry *entries;
  int current;
  char saved_name[FOCUS_NAME_MAX];
  FocusKind saved_kind;
  bool saved_seen;
} FocusRegistry;

static FocusRegistry g_focus;

#define s_entries g_focus.entries
#define s_count (int)arrlenu(g_focus.entries)
#define s_current g_focus.current
#define s_saved_name g_focus.saved_name
#define s_saved_kind g_focus.saved_kind
#define s_saved_seen g_focus.saved_seen

static bool focus_is_widget_kind(FocusKind kind) {
  return kind == FOCUS_KIND_BUTTON || kind == FOCUS_KIND_CHECKBOX ||
         kind == FOCUS_KIND_SLIDER || kind == FOCUS_KIND_CUSTOM;
}

static bool focus_is_selectable(FocusKind kind) {
  return focus_is_widget_kind(kind) || kind == FOCUS_KIND_LISTBOX ||
         kind == FOCUS_KIND_EDITBOX;
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

void focus_frame_begin(FocusFrameBegin *argspec) {
  arrsetlen(s_entries, 0);
  s_current = -1;
  s_saved_seen = false;
  if (argspec->include_listbox) {
    focus_register(&(FocusRegister){.name = FOCUS_LISTBOX_NAME,
                                    .kind = FOCUS_KIND_LISTBOX,
                                    .x = argspec->lb_x,
                                    .y = argspec->lb_y,
                                    .w = argspec->lb_w,
                                    .h = argspec->lb_h,
                                    .owner = NULL,
                                    .activate = NULL,
                                    .adjust = NULL});
    if (!s_saved_name[0] && s_saved_kind != FOCUS_KIND_EDITBOX)
      focus_select(FOCUS_LISTBOX_NAME);
  }
}

static int focus_entry_cmp(const void *a, const void *b) {
  const FocusEntry *ea = (const FocusEntry *)a;
  const FocusEntry *eb = (const FocusEntry *)b;
  if (ea->y != eb->y)
    return ea->y < eb->y ? -1 : 1;
  if (ea->x != eb->x)
    return ea->x < eb->x ? -1 : 1;
  return strcmp(ea->name, eb->name);
}

void focus_frame_end(void) {
  if (focus_is_widget_kind(s_saved_kind) && !s_saved_seen) {
    log_warn("focus: dropped stale widget '%s' (kind=%d)", s_saved_name,
             (int)s_saved_kind);
    focus_clear();
  }
  if (s_count > 1) {
    char selected[FOCUS_NAME_MAX];
    bool had = s_current >= 0;
    if (had)
      snprintf(selected, sizeof(selected), "%s", s_entries[s_current].name);
    qsort(s_entries, (size_t)s_count, sizeof(*s_entries), focus_entry_cmp);
    if (had)
      s_current = focus_find(selected);
  }
}

void focus_register(FocusRegister *argspec) {
  if (!argspec->name || !argspec->name[0])
    return;
  int idx = focus_find(argspec->name);
  FocusEntry *e;
  if (idx >= 0) {
    e = &s_entries[idx];
  } else {
    e = arraddnptr(s_entries, 1);
    idx = s_count - 1;
  }
  snprintf(e->name, sizeof(e->name), "%s", argspec->name);
  e->kind = argspec->kind;
  e->x = argspec->x;
  e->y = argspec->y;
  e->w = argspec->w;
  e->h = argspec->h;
  e->owner = argspec->owner;
  e->activate = argspec->activate;
  e->adjust = argspec->adjust;
  if (s_saved_name[0] && strcmp(s_saved_name, argspec->name) == 0 &&
      s_saved_kind == argspec->kind) {
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
  if (s_entries[idx].kind == FOCUS_KIND_EDITBOX && s_entries[idx].activate)
    s_entries[idx].activate(s_entries[idx].owner);
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
    long score = forward * 8 + lateral;
    if (s_entries[i].kind == FOCUS_KIND_LISTBOX &&
        focus_is_widget_kind(cur->kind))
      score += 1000000;
    if (best < 0 || score < best_score) {
      best = i;
      best_score = score;
    }
  }
  if (best < 0) {
    log_warn("focus: no entry direction=%d from '%s'", (int)dir, cur->name);
    return false;
  }
  log_info("focus: move direction=%d -> '%s'", (int)dir, s_entries[best].name);
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

void focus_set_pseudo_named(FocusKind kind, const char *name) {
  if (kind == FOCUS_KIND_LISTBOX && focus_find(FOCUS_LISTBOX_NAME) >= 0) {
    log_info("focus: select listbox ring entry");
    focus_select(FOCUS_LISTBOX_NAME);
    return;
  }
  int idx = name ? focus_find(name) : -1;
  s_saved_name[0] = '\0';
  if (name)
    snprintf(s_saved_name, sizeof(s_saved_name), "%s", name);
  s_saved_kind = kind;
  s_current = idx;
  s_saved_seen = idx >= 0;
  log_info("focus: set pseudo kind=%d name='%s' (idx=%d)", (int)kind,
           s_saved_name[0] ? s_saved_name : "-", idx);
}

void focus_set_pseudo(FocusKind kind) { focus_set_pseudo_named(kind, NULL); }

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
