#include "core/skapi.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

static size_t ccx_strcpy_trunc(char *dst, size_t dst_size, const char *src);

typedef void (*CcFill)(void);

typedef struct {
  char label[256];
  char type[64];
  char payload[MAX_PATH];
  char icon[MAX_PATH];
  bool (*on_enter)(void);
  bool (*on_ctrl_enter)(void);
  bool (*on_shift_enter)(void);
  bool (*on_ctrl_shift_enter)(void);
  bool (*on_ctrl_alt_enter)(void);
  bool (*on_shift_alt_enter)(void);
  bool (*on_tab)(void);
  void (*on_action)(void);
} CcEntry;

#define CC_MAX_TYPES 16
#define CC_MAX_ACTIONS 64
#define CC_MAYBE_UNUSED __attribute__((unused))

static volatile int cc_stopping = 0;

#define CC_ITEM_TEXT_X 10
#define CC_ITEM_TEXT_Y 4

#define CC_ICON_GAP 8
#define CC_TYPE_GAP 6
#define CC_TYPE_SIZE_DELTA 2
#define CC_TYPE_LABEL "Command"

static const char *type_names[CC_MAX_TYPES];
static CcFill type_fills[CC_MAX_TYPES];
static int type_count;

typedef struct {
  char label[256];
  char type[64];
  void (*fn)(void);
} CcActionDef;

static CcActionDef actions[CC_MAX_ACTIONS];
static int action_count;

static CcEntry *entries;
static int entry_count;
static int entry_cap;

static CcEntry *cc_entry_new(void) {
  if (entry_count >= entry_cap) {
    int cap = entry_cap ? entry_cap * 2 : 128;
    CcEntry *grown = (CcEntry *)realloc(entries, (size_t)cap * sizeof(CcEntry));
    if (!grown)
      return NULL;
    entries = grown;
    entry_cap = cap;
  }
  CcEntry *e = &entries[entry_count++];
  memset(e, 0, sizeof(*e));
  return e;
}

static CcEntry *cc_entry_add_icon(const char *label, const char *type,
                                  const char *payload, const char *icon) {
  if (!label || !label[0])
    return NULL;
  if (entry_count >= MAX_VISIBLE_ITEMS)
    return NULL;
  CcEntry *e = cc_entry_new();
  if (!e)
    return NULL;
  ccx_strcpy_trunc(e->label, sizeof(e->label), label);
  ccx_strcpy_trunc(e->type, sizeof(e->type), type ? type : "");
  ccx_strcpy_trunc(e->payload, sizeof(e->payload), payload ? payload : "");
  ccx_strcpy_trunc(e->icon, sizeof(e->icon), icon ? icon : "");
  return e;
}

static CcEntry *cc_entry_add(const char *label, const char *type,
                             const char *payload) {
  return cc_entry_add_icon(label, type, payload, NULL);
}

static bool cc_register_type(const char *name, CcFill fill) {
  if (!name || !fill || type_count >= CC_MAX_TYPES)
    return false;
  type_names[type_count] = name;
  type_fills[type_count] = fill;
  type_count++;
  return true;
}

static bool cc_fallback_active = false;

static CcEntry *cc_add_fallback(void) {
  CcEntry *fallback = cc_entry_new();
  if (!fallback)
    return NULL;
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s Search", UsedSearchEngine);
  buffer[0] = toupper((unsigned char)buffer[0]);
  ccx_strcpy_trunc(fallback->label, sizeof(fallback->label), buffer);
  snprintf(fallback->icon, sizeof(fallback->icon), "/%s.png", UsedSearchEngine);
  return fallback;
}

static void cc_register_persisted_hotkeys(void);

static void cc_build(void) {
  entry_count = 0;
  for (int i = 0; i < type_count; i++)
    type_fills[i]();
  for (int i = 0; i < action_count; i++) {
    CcEntry *e = cc_entry_add(actions[i].label, actions[i].type, "");
    if (e)
      e->on_action = actions[i].fn;
  }
  for (int i = 0; i < sk_cc_entry_count(); i++) {
    const SkCcEntrySpec *s = sk_cc_entry_at(i);
    if (!s || !s->display_name || !s->display_name[0])
      continue;
    CcEntry *e = cc_entry_add_icon(s->display_name, s->type ? s->type : "", "",
                                   s->icon ? s->icon : "");
    if (!e)
      continue;
    if (s->on_enter)
      e->on_enter = s->on_enter;
    if (s->on_ctrl_enter)
      e->on_ctrl_enter = s->on_ctrl_enter;
    if (s->on_shift_enter)
      e->on_shift_enter = s->on_shift_enter;
    if (s->on_ctrl_shift_enter)
      e->on_ctrl_shift_enter = s->on_ctrl_shift_enter;
    if (s->on_ctrl_alt_enter)
      e->on_ctrl_alt_enter = s->on_ctrl_alt_enter;
    if (s->on_shift_alt_enter)
      e->on_shift_alt_enter = s->on_shift_alt_enter;
    if (s->on_tab)
      e->on_tab = s->on_tab;
    if (s->on_action)
      e->on_action = s->on_action;
  }
  if (cc_fallback_active)
    cc_add_fallback();
  cc_register_persisted_hotkeys();
}

static void cc_run_entry_by_label(const char *label);

#define CC_HK_SLOTS 64
static char cc_hotkey_targets[CC_HK_SLOTS][256];
static void cc_build(void);

static void cc_deferred_entry_build(void) { cc_build(); }

static const CcEntry *cc_find(const char *label);
static void cc_stage(const CcEntry *e, int mode);
static void cc_run_pending(void);

static void *cc_hotkey_ud(int slot) { return (void *)(uintptr_t)(slot + 1); }

static int cc_hotkey_slot_from_ud(void *ud) {
  int slot = (int)(uintptr_t)ud;
  return slot > 0 ? slot - 1 : -1;
}

static void cc_entry_hotkey_cb(void *ud) {
  int slot = cc_hotkey_slot_from_ud(ud);
  if (slot >= 0 && slot < CC_HK_SLOTS)
    cc_run_entry_by_label(cc_hotkey_targets[slot]);
}

static bool cc_hotkey_for_label(const char *label, char *out, size_t cap) {
  if (!label || !out || cap == 0)
    return false;
  out[0] = '\0';
  for (int i = 0; i < CC_HK_SLOTS; i++)
    if (cc_hotkey_targets[i][0] && strcmp(cc_hotkey_targets[i], label) == 0)
      return sk_hotkey_for_ud(cc_hotkey_ud(i), out, cap);
  return false;
}

static void cc_run_entry_by_label(const char *label) {
  const CcEntry *e = cc_find(label);
  if (!e)
    return;
  cc_stage(e, 0);
  cc_run_pending();
}

static int cc_hotkey_slot_for(const CcEntry *e) {
  if (!e)
    return -1;
  const char *name = e->label;
  int slot = -1;
  for (int i = 0; i < CC_HK_SLOTS; i++)
    if (cc_hotkey_targets[i][0] && strcmp(cc_hotkey_targets[i], name) == 0) {
      slot = i;
      break;
    }
  if (slot < 0) {
    for (int i = 0; i < CC_HK_SLOTS; i++)
      if (!cc_hotkey_targets[i][0]) {
        slot = i;
        break;
      }
  }
  if (slot < 0)
    return -1;
  ccx_strcpy_trunc(cc_hotkey_targets[slot], sizeof(cc_hotkey_targets[slot]),
                   name);
  return slot;
}

static const CcEntry *cc_find(const char *label) {
  if (!label || !label[0])
    return NULL;
  for (int i = 0; i < entry_count; i++) {
    if (strcmp(entries[i].label, label) == 0) {
      return &entries[i];
    }
  }
  return NULL;
}

static const char *cc_staged_label = NULL;
static void (*pending_action)(void);
static bool (*pending_fn)(void);
static char pending_label[256];

static void cc_stage(const CcEntry *e, int mode) {
  pending_action = NULL;
  pending_fn = NULL;
  pending_label[0] = '\0';
  if (!e)
    return;
  if (mode == 1) {
    if (e->on_ctrl_enter) {
      pending_fn = e->on_ctrl_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    }
    return;
  }
  if (mode == 2) {
    if (e->on_shift_enter) {
      pending_fn = e->on_shift_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    } else if (e->on_enter) {
      pending_fn = e->on_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    }
    return;
  }
  if (mode == 3) {
    if (e->on_ctrl_shift_enter) {
      pending_fn = e->on_ctrl_shift_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    } else if (e->on_ctrl_enter) {
      pending_fn = e->on_ctrl_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    }
    return;
  }
  if (mode == 4) {
    if (e->on_ctrl_alt_enter) {
      pending_fn = e->on_ctrl_alt_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    } else if (e->on_ctrl_enter) {
      pending_fn = e->on_ctrl_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    }
    return;
  }
  if (mode == 5) {
    if (e->on_shift_alt_enter) {
      pending_fn = e->on_shift_alt_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    } else if (e->on_enter) {
      pending_fn = e->on_enter;
      ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
    }
    return;
  }
  if (e->on_action) {
    pending_action = e->on_action;
    ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
  } else if (e->on_enter) {
    pending_fn = e->on_enter;
    ccx_strcpy_trunc(pending_label, sizeof(pending_label), e->label);
  }
}

static void cc_run_pending(void) {
  void (*act)(void) = pending_action;
  bool (*pfn)(void) = pending_fn;
  pending_action = NULL;
  pending_fn = NULL;
  cc_staged_label = pending_label;
  if (act)
    act();
  else if (pfn)
    pfn();
  cc_staged_label = NULL;
}

static const char *ListOfShitToIgnore[] = {
    "node_modules",
    "git",
    "dll",
    NULL,
};

typedef struct {
  const char *path;
  int max_depth;
} CcScanPath;

static const CcScanPath ListOfProgramPaths[] = {
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
    {"C:\\C:\\Program Files\\WindowsApps", 5},
    {"C:\\Program Files (x86)\\Steam\\steamapps\\common", 5},
    {"C:\\Program Files\\Steam\\steamapps\\common", 5},
#elif defined(__linux__)
    {"/usr/share/applications", SK_DIR_WALK_UNLIMITED},
    {"~", 6},
#endif
};

#define CC_APPCACHE_FILE "app_launcher_cache.txt"
#define CC_HOTKEY_FILE "hotkeys.txt"
#define CC_APPCACHE_VERSION "#cc4"

static bool cc_icon_is_shell_only(const char *icon) {
  if (!icon || !icon[0])
    return false;
  const char *dot = strrchr(icon, '.');
  if (!dot)
    return false;
  return (strcmp(dot, ".lnk") == 0 || strcmp(dot, ".exe") == 0 ||
          strcmp(dot, ".url") == 0);
}

static ListOfFiles apps = {0};

static char (*app_icons)[MAX_PATH] = NULL;
static size_t app_icons_cap = 0;

static bool cc_app_icons_reserve(size_t n) {
  if (n <= app_icons_cap)
    return true;
  size_t cap = app_icons_cap ? app_icons_cap : 64;
  while (cap < n)
    cap *= 2;
  char (*grown)[MAX_PATH] =
      (char (*)[MAX_PATH])realloc(app_icons, cap * MAX_PATH);
  if (!grown)
    return false;
  memset(grown + app_icons_cap, 0, (cap - app_icons_cap) * MAX_PATH);
  app_icons = grown;
  app_icons_cap = cap;
  return true;
}

static void cc_app_icons_clear(void) {
  free(app_icons);
  app_icons = NULL;
  app_icons_cap = 0;
}

static void cc_resolve_app_icon(size_t idx) {
  if (idx >= app_icons_cap)
    return;
  app_icons[idx][0] = '\0';
  char *icon = sk_get_icon_path(apps.files[idx].path);
  if (!icon)
    return;
  snprintf(app_icons[idx], MAX_PATH, "%s", icon);
  free(icon);
}

static const char *cc_app_icon(size_t idx) {
  if (idx >= app_icons_cap)
    return "";
  return app_icons[idx];
}

static bool cc_app_cache_grow(ListOfFiles *l) {
  size_t new_cap = l->capacity ? l->capacity * 2 : 64;
  FileItem *grown = (FileItem *)realloc(l->files, new_cap * sizeof(FileItem));
  if (!grown)
    return false;
  l->files = grown;
  l->capacity = new_cap;
  return true;
}

static void cc_save_app_cache(void) {
  size_t n = apps.count;
  if (n == 0)
    return;
  if (!cc_app_icons_reserve(n))
    return;
  size_t total = 0;
  for (size_t i = 0; i < n; i++) {
    total += strlen(apps.files[i].name) + 1;
    total += strlen(apps.files[i].path) + 1;
    total += strlen(cc_app_icon(i)) + 2;
  }
  total += sizeof(CC_APPCACHE_VERSION);
  const char **lines = (const char **)malloc((n + 1) * sizeof(char *));
  char *store = (char *)malloc(total);
  if (!lines || !store) {
    free(lines);
    free(store);
    return;
  }
  size_t off = 0;
  size_t written = 0;
  {
    int k = snprintf(store + off, total - off, "%s", CC_APPCACHE_VERSION);
    if (k > 0 && (size_t)k < total - off) {
      lines[written++] = store + off;
      off += (size_t)k + 1;
    }
  }
  for (size_t i = 0; i < n; i++) {
    const char *nm = apps.files[i].name;
    const char *pth = apps.files[i].path;
    if (!nm[0] || !pth[0])
      continue;
    int k =
        snprintf(store + off, total - off, "%s;%s;%s", nm, pth, cc_app_icon(i));
    if (k <= 0 || (size_t)k >= total - off)
      continue;
    lines[written++] = store + off;
    off += (size_t)k + 1;
  }
  sk_write_to_file(CC_APPCACHE_FILE, lines, (int)written);
  free(store);
  free(lines);
}

static bool cc_reexported = false;

static bool cc_load_app_cache(void) {
  SkStringList lines = sk_load_file_list(CC_APPCACHE_FILE);
  if (!lines.items || lines.count <= 0) {
    if (lines.items)
      sk_free_file_list(&lines);
    return false;
  }
  if (strcmp(lines.items[0], CC_APPCACHE_VERSION) != 0) {
    sk_free_file_list(&lines);
    return false;
  }
  ListOfFiles tmp = {0};
  char (*icons)[MAX_PATH] = NULL;
  size_t icons_cap = 0;
  if (lines.count > 0) {
    icons_cap = 64;
    icons = (char (*)[MAX_PATH])calloc(icons_cap, MAX_PATH);
    if (!icons) {
      sk_free_file_list(&lines);
      return false;
    }
  }
  for (size_t i = 1; i < lines.count; i++) {
    char *entry = lines.items[i];
    if (!entry)
      continue;
    char *first = strchr(entry, ';');
    if (!first || !first[1])
      continue;
    char *second = strchr(first + 1, ';');
    const char *name = entry;
    const char *path = first + 1;
    const char *icon = "";
    *first = '\0';
    if (second) {
      *second = '\0';
      icon = second + 1;
    }
    if (!path[0])
      continue;
    if (tmp.count >= tmp.capacity && !cc_app_cache_grow(&tmp))
      break;
    while (tmp.count >= icons_cap) {
      size_t grown_cap = icons_cap * 2;
      char (*grown)[MAX_PATH] =
          (char (*)[MAX_PATH])realloc(icons, grown_cap * MAX_PATH);
      if (!grown)
        break;
      memset(grown + icons_cap, 0, (grown_cap - icons_cap) * MAX_PATH);
      icons = grown;
      icons_cap = grown_cap;
    }
    if (tmp.count >= icons_cap)
      break;
    strncpy(tmp.files[tmp.count].name, name,
            sizeof(tmp.files[tmp.count].name) - 1);
    tmp.files[tmp.count].name[sizeof(tmp.files[tmp.count].name) - 1] = '\0';
    strncpy(tmp.files[tmp.count].path, path,
            sizeof(tmp.files[tmp.count].path) - 1);
    tmp.files[tmp.count].path[sizeof(tmp.files[tmp.count].path) - 1] = '\0';
    snprintf(icons[tmp.count], MAX_PATH, "%s", icon);
    tmp.count++;
  }
  sk_free_file_list(&lines);
  if (tmp.count == 0) {
    free(tmp.files);
    free(icons);
    return false;
  }
  bool reexported = false;
  cc_reexported = false;
  for (size_t i = 0; i < tmp.count; i++) {
    if (icons[i][0] == '\0' || cc_icon_is_shell_only(icons[i]) ||
        !sk_file_exists(icons[i])) {
      char *resolved = sk_get_icon_path(tmp.files[i].path);
      if (resolved) {
        if (strcmp(resolved, icons[i]) != 0)
          reexported = true;
        snprintf(icons[i], MAX_PATH, "%s", resolved);
        free(resolved);
      }
    }
  }
  cc_reexported = reexported;
  if (apps.files)
    free(apps.files);
  apps = tmp;
  cc_app_icons_clear();
  if (icons) {
    app_icons = icons;
    app_icons_cap = icons_cap;
  }
  return true;
}

static const char **xdg_paths = NULL;
static size_t xdg_path_count = 0;
static int *xdg_depths = NULL;

static void cc_build_xdg_paths(void) {
  if (xdg_paths)
    return;
  xdg_paths = calloc(ARRAY_LEN(ListOfProgramPaths) + 16, sizeof(char *));
  xdg_depths = calloc(ARRAY_LEN(ListOfProgramPaths) + 16, sizeof(int));
  if (!xdg_paths || !xdg_depths)
    return;
  for (size_t i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
    xdg_paths[i] = ListOfProgramPaths[i].path;
    xdg_depths[i] = ListOfProgramPaths[i].max_depth;
  }
  xdg_path_count = ARRAY_LEN(ListOfProgramPaths);
#if defined(__linux__)
  const char *data_dirs = getenv("XDG_DATA_DIRS");
  if (!data_dirs || !data_dirs[0])
    data_dirs = "/usr/local/share:/usr/share";
  char buf[4096];
  ccx_strcpy_trunc(buf, sizeof(buf), data_dirs);
  char *save = buf;
  for (char *tok = strtok_r(buf, ":", &save); tok;
       tok = strtok_r(NULL, ":", &save)) {
    char *dir = malloc(strlen(tok) + 16);
    if (!dir)
      continue;
    snprintf(dir, strlen(tok) + 16, "%s/applications", tok);
    xdg_paths[xdg_path_count] = dir;
    xdg_depths[xdg_path_count] = SK_DIR_WALK_UNLIMITED;
    xdg_path_count++;
  }
#endif
}

static void cc_scan_app_dirs(void) {
#if defined(_WIN32)
  char *allowed[] = {"lnk", "exe", "url", NULL};
#elif defined(__linux__)
  char *allowed[] = {"desktop", NULL};
#endif
  ListOfFiles tmp = {0};
  cc_build_xdg_paths();
  for (size_t i = 0; i < xdg_path_count; i++) {
    if (cc_stopping) {
      if (tmp.files)
        free(tmp.files);
      return;
    }
    const char *p = xdg_paths[i];
    if (!p || p[0] == '\0')
      continue;
    sk_get_all_files_from_dir((char *)p, allowed, &tmp,
                              (char **)ListOfShitToIgnore, true, xdg_depths[i]);
  }
  if (cc_stopping) {
    if (tmp.files)
      free(tmp.files);
    return;
  }
  for (size_t i = 0; i < tmp.count;) {
    bool skip = false;
    char display_name[256] = {0};
    if (strstr(tmp.files[i].path, ".desktop")) {
      bool nodisplay = false;
      sk_parse_desktop_file(tmp.files[i].path, display_name,
                            sizeof(display_name), &nodisplay);
      if (nodisplay)
        skip = true;
    }
    if (skip) {
      for (size_t k = i; k + 1 < tmp.count; k++)
        tmp.files[k] = tmp.files[k + 1];
      tmp.count--;
      continue;
    }
    i++;
  }
  for (size_t i = 0; i < tmp.count; i++) {
    for (size_t j = i + 1; j < tmp.count;) {
      if (sk_string_contains(tmp.files[j].name, tmp.files[i].name)) {
        for (size_t k = j; k + 1 < tmp.count; k++)
          tmp.files[k] = tmp.files[k + 1];
        tmp.count--;
      } else {
        j++;
      }
    }
  }
  if (apps.files)
    free(apps.files);
  apps = tmp;
  cc_app_icons_clear();
  if (cc_app_icons_reserve(apps.count)) {
    for (size_t i = 0; i < apps.count; i++)
      cc_resolve_app_icon(i);
  }
}

static SkMutex app_cache_mtx = NULL;
static bool app_cache_ready = false;
static SkThread prewarm_thread = NULL;

static void cc_ensure_app_cache(void) {
  if (!app_cache_mtx)
    return;
  sk_mutex_lock(app_cache_mtx);
  if (!app_cache_ready) {
    if (cc_stopping) {
      sk_mutex_unlock(app_cache_mtx);
      return;
    }
    int loaded = cc_load_app_cache();
    if (!loaded) {
      cc_scan_app_dirs();
      cc_save_app_cache();
    } else if (cc_reexported) {
      cc_save_app_cache();
    }
    app_cache_ready = true;
  }
  sk_mutex_unlock(app_cache_mtx);
}

static void *cc_prewarm_app_cache(void *arg) {
  (void)arg;
  cc_ensure_app_cache();
  return NULL;
}

#define CC_APP_MAP_SLOTS 2048
static char cc_app_map_key[CC_APP_MAP_SLOTS][128];
static char cc_app_map_path[CC_APP_MAP_SLOTS][MAX_PATH];

static unsigned cc_map_hash(const char *s) {
  unsigned h = 2166136261u;
  while (s && *s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

static void cc_app_map_clear(void) {
  memset(cc_app_map_key, 0, sizeof(cc_app_map_key));
  memset(cc_app_map_path, 0, sizeof(cc_app_map_path));
}

static void cc_app_map_put(const char *name, const char *path) {
  if (!name || !name[0] || !path || !path[0])
    return;
  unsigned i = cc_map_hash(name) % CC_APP_MAP_SLOTS;
  for (unsigned n = 0; n < CC_APP_MAP_SLOTS; n++) {
    unsigned slot = (i + n) % CC_APP_MAP_SLOTS;
    if (cc_app_map_key[slot][0] == '\0' ||
        strcmp(cc_app_map_key[slot], name) == 0) {
      ccx_strcpy_trunc(cc_app_map_key[slot], sizeof(cc_app_map_key[slot]),
                       name);
      ccx_strcpy_trunc(cc_app_map_path[slot], sizeof(cc_app_map_path[slot]),
                       path);
      return;
    }
  }
}

static const char *cc_app_map_get(const char *name) {
  if (!name || !name[0])
    return NULL;
  unsigned i = cc_map_hash(name) % CC_APP_MAP_SLOTS;
  for (unsigned n = 0; n < CC_APP_MAP_SLOTS; n++) {
    unsigned slot = (i + n) % CC_APP_MAP_SLOTS;
    if (cc_app_map_key[slot][0] == '\0')
      return NULL;
    if (strcmp(cc_app_map_key[slot], name) == 0)
      return cc_app_map_path[slot];
  }
  return NULL;
}

static bool cc_launch_app(void) {
  const char *path = cc_staged_label ? cc_app_map_get(cc_staged_label) : NULL;
  if (path && path[0])
    sk_launch_with_args((char *)path, NULL);
  return true;
}

static bool cc_launch_app_as_admin(void) {
  const char *path = cc_staged_label ? cc_app_map_get(cc_staged_label) : NULL;
  if (path && path[0])
    sk_launch_with_args_as_admin((char *)path, NULL);
  return true;
}

static void cc_fill_applications(void) {
  cc_ensure_app_cache();
  cc_app_map_clear();
  for (size_t i = 0; i < apps.count; i++) {
    CcEntry *e = cc_entry_add_icon(apps.files[i].name, "Application",
                                   apps.files[i].path, cc_app_icon(i));
    if (!e)
      break;
    cc_app_map_put(apps.files[i].name, apps.files[i].path);
    e->on_enter = cc_launch_app;
    e->on_ctrl_enter = cc_launch_app_as_admin;
  }
}

static const char *names[MAX_VISIBLE_ITEMS + 1];
static const char **no_results = NULL;

typedef struct {
  const char *canonical;
  double to_seconds;
  const char *aliases[16];
  int alias_count;
} CcCalcUnit;

static CcCalcUnit CC_CALC_UNITS[] = {
    {"ms",
     0.001,
     {"ms", "millis", "millisecond", "milliseconds", "milli", "msec", "msecs"},
     7},
    {"s",
     1.0,
     {"s", "sec", "secs", "second", "seconds", "sec.", "secs.", "sec"},
     6},
    {"min", 60.0, {"m", "min", "mins", "minute", "minutes"}, 5},
    {"hour", 3600.0, {"h", "hr", "hrs", "hour", "hours"}, 5},
    {"day", 86400.0, {"d", "day", "days"}, 3},
    {"week", 604800.0, {"w", "wk", "wks", "week", "weeks"}, 5},
    {"month", 2629746.0, {"mo", "mos", "month", "months"}, 4},
    {"year", 31557600.0, {"y", "yr", "yrs", "year", "years"}, 5},
};
#define CC_CALC_UNIT_COUNT (sizeof(CC_CALC_UNITS) / sizeof(CC_CALC_UNITS[0]))
static int ccx_str_casecmp_n(const char *a, const char *b) {
  while (*a && *b) {
    int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
    if (ca != cb)
      return ca - cb;
    a++;
    b++;
  }
  return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
static bool ccx_str_equals_ci(const char *a, const char *b) {
  if (!a || !b)
    return false;
  if (strlen(a) != strlen(b))
    return false;
  return ccx_str_casecmp_n(a, b) == 0;
}
static int ccx_levenshtein_ci(const char *s, const char *t) {
  int ls = strlen(s), lt = strlen(t);
  if (ls > 32 || lt > 32)
    return 999;
  int d[33][33];
  for (int i = 0; i <= ls; i++)
    d[i][0] = i;
  for (int j = 0; j <= lt; j++)
    d[0][j] = j;
  for (int i = 1; i <= ls; i++)
    for (int j = 1; j <= lt; j++) {
      int cost =
          tolower((unsigned char)s[i - 1]) == tolower((unsigned char)t[j - 1])
              ? 0
              : 1;
      int del = d[i - 1][j] + 1, ins = d[i][j - 1] + 1,
          sub = d[i - 1][j - 1] + cost;
      int m = del < ins ? del : ins;
      m = m < sub ? m : sub;
      d[i][j] = m;
    }
  return d[ls][lt];
}
static CcCalcUnit *ccx_find_unit_fuzzy(const char *input) {
  if (!input || !*input)
    return NULL;

  char tmp[64];
  strncpy(tmp, input, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';

  char *s = tmp;
  while (*s && isspace((unsigned char)*s))
    s++;
  char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)*(e - 1)))
    *--e = '\0';
  if (!*s)
    return NULL;

  for (size_t i = 0; i < CC_CALC_UNIT_COUNT; i++)
    for (int j = 0; j < CC_CALC_UNITS[i].alias_count; j++)
      if (ccx_str_equals_ci(s, CC_CALC_UNITS[i].aliases[j]))
        return &CC_CALC_UNITS[i];
  for (size_t i = 0; i < CC_CALC_UNIT_COUNT; i++)
    if (ccx_str_equals_ci(s, CC_CALC_UNITS[i].canonical))
      return &CC_CALC_UNITS[i];

  for (size_t i = 0; i < CC_CALC_UNIT_COUNT; i++)
    for (int j = 0; j < CC_CALC_UNITS[i].alias_count; j++) {
      if (sk_string_contains(s, CC_CALC_UNITS[i].aliases[j]) ||
          sk_string_contains(s, CC_CALC_UNITS[i].aliases[j]))
        return &CC_CALC_UNITS[i];
    }

  CcCalcUnit *best = NULL;
  int bestd = 999;
  for (size_t i = 0; i < CC_CALC_UNIT_COUNT; i++)
    for (int j = 0; j < CC_CALC_UNITS[i].alias_count; j++) {
      int d = ccx_levenshtein_ci(s, CC_CALC_UNITS[i].aliases[j]);
      if (d < bestd && d <= 2 && strlen(s) >= 3) {
        bestd = d;
        best = &CC_CALC_UNITS[i];
      }
    }
  if (best)
    return best;

  for (size_t i = 0; i < CC_CALC_UNIT_COUNT; i++) {
    int d = ccx_levenshtein_ci(s, CC_CALC_UNITS[i].canonical);
    if (d < bestd && d <= 2 && strlen(s) >= 3) {
      bestd = d;
      best = &CC_CALC_UNITS[i];
    }
  }
  return best;
}

static void ccx_format_number_plain(double num, char *out, size_t max_len) {
  if (isnan(num) || isinf(num)) {
    snprintf(out, max_len, "%g", num);
    return;
  }
  double a = fabs(num);
  if (a >= 1e12 || (a != 0 && a < 1e-6)) {
    snprintf(out, max_len, "%.6e", num);
  } else {

    snprintf(out, max_len, "%.12g", num);
  }
}

static double ccx_parse_number_with_suffix(const char *str) {
  char cleaned[64];
  strncpy(cleaned, str, sizeof(cleaned) - 1);
  cleaned[sizeof(cleaned) - 1] = '\0';
  int len = strlen(cleaned);
  while (len > 0 && isspace((unsigned char)cleaned[len - 1]))
    cleaned[--len] = '\0';
  char *p = cleaned;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p == '\0')
    return 0.0;

  len = strlen(p);
  if (len == 0)
    return 0.0;
  char suffix = tolower((unsigned char)p[len - 1]);
  if (suffix == 'k' || suffix == 'm') {

    char num_part[64];
    size_t num_len = strnlen(p, sizeof(num_part) - 1);
    if (num_len > 0)
      num_len -= 1;
    memcpy(num_part, p, num_len);
    num_part[num_len] = '\0';
    char *end = NULL;
    double v = strtod(num_part, &end);
    if (end && *end == '\0') {
      if (suffix == 'k')
        return v * 1000.0;
      if (suffix == 'm')
        return v * 1000000.0;
    }
  }
  return atof(p);
}

static size_t ccx_strcpy_trunc(char *dst, size_t dst_size, const char *src) {
  if (dst_size == 0)
    return 0;
  const char *s = src ? src : "";
  size_t n = strlen(s);
  if (n >= dst_size)
    n = dst_size - 1;
  memcpy(dst, s, n);
  dst[n] = '\0';
  return n;
}

static void ccx_trim_trailinspaces(char *str) {
  int len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1]))
    str[--len] = '\0';
}

static int ccx_precedence(char op) {
  if (op == '*' || op == '/' || op == '^')
    return 2;
  if (op == '+' || op == '-')
    return 1;
  return 0;
}
static bool ccx_apply_op(double *vals, int *vtop, char *ops, int *otop,
                         bool *ok) {
  if (*vtop < 1 || *otop < 0) {
    *ok = false;
    return false;
  }
  double b = vals[(*vtop)--];
  double a = vals[(*vtop)--];
  char op = ops[(*otop)--];
  double r = 0;
  switch (op) {
  case '+':
    r = a + b;
    break;
  case '-':
    r = a - b;
    break;
  case '*':
    r = a * b;
    break;
  case '^':
    r = pow(a, b);
    break;
  case '/':
    if (b == 0) {
      *ok = false;
      return false;
    }
    r = a / b;
    break;
  default:
    *ok = false;
    return false;
  }
  vals[++(*vtop)] = r;
  return true;
}
static double ccx_evaluate_expression(const char *expr, bool *out_ok) {
  if (!expr) {
    if (out_ok)
      *out_ok = false;
    return 0;
  }
  char clean[512];
  strncpy(clean, expr, sizeof(clean) - 1);
  clean[sizeof(clean) - 1] = '\0';

  for (int i = 0; clean[i]; i++)
    if (clean[i] == 'x' || clean[i] == 'X')
      clean[i] = '*';
  double vals[128];
  int vtop = -1;
  char ops[128];
  int otop = -1;
  bool ok = true;
  bool expect_number = true;
  int n = strlen(clean);
  for (int i = 0; i < n;) {
    if (isspace((unsigned char)clean[i])) {
      i++;
      continue;
    }
    if (clean[i] == '(') {
      ops[++otop] = '(';
      expect_number = true;
      i++;
      continue;
    }
    if (clean[i] == ')') {
      while (otop >= 0 && ops[otop] != '(') {
        if (!ccx_apply_op(vals, &vtop, ops, &otop, &ok))
          break;
      }
      if (otop >= 0 && ops[otop] == '(')
        otop--;
      else
        ok = false;
      expect_number = false;
      i++;
      continue;
    }
    if (expect_number && (clean[i] == '-' || clean[i] == '+')) {

      int j = i + 1;
      while (j < n && isspace((unsigned char)clean[j]))
        j++;
      if (j < n && (isdigit((unsigned char)clean[j]) || clean[j] == '.')) {

      } else if (clean[j] == '(') {

        vals[++vtop] = 0;
        ops[++otop] = clean[i];
        expect_number = true;
        i++;
        continue;
      } else {
        ok = false;
        break;
      }
    }
    if (isdigit((unsigned char)clean[i]) || clean[i] == '.' ||
        (expect_number && (clean[i] == '-' || clean[i] == '+'))) {
      int j = i;
      if (clean[j] == '-' || clean[j] == '+')
        j++;
      while (j < n && (isdigit((unsigned char)clean[j]) || clean[j] == '.'))
        j++;

      if (j < n && (clean[j] == 'k' || clean[j] == 'K' || clean[j] == 'm' ||
                    clean[j] == 'M')) {

        char nxt = (j + 1 < n) ? clean[j + 1] : '\0';
        if (nxt == '\0' || isspace((unsigned char)nxt) ||
            strchr("+-*^/()", nxt)) {
          j++;
        }
      }
      char numtok[64] = {0};
      int len = j - i;
      if (len >= (int)sizeof(numtok))
        len = sizeof(numtok) - 1;
      strncpy(numtok, clean + i, len);
      numtok[len] = '\0';
      double v = ccx_parse_number_with_suffix(numtok);

      if (v == 0 && numtok[0] != '0') {
      }
      vals[++vtop] = v;
      expect_number = false;
      i = j;
      continue;
    }
    if (strchr("+-^*/", clean[i])) {
      char cur = clean[i];
      while (otop >= 0 && ops[otop] != '(' &&
             ccx_precedence(ops[otop]) >= ccx_precedence(cur)) {
        if (!ccx_apply_op(vals, &vtop, ops, &otop, &ok))
          break;
      }
      ops[++otop] = cur;
      expect_number = true;
      i++;
      continue;
    }

    ok = false;
    break;
  }
  while (ok && otop >= 0) {
    if (ops[otop] == '(' || ops[otop] == ')') {
      ok = false;
      break;
    }
    if (!ccx_apply_op(vals, &vtop, ops, &otop, &ok))
      break;
  }
  if (!ok || vtop != 0) {
    if (out_ok)
      *out_ok = false;
    return 0;
  }
  if (out_ok)
    *out_ok = true;
  return vals[0];
}

static int ccx_find_in_keyword(const char *s) {
  if (!s)
    return -1;
  int n = strlen(s);
  for (int i = 0; i < n - 2; i++) {
    if (tolower((unsigned char)s[i]) == ' ' &&
        tolower((unsigned char)s[i + 1]) == 'i' &&
        tolower((unsigned char)s[i + 2]) == 'n') {

      if (i + 3 >= n || isspace((unsigned char)s[i + 3]) || s[i + 3] == '\0') {
        return i + 1;
      }
    }
  }
  return -1;
}
static void ccx_trim_copy(const char *src, char *dst, size_t cap) {
  if (!src || !dst || cap == 0)
    return;
  while (*src && isspace((unsigned char)*src))
    src++;
  size_t len = strlen(src);
  while (len > 0 && isspace((unsigned char)src[len - 1]))
    len--;
  if (len >= cap)
    len = cap - 1;
  strncpy(dst, src, len);
  dst[len] = '\0';
}

#define CC_CALC_ROW_ICON "/calculator.png"

static char cc_calc_label[512] = {0};

static bool cc_calc_strict_number(const char *str, double *out) {
  if (!str || !*str)
    return false;
  while (*str && isspace((unsigned char)*str))
    str++;
  char *end = NULL;
  double v = strtod(str, &end);
  if (end == str)
    return false;
  while (*end && isspace((unsigned char)*end))
    end++;
  if (*end == 'k' || *end == 'K') {
    v *= 1000.0;
    end++;
  } else if (*end == 'm' || *end == 'M') {
    v *= 1000000.0;
    end++;
  }
  while (*end && isspace((unsigned char)*end))
    end++;
  if (*end != '\0')
    return false;
  if (out)
    *out = v;
  return true;
}

static bool cc_calc_result_for(char *out_value, const char *filter,
                               size_t cap) {
  if (!filter || !filter[0])
    return false;
  char clean[512];
  ccx_strcpy_trunc(clean, sizeof(clean), filter);
  int in_pos = ccx_find_in_keyword(clean);
  if (in_pos >= 0) {
    char target_unit[64] = {0};
    ccx_strcpy_trunc(target_unit, sizeof(target_unit), clean + in_pos + 2);
    ccx_trim_trailinspaces(target_unit);
    char *tp = target_unit;
    while (*tp && isspace((unsigned char)*tp))
      tp++;
    memmove(target_unit, tp, strlen(tp) + 1);
    clean[in_pos - 1] = '\0';
    ccx_trim_trailinspaces(clean);
    char left_trim[256];
    ccx_trim_copy(clean, left_trim, sizeof(left_trim));
    char *last_sp = strrchr(left_trim, ' ');
    CcCalcUnit *fromU = NULL;
    char expr_part[256] = {0};
    double val = 0;
    bool ok = false;
    if (last_sp) {
      char cand[64];
      ccx_trim_copy(last_sp + 1, cand, sizeof(cand));
      CcCalcUnit *u = ccx_find_unit_fuzzy(cand);
      if (u) {
        fromU = u;
        *last_sp = '\0';
        ccx_trim_copy(left_trim, expr_part, sizeof(expr_part));
        if (expr_part[0] == '\0')
          strcpy(expr_part, "1");
        val = ccx_evaluate_expression(expr_part, &ok);
        if (!ok)
          val = ccx_parse_number_with_suffix(expr_part);
      } else {
        return false;
      }
    } else {
      int L = (int)strlen(left_trim);
      int p = L - 1;
      while (p >= 0 && isalpha((unsigned char)left_trim[p]))
        p--;
      if (p >= L - 1)
        return false;
      char unit_cand[64] = {0};
      ccx_strcpy_trunc(unit_cand, sizeof(unit_cand), left_trim + p + 1);
      CcCalcUnit *u = ccx_find_unit_fuzzy(unit_cand);
      if (!u)
        return false;
      fromU = u;
      char numpart[256] = {0};
      ccx_strcpy_trunc(numpart, sizeof(numpart), left_trim);
      numpart[p + 1] = '\0';
      ccx_trim_copy(numpart, expr_part, sizeof(expr_part));
      if (expr_part[0] == '\0')
        strcpy(expr_part, "1");
      val = ccx_evaluate_expression(expr_part, &ok);
      if (!ok)
        val = ccx_parse_number_with_suffix(expr_part);
    }
    CcCalcUnit *toU = ccx_find_unit_fuzzy(target_unit);
    if (!fromU || !toU)
      return false;
    double result = val * (fromU->to_seconds / toU->to_seconds);
    char formatted[64];
    ccx_format_number_plain(result, formatted, sizeof(formatted));
    snprintf(out_value, cap, "%s %s", formatted, toU->canonical);
    return true;
  }

  bool ok = false;
  double result = ccx_evaluate_expression(clean, &ok);
  if (ok) {
    char formatted[64];
    ccx_format_number_plain(result, formatted, sizeof(formatted));
    snprintf(out_value, cap, "%s", formatted);
    return true;
  }
  double val = 0;
  if (cc_calc_strict_number(clean, &val)) {
    char formatted[64];
    ccx_format_number_plain(val, formatted, sizeof(formatted));
    snprintf(out_value, cap, "%s", formatted);
    return true;
  }
  return false;
}

static bool cc_calc_build_label(const char *filter) {
  cc_calc_label[0] = '\0';
  if (!filter || !filter[0])
    return false;
  char trimmed[256];
  ccx_trim_copy(filter, trimmed, sizeof(trimmed));
  if (!trimmed[0])
    return false;
  int has_digit = 0;
  for (const char *p = trimmed; *p; p++) {
    if (isdigit((unsigned char)*p) || *p == '.') {
      has_digit = 1;
      break;
    }
  }
  if (!has_digit)
    return false;
  char value[128] = {0};
  if (!cc_calc_result_for(value, trimmed, sizeof(value)))
    return false;
  snprintf(cc_calc_label, sizeof(cc_calc_label), "%s = %s", trimmed, value);
  return true;
}
static bool cc_contains_ci(const char *hay, const char *needle) {
  if (!hay || !needle || !needle[0])
    return true;
  size_t nlen = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t i = 0;
    while (i < nlen && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
      i++;
    if (i == nlen)
      return true;
  }
  return false;
}

static bool cc_filter_match(const char *item, const char *filter) {
  if (!item || !filter || !filter[0])
    return true;
  if (cc_contains_ci(item, filter))
    return true;
  const char *al = sk_alias_for(item);
  if (al && cc_contains_ci(al, filter))
    return true;
  return false;
}

static void cc_refresh(PromptState *state) {
  cc_build();
  (void)no_results;
  int n = entry_count;
  if (n > MAX_VISIBLE_ITEMS)
    n = MAX_VISIBLE_ITEMS;
  for (int i = 0; i < n; i++)
    names[i] = entries[i].label;
  if (n < MAX_VISIBLE_ITEMS && cc_calc_build_label(state->eb->filter)) {
    names[n] = cc_calc_label;
    n++;
  }
  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = names, .count = n,
                      .filter_match = cc_filter_match);
  if (state->lb->total_matches > 0)
    cc_fallback_active = false;
  if (state->lb->total_matches == 0) {
    CcEntry *fallback = cc_add_fallback();
    if (fallback && n < MAX_VISIBLE_ITEMS) {
      names[n] = fallback->label;
      sk_populate_listbox(.lb = state->lb, .filter = "", .items = &names[n],
                          .count = 1);
      cc_fallback_active = true;
    }
  }
}

#define TYPE_COLOR SK_GRAY

static void cc_subwindow_refresh(PromptState *state) {
  const char *items[] = {"Register Hotkey", "Register Alias"};
  sk_populate_listbox(.lb = state->lb, .filter = state->eb->filter,
                      .items = (const char **)items, .count = ARRAY_LEN(items));
}

static int cc_width = 0, cc_height = 0;
static int actions_width = 0, actions_height = 0;

static void cc_subwindow_render(PromptState *state) {
  actions_width = state->width;
  actions_height = state->height;
  SkListSpec spec = {.lb = state->lb};
  sk_place_editbox();
  if (!sk_listbox_begin(&spec))
    return;
  SkListItemCtx ctx;

  while (sk_listbox_next(&ctx)) {
    sk_draw_text(.y = ctx.y + 6, .text = ctx.text, .align = "LEFT", .pad = 10,
                 .fontsize = 12);

    const char *hk = ctx.idx == 0 ? RegHotkey : RegAliasHotkey;
    SkRect r =
        sk_text_bounds(.text = hk, .fontsize = 10, .x = actions_width - 6,
                       .y = ctx.y + 8, .align = "RIGHT");
    r.filled = true;
    r.radius = 4;
    r.color = SK_DARK_GRAY;
    if (state->width > (10 * strlen(hk)) + 20) {
      ski_draw_rect(&r);
      sk_draw_text(.x = actions_width - 6, .y = ctx.y + 8, .text = hk,
                   .align = "RIGHT", .fontsize = 10, .color = SK_GRAY);
    }
  }
  sk_listbox_end();
}

static char current_hotkey_reg[128];
static char current_reg_action[128];
static char cc_main_selected[256];
static SkTimer reg_flash_timer;
static bool reg_flashing = false;
static int reg_last_key = 0;

static void cc_register_selected_hotkey(const char *name, const char *hotkey) {
  if (!name || !name[0] || !hotkey || !hotkey[0])
    return;
  const CcEntry *e = cc_find(name);
  int slot = cc_hotkey_slot_for(e);
  if (slot >= 0) {
    sk_register_hotkey(.hotkey = hotkey, .cb_ud = cc_entry_hotkey_cb,
                       .ud = cc_hotkey_ud(slot));
    char line[320];
    snprintf(line, sizeof(line), "%s\t%s", hotkey, name);
    sk_append_to_file(CC_HOTKEY_FILE, line);
    return;
  }
}

static const struct {
  int vk;
  const char *name;
} kCapturableKeys[] = {
    {VK_OEM_1, ";"},     {VK_OEM_2, "/"},     {VK_OEM_3, "`"},
    {VK_OEM_4, "["},     {VK_OEM_5, "\\"},    {VK_OEM_6, "]"},
    {VK_OEM_7, "'"},     {VK_OEM_COMMA, ","}, {VK_OEM_PERIOD, "."},
    {VK_OEM_MINUS, "-"}, {VK_OEM_PLUS, "="},
};

static void cc_subwindow_reg_render(PromptState *state) {
  char buf[128] = {0};
  int pos = 0;
  int rem = (int)sizeof(buf);
  sk_disable_global_hotkeys();

  if (sk_ctrl_pressed()) {
    int written = snprintf(buf + pos, rem, "Ctrl+");
    if (written > 0 && written < rem) {
      pos += written;
      rem -= written;
    }
  }
  if (sk_alt_pressed()) {
    int written = snprintf(buf + pos, rem, "Alt+");
    if (written > 0 && written < rem) {
      pos += written;
      rem -= written;
    }
  }
  if (sk_shift_pressed()) {
    int written = snprintf(buf + pos, rem, "Shift+");
    if (written > 0 && written < rem) {
      pos += written;
      rem -= written;
    }
  }

  int k = 0;
  char kname[8] = {0};
  for (int c = 'A'; c <= 'Z' && !k; c++) {
    if (sk_is_key_pressed(c)) {
      k = c;
      snprintf(kname, sizeof(kname), "%c", c);
    }
  }
  for (int c = '0'; c <= '9' && !k; c++) {
    if (sk_is_key_pressed(c)) {
      k = c;
      snprintf(kname, sizeof(kname), "%c", c);
    }
  }
  for (size_t i = 0; i < ARRAY_LEN(kCapturableKeys) && !k; i++) {
    if (sk_is_key_pressed(kCapturableKeys[i].vk)) {
      k = kCapturableKeys[i].vk;
      snprintf(kname, sizeof(kname), "%s", kCapturableKeys[i].name);
    }
  }
  if (k && rem > 1) {
    snprintf(buf + pos, rem, "%s", kname);
  }
  bool has_mod = sk_ctrl_pressed() || sk_alt_pressed() || sk_shift_pressed() ||
                 sk_super_pressed();

  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '+') {
    buf[len - 1] = '\0';
  }

  if (buf[0] && k != 0 && k != reg_last_key && has_mod) {
    cc_register_selected_hotkey(current_reg_action, buf);
    ccx_strcpy_trunc(current_hotkey_reg, sizeof(current_hotkey_reg), buf);
    reg_flashing = true;
    reg_flash_timer = sk_start_timer();
    sk_enable_global_hotkeys();
  }
  if (k == 0)
    reg_last_key = 0;
  else
    reg_last_key = k;
  if (reg_flashing && sk_end_timer(reg_flash_timer) >= 1.0)
    reg_flashing = false;

  unsigned long rect_color = reg_flashing ? SK_GRAY : SK_DARK_GRAY;
  unsigned long text_color = reg_flashing ? SK_DARK_GREEN : SK_GRAY;

  sk_draw_text(.x = 15, .y = 20, .text = "Hold A Key Sequence", .fontsize = 15,
               .color = SK_GRAY, .align = "LEFT");
  sk_draw_rect(.x = 10, .y = state->height / 2, .h = 40, .w = 180, .radius = 6,
               .color = rect_color, .filled = true);
  if (buf[0] || reg_flashing) {
    sk_draw_text(.x = 17, .y = state->height / 2,
                 .text = buf[0] ? buf : current_hotkey_reg, .fontsize = 22,
                 .color = text_color, .align = "LEFT");
  }
}

static void cc_subwindow_reg_onclose() { sk_enable_global_hotkeys(); }

static char g_alias_target[256];

static void cc_alias_reg_render(PromptState *state) {
  char label[284];
  snprintf(label, sizeof(label), "Alias For: %s", g_alias_target);
  sk_draw_text(.x = 14, .y = 8, .text = label, .fontsize = 11, .color = SK_GRAY,
               .align = "LEFT", .pad = 0);

  sk_place_editbox(.x = (state->width - 160) / 2,
                   .y = (state->height - 30) / 2 + 6, .w = 160, .h = 26,
                   .placeholder = "alias name", .caret_height = 14,
                   .caret_width = 1, .caret_y = 47, .fontsize = 14);
}

static bool cc_alias_reg_enter(PromptState *state) {
  const char *input = state->eb->filter;
  char alias[256];
  ccx_strcpy_trunc(alias, sizeof(alias), input ? input : "");
  char *p = alias;
  while (*p == ' ')
    p++;
  char *end = alias + strlen(alias) - 1;
  while (end > alias && *end == ' ') {
    *end = '\0';
    end--;
  }
  if (alias[0])
    sk_add_alias(.alias = alias, .target = g_alias_target);
  return true;
}

static void cc_alias_reg_onclose() { sk_enable_global_hotkeys(); }

#define SUBWINDOW_W cc_width / 4
#define SUBWINDOW_H cc_height / 9
#define SUBWINDOW_X (cc_width / 2) + SUBWINDOW_W - 10
#define SUBWINDOW_Y (cc_height / 2) - 50

static bool cc_reg_select(int type) {
  switch (type) {
  case 0: {
    ccx_strcpy_trunc(current_reg_action, sizeof(current_reg_action),
                     cc_main_selected);
    sk_disable_global_hotkeys();
    sk_subwindow(.title = "Hotkey Reg", .x = SUBWINDOW_X,
                 .y = SUBWINDOW_Y + (SUBWINDOW_H * 2), .w = SUBWINDOW_W, .h = SUBWINDOW_H,
                 .render = cc_subwindow_reg_render,
                 .on_close = cc_subwindow_reg_onclose);
    break;
  }
  case 1: {
    ccx_strcpy_trunc(current_reg_action, sizeof(current_reg_action),
                     cc_main_selected);
    ccx_strcpy_trunc(g_alias_target, sizeof(g_alias_target), cc_main_selected);
    sk_disable_global_hotkeys();
    sk_subwindow(.title = "Alias Reg", .x = SUBWINDOW_X,
                 .y = SUBWINDOW_Y + (SUBWINDOW_H * 2), .w = SUBWINDOW_W, .h = SUBWINDOW_H,
                 .render = cc_alias_reg_render, .enter = cc_alias_reg_enter,
                 .on_close = cc_alias_reg_onclose);
    break;
  }
  }
  return true;
}

static bool cc_subwindow_enter(PromptState *state) {
  return cc_reg_select(state->lb->selected_index);
}

#define CC_PERSIST_MAX 64

static char persisted_hotkeys[CC_PERSIST_MAX][64];
static char persisted_actions[CC_PERSIST_MAX][256];
static size_t persisted_count;

static void cc_load_persisted_hotkeys(void) {
  SkStringList lines = sk_load_file_list(CC_HOTKEY_FILE);
  for (size_t li = 0; li < lines.count && persisted_count < CC_PERSIST_MAX;
       li++) {
    char copy[512];
    ccx_strcpy_trunc(copy, sizeof(copy),
                     lines.items[li] ? lines.items[li] : "");
    char *tab = strchr(copy, '\t');
    if (!tab)
      continue;
    *tab = '\0';
    ccx_strcpy_trunc(persisted_hotkeys[persisted_count],
                     sizeof(persisted_hotkeys[0]), copy);
    ccx_strcpy_trunc(persisted_actions[persisted_count],
                     sizeof(persisted_actions[0]), tab + 1);
    persisted_count++;
  }
  sk_free_file_list(&lines);
}

static void cc_register_persisted_hotkeys(void) {
  for (size_t li = 0; li < persisted_count; li++) {
    const char *hotkey = persisted_hotkeys[li];
    const char *action = persisted_actions[li];
    const CcEntry *e = cc_find(action);
    int slot = cc_hotkey_slot_for(e);
    if (slot >= 0)
      sk_register_hotkey(.hotkey = hotkey, .cb_ud = cc_entry_hotkey_cb,
                         .ud = cc_hotkey_ud(slot));
  }
}

static void cc_open_actions_subwindow(void) {
  sk_subwindow(.title = "Actions", .x = SUBWINDOW_X,
               .y = SUBWINDOW_Y, .w = SUBWINDOW_W, .h = 300,
               .render = cc_subwindow_render, .refresh = cc_subwindow_refresh,
               .enter = cc_subwindow_enter);
}

static void cc_on_actions_render(SkPlaceState state) {
  char *hotkey_str = strdup(ActionsHotkey);
  if (hotkey_str) {
    char *plus = strchr(hotkey_str, '+');
    if (plus)
      *plus = ' ';
  }

  sk_draw_rect(.x = state.rect.x + 95, .y = state.rect.y - 2,
               .w = state.rect.w - 95, .h = state.rect.h, .filled = true,
               .color = SK_DARK_GRAY, .radius = 6);

  if (state.hovered) {
    sk_draw_rect(.x = state.rect.x + 15, .y = state.rect.y - 2,
                 .w = state.rect.w - 15, .h = state.rect.h, .filled = true,
                 .color = SK_GRAY, .radius = 6);
  }

  sk_draw_text(.x = state.rect.x + 20, .y = state.rect.y - 4, .text = "Actions",
               .align = "LEFT", .color = SK_DARK_GRAY, .fontsize = 16);
  if (hotkey_str)
    sk_draw_text(.x = state.rect.x + 100, .y = state.rect.y - 7,
                 .text = hotkey_str, .align = "LEFT", .fontsize = 20);

  free(hotkey_str);
}

static void cc_render(PromptState *state) {
  if (state->lb && state->lb->selected_index != -1) {
    const char *cur = ListBoxInfo_Row(state->lb, state->lb->selected_index);
    if (cur)
      ccx_strcpy_trunc(cc_main_selected, sizeof(cc_main_selected), cur);
  } else if (state->eb) {
    ccx_strcpy_trunc(cc_main_selected, sizeof(cc_main_selected),
                     state->eb->filter);
  }
  cc_width = state->width;
  cc_height = state->height;
  SkListSpec spec = {.lb = state->lb, .item_height = FontSize * 2};
  sk_place_editbox();
  sk_place_button(.name = "Actions", .x = state->width - 200,
                  .y = state->height - 30, .w = 180,
                  .render = cc_on_actions_render,
                  .on_click = cc_open_actions_subwindow);

  if (sk_hotkey_down(sk_hotkey_from_string(ActionsHotkey))) {
    cc_open_actions_subwindow();
  }
  if (sk_hotkey_down(sk_hotkey_from_string(RegHotkey))) {
    cc_reg_select(0);
  }
  if (sk_hotkey_down(sk_hotkey_from_string(RegAliasHotkey))) {
    cc_reg_select(1);
  }
  if (!sk_listbox_begin(&spec))
    return;
  int type_x = state->width - CC_ITEM_TEXT_X;
  SkListItemCtx ctx;
  while (sk_listbox_next(&ctx)) {
    const CcEntry *e = cc_find(ctx.text);
    const char *row_icon = (e && e->icon[0]) ? e->icon : NULL;
    int icon_size = FontSize;
    int name_x = ctx.x + CC_ITEM_TEXT_X + icon_size + CC_ICON_GAP;
    if (!row_icon && cc_calc_label[0] && strcmp(ctx.text, cc_calc_label) == 0) {
      row_icon = CC_CALC_ROW_ICON;
    }
    if (row_icon) {
      int icon_x = ctx.x + CC_ITEM_TEXT_X;
      int icon_y = ctx.y + (ctx.h - icon_size) / 2;
      sk_draw_image(.path = row_icon, .x = icon_x, .y = icon_y, .w = icon_size,
                    .h = icon_size);
    }

    sk_draw_text(.x = name_x, .y = ctx.y + CC_ITEM_TEXT_Y, .text = ctx.text,
                 .align = "LEFT", .pad = 0);

    const char *al = sk_alias_for(ctx.text);
    if (al) {
      char abuf[160];
      snprintf(abuf, sizeof(abuf), "[%s]", al);
      int name_w = sk_get_text_width(ctx.text, FontSize);
      sk_draw_text(.x = name_x + name_w + CC_TYPE_GAP,
                   .y = ctx.y + CC_ITEM_TEXT_Y + 2, .text = abuf,
                   .align = "LEFT", .pad = 0, .color = SK_GRAY,
                   .fontsize = FontSize - 3);
    }

    bool plain_type = e && (strcmp(e->type, "Application") == 0 ||
                            strcmp(e->type, "Feature") == 0);
    if (e) {
      const char *right_text = plain_type ? e->type : CC_TYPE_LABEL;
      char hk[64];
      const char *hkp =
          cc_hotkey_for_label(ctx.text, hk, sizeof(hk)) ? hk : NULL;
      if (hkp && hkp[0]) {
        int type_w = sk_get_text_width(right_text, FontSize);
        int hk_fs = FontSize - 6;
        int hk_x = type_x - type_w - CC_TYPE_GAP;
        SkRect r = sk_text_bounds(.text = hk, .fontsize = hk_fs, .x = hk_x - 4,
                                  .y = ctx.y + 10, .align = "RIGHT");
        r.filled = true;
        r.radius = 4;
        r.color = SK_DARK_GRAY;
        ski_draw_rect(&r);

        sk_draw_text(.x = hk_x - 4, .y = ctx.y + 10, .text = hk,
                     .align = "RIGHT", .pad = 0, .color = SK_GRAY,
                     .fontsize = hk_fs);
      }
    }
    if (plain_type) {
      sk_draw_text(.x = type_x, .y = ctx.y + CC_ITEM_TEXT_Y, .text = e->type,
                   .align = "RIGHT", .color = TYPE_COLOR);
    } else if (e) {
      int type_sz = FontSize - CC_TYPE_SIZE_DELTA;
      if (type_sz < 8)
        type_sz = FontSize;
      if (e->type[0]) {
        int name_w = sk_get_text_width(ctx.text, FontSize);
        sk_draw_text(.x = name_x + name_w + CC_TYPE_GAP,
                     .y = ctx.y + CC_ITEM_TEXT_Y + CC_TYPE_SIZE_DELTA,
                     .text = e->type, .align = "LEFT", .pad = 0,
                     .fontsize = type_sz, .color = TYPE_COLOR);
      }
      sk_draw_text(.x = type_x, .y = ctx.y + CC_ITEM_TEXT_Y,
                   .text = CC_TYPE_LABEL, .align = "RIGHT", .pad = 0,
                   .color = TYPE_COLOR);
    }
  }
  sk_listbox_end();
}

static bool cc_enter(PromptState *state) {
  if (cc_calc_label[0] && strcmp(state->eb->filter, cc_calc_label) == 0) {
    const char *eq = strchr(cc_calc_label, '=');
    if (eq) {
      char value[128];
      ccx_strcpy_trunc(value, sizeof(value), eq + 1);
      char *v = value;
      while (*v && isspace((unsigned char)*v))
        v++;
      ccx_trim_trailinspaces(v);
      sk_copy_to_clipboard(v);
    }
    return true;
  }
  if (sk_is_selected(state->lb)) {
    cc_stage(cc_find(state->lb->selected_text), 0);
  } else {
    sk_query_browser(state->eb->filter);
  }
  return true;
}

static bool cc_ctrl_enter(PromptState *state) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->lb->selected_text);
  }
  cc_stage(cc_find(state->lb->selected_text), 1);
  return true;
}

static bool cc_shift_enter(PromptState *state) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->lb->selected_text);
  }
  cc_stage(cc_find(state->lb->selected_text), 2);
  return true;
}
static bool cc_ctrl_shift_enter(PromptState *state) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->lb->selected_text);
  }
  cc_stage(cc_find(state->lb->selected_text), 3);
  return true;
}
static bool cc_ctrl_alt_enter(PromptState *state) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->lb->selected_text);
  }
  cc_stage(cc_find(state->lb->selected_text), 4);
  return true;
}
static bool cc_shift_alt_enter(PromptState *state) {
  if (!sk_is_selected(state->lb)) {
    sk_query_browser(state->lb->selected_text);
  }
  cc_stage(cc_find(state->lb->selected_text), 5);
  return true;
}

static void cc_on_close(void) { cc_run_pending(); }

static void cc_tab(PromptState *state) {
  const CcEntry *e = cc_find(state->lb->selected_text);
  if (e && e->on_tab)
    e->on_tab();
}

static void reload_features_and_config(void) {
  int features_reloaded = sk_reload_all_features();
  int config_reloaded = sk_reload_config();
  char buf[128];

  if (features_reloaded > 0 && config_reloaded) {
    snprintf(buf, sizeof(buf), "Reloaded %d Feature%s And Config",
             features_reloaded, features_reloaded == 1 ? "" : "s");
    sk_message(.message = buf);
  } else if (features_reloaded > 0) {
    snprintf(buf, sizeof(buf), "Reloaded %d Feature%s", features_reloaded,
             features_reloaded == 1 ? "" : "s");
    sk_message(.message = buf);
  } else if (config_reloaded) {
    sk_message(.message = "Reloaded Config");
  } else {
    sk_message(.message = "Nothing Reloaded");
  }
}

static void TriggerControl(void) {
  cc_build_xdg_paths();

  sk_window(.title = "Control", .placeholder = "Search For Something...",
            .refresh = cc_refresh, .enter = cc_enter,
            .ctrl_enter = cc_ctrl_enter, .shift_enter = cc_shift_enter,
            .ctrl_shift_enter = cc_ctrl_shift_enter,
            .ctrl_alt_enter = cc_ctrl_alt_enter,
            .shift_alt_enter = cc_shift_alt_enter, .tab = cc_tab,
            .render = cc_render, .on_close = cc_on_close);
}

SKALD_FEATURE_INIT {
  cc_stopping = 0;
  cc_load_persisted_hotkeys();
  app_cache_mtx = sk_mutex_create();
  cc_register_type("Application", cc_fill_applications);
  cc_build();
  if (app_cache_mtx)
    prewarm_thread = sk_thread_create(cc_prewarm_app_cache, NULL);
  sk_register_hotkey(.hotkey = (char *)ControlHotkey, .cb = TriggerControl);
  sk_register_hotkey(.hotkey = (char *)ReloadFeaturesHotkey,
                     .cb = reload_features_and_config);
  sk_register_deferred_init(cc_deferred_entry_build);
}

SKALD_FEATURE_SHUTDOWN {
  cc_stopping = 1;
  if (prewarm_thread) {
    sk_thread_join(prewarm_thread);
    prewarm_thread = NULL;
  }
}
