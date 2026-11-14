#include "core/skapi.h"
#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_MAX_BINDINGS 64
#define CC_HOTKEY_FILE "hotkeys.txt"
#define CC_APPCACHE_FILE "app_launcher_cache.txt"
#define CC_APPCACHE_VERSION "#cc5"

#define CC_ITEM_TEXT_X 10
#define CC_ITEM_TEXT_Y 4
#define CC_ICON_GAP 8
#define CC_TYPE_GAP 6
#define CC_TYPE_SIZE_DELTA 2
#define CC_HOTKEY_FONT_DELTA 6
#define CC_TYPE_LABEL "Command"
#define CC_CALC_ROW_ICON "/calculator.png"
#define CC_TYPE_COLOR SK_GRAY

static size_t cc_copy(char *dst, size_t size, const char *src) {
  if (size == 0)
    return 0;
  const char *s = src ? src : "";
  size_t n = strlen(s);
  if (n >= size)
    n = size - 1;
  memcpy(dst, s, n);
  dst[n] = '\0';
  return n;
}

static void cc_trim_copy(const char *src, char *dst, size_t cap) {
  if (!dst || cap == 0)
    return;
  if (!src)
    src = "";
  while (*src && isspace((unsigned char)*src))
    src++;
  size_t len = strlen(src);
  while (len > 0 && isspace((unsigned char)src[len - 1]))
    len--;
  if (len >= cap)
    len = cap - 1;
  memmove(dst, src, len);
  dst[len] = '\0';
}

static int cc_lower(char c) { return tolower((unsigned char)c); }

static char *cc_strdup(const char *s) {
  if (!s)
    s = "";
  size_t n = strlen(s) + 1;
  char *copy = (char *)malloc(n);
  if (copy)
    memcpy(copy, s, n);
  return copy;
}

typedef bool (*CcHandler)(void);

typedef struct {
  char label[256];
  char type[64];
  const char *payload;
  char icon[MAX_PATH];
  CcHandler on_enter;
  CcHandler on_ctrl_enter;
  CcHandler on_shift_enter;
  CcHandler on_ctrl_shift_enter;
  CcHandler on_ctrl_alt_enter;
  CcHandler on_shift_alt_enter;
  bool (*on_tab)(void);
  void (*on_action)(void);
} CcEntry;

typedef enum {
  CC_RUN_ENTER,
  CC_RUN_CTRL,
  CC_RUN_SHIFT,
  CC_RUN_CTRL_SHIFT,
  CC_RUN_CTRL_ALT,
  CC_RUN_SHIFT_ALT,
} CcRunMode;

static CcEntry *cc_entries;
static int cc_entry_count;
static int cc_entry_cap;
static int cc_entry_base;

static atomic_bool cc_entries_dirty = true;
static atomic_bool cc_stopping;

static void cc_fill_applications(int limit);
static void cc_register_pending_bindings(void);

static CcEntry *cc_entry_new(void) {
  if (cc_entry_count >= cc_entry_cap) {
    int cap = cc_entry_cap ? cc_entry_cap * 2 : 128;
    CcEntry *grown =
        (CcEntry *)realloc(cc_entries, (size_t)cap * sizeof(CcEntry));
    if (!grown)
      return NULL;
    cc_entries = grown;
    cc_entry_cap = cap;
  }
  CcEntry *e = &cc_entries[cc_entry_count++];
  memset(e, 0, sizeof(*e));
  return e;
}

static CcEntry *cc_entry_add(const char *label, const char *type,
                             const char *payload, const char *icon) {
  if (!label || !label[0] || cc_entry_count >= MAX_VISIBLE_ITEMS)
    return NULL;
  CcEntry *e = cc_entry_new();
  if (!e)
    return NULL;
  cc_copy(e->label, sizeof(e->label), label);
  cc_copy(e->type, sizeof(e->type), type);
  e->payload = payload;
  cc_copy(e->icon, sizeof(e->icon), icon);
  return e;
}

static CcEntry *cc_add_fallback(void) {
  CcEntry *e = cc_entry_new();
  if (!e)
    return NULL;
  snprintf(e->label, sizeof(e->label), "%s Search", UsedSearchEngine);
  e->label[0] = (char)toupper((unsigned char)e->label[0]);
  snprintf(e->icon, sizeof(e->icon), "/%s.png", UsedSearchEngine);
  return e;
}

static void cc_fill_feature_entries(void) {
  for (int i = 0; i < sk_cc_entry_count(); i++) {
    const SkCcEntrySpec *s = sk_cc_entry_at(i);
    if (!s || !s->display_name || !s->display_name[0])
      continue;
    CcEntry *e = cc_entry_add(s->display_name, s->type, NULL, s->icon);
    if (!e)
      continue;
    e->on_enter = s->on_enter;
    e->on_ctrl_enter = s->on_ctrl_enter;
    e->on_shift_enter = s->on_shift_enter;
    e->on_ctrl_shift_enter = s->on_ctrl_shift_enter;
    e->on_ctrl_alt_enter = s->on_ctrl_alt_enter;
    e->on_shift_alt_enter = s->on_shift_alt_enter;
    e->on_tab = s->on_tab;
    e->on_action = s->on_action;
  }
}

static void cc_build(void) {
  atomic_store(&cc_entries_dirty, false);
  cc_entry_count = 0;

  cc_fill_applications(MAX_VISIBLE_ITEMS - sk_cc_entry_count());
  cc_fill_feature_entries();
  cc_entry_base = cc_entry_count;
  cc_register_pending_bindings();
}

static const CcEntry *cc_find(const char *label) {
  if (!label || !label[0])
    return NULL;
  for (int i = 0; i < cc_entry_count; i++)
    if (strcmp(cc_entries[i].label, label) == 0)
      return &cc_entries[i];
  return NULL;
}

static void (*cc_pending_action)(void);
static CcHandler cc_pending_fn;
static char cc_pending_label[256];
static char cc_pending_payload[MAX_PATH];
static const char *cc_staged_label;
static const char *cc_staged_payload;

static CcHandler cc_pick_handler(const CcEntry *e, CcRunMode mode) {
  switch (mode) {
  case CC_RUN_CTRL:
    return e->on_ctrl_enter;
  case CC_RUN_SHIFT:
    return e->on_shift_enter ? e->on_shift_enter : e->on_enter;
  case CC_RUN_CTRL_SHIFT:
    return e->on_ctrl_shift_enter ? e->on_ctrl_shift_enter : e->on_ctrl_enter;
  case CC_RUN_CTRL_ALT:
    return e->on_ctrl_alt_enter ? e->on_ctrl_alt_enter : e->on_ctrl_enter;
  case CC_RUN_SHIFT_ALT:
    return e->on_shift_alt_enter ? e->on_shift_alt_enter : e->on_enter;
  case CC_RUN_ENTER:
  default:
    return e->on_enter;
  }
}

static void cc_stage(const CcEntry *e, CcRunMode mode) {
  cc_pending_action = NULL;
  cc_pending_fn = NULL;
  cc_pending_label[0] = '\0';
  cc_pending_payload[0] = '\0';
  if (!e)
    return;
  if (mode == CC_RUN_ENTER && e->on_action)
    cc_pending_action = e->on_action;
  else
    cc_pending_fn = cc_pick_handler(e, mode);
  if (cc_pending_action || cc_pending_fn) {
    cc_copy(cc_pending_label, sizeof(cc_pending_label), e->label);
    cc_copy(cc_pending_payload, sizeof(cc_pending_payload), e->payload);
  }
}

static void cc_run_pending(void) {
  void (*action)(void) = cc_pending_action;
  CcHandler fn = cc_pending_fn;
  cc_pending_action = NULL;
  cc_pending_fn = NULL;
  cc_staged_label = cc_pending_label;
  cc_staged_payload = cc_pending_payload;
  if (action)
    action();
  else if (fn)
    fn();
  cc_staged_label = NULL;
  cc_staged_payload = NULL;
}

static void cc_run_entry_by_label(const char *label) {
  cc_stage(cc_find(label), CC_RUN_ENTER);
  cc_run_pending();
}

typedef struct {
  char hotkey[64];
  char action[256];
  bool registered;
} CcBinding;

static CcBinding cc_bindings[CC_MAX_BINDINGS];
static int cc_binding_count;

static bool cc_binding_used(int i) { return cc_bindings[i].action[0] != '\0'; }

static void cc_binding_cb(void *ud) {
  int i = (int)(uintptr_t)ud - 1;
  if (i >= 0 && i < cc_binding_count && cc_binding_used(i))
    cc_run_entry_by_label(cc_bindings[i].action);
}

static void cc_binding_register(int i) {
  sk_register_hotkey(.hotkey = cc_bindings[i].hotkey, .cb_ud = cc_binding_cb,
                     .ud = (void *)(uintptr_t)(i + 1));
  cc_bindings[i].registered = true;
}

static void cc_binding_unregister(int i) {
  if (cc_bindings[i].registered)
    sk_unregister_hotkey(cc_bindings[i].hotkey);
  cc_bindings[i].registered = false;
}

static void cc_binding_release(int i) {
  cc_binding_unregister(i);
  memset(&cc_bindings[i], 0, sizeof(cc_bindings[i]));
}

static int cc_binding_find(const char *action) {
  for (int i = 0; i < cc_binding_count; i++)
    if (cc_binding_used(i) && strcmp(cc_bindings[i].action, action) == 0)
      return i;
  return -1;
}

static int cc_binding_find_hotkey(const char *hotkey) {
  for (int i = 0; i < cc_binding_count; i++)
    if (cc_binding_used(i) && sk_string_equals_ci(cc_bindings[i].hotkey, hotkey))
      return i;
  return -1;
}

static int cc_binding_set(const char *hotkey, const char *action) {
  int i = cc_binding_find(action);
  if (i < 0) {
    for (int j = 0; j < cc_binding_count && i < 0; j++)
      if (!cc_binding_used(j))
        i = j;
    if (i < 0) {
      if (cc_binding_count >= CC_MAX_BINDINGS)
        return -1;
      i = cc_binding_count++;
    }
    cc_copy(cc_bindings[i].action, sizeof(cc_bindings[i].action), action);
  }
  cc_copy(cc_bindings[i].hotkey, sizeof(cc_bindings[i].hotkey), hotkey);
  cc_bindings[i].registered = false;
  return i;
}

static void cc_bindings_load(void) {
  cc_binding_count = 0;
  memset(cc_bindings, 0, sizeof(cc_bindings));
  SkStringList lines = sk_load_file_list(CC_HOTKEY_FILE);
  for (size_t i = 0; i < lines.count; i++) {
    char copy[512];
    cc_copy(copy, sizeof(copy), lines.items[i]);
    char *tab = strchr(copy, '\t');
    if (!tab)
      continue;
    *tab = '\0';
    const char *hotkey = copy;
    const char *action = tab + 1;
    if (!hotkey[0] || !action[0])
      continue;

    int owner = cc_binding_find_hotkey(hotkey);
    if (owner >= 0 && strcmp(cc_bindings[owner].action, action) != 0)
      cc_binding_release(owner);
    cc_binding_set(hotkey, action);
  }
  sk_free_file_list(&lines);
}

static void cc_bindings_save(void) {
  char buf[CC_MAX_BINDINGS][64 + 1 + 256 + 1];
  const char *lines[CC_MAX_BINDINGS];
  int n = 0;
  for (int i = 0; i < cc_binding_count; i++) {
    if (!cc_binding_used(i))
      continue;
    snprintf(buf[n], sizeof(buf[n]), "%s\t%s", cc_bindings[i].hotkey,
             cc_bindings[i].action);
    lines[n] = buf[n];
    n++;
  }
  sk_write_to_file(CC_HOTKEY_FILE, lines, n);
}

static void cc_register_pending_bindings(void) {
  for (int i = 0; i < cc_binding_count; i++)
    if (cc_binding_used(i) && !cc_bindings[i].registered &&
        cc_find(cc_bindings[i].action))
      cc_binding_register(i);
}

static void cc_bind_hotkey(const char *action, const char *hotkey) {
  if (!action || !action[0] || !hotkey || !hotkey[0] || !cc_find(action))
    return;

  int existing = cc_binding_find(action);
  if (existing >= 0 && cc_bindings[existing].registered &&
      sk_string_equals_ci(cc_bindings[existing].hotkey, hotkey))
    return;

  int owner = cc_binding_find_hotkey(hotkey);
  if (owner >= 0 && owner != existing)
    cc_binding_release(owner);
  if (existing >= 0)
    cc_binding_unregister(existing);

  int i = cc_binding_set(hotkey, action);
  if (i < 0)
    return;
  cc_binding_register(i);
  cc_bindings_save();
}

static bool cc_hotkey_for_label(const char *label, char *out, size_t cap) {
  if (!label || !out || cap == 0)
    return false;
  out[0] = '\0';
  int i = cc_binding_find(label);
  if (i < 0 || !cc_bindings[i].registered)
    return false;
  cc_copy(out, cap, cc_bindings[i].hotkey);
  return true;
}

typedef struct {
  char *name;
  char *path;
  char *icon;
} CcApp;

static CcApp *cc_apps;
static size_t cc_app_count;
static size_t cc_app_cap;
static atomic_bool cc_apps_ready;
static SkThread cc_prewarm_thread;

typedef struct {
  const char *path;
  int max_depth;
} CcScanPath;

static const CcScanPath cc_scan_paths[] = {
#if defined(_WIN32)
    {"~\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu",
     SK_DIR_WALK_UNLIMITED},
    {"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs",
     SK_DIR_WALK_UNLIMITED},
    {"C:\\miscellaneous", SK_DIR_WALK_UNLIMITED},
    {"shell:AppsFolder", SK_DIR_WALK_UNLIMITED},
    {"C:\\Program Files\\Epic Games", 5},
    {"C:\\Program Files (x86)\\Epic Games", 5},
    {"D:\\Epic Games", 5},
    {"C:\\Games", 5},
    {"C:\\Program Files\\WindowsApps", 5},
    {"C:\\Program Files (x86)\\Steam\\steamapps\\common", 5},
    {"C:\\Program Files\\Steam\\steamapps\\common", 5},
#elif defined(__linux__)
    {"/usr/share/applications", SK_DIR_WALK_UNLIMITED},
    {"~", 6},
#else
    {"/Applications", 2},
#endif
};

static void cc_apps_free(void) {
  for (size_t i = 0; i < cc_app_count; i++) {
    free(cc_apps[i].name);
    free(cc_apps[i].path);
    free(cc_apps[i].icon);
  }
  free(cc_apps);
  cc_apps = NULL;
  cc_app_count = 0;
  cc_app_cap = 0;
}

static bool cc_apps_push(char *name, char *path, char *icon) {
  if (!name || !path || !icon)
    goto fail;
  if (cc_app_count >= cc_app_cap) {
    size_t cap = cc_app_cap ? cc_app_cap * 2 : 128;
    CcApp *grown = (CcApp *)realloc(cc_apps, cap * sizeof(CcApp));
    if (!grown)
      goto fail;
    cc_apps = grown;
    cc_app_cap = cap;
  }
  cc_apps[cc_app_count++] = (CcApp){name, path, icon};
  return true;
fail:
  free(name);
  free(path);
  free(icon);
  return false;
}

static bool cc_icon_is_shell_only(const char *icon) {
  const char *dot = strrchr(icon, '.');
  return dot && (strcmp(dot, ".lnk") == 0 || strcmp(dot, ".exe") == 0 ||
                 strcmp(dot, ".url") == 0);
}

static bool cc_apps_load_cache(bool *needs_save) {
  SkStringList lines = sk_load_file_list(CC_APPCACHE_FILE);
  if (!lines.items)
    return false;
  if (lines.count == 0 || !lines.items[0] ||
      strcmp(lines.items[0], CC_APPCACHE_VERSION) != 0) {
    sk_free_file_list(&lines);
    return false;
  }
  for (size_t i = 1; i < lines.count; i++) {
    char *line = lines.items[i];
    if (!line)
      continue;
    char *tab1 = strchr(line, '\t');
    if (!tab1)
      continue;
    char *tab2 = strchr(tab1 + 1, '\t');
    const char *name = line;
    const char *path = tab1 + 1;
    const char *icon = "";
    *tab1 = '\0';
    if (tab2) {
      *tab2 = '\0';
      icon = tab2 + 1;
    }
    if (!path[0])
      continue;

    char *icon_copy = cc_strdup(icon);
    if (icon_copy && (!icon[0] || cc_icon_is_shell_only(icon) ||
                      !sk_file_exists(icon))) {
      char *resolved = sk_get_icon_path(path);
      if (resolved) {
        if (strcmp(resolved, icon) != 0)
          *needs_save = true;
        free(icon_copy);
        icon_copy = resolved;
      }
    }
    if (!cc_apps_push(cc_strdup(name), cc_strdup(path), icon_copy))
      break;
  }
  sk_free_file_list(&lines);
  return cc_app_count > 0;
}

static void cc_apps_save_cache(void) {
  if (cc_app_count == 0)
    return;
  char **lines = (char **)calloc(cc_app_count + 1, sizeof(char *));
  if (!lines)
    return;
  int n = 0;
  lines[n++] = cc_strdup(CC_APPCACHE_VERSION);
  for (size_t i = 0; i < cc_app_count; i++) {
    const CcApp *a = &cc_apps[i];
    if (!a->name[0] || !a->path[0])
      continue;
    size_t len = strlen(a->name) + strlen(a->path) + strlen(a->icon) + 3;
    char *line = (char *)malloc(len);
    if (!line)
      continue;
    snprintf(line, len, "%s\t%s\t%s", a->name, a->path, a->icon);
    lines[n++] = line;
  }
  sk_write_to_file(CC_APPCACHE_FILE, (const char **)lines, n);
  for (int i = 0; i < n; i++)
    free(lines[i]);
  free(lines);
}

static void cc_scan_dir(const char *path, int depth, char **allowed,
                        ListOfFiles *out) {
  sk_get_all_files_from_dir((char *)path, allowed, out,
                            NULL, true, depth);
}

#if defined(__linux__)
static bool cc_is_builtin_scan_path(const char *dir) {
  for (size_t i = 0; i < ARRAY_LEN(cc_scan_paths); i++)
    if (strcmp(cc_scan_paths[i].path, dir) == 0)
      return true;
  return false;
}

static void cc_scan_xdg_dirs(char **allowed, ListOfFiles *out) {
  const char *data_dirs = getenv("XDG_DATA_DIRS");
  if (!data_dirs || !data_dirs[0])
    data_dirs = "/usr/local/share:/usr/share";
  char buf[4096];
  cc_copy(buf, sizeof(buf), data_dirs);
  char *save = NULL;
  for (char *tok = strtok_r(buf, ":", &save); tok;
       tok = strtok_r(NULL, ":", &save)) {
    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/applications", tok);
    if (cc_is_builtin_scan_path(dir))
      continue;
    if (atomic_load(&cc_stopping))
      return;
    cc_scan_dir(dir, SK_DIR_WALK_UNLIMITED, allowed, out);
  }
}
#endif

static void cc_apps_scan(void) {
#if defined(_WIN32)
  char *allowed[] = {"lnk", "exe", "url", NULL};
#elif defined(__linux__)
  char *allowed[] = {"desktop", NULL};
#else
  char *allowed[] = {"app", NULL};
#endif
  ListOfFiles found = {0};

  for (size_t i = 0; i < ARRAY_LEN(cc_scan_paths); i++) {
    if (atomic_load(&cc_stopping))
      goto done;
    const CcScanPath *p = &cc_scan_paths[i];
    if (p->path && p->path[0])
      cc_scan_dir(p->path, p->max_depth, allowed, &found);
  }
#if defined(__linux__)
  cc_scan_xdg_dirs(allowed, &found);
#endif
  if (atomic_load(&cc_stopping) || found.count == 0)
    goto done;

  size_t kept = 0;
  for (size_t i = 0; i < found.count; i++) {
    bool no_display = false;
    if (strstr(found.files[i].path, ".desktop")) {
      char display_name[256] = {0};
      sk_parse_desktop_file(found.files[i].path, display_name,
                            sizeof(display_name), &no_display);
    }
    if (!no_display)
      found.files[kept++] = found.files[i];
  }
  found.count = kept;

  bool *drop = (bool *)calloc(found.count ? found.count : 1, sizeof(bool));
  if (!drop)
    goto done;
  for (size_t i = 0; i < found.count; i++) {
    if (drop[i] || !found.files[i].name[0])
      continue;
    for (size_t j = i + 1; j < found.count; j++)
      if (!drop[j] && sk_string_equals_ci(found.files[j].name, found.files[i].name))
        drop[j] = true;
  }

  for (size_t i = 0; i < found.count; i++) {
    if (drop[i])
      continue;
    char *icon = sk_get_icon_path(found.files[i].path);
    if (!cc_apps_push(cc_strdup(found.files[i].name),
                      cc_strdup(found.files[i].path),
                      icon ? icon : cc_strdup("")))
      break;
  }
  free(drop);
done:
  free(found.files);
}

static void cc_load_apps(void) {
  bool needs_save = false;
  if (!cc_apps_load_cache(&needs_save)) {
    cc_apps_free();
    cc_apps_scan();
    if (!atomic_load(&cc_stopping))
      cc_apps_save_cache();
  } else if (needs_save) {
    cc_apps_save_cache();
  }
  if (atomic_load(&cc_stopping))
    return;
  atomic_store(&cc_apps_ready, true);
  atomic_store(&cc_entries_dirty, true);
}

static void *cc_prewarm_apps(void *arg) {
  (void)arg;
  cc_load_apps();
  return NULL;
}

static bool cc_launch_app(void) {
  if (cc_staged_payload && cc_staged_payload[0])
    sk_launch_with_args((char *)cc_staged_payload, NULL);
  return true;
}

static bool cc_launch_app_as_admin(void) {
  if (cc_staged_payload && cc_staged_payload[0])
    sk_launch_with_args_as_admin((char *)cc_staged_payload, NULL);
  return true;
}

static void cc_fill_applications(int limit) {
  if (!atomic_load(&cc_apps_ready)) {
    if (cc_prewarm_thread)
      return;
    cc_load_apps();
  }
  for (size_t i = 0; i < cc_app_count && (int)i < limit; i++) {
    CcEntry *e = cc_entry_add(cc_apps[i].name, "Application", cc_apps[i].path,
                              cc_apps[i].icon);
    if (!e)
      break;
    e->on_enter = cc_launch_app;
    e->on_ctrl_enter = cc_launch_app_as_admin;
  }
}

typedef struct {
  const char *name;
  double seconds;
  const char *aliases[8];
} CcUnit;

static const CcUnit cc_units[] = {
    {"ms",
     0.001,
     {"ms", "millis", "millisecond", "milliseconds", "milli", "msec", "msecs",
      NULL}},
    {"s", 1.0, {"s", "sec", "secs", "second", "seconds", NULL}},
    {"min", 60.0, {"m", "min", "mins", "minute", "minutes", NULL}},
    {"hour", 3600.0, {"h", "hr", "hrs", "hour", "hours", NULL}},
    {"day", 86400.0, {"d", "day", "days", NULL}},
    {"week", 604800.0, {"w", "wk", "wks", "week", "weeks", NULL}},
    {"month", 2629746.0, {"mo", "mos", "month", "months", NULL}},
    {"year", 31557600.0, {"y", "yr", "yrs", "year", "years", NULL}},
};

static int cc_edit_distance(const char *s, const char *t) {
  size_t ls = strlen(s), lt = strlen(t);
  if (ls > 32 || lt > 32)
    return 999;
  int d[33][33];
  for (size_t i = 0; i <= ls; i++)
    d[i][0] = (int)i;
  for (size_t j = 0; j <= lt; j++)
    d[0][j] = (int)j;
  for (size_t i = 1; i <= ls; i++)
    for (size_t j = 1; j <= lt; j++) {
      int cost = cc_lower(s[i - 1]) == cc_lower(t[j - 1]) ? 0 : 1;
      int best = d[i - 1][j] + 1;
      if (d[i][j - 1] + 1 < best)
        best = d[i][j - 1] + 1;
      if (d[i - 1][j - 1] + cost < best)
        best = d[i - 1][j - 1] + cost;
      d[i][j] = best;
    }
  return d[ls][lt];
}

static const CcUnit *cc_find_unit(const char *input) {
  char s[64];
  cc_trim_copy(input, s, sizeof(s));
  size_t len = strlen(s);
  if (len == 0)
    return NULL;

  for (size_t i = 0; i < sizeof(cc_units) / sizeof(cc_units[0]); i++) {
    if (sk_string_equals_ci(s, cc_units[i].name))
      return &cc_units[i];
    for (int j = 0; cc_units[i].aliases[j]; j++)
      if (sk_string_equals_ci(s, cc_units[i].aliases[j]))
        return &cc_units[i];
  }
  if (len < 3)
    return NULL;

  for (size_t i = 0; i < sizeof(cc_units) / sizeof(cc_units[0]); i++)
    for (int j = 0; cc_units[i].aliases[j]; j++)
      if (sk_string_starts_with_ci(cc_units[i].aliases[j], s))
        return &cc_units[i];

  const CcUnit *best = NULL;
  int best_d = 999;
  int max_d = len >= 6 ? 2 : 1;
  for (size_t i = 0; i < sizeof(cc_units) / sizeof(cc_units[0]); i++)
    for (int j = 0; cc_units[i].aliases[j]; j++) {
      int d = cc_edit_distance(s, cc_units[i].aliases[j]);
      if (d < best_d && d <= max_d) {
        best_d = d;
        best = &cc_units[i];
      }
    }
  return best;
}

static void cc_format_number(double num, char *out, size_t cap) {
  if (isnan(num) || isinf(num)) {
    snprintf(out, cap, "%g", num);
    return;
  }
  double a = fabs(num);
  if (a >= 1e12 || (a != 0 && a < 1e-6))
    snprintf(out, cap, "%.6e", num);
  else
    snprintf(out, cap, "%.12g", num);
}

#define CC_MAX_DEPTH 48

typedef struct {
  const char *p;
  bool ok;
  int depth;
} CcParser;

static double cc_parse_expr(CcParser *ps);

static void cc_skip_ws(CcParser *ps) {
  while (isspace((unsigned char)*ps->p))
    ps->p++;
}

static bool cc_enter_level(CcParser *ps) {
  if (++ps->depth > CC_MAX_DEPTH) {
    ps->ok = false;
    return false;
  }
  return true;
}

static double cc_parse_number(CcParser *ps) {
  const char *s = ps->p;
  const char *q = s;
  int digits = 0;
  while (isdigit((unsigned char)*q)) {
    q++;
    digits++;
  }
  if (*q == '.') {
    q++;
    while (isdigit((unsigned char)*q)) {
      q++;
      digits++;
    }
  }
  if (digits == 0) {
    ps->ok = false;
    return 0;
  }
  if (*q == 'e' || *q == 'E') {
    const char *r = q + 1;
    if (*r == '+' || *r == '-')
      r++;
    if (isdigit((unsigned char)*r)) {
      while (isdigit((unsigned char)*r))
        r++;
      q = r;
    }
  }
  char tok[64];
  size_t len = (size_t)(q - s);
  if (len >= sizeof(tok)) {
    ps->ok = false;
    return 0;
  }
  memcpy(tok, s, len);
  tok[len] = '\0';
  double v = strtod(tok, NULL);

  char c = *q;
  if (c == 'k' || c == 'K' || c == 'm' || c == 'M') {
    char next = q[1];
    if (next == '\0' || isspace((unsigned char)next) || strchr("+-*/^()xX", next)) {
      v *= (c == 'k' || c == 'K') ? 1e3 : 1e6;
      q++;
    }
  }
  ps->p = q;
  return v;
}

static double cc_parse_primary(CcParser *ps) {
  cc_skip_ws(ps);
  if (*ps->p == '(') {
    if (!cc_enter_level(ps))
      return 0;
    ps->p++;
    double v = cc_parse_expr(ps);
    cc_skip_ws(ps);
    if (*ps->p != ')') {
      ps->ok = false;
      return 0;
    }
    ps->p++;
    ps->depth--;
    return v;
  }
  return cc_parse_number(ps);
}

static double cc_parse_unary(CcParser *ps);

static double cc_parse_power(CcParser *ps) {
  double base = cc_parse_primary(ps);
  cc_skip_ws(ps);
  if (*ps->p == '^') {
    ps->p++;
    if (!cc_enter_level(ps))
      return 0;
    double exponent = cc_parse_unary(ps);
    ps->depth--;
    return pow(base, exponent);
  }
  return base;
}

static double cc_parse_unary(CcParser *ps) {
  cc_skip_ws(ps);
  if (*ps->p == '-' || *ps->p == '+') {
    char op = *ps->p++;
    if (!cc_enter_level(ps))
      return 0;
    double v = cc_parse_unary(ps);
    ps->depth--;
    return op == '-' ? -v : v;
  }
  return cc_parse_power(ps);
}

static double cc_parse_term(CcParser *ps) {
  double v = cc_parse_unary(ps);
  for (;;) {
    cc_skip_ws(ps);
    char c = *ps->p;
    if (c == '*' || c == 'x' || c == 'X') {
      ps->p++;
      v *= cc_parse_unary(ps);
    } else if (c == '/') {
      ps->p++;
      double d = cc_parse_unary(ps);
      if (d == 0) {
        ps->ok = false;
        return 0;
      }
      v /= d;
    } else {
      return v;
    }
    if (!ps->ok)
      return 0;
  }
}

static double cc_parse_expr(CcParser *ps) {
  double v = cc_parse_term(ps);
  for (;;) {
    cc_skip_ws(ps);
    char c = *ps->p;
    if (c == '+') {
      ps->p++;
      v += cc_parse_term(ps);
    } else if (c == '-') {
      ps->p++;
      v -= cc_parse_term(ps);
    } else {
      return v;
    }
    if (!ps->ok)
      return 0;
  }
}

static double cc_eval(const char *expr, bool *ok) {
  CcParser ps = {expr ? expr : "", true, 0};
  double v = cc_parse_expr(&ps);
  cc_skip_ws(&ps);
  *ok = ps.ok && *ps.p == '\0' && !isnan(v);
  return *ok ? v : 0;
}

static int cc_find_in_keyword(const char *s) {
  for (int i = 0; s[i] && s[i + 1] && s[i + 2]; i++)
    if (s[i] == ' ' && cc_lower(s[i + 1]) == 'i' && cc_lower(s[i + 2]) == 'n' &&
        (s[i + 3] == '\0' || isspace((unsigned char)s[i + 3])))
      return i;
  return -1;
}

static bool cc_calc_result(const char *input, char *out, size_t cap) {
  char text[512];
  cc_trim_copy(input, text, sizeof(text));
  if (!text[0])
    return false;

  bool ok = false;
  char formatted[64];
  int in_pos = cc_find_in_keyword(text);
  if (in_pos < 0) {
    double v = cc_eval(text, &ok);
    if (!ok)
      return false;
    cc_format_number(v, formatted, sizeof(formatted));
    cc_copy(out, cap, formatted);
    return true;
  }

  char left[512];
  memcpy(left, text, (size_t)in_pos);
  left[in_pos] = '\0';
  cc_trim_copy(left, left, sizeof(left));

  size_t len = strlen(left);
  size_t split = len;
  while (split > 0 && isalpha((unsigned char)left[split - 1]))
    split--;
  if (split == len)
    return false;
  const CcUnit *from = cc_find_unit(left + split);
  const CcUnit *to = cc_find_unit(text + in_pos + 3);
  if (!from || !to)
    return false;

  char expr[512];
  left[split] = '\0';
  cc_trim_copy(left, expr, sizeof(expr));
  if (!expr[0])
    cc_copy(expr, sizeof(expr), "1");
  double v = cc_eval(expr, &ok);
  if (!ok)
    return false;

  cc_format_number(v * (from->seconds / to->seconds), formatted,
                   sizeof(formatted));
  snprintf(out, cap, "%s %s", formatted, to->name);
  return true;
}

static char cc_calc_label[512];

static bool cc_calc_build_label(const char *filter) {
  cc_calc_label[0] = '\0';
  if (!filter || !filter[0])
    return false;
  char trimmed[256];
  cc_trim_copy(filter, trimmed, sizeof(trimmed));
  bool has_digit = false;
  for (const char *p = trimmed; *p; p++)
    if (isdigit((unsigned char)*p)) {
      has_digit = true;
      break;
    }
  if (!has_digit)
    return false;
  char value[128];
  if (!cc_calc_result(trimmed, value, sizeof(value)))
    return false;
  if (strcmp(trimmed, value) == 0)
    return false;
  snprintf(cc_calc_label, sizeof(cc_calc_label), "%s = %s", trimmed, value);
  return true;
}

static const char *cc_names[MAX_VISIBLE_ITEMS + 1];
static int cc_width, cc_height;
static char cc_selected_text[256];

static bool cc_filter_match(const char *item, const char *filter) {
  if (!item || !filter || !filter[0])
    return true;
  if (sk_string_contains_ci(item, filter))
    return true;
  const char *alias = sk_alias_for(item);
  return alias && sk_string_contains_ci(alias, filter);
}

static void cc_refresh(PromptState *state) {
  if (atomic_load(&cc_entries_dirty))
    cc_build();
  else
    cc_entry_count = cc_entry_base;

  int n = cc_entry_count < MAX_VISIBLE_ITEMS ? cc_entry_count
                                             : MAX_VISIBLE_ITEMS;
  for (int i = 0; i < n; i++)
    cc_names[i] = cc_entries[i].label;
  if (n < MAX_VISIBLE_ITEMS && cc_calc_build_label(state->eb->filter))
    cc_names[n++] = cc_calc_label;

  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = cc_names, .count = n,
                      .filter_match = cc_filter_match);

  if (state->lb->total_matches == 0 && n < MAX_VISIBLE_ITEMS) {
    CcEntry *fallback = cc_add_fallback();
    if (fallback) {
      cc_names[n] = fallback->label;
      sk_populate_listbox(.lb = state->lb, .filter = "", .items = &cc_names[n],
                          .count = 1);
    }
  }
}

static void cc_draw_hotkey_chip(const char *hotkey, int right_x, int y) {
  int font = FontSize - CC_HOTKEY_FONT_DELTA;
  SkRect r = sk_text_bounds(.text = hotkey, .fontsize = font, .x = right_x,
                            .y = y, .align = "RIGHT");
  r.filled = true;
  r.radius = 4;
  r.color = SK_DARK_GRAY;
  ski_draw_rect(&r);
  sk_draw_text(.x = right_x, .y = y, .text = hotkey, .align = "RIGHT", .pad = 0,
               .color = SK_GRAY, .fontsize = font);
}

static void cc_draw_row(const SkListItemCtx *ctx, int type_x) {
  const CcEntry *e = cc_find(ctx->text);
  const char *icon = (e && e->icon[0]) ? e->icon : NULL;
  if (!icon && cc_calc_label[0] && strcmp(ctx->text, cc_calc_label) == 0)
    icon = CC_CALC_ROW_ICON;

  int name_x = ctx->x + CC_ITEM_TEXT_X + FontSize + CC_ICON_GAP;
  int name_w = sk_get_text_width(ctx->text, FontSize);
  if (icon)
    sk_draw_image(.path = icon, .x = ctx->x + CC_ITEM_TEXT_X,
                  .y = ctx->y + (ctx->h - FontSize) / 2, .w = FontSize,
                  .h = FontSize);
  sk_draw_text(.x = name_x, .y = ctx->y + CC_ITEM_TEXT_Y, .text = ctx->text,
               .align = "LEFT", .pad = 0);

  const char *alias = sk_alias_for(ctx->text);
  if (alias) {
    char chip[160];
    snprintf(chip, sizeof(chip), "[%s]", alias);
    sk_draw_text(.x = name_x + name_w + CC_TYPE_GAP,
                 .y = ctx->y + CC_ITEM_TEXT_Y + 2, .text = chip,
                 .align = "LEFT", .pad = 0, .color = SK_GRAY,
                 .fontsize = FontSize - 3);
  }
  if (!e)
    return;

  bool plain = strcmp(e->type, "Application") == 0 ||
               strcmp(e->type, "Feature") == 0;
  char hotkey[64];
  bool has_hotkey =
      cc_hotkey_for_label(ctx->text, hotkey, sizeof(hotkey)) && hotkey[0];
  int right_w = sk_get_text_width(plain ? e->type : CC_TYPE_LABEL, FontSize);
  if (has_hotkey)
    cc_draw_hotkey_chip(hotkey, type_x - right_w - CC_TYPE_GAP - 4,
                        ctx->y + 10);

  if (plain) {
    sk_draw_text(.x = type_x, .y = ctx->y + CC_ITEM_TEXT_Y, .text = e->type,
                 .align = "RIGHT", .color = CC_TYPE_COLOR);
    return;
  }

  if (e->type[0]) {
    int tag_size = FontSize - CC_TYPE_SIZE_DELTA;
    if (tag_size < 8)
      tag_size = FontSize;
    int tag_w = sk_get_text_width(e->type, tag_size);
    int limit = type_x - sk_get_text_width(CC_TYPE_LABEL, FontSize);
    if (has_hotkey)
      limit -= CC_TYPE_GAP + 4 +
               sk_get_text_width(hotkey, FontSize - CC_HOTKEY_FONT_DELTA);
    if (name_x + name_w + CC_TYPE_GAP + tag_w + CC_TYPE_GAP <= limit)
      sk_draw_text(.x = name_x + name_w + CC_TYPE_GAP,
                   .y = ctx->y + CC_ITEM_TEXT_Y + CC_TYPE_SIZE_DELTA,
                   .text = e->type, .align = "LEFT", .pad = 0,
                   .fontsize = tag_size, .color = CC_TYPE_COLOR);
  }
  sk_draw_text(.x = type_x, .y = ctx->y + CC_ITEM_TEXT_Y, .text = CC_TYPE_LABEL,
               .align = "RIGHT", .pad = 0, .color = CC_TYPE_COLOR);
}

static void cc_open_actions_window(void);
static void cc_open_register_window(int kind);

static void cc_on_actions_render(SkPlaceState state) {

  char hotkey[64];
  cc_copy(hotkey, sizeof(hotkey), ActionsHotkey);
  char *plus = strchr(hotkey, '+');
  if (plus)
    *plus = ' ';

  sk_draw_rect(.x = state.rect.x + 95, .y = state.rect.y - 2,
               .w = state.rect.w - 95, .h = state.rect.h, .filled = true,
               .color = SK_DARK_GRAY, .radius = 6);
  if (state.hovered)
    sk_draw_rect(.x = state.rect.x + 15, .y = state.rect.y - 2,
                 .w = state.rect.w - 15, .h = state.rect.h, .filled = true,
                 .color = SK_GRAY, .radius = 6);
  sk_draw_text(.x = state.rect.x + 20, .y = state.rect.y - 4, .text = "Actions",
               .align = "LEFT", .color = SK_DARK_GRAY, .fontsize = 16);
  sk_draw_text(.x = state.rect.x + 100, .y = state.rect.y - 7, .text = hotkey,
               .align = "LEFT", .fontsize = 20);
}

static void cc_render(PromptState *state) {
  if (state->lb && state->lb->selected_index != -1) {
    const char *current = ListBoxInfo_Row(state->lb, state->lb->selected_index);
    if (current)
      cc_copy(cc_selected_text, sizeof(cc_selected_text), current);
  } else if (state->eb) {
    cc_copy(cc_selected_text, sizeof(cc_selected_text), state->eb->filter);
  }
  cc_width = state->width;
  cc_height = state->height;

  SkListSpec spec = {.lb = state->lb, .item_height = FontSize * 2};
  sk_place_editbox();
  sk_place_button(.name = "Actions", .x = state->width - 200,
                  .y = state->height - 30, .w = 180,
                  .render = cc_on_actions_render,
                  .on_click = cc_open_actions_window);

  if (sk_hotkey_down(sk_hotkey_from_string(ActionsHotkey)))
    cc_open_actions_window();
  if (sk_hotkey_down(sk_hotkey_from_string(RegHotkey)))
    cc_open_register_window(0);
  if (sk_hotkey_down(sk_hotkey_from_string(RegAliasHotkey)))
    cc_open_register_window(1);

  if (!sk_listbox_begin(&spec))
    return;
  int type_x = state->width - CC_ITEM_TEXT_X;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx))
    cc_draw_row(&ctx, type_x);
  sk_listbox_end();
}

static bool cc_copy_calc_result(const PromptState *state) {
  if (!cc_calc_label[0] || strcmp(state->eb->filter, cc_calc_label) != 0)
    return false;
  const char *eq = strchr(cc_calc_label, '=');
  if (eq) {
    char value[128];
    cc_trim_copy(eq + 1, value, sizeof(value));
    sk_copy_to_clipboard(value);
  }
  return true;
}

static bool cc_enter(PromptState *state) {
  if (cc_copy_calc_result(state))
    return true;
  if (sk_is_selected(state->lb))
    cc_stage(cc_find(state->lb->selected_text), CC_RUN_ENTER);
  else
    sk_query_browser(state->eb->filter);
  return true;
}

static bool cc_enter_with(PromptState *state, CcRunMode mode) {
  if (!sk_is_selected(state->lb))
    sk_query_browser(state->lb->selected_text);
  cc_stage(cc_find(state->lb->selected_text), mode);
  return true;
}

#define CC_DEFINE_ENTER(fn, mode)                                              \
  static bool fn(PromptState *state) { return cc_enter_with(state, mode); }
CC_DEFINE_ENTER(cc_ctrl_enter, CC_RUN_CTRL)
CC_DEFINE_ENTER(cc_shift_enter, CC_RUN_SHIFT)
CC_DEFINE_ENTER(cc_ctrl_shift_enter, CC_RUN_CTRL_SHIFT)
CC_DEFINE_ENTER(cc_ctrl_alt_enter, CC_RUN_CTRL_ALT)
CC_DEFINE_ENTER(cc_shift_alt_enter, CC_RUN_SHIFT_ALT)

static void cc_on_close(void) { cc_run_pending(); }

static void cc_tab(PromptState *state) {
  const CcEntry *e = cc_find(state->lb->selected_text);
  if (e && e->on_tab)
    e->on_tab();
}

static void cc_reload(void) {
  int features = sk_reload_all_features();
  int config = sk_reload_config();
  char msg[128];
  if (features > 0 && config)
    snprintf(msg, sizeof(msg), "Reloaded %d Feature%s And Config", features,
             features == 1 ? "" : "s");
  else if (features > 0)
    snprintf(msg, sizeof(msg), "Reloaded %d Feature%s", features,
             features == 1 ? "" : "s");
  else if (config)
    snprintf(msg, sizeof(msg), "Reloaded Config");
  else
    snprintf(msg, sizeof(msg), "Nothing Reloaded");
  sk_message(.message = msg);
}

static void cc_open_control(void) {
  atomic_store(&cc_entries_dirty, true);
  sk_window(.title = "Control", .placeholder = "Search For Something...",
            .refresh = cc_refresh, .enter = cc_enter,
            .ctrl_enter = cc_ctrl_enter, .shift_enter = cc_shift_enter,
            .ctrl_shift_enter = cc_ctrl_shift_enter,
            .ctrl_alt_enter = cc_ctrl_alt_enter,
            .shift_alt_enter = cc_shift_alt_enter, .tab = cc_tab,
            .render = cc_render, .on_close = cc_on_close);
}

static int cc_sub_w(void) { return cc_width / 4; }
static int cc_sub_h(void) { return cc_height / 9; }
static int cc_sub_x(void) { return cc_width / 2 + cc_sub_w() - 10; }
static int cc_sub_y(void) { return cc_height / 2 - 50; }

static const char *cc_action_names[] = {"Register Hotkey", "Register Alias"};

static void cc_actions_refresh(PromptState *state) {
  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = cc_action_names,
                      .count = (int)ARRAY_LEN(cc_action_names));
}

static void cc_actions_render(PromptState *state) {
  int width = state->width;
  SkListSpec spec = {.lb = state->lb};
  sk_place_editbox();
  if (!sk_listbox_begin(&spec))
    return;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx)) {
    sk_draw_text(.y = ctx.y + 6, .text = ctx.text, .align = "LEFT", .pad = 10,
                 .fontsize = 12);
    const char *hotkey = ctx.idx == 0 ? RegHotkey : RegAliasHotkey;
    if (width <= sk_get_text_width(ctx.text, 10) + 28)
      continue;
    SkRect r = sk_text_bounds(.text = hotkey, .fontsize = 10, .x = width - 6,
                              .y = ctx.y + 8, .align = "RIGHT");
    r.filled = true;
    r.radius = 4;
    r.color = SK_DARK_GRAY;
    ski_draw_rect(&r);
    sk_draw_text(.x = width - 6, .y = ctx.y + 8, .text = hotkey,
                 .align = "RIGHT", .fontsize = 10, .color = SK_GRAY);
  }
  sk_listbox_end();
}

static bool cc_actions_enter(PromptState *state) {
  cc_open_register_window(state->lb->selected_index);
  return true;
}

static void cc_open_actions_window(void) {
  sk_subwindow(.title = "Actions", .x = cc_sub_x(), .y = cc_sub_y(),
               .w = cc_sub_w(), .h = 300, .render = cc_actions_render,
               .refresh = cc_actions_refresh, .enter = cc_actions_enter);
}

static char cc_register_target[256];
static char cc_captured_hotkey[128];
static SkTimer cc_flash_timer;
static bool cc_flashing;
static int cc_last_key;

static const struct {
  int vk;
  const char *name;
} cc_punctuation_keys[] = {
    {VK_OEM_1, ";"},     {VK_OEM_2, "/"},     {VK_OEM_3, "`"},
    {VK_OEM_4, "["},     {VK_OEM_5, "\\"},    {VK_OEM_6, "]"},
    {VK_OEM_7, "'"},     {VK_OEM_COMMA, ","}, {VK_OEM_PERIOD, "."},
    {VK_OEM_MINUS, "-"}, {VK_OEM_PLUS, "="},
};

static void cc_append(char *buf, size_t cap, const char *s) {
  size_t len = strlen(buf);
  if (len < cap)
    snprintf(buf + len, cap - len, "%s", s);
}

static int cc_pressed_key(char *name, size_t cap) {
  for (int c = 'A'; c <= 'Z'; c++)
    if (sk_is_key_pressed(c)) {
      snprintf(name, cap, "%c", c);
      return c;
    }
  for (int c = '0'; c <= '9'; c++)
    if (sk_is_key_pressed(c)) {
      snprintf(name, cap, "%c", c);
      return c;
    }
  for (size_t i = 0; i < ARRAY_LEN(cc_punctuation_keys); i++)
    if (sk_is_key_pressed(cc_punctuation_keys[i].vk)) {
      cc_copy(name, cap, cc_punctuation_keys[i].name);
      return cc_punctuation_keys[i].vk;
    }
  return 0;
}

static void cc_hotkey_capture_render(PromptState *state) {
  sk_disable_global_hotkeys();

  char combo[128] = "";
  if (sk_ctrl_pressed())
    cc_append(combo, sizeof(combo), "Ctrl+");
  if (sk_alt_pressed())
    cc_append(combo, sizeof(combo), "Alt+");
  if (sk_shift_pressed())
    cc_append(combo, sizeof(combo), "Shift+");

  bool has_modifier =
      sk_ctrl_pressed() || sk_alt_pressed() || sk_shift_pressed();

  char key_name[8] = "";
  int key = cc_pressed_key(key_name, sizeof(key_name));
  if (key)
    cc_append(combo, sizeof(combo), key_name);
  size_t len = strlen(combo);
  if (len > 0 && combo[len - 1] == '+')
    combo[len - 1] = '\0';

  if (combo[0] && key && key != cc_last_key && has_modifier) {
    cc_bind_hotkey(cc_register_target, combo);
    cc_copy(cc_captured_hotkey, sizeof(cc_captured_hotkey), combo);
    cc_flashing = true;
    cc_flash_timer = sk_start_timer();
    sk_enable_global_hotkeys();
  }
  cc_last_key = key;
  if (cc_flashing && sk_end_timer(cc_flash_timer) >= 1.0)
    cc_flashing = false;

  sk_draw_text(.x = 15, .y = 20, .text = "Hold A Key Sequence", .fontsize = 15,
               .color = SK_GRAY, .align = "LEFT");
  sk_draw_rect(.x = 10, .y = state->height / 2, .h = 40, .w = 180, .radius = 6,
               .color = cc_flashing ? SK_GRAY : SK_DARK_GRAY, .filled = true);
  if (combo[0] || cc_flashing)
    sk_draw_text(.x = 17, .y = state->height / 2,
                 .text = combo[0] ? combo : cc_captured_hotkey, .fontsize = 22,
                 .color = cc_flashing ? SK_DARK_GREEN : SK_GRAY,
                 .align = "LEFT");
}

static void cc_hotkey_capture_close(void) { sk_enable_global_hotkeys(); }

static void cc_alias_render(PromptState *state) {
  char label[284];
  snprintf(label, sizeof(label), "Alias For: %s", cc_register_target);
  sk_draw_text(.x = 14, .y = 8, .text = label, .fontsize = 11, .color = SK_GRAY,
               .align = "LEFT", .pad = 0);
  sk_place_editbox(.x = (state->width - 160) / 2,
                   .y = (state->height - 30) / 2 + 6, .w = 160, .h = 26,
                   .placeholder = "alias name", .caret_height = 14,
                   .caret_width = 1, .caret_y = 47, .fontsize = 14);
}

static bool cc_alias_enter(PromptState *state) {
  char alias[256];
  cc_trim_copy(state->eb->filter, alias, sizeof(alias));
  if (alias[0])
    sk_add_alias(.alias = alias, .target = cc_register_target);
  return true;
}

static void cc_alias_close(void) { sk_enable_global_hotkeys(); }

static void cc_open_register_window(int kind) {
  if (kind != 0 && kind != 1)
    return;
  cc_copy(cc_register_target, sizeof(cc_register_target), cc_selected_text);
  sk_disable_global_hotkeys();
  if (kind == 0)
    sk_subwindow(.title = "Hotkey Reg", .x = cc_sub_x(),
                 .y = cc_sub_y() + cc_sub_h() * 2, .w = cc_sub_w(),
                 .h = cc_sub_h(), .render = cc_hotkey_capture_render,
                 .on_close = cc_hotkey_capture_close);
  else
    sk_subwindow(.title = "Alias Reg", .x = cc_sub_x(),
                 .y = cc_sub_y() + cc_sub_h() * 2, .w = cc_sub_w(),
                 .h = cc_sub_h(), .render = cc_alias_render,
                 .enter = cc_alias_enter, .on_close = cc_alias_close);
}

static void cc_deferred_build(void) { cc_build(); }

SKALD_FEATURE_INIT {
  atomic_store(&cc_stopping, false);
  atomic_store(&cc_apps_ready, false);
  atomic_store(&cc_entries_dirty, true);
  cc_entry_count = 0;
  cc_entry_base = 0;
  cc_bindings_load();

  cc_prewarm_thread = sk_thread_create(cc_prewarm_apps, NULL);
  cc_build();
  sk_register_hotkey(.hotkey = (char *)ControlHotkey, .cb = cc_open_control);
  sk_register_hotkey(.hotkey = (char *)ReloadFeaturesHotkey, .cb = cc_reload);
  sk_register_deferred_init(cc_deferred_build);
}

SKALD_FEATURE_SHUTDOWN {
  atomic_store(&cc_stopping, true);
  if (cc_prewarm_thread) {
    sk_thread_join(cc_prewarm_thread);
    cc_prewarm_thread = NULL;
  }
  cc_apps_free();
  free(cc_entries);
  cc_entries = NULL;
  cc_entry_count = 0;
  cc_entry_cap = 0;
  cc_entry_base = 0;
}
