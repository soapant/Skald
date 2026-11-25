#include "core/skapi.h"
#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_APPCACHE_FILE "app_launcher_cache.txt"

#define CC_ITEM_TEXT_X 10
#define CC_ITEM_TEXT_Y 2
#define CC_ICON_GAP 8
#define CC_TYPE_GAP 6
#define CC_TYPE_SIZE_DELTA 2
#define CC_HOTKEY_FONT_DELTA 6
#define CC_TYPE_LABEL "Command"
#define CC_CALC_ROW_ICON "/calculator.png"
#define CC_TYPE_COLOR sk_color_lerp(COLOR_BG, SK_BLACK, 0.32f)

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
  bool keep_open;
} CcEntry;

typedef enum {
  CC_RUN_ENTER,
  CC_RUN_CTRL,
  CC_RUN_SHIFT,
  CC_RUN_CTRL_SHIFT,
  CC_RUN_CTRL_ALT,
  CC_RUN_SHIFT_ALT,
} CcRunMode;

typedef enum {
  CC_STATE_LAUNCHER,
  CC_STATE_ADD_QUICKLINK,
  CC_STATE_ADD_SHORTCUT,
} CcState;

static CcState cc_state = CC_STATE_LAUNCHER;
static bool cc_state_changed;
static bool cc_quicklink_name_seeded;

static void cc_set_state(CcState state) {
  if (cc_state == state)
    return;
  cc_state = state;
  cc_state_changed = true;
  sk_request_refresh();
}

static CcEntry *cc_entries;
static int cc_entry_base;
static const CcEntry *cc_fallback_entry;

static int cc_entry_total(void) { return (int)sk_array_len(cc_entries); }

static atomic_bool cc_entries_dirty = true;
static atomic_bool cc_stopping;

static void cc_fill_applications(int limit);
static void cc_register_config_hotkeys(void);
static void cc_fill_quicklinks(void);
static void cc_quicklinks_clear(void);
static void cc_fill_add_quicklink(void);
static void cc_fill_add_feature_shortcut(void);
static void cc_fill_feature_shortcuts(void);
static void cc_feature_shortcuts_clear(void);
static void cc_stage(const CcEntry *e, CcRunMode mode);
static bool cc_run_pending(void);
static const CcEntry *cc_find(const char *label);

static CcEntry *cc_entry_new(void) {
  CcEntry blank = {0};
  cc_entries = sk_array_push(cc_entries, sizeof(blank), &blank);
  if (!cc_entries)
    return NULL;
  return &cc_entries[sk_array_len(cc_entries) - 1];
}

static CcEntry *cc_entry_add(const char *label, const char *type,
                             const char *payload, const char *icon) {
  if (!label || !label[0] || cc_entry_total() >= MAX_VISIBLE_ITEMS)
    return NULL;
  CcEntry *e = cc_entry_new();
  if (!e)
    return NULL;
  if (!icon)
    icon = "./resources/Skald.png";
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
  cc_quicklinks_clear();
  cc_feature_shortcuts_clear();
  cc_entries = sk_array_set_len(cc_entries, sizeof(*cc_entries), 0);

  cc_fill_applications(MAX_VISIBLE_ITEMS - sk_cc_entry_count() -
                       sk_config_quicklink_count() -
                       sk_config_feature_shortcut_count());
  cc_fill_feature_entries();
  cc_fill_add_quicklink();
  cc_fill_add_feature_shortcut();
  cc_fill_quicklinks();
  cc_fill_feature_shortcuts();
  cc_entry_base = cc_entry_total();
  cc_register_config_hotkeys();
}

static const CcEntry *cc_find(const char *label) {
  if (!label || !label[0])
    return NULL;
  for (int i = 0; i < cc_entry_total(); i++)
    if (strcmp(cc_entries[i].label, label) == 0)
      return &cc_entries[i];
  return NULL;
}

static const CcEntry *cc_entry_for_row(const ListBox *lb, int row) {
  if (!lb || row < 0 || row >= lb->item_count)
    return NULL;
  const char *text = lb->items ? lb->items[row] : NULL;
  if (cc_fallback_entry && text &&
      strcmp(text, cc_fallback_entry->label) == 0)
    return cc_fallback_entry;
  if (row >= lb->total_matches)
    return NULL;
  int raw = lb->matching_indices[row];
  if (raw < 0 || raw >= cc_entry_total())
    return NULL;
  return &cc_entries[raw];
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

static bool cc_run_pending(void) {
  void (*action)(void) = cc_pending_action;
  CcHandler fn = cc_pending_fn;
  cc_pending_action = NULL;
  cc_pending_fn = NULL;
  cc_staged_label = cc_pending_label;
  cc_staged_payload = cc_pending_payload;
  bool close = true;
  if (action)
    action();
  else if (fn)
    close = fn();
  cc_staged_label = NULL;
  cc_staged_payload = NULL;
  return close;
}

#define CC_HOTKEY_UD_MAX 512
static char cc_hotkey_ud_name[CC_HOTKEY_UD_MAX][256];
static int cc_hotkey_ud_count;

static void *cc_hotkey_ud_for(const char *label) {
  if (!label || !label[0])
    return NULL;
  for (int i = 0; i < cc_hotkey_ud_count; i++)
    if (strcmp(cc_hotkey_ud_name[i], label) == 0)
      return cc_hotkey_ud_name[i];
  if (cc_hotkey_ud_count >= CC_HOTKEY_UD_MAX) {
    return NULL;
  }
  snprintf(cc_hotkey_ud_name[cc_hotkey_ud_count],
           sizeof(cc_hotkey_ud_name[cc_hotkey_ud_count]), "%s", label);
  return cc_hotkey_ud_name[cc_hotkey_ud_count++];
}

static void cc_config_hotkey_cb(void *ud) {
  const char *label = ud;
  if (!label)
    return;
  cc_stage(cc_find(label), CC_RUN_ENTER);
  cc_run_pending();
}

static void cc_register_config_hotkeys(void) {
  for (int i = 0; i < cc_entry_total(); i++) {
    CcEntry *e = &cc_entries[i];
    if (!e->label[0])
      continue;
    void *ud = cc_hotkey_ud_for(e->label);
    if (!ud)
      continue;
    sk_config_bind_hotkey(e->label, cc_config_hotkey_cb, ud);
  }
}

static void cc_bind_hotkey(const char *action, const char *hotkey) {
  if (!action || !action[0] || !hotkey || !hotkey[0] || !cc_find(action))
    return;
  void *ud = cc_hotkey_ud_for(action);
  if (!ud)
    return;
  sk_config_set_hotkey(.name = action, .hotkey = hotkey,
                       .cb = cc_config_hotkey_cb, .ud = ud);
}

static bool cc_hotkey_for_label(const char *label, char *out, size_t cap) {
  char *hotkey = sk_config_get_hotkey(label);
  if (!hotkey)
    return false;
  snprintf(out, cap, "%s", hotkey);
  free(hotkey);
  return true;
}

typedef struct {
  char *name;
  char *path;
  char *icon;
} CcApp;

static CcApp *cc_apps;
static atomic_bool cc_apps_ready;

static size_t cc_app_total(void) { return sk_array_len(cc_apps); }

typedef struct {
  const char *path;
  int max_depth;
} CcScanPath;

static const CcScanPath cc_scan_paths[] = {
#if defined(_WIN32)
    {"~\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu",
     SK_DIR_WALK_UNLIMITED},
    {"C:\\ProgramData\\Microsoft\\Windows\\Start Menu", SK_DIR_WALK_UNLIMITED},
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
  size_t count = cc_app_total();
  for (size_t i = 0; i < count; i++) {
    free(cc_apps[i].name);
    free(cc_apps[i].path);
    free(cc_apps[i].icon);
  }
  sk_array_free(cc_apps);
  cc_apps = NULL;
}

static bool cc_apps_push(char *name, char *path, char *icon) {
  if (!name || !path || !icon)
    goto fail;
  CcApp app = {name, path, icon};
  cc_apps = sk_array_push(cc_apps, sizeof(app), &app);
  if (!cc_apps)
    goto fail;
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
  if (lines.count == 0 || !lines.items[0]) {
    sk_free_string_list(&lines);
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
    if (icon_copy &&
        (!icon[0] || cc_icon_is_shell_only(icon) || !sk_file_exists(icon))) {
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
  sk_free_string_list(&lines);
  return cc_app_total() > 0;
}

static void cc_apps_save_cache(void) {
  size_t count = cc_app_total();
  if (count == 0)
    return;
  char **lines = (char **)calloc(count + 1, sizeof(char *));
  if (!lines)
    return;
  int n = 0;
  for (size_t i = 0; i < count; i++) {
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
  sk_dir_scan(.dir_path = path, .allowed_extensions = allowed, .out = out,
              .strip_extension = true, .max_depth = depth);
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
      char *display_name =
          sk_parse_desktop_file(found.files[i].path, &no_display);
      free(display_name);
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
      if (!drop[j] &&
          sk_string_equals_ci(found.files[j].name, found.files[i].name))
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

static void cc_prewarm_apps(void) {
  cc_load_apps();
  size_t total = cc_app_total();
  size_t n = total < 48 ? total : 48;
  for (size_t i = 0; i < n; i++)
    sk_prewarm_image(cc_apps[i].icon);
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

static void cc_add_quicklink_render(PromptState *state) {
  char *clip = sk_get_last_clipboard_entry();
  bool is_url = clip && sk_string_starts_with(clip, "https://");
  int starting_x = state->width / 2 - 270;

  sk_draw_text(.text = "Link", .color = CC_TYPE_COLOR, .x = starting_x, .y = 45);
  sk_place_editbox_wrapped(.name="quicklink_link",.x = starting_x + 60, .y = 40, .w = 400, .h = 200,
                           .placeholder = "Enter URL or Link",
                           .initial_text = is_url ? clip : "",
                           .color_bg = CC_TYPE_COLOR,
                           .text_y = 8,
                           .auto_focus = true);

  char name_buf[256] = "";
  char *after_scheme = sk_string_cut_after(sk_editbox_text("quicklink_link"), ':');
  if (after_scheme) {
    const char *host = after_scheme;
    while (*host == '/')
      host++;
    if (strncmp(host, "www.", 4) == 0)
      host += 4;
    if (strchr(host, '.')) {
      char *domain = sk_string_cut_before(host, '.');
      if (domain && domain[0]) {
        domain[0] = (char)toupper((unsigned char)domain[0]);
        snprintf(name_buf, sizeof(name_buf), "%s", domain);
      }
      free(domain);
    }
    free(after_scheme);
  }

  sk_draw_text(.text = "Name", .color = CC_TYPE_COLOR, .x = starting_x, .y = 335);
  sk_place_editbox_wrapped(.name="quicklink_name",.x = starting_x + 60, .y = 320, .w = 400, .h = 60,
                           .placeholder = "Enter Name",
                           .color_bg = CC_TYPE_COLOR,
                           .initial_text = name_buf,
                           .text_y = 12);
  if (!cc_quicklink_name_seeded && name_buf[0] &&
      sk_editbox_set_text("quicklink_name", name_buf))
    cc_quicklink_name_seeded = true;
  free(clip);
}

static bool cc_add_quicklink_enter(PromptState *state) {
  const char* name = sk_editbox_text("quicklink_name");
  const char* link = sk_editbox_text("quicklink_link");
  if (name && link && name[0] && link[0])
    sk_config_set_quicklink(name, link);
  atomic_store(&cc_entries_dirty, true);
  return false;
}

static void cc_add_shortcut_render(PromptState *state) { (void)state; }

static bool cc_add_shortcut_enter(PromptState *state) {
  (void)state;
  return false;
}

static bool cc_open_quicklink_state(void) {
  cc_quicklink_name_seeded = false;
  cc_set_state(CC_STATE_ADD_QUICKLINK);
  return false;
}

static bool cc_open_shortcut_state(void) {
  cc_set_state(CC_STATE_ADD_SHORTCUT);
  return false;
}

static void cc_fill_add_quicklink(void) {
  CcEntry *e =
      cc_entry_add("Add Quicklink", "Quicklink", NULL, "/internet.svg");
  if (e) {
    e->on_enter = cc_open_quicklink_state;
    e->keep_open = true;
  }
}

static void cc_fill_add_feature_shortcut(void) {
  CcEntry *e = cc_entry_add("Add Shortcut", "Shortcut", NULL, NULL);
  if (e) {
    e->on_enter = cc_open_shortcut_state;
    e->keep_open = true;
  }
}

static bool cc_run_feature_shortcut(void) {
  if (!cc_staged_payload || !cc_staged_payload[0])
    return false;
  return sk_run_feature_shortcut(cc_staged_payload);
}

static char **cc_feature_shortcut_names;

static void cc_feature_shortcuts_clear(void) {
  size_t count = sk_array_len(cc_feature_shortcut_names);
  for (size_t i = 0; i < count; i++)
    free(cc_feature_shortcut_names[i]);
  sk_array_free(cc_feature_shortcut_names);
  cc_feature_shortcut_names = NULL;
}

static const char *cc_feature_shortcut_track(char *name) {
  if (!name)
    return NULL;
  cc_feature_shortcut_names =
      sk_array_push(cc_feature_shortcut_names, sizeof(name), &name);
  if (!cc_feature_shortcut_names)
    return NULL;
  return cc_feature_shortcut_names[sk_array_len(cc_feature_shortcut_names) - 1];
}

static void cc_fill_feature_shortcuts(void) {
  int n = sk_config_feature_shortcut_count();
  for (int i = 0; i < n; i++) {
    char *name = sk_config_feature_shortcut_name_at(i);
    if (!name)
      continue;
    const char *tracked = cc_feature_shortcut_track(name);
    if (!tracked) {
      free(name);
      continue;
    }
    CcEntry *e = cc_entry_add(tracked, "Shortcut", tracked, NULL);
    if (e) {
      e->on_enter = cc_run_feature_shortcut;
      e->keep_open = true;
    }
  }
}

static bool cc_launch_quicklink(void) {
  if (cc_staged_payload && cc_staged_payload[0])
    sk_launch_browser(cc_staged_payload);
  return true;
}

static char **cc_quicklink_targets;

static void cc_quicklinks_clear(void) {
  size_t count = sk_array_len(cc_quicklink_targets);
  for (size_t i = 0; i < count; i++)
    free(cc_quicklink_targets[i]);
  sk_array_free(cc_quicklink_targets);
  cc_quicklink_targets = NULL;
}

static const char *cc_quicklink_track(char *target) {
  if (!target)
    return NULL;
  cc_quicklink_targets =
      sk_array_push(cc_quicklink_targets, sizeof(target), &target);
  if (!cc_quicklink_targets)
    return NULL;
  return cc_quicklink_targets[sk_array_len(cc_quicklink_targets) - 1];
}

static void cc_fill_quicklinks(void) {
  int n = sk_config_quicklink_count();
  for (int i = 0; i < n; i++) {
    char *name = sk_config_quicklink_name_at(i);
    char *target = sk_config_quicklink_value_at(i);
    if (!name || !target) {
      free(name);
      free(target);
      continue;
    }
    const char *tracked = cc_quicklink_track(target);
    if (!tracked) {
      free(target);
      free(name);
      continue;
    }
    CcEntry *e = cc_entry_add(name, "QuickLink", tracked, NULL);
    free(name);
    if (e)
      e->on_enter = cc_launch_quicklink;
  }
}

static void cc_fill_applications(int limit) {
  if (!atomic_load(&cc_apps_ready)) {
    cc_load_apps();
  }
  size_t total = cc_app_total();
  for (size_t i = 0; i < total && (int)i < limit; i++) {
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

  for (size_t i = 0; i < ARRAY_LEN(cc_units); i++) {
    if (sk_string_equals_ci(s, cc_units[i].name))
      return &cc_units[i];
    for (int j = 0; cc_units[i].aliases[j]; j++)
      if (sk_string_equals_ci(s, cc_units[i].aliases[j]))
        return &cc_units[i];
  }
  if (len < 3)
    return NULL;

  for (size_t i = 0; i < ARRAY_LEN(cc_units); i++)
    for (int j = 0; cc_units[i].aliases[j]; j++)
      if (sk_string_starts_with_ci(cc_units[i].aliases[j], s))
        return &cc_units[i];

  const CcUnit *best = NULL;
  int best_d = 999;
  int max_d = len >= 6 ? 2 : 1;
  for (size_t i = 0; i < ARRAY_LEN(cc_units); i++)
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
    if (next == '\0' || isspace((unsigned char)next) ||
        strchr("+-*/^()xX", next)) {
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

#define CC_COMPACT_HEIGHT 55
static bool cc_compact_mode = false;
static bool cc_compact_expanded = false;

static void cc_update_compact_size(const PromptState *state) {
  if (!cc_compact_mode || !state || !state->eb)
    return;
  bool expanded = state->eb->filter[0] != '\0';
  if (expanded == cc_compact_expanded)
    return;
  SkRect bounds = sk_get_window_bounds();
  if (bounds.w <= 0 || bounds.h <= 0)
    return;
  int target_h = expanded ? PromptH : CC_COMPACT_HEIGHT;
  bounds.h = target_h;
  ski_resize_window_animated(&bounds, 40);
  cc_compact_expanded = expanded;
}

static bool cc_filter_match(const char *item, const char *filter) {
  if (!item || !filter || !filter[0])
    return true;
  if (sk_string_contains_ci(item, filter))
    return true;
  const char *alias = sk_alias_for(item);
  return alias && sk_string_contains_ci(alias, filter);
}

static int cc_item_rank(int index) {
  if (index < 0 || index >= cc_entry_total())
    return 1;
  return strcmp(cc_entries[index].type, "Feature") == 0 ? 0 : 1;
}

static void cc_launcher_refresh(PromptState *state) {
  cc_fallback_entry = NULL;
  if (atomic_load(&cc_entries_dirty)) {
    cc_build();
  } else {
    cc_entries = sk_array_set_len(cc_entries, sizeof(*cc_entries),
                                  (size_t)cc_entry_base);
  }

  int total = cc_entry_total();
  int n = total < MAX_VISIBLE_ITEMS ? total : MAX_VISIBLE_ITEMS;
  for (int i = 0; i < n; i++)
    cc_names[i] = cc_entries[i].label;
  if (n < MAX_VISIBLE_ITEMS && cc_calc_build_label(state->eb->filter))
    cc_names[n++] = cc_calc_label;

  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = cc_names, .count = n,
                      .filter_match = cc_filter_match,
                      .item_rank = cc_item_rank);

  if (state->lb->total_matches == 0 && n < MAX_VISIBLE_ITEMS) {
    CcEntry *fallback = cc_add_fallback();
    if (fallback) {
      cc_fallback_entry = fallback;
      cc_names[n] = fallback->label;
      sk_populate_listbox(.lb = state->lb, .filter = "", .items = &cc_names[n],
                          .count = 1);
    }
  }
}

static void cc_draw_hotkey_chip(const char *hotkey, int right_x, int y) {
  int font = FontSize - CC_HOTKEY_FONT_DELTA;
  SkRect r = sk_text_bounds(.text = hotkey, .font_size = font, .x = right_x,
                            .y = y, .align = "RIGHT");
  r.filled = true;
  r.radius = 4;
  r.color = CC_TYPE_COLOR;
  r.w += 4.5f;
  ski_draw_rect(&r);
  sk_draw_text(.x = right_x + 2, .y = y, .text = hotkey, .align = "RIGHT", .pad = 0,
               .color = SK_GRAY, .font_size = font);
}

static void cc_draw_row(const SkListItemCtx *ctx, const ListBox *lb, int row,
                        int type_x) {
  const CcEntry *e = cc_entry_for_row(lb, row);
  if (!e)
    e = cc_find(ctx->text);
  const char *icon = NULL;
  if (EnableIcons) {
    icon = (e && e->icon[0]) ? e->icon : NULL;
    if (!icon && cc_calc_label[0] && strcmp(ctx->text, cc_calc_label) == 0)
      icon = CC_CALC_ROW_ICON;
  }

  int name_x = ctx->x + CC_ITEM_TEXT_X + (icon ? FontSize + CC_ICON_GAP : 0);
  int name_w = sk_get_text_width(ctx->text, FontSize);
  if (icon)
    sk_draw_image(.path = icon, .x = ctx->x + CC_ITEM_TEXT_X,
                  .y = ctx->y + (ctx->h - FontSize) / 2 - 1, .w = FontSize,
                  .h = FontSize);
  sk_draw_text(.x = name_x, .y = ctx->y + CC_ITEM_TEXT_Y, .text = ctx->text,
               .align = "LEFT", .pad = 0);

  int cursor = name_x + name_w + CC_TYPE_GAP;
  const char *alias = sk_alias_for(ctx->text);

  if (!e) {
    return;
  }

  bool plain =
      strcmp(e->type, "Application") == 0 || strcmp(e->type, "Feature") == 0;
  const char *right_label = plain ? e->type : CC_TYPE_LABEL;
  int right_w = sk_get_text_width(right_label, FontSize);

  char hotkey[64];
  bool has_hotkey =
      cc_hotkey_for_label(ctx->text, hotkey, sizeof(hotkey)) && hotkey[0];
  int hk_font = FontSize - CC_HOTKEY_FONT_DELTA;
  int hk_w = has_hotkey ? sk_get_text_width(hotkey, hk_font) : 0;

  int left_limit = type_x - right_w;
  if (has_hotkey)
    left_limit -= hk_w + 4 + CC_TYPE_GAP;

  if (alias) {
    char chip[160];
    snprintf(chip, sizeof(chip), "[%s]", alias);
    int alias_w = sk_get_text_width(chip, FontSize - 3);
    if (cursor + alias_w + CC_TYPE_GAP <= left_limit) {
      sk_draw_text(.x = cursor, .y = ctx->y + CC_ITEM_TEXT_Y, .text = chip,
                   .align = "LEFT", .color = SK_GRAY, .font_size = FontSize);
      cursor += alias_w + CC_TYPE_GAP;
    }
  }

  if (!plain && e->type[0]) {
    int tag_size = FontSize - CC_TYPE_SIZE_DELTA;
    if (tag_size < 8)
      tag_size = FontSize;
    int tag_w = sk_get_text_width(e->type, tag_size);
    if (cursor + tag_w + CC_TYPE_GAP <= left_limit) {
      sk_draw_text(.x = cursor,
                   .y = ctx->y + CC_ITEM_TEXT_Y + CC_TYPE_SIZE_DELTA,
                   .text = e->type, .align = "LEFT", .pad = 0,
                   .font_size = tag_size, .color = CC_TYPE_COLOR);
      cursor += tag_w + CC_TYPE_GAP;
    }
  }

  bool far_fits = cursor + CC_TYPE_GAP <= type_x - right_w;
  int hk_right = (far_fits ? type_x - right_w - CC_TYPE_GAP : type_x) - 4;
  if (has_hotkey && cursor + CC_TYPE_GAP + hk_w <= hk_right)
    cc_draw_hotkey_chip(hotkey, hk_right, ctx->y + 7);
  if (far_fits)
    sk_draw_text(.x = type_x, .y = ctx->y + CC_ITEM_TEXT_Y, .text = right_label,
                 .align = "RIGHT", .pad = 0, .color = CC_TYPE_COLOR);
}

static void cc_open_actions_window(void);
static void cc_open_register_window(int kind);

static SkRect cc_keycap_box(const char *key, float x, int y, float font_size,
                            int narrow_radius) {
  SkRect box = sk_text_bounds(.text = key, .font_size = font_size, .x = x,
                              .y = y, .align = "LEFT");
  box.radius = 6;
  box.y -= 0.5f;
  box.h -= 8.5f;
  if (box.w < 15.0f) {
    box.w = 15.0f;
    box.radius = narrow_radius;
  }
  return box;
}

static void cc_on_actions_render(SkPlaceState state) {
  char hotkey[64];
  cc_copy(hotkey, sizeof(hotkey), ActionsHotkey);
  SkStringList keys = sk_string_split(hotkey, "+");
  const float spacing = keys.count > 2 ? 4.0f : 6.0f;
  const float font_size = 20;

  float caps_w = 0.0f;
  for (size_t i = 0; i < keys.count; i++) {
    int tw = sk_get_text_width(keys.items[i], (int)font_size);
    caps_w += tw < 15 ? 15 : tw;
  }
  if (keys.count > 1)
    caps_w += (float)(keys.count - 1) * spacing;

  float cap_left = state.rect.x + sk_get_text_width("Actions", 16);
  float cap_right = state.rect.x + state.rect.w - caps_w + 12;
  float start_x = state.rect.x + (state.rect.w / 2.0f) + 10.0f;
  if (start_x < cap_left)
    start_x = cap_left;
  if (start_x > cap_right)
    start_x = cap_right;

  float cursor_x = start_x;
  for (size_t i = 0; i < keys.count; i++) {
    SkRect box = cc_keycap_box(keys.items[i], cursor_x, state.rect.y,
                               font_size, 4);
    box.filled = true;
    box.color = CC_TYPE_COLOR;
    ski_draw_rect(&box);
    cursor_x += box.w + spacing;
  }

  int hover_w = (int)(cursor_x - spacing + 10.0f) - (state.rect.x + 15);
  if (hover_w < state.rect.w - 15)
    hover_w = state.rect.w - 15;
  if (state.hovered)
    sk_draw_rect(.x = state.rect.x + 15, .y = state.rect.y - 2,
                 .w = hover_w, .h = state.rect.h, .filled = true,
                 .color = sk_color_lerp(COLOR_BG, SK_BLACK, 0.3f), .radius = 6);

  cursor_x = start_x;
  for (size_t i = 0; i < keys.count; i++) {
    SkRect box = cc_keycap_box(keys.items[i], cursor_x, state.rect.y,
                               font_size, 2);
    sk_draw_text(.x = box.x, .y = box.y - 4.0f, .text = keys.items[i],
                 .align = "LEFT", .font_size = font_size,
                 .color = sk_color_lerp(COLOR_BG, SK_WHITE, 0.32f));
    cursor_x += box.w + spacing;
  }

  int lh_title = sk_get_text_height(16);
  sk_draw_text(.x = state.rect.x + 16,
               .y = state.rect.y - 2 + (state.rect.h - lh_title) / 2,
               .text = "Actions", .align = "LEFT",
               .color = sk_color_lerp(COLOR_BG, SK_GRAY, 0.52f),
               .font_size = 16);

  sk_string_list_free(&keys);
}

static void cc_launcher_render(PromptState *state) {
  sk_place_editbox(.placeholder = "Search For Something...");
  if ((!cc_compact_mode || cc_compact_expanded) &&
      state->lb->selected_index != -1) {
    int parts = 1;
    for (const char *p = ActionsHotkey; *p; p++)
      if (*p == '+')
        parts++;
    int btn_w = parts > 2 ? 210 : 180;
    sk_place_button(.name = "Actions", .x = state->width - btn_w - 25,
                    .y = state->height - 30, .w = btn_w,
                    .render = cc_on_actions_render,
                    .on_click = cc_open_actions_window);
  }

  if (sk_hotkey_down(sk_hotkey_from_string(ActionsHotkey)))
    cc_open_actions_window();
  if (sk_hotkey_down(sk_hotkey_from_string(RegHotkey)))
    cc_open_register_window(0);
  if (sk_hotkey_down(sk_hotkey_from_string(RegAliasHotkey)))
    cc_open_register_window(1);

  if (!sk_listbox_begin(.lb = state->lb, .item_height = FontSize * 2))
    return;
  int type_x = state->width - CC_ITEM_TEXT_X;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx))
    cc_draw_row(&ctx, state->lb, ctx.idx, type_x);
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

static bool cc_launcher_enter(PromptState *state) {
  if (cc_copy_calc_result(state))
    return true;
  if (sk_is_selected(state->lb)) {
    const CcEntry *e = cc_entry_for_row(state->lb, state->lb->selected_index);
    if (!e)
      e = cc_find(state->lb->selected_text);
    cc_stage(e, CC_RUN_ENTER);
    if (e && e->keep_open)
      return cc_run_pending();
    return true;
  }
  sk_query_browser(state->eb->filter);
  return true;
}

static bool cc_launcher_enter_with(PromptState *state, CcRunMode mode) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->eb->filter);
    return true;
  }
  const CcEntry *e = cc_entry_for_row(state->lb, state->lb->selected_index);
  if (!e)
    e = cc_find(state->lb->selected_text);
  cc_stage(e, mode);
  if (e && e->keep_open)
    return cc_run_pending();
  return true;
}

static void cc_state_reset_prompt(PromptState *state) {
  if (!cc_state_changed)
    return;
  cc_state_changed = false;
  if (state->eb) {
    state->eb->filter[0] = '\0';
    state->eb->cursor = 0;
  }
  if (state->lb) {
    state->lb->selected_index = -1;
    state->lb->selected_text = NULL;
  }
  cc_calc_label[0] = '\0';
}

static void cc_state_empty_refresh(PromptState *state) {
  sk_populate_listbox(.lb = state->lb, .filter = "", .items = NULL, .count = 0);
}

static void cc_refresh(PromptState *state) {
  cc_state_reset_prompt(state);
  cc_update_compact_size(state);
  switch (cc_state) {
  case CC_STATE_ADD_QUICKLINK:
  case CC_STATE_ADD_SHORTCUT:
    cc_state_empty_refresh(state);
    break;
  case CC_STATE_LAUNCHER:
  default:
    cc_launcher_refresh(state);
    break;
  }
}

static void cc_render(PromptState *state) {
  if (state->lb && state->lb->selected_index != -1) {
    snprintf(cc_selected_text, sizeof(cc_selected_text), "%s",
             state->lb->selected_text);
  } else if (state->eb) {
    cc_copy(cc_selected_text, sizeof(cc_selected_text), state->eb->filter);
  }
  cc_width = state->width;
  cc_height = state->height;
  switch (cc_state) {
  case CC_STATE_ADD_QUICKLINK:
    cc_add_quicklink_render(state);
    break;
  case CC_STATE_ADD_SHORTCUT:
    cc_add_shortcut_render(state);
    break;
  case CC_STATE_LAUNCHER:
  default:
    cc_launcher_render(state);
    break;
  }
}

static bool cc_mode_enter(PromptState *state) {
  switch (cc_state) {
  case CC_STATE_ADD_QUICKLINK:
    return cc_add_quicklink_enter(state);
  case CC_STATE_ADD_SHORTCUT:
    return cc_add_shortcut_enter(state);
  default:
    return false;
  }
}

static bool cc_finish_enter(CcState was, bool close) {
  if (was == CC_STATE_LAUNCHER)
    return close;
  cc_state = CC_STATE_LAUNCHER;
  cc_state_changed = true;
  sk_editbox_set_active(NULL);
  sk_request_refresh();
  return false;
}

static bool cc_enter(PromptState *state) {
  CcState was = cc_state;
  bool close = was == CC_STATE_LAUNCHER ? cc_launcher_enter(state)
                                        : cc_mode_enter(state);
  return cc_finish_enter(was, close);
}

static bool cc_enter_with(PromptState *state, CcRunMode mode) {
  CcState was = cc_state;
  bool close = was == CC_STATE_LAUNCHER ? cc_launcher_enter_with(state, mode)
                                        : cc_mode_enter(state);
  return cc_finish_enter(was, close);
}

#define CC_DEFINE_ENTER(fn, mode)                                              \
  static bool fn(PromptState *state) { return cc_enter_with(state, mode); }
CC_DEFINE_ENTER(cc_ctrl_enter, CC_RUN_CTRL)
CC_DEFINE_ENTER(cc_shift_enter, CC_RUN_SHIFT)
CC_DEFINE_ENTER(cc_ctrl_shift_enter, CC_RUN_CTRL_SHIFT)
CC_DEFINE_ENTER(cc_ctrl_alt_enter, CC_RUN_CTRL_ALT)
CC_DEFINE_ENTER(cc_shift_alt_enter, CC_RUN_SHIFT_ALT)

static void cc_on_close(void) {
  cc_state = CC_STATE_LAUNCHER;
  cc_state_changed = false;
  cc_compact_mode = false;
  cc_compact_expanded = false;
  cc_run_pending();
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
  cc_state = CC_STATE_LAUNCHER;
  cc_state_changed = false;
  cc_compact_mode = CompactMode;
  cc_compact_expanded = false;
  sk_window(.title = "Control", .h = cc_compact_mode ? CC_COMPACT_HEIGHT : PromptH,
            .anchor_y_to_prompt_h = cc_compact_mode,
            .refresh = cc_refresh, .enter = cc_enter,
            .ctrl_enter = cc_ctrl_enter, .shift_enter = cc_shift_enter,
            .ctrl_shift_enter = cc_ctrl_shift_enter,
            .ctrl_alt_enter = cc_ctrl_alt_enter,
            .shift_alt_enter = cc_shift_alt_enter, .render = cc_render,
            .on_close = cc_on_close);
}

static int cc_sub_w(void) { return cc_width / 4; }
static int cc_sub_h(void) { return cc_height / 7; }
static int cc_sub_x(void) { return cc_width / 2 + cc_sub_w() - 10; }
static int cc_sub_y(void) { return cc_height / 2 - 30; }

static const char *cc_action_names[] = {"Register Hotkey", "Register Alias"};

static void cc_actions_refresh(PromptState *state) {
  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = cc_action_names,
                      .count = (int)ARRAY_LEN(cc_action_names));
}

static void cc_actions_render(PromptState *state) {
  sk_place_editbox();
  if (!sk_listbox_begin(.lb = state->lb))
    return;
  SkListItemCtx ctx;
  sk_draw_line(.x1 = 0, .y1 = 55, .x2 = state->width, .y2 = 55,
               .color = COLOR_BORDER);
  while (sk_listbox_next(&ctx)) {
    sk_draw_text(.y = ctx.y - 1, .text = ctx.text, .align = "LEFT", .pad = 10,
                 .font_size = 20);
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
               .refresh = cc_actions_refresh, .enter = cc_actions_enter,
               .no_separators = true);
}

static char cc_register_target[256];
static char cc_captured_hotkey[128];
static SkTimer cc_flash_timer;
static bool cc_flashing;

static void cc_hotkey_capture_render(PromptState *state) {
  char combo[128] = "";
  if (sk_hotkey_capture_poll(combo, sizeof(combo))) {
    cc_bind_hotkey(cc_register_target, combo);
    cc_copy(cc_captured_hotkey, sizeof(cc_captured_hotkey), combo);
    cc_flashing = true;
    cc_flash_timer = sk_timer_start();
    sk_hotkey_capture_begin();
  }
  if (cc_flashing && sk_timer_end(cc_flash_timer) >= 1.0)
    cc_flashing = false;

  const char *preview = sk_hotkey_capture_preview();
  sk_draw_text(.x = 25, .y = 20, .text = "Hold A Key Sequence", .font_size = 15,
               .color = CC_TYPE_COLOR, .align = "LEFT");

  sk_draw_rect(.x = 20, .y = state->height / 2, .h = 40, .w = 180, .radius = 6,
               .color = cc_flashing ? CC_TYPE_COLOR
                                    : sk_color_lerp(COLOR_BG, SK_BLACK, 0.12f),
               .filled = true);

  if (preview[0] || cc_flashing)
    sk_draw_text(.x = 27, .y = state->height / 2 + 5,
                 .text = preview[0] ? preview : cc_captured_hotkey,
                 .font_size = 22,
                 .color = cc_flashing ? SK_DARK_GREEN : SK_GRAY,
                 .align = "LEFT");
}

static void cc_hotkey_capture_close(void) { sk_hotkey_capture_end(); }

static void cc_alias_render(PromptState *state) {
  char label[284];
  snprintf(label, sizeof(label), "Alias For: %s", cc_register_target);

  sk_draw_text(.x = 14, .y = 8, .text = label, .font_size = 15,
               .color = SK_GRAY, .align = "LEFT", .pad = 0);

  sk_place_editbox(.x = (state->width - 160) / 2,
                   .y = (state->height - 30) / 2 + 6, .w = 160, .h = 26,
                   .placeholder = "alias name", .caret_h = 16, .caret_w = 1,
                   .caret_y = 40, .font_size = 14);
}

static bool cc_alias_enter(PromptState *state) {
  char alias[256];
  cc_trim_copy(state->eb->filter, alias, sizeof(alias));
  if (alias[0])
    sk_config_set_alias(cc_register_target, alias);
  return true;
}

static void cc_alias_close(void) { sk_enable_global_hotkeys(); }

static void cc_open_register_window(int kind) {
  if (kind != 0 && kind != 1)
    return;
  cc_copy(cc_register_target, sizeof(cc_register_target), cc_selected_text);
  int x = cc_sub_x();
  int y = cc_sub_y() + cc_sub_h() * 2 + 30;
  int w = cc_sub_w();
  int h = cc_sub_h();
  if (kind == 0) {
    sk_hotkey_capture_begin();
    sk_subwindow(.title = "Hotkey Reg", .x = x, .y = y, .w = w, .h = h,
                 .render = cc_hotkey_capture_render,
                 .on_close = cc_hotkey_capture_close);
  } else {
    sk_disable_global_hotkeys();
    sk_subwindow(.title = "Alias Reg", .x = x, .y = y, .w = w, .h = h,
                 .render = cc_alias_render, .enter = cc_alias_enter,
                 .on_close = cc_alias_close);
  }
}

static void cc_deferred_build(void) { cc_build(); }

SKALD_FEATURE_INIT {
  atomic_store(&cc_stopping, false);
  atomic_store(&cc_apps_ready, false);
  atomic_store(&cc_entries_dirty, true);
  cc_entries = sk_array_set_len(cc_entries, sizeof(*cc_entries), 0);
  cc_entry_base = 0;

  cc_prewarm_apps();
  cc_build();
  sk_register_hotkey(.hotkey = (char *)ControlHotkey, .cb = cc_open_control);
  sk_register_hotkey(.hotkey = (char *)ReloadFeaturesHotkey, .cb = cc_reload);
  sk_register_deferred_init(cc_deferred_build);
}

SKALD_FEATURE_SHUTDOWN {
  atomic_store(&cc_stopping, true);
  cc_apps_free();
  sk_array_free(cc_entries);
  cc_entries = NULL;
  cc_entry_base = 0;
}
