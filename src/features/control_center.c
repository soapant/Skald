<<<<<<< HEAD
#include "core/flapi.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef void (*CcFill)(void);

typedef struct {
    char label[256];
    char type[64];
    char payload[MAX_PATH];
    char icon[MAX_PATH];
    void (*on_enter)(void);
    void (*on_ctrl_enter)(void);
    void (*on_tab)(void);
    void (*on_action)(void);
} CcEntry;

#define CC_MAX_TYPES 16
#define CC_MAX_ACTIONS 64
#define CC_FEATURE_MAX 130
#define CC_MAYBE_UNUSED __attribute__((unused))

#define CC_ITEM_TEXT_X 10
#define CC_ITEM_TEXT_Y 4

#define CC_ICON_GAP 8

static const char *g_type_names[CC_MAX_TYPES];
static CcFill g_type_fills[CC_MAX_TYPES];
static int g_type_count;

typedef struct {
    char label[256];
    char type[64];
    void (*fn)(void);
} CcActionDef;

static CcActionDef g_actions[CC_MAX_ACTIONS];
static int g_action_count;

static CcEntry *g_entries;
static int g_entry_count;
static int g_entry_cap;

static CcEntry *cc_entry_new(void) {
        if (g_entry_count >= g_entry_cap) {
        int cap = g_entry_cap ? g_entry_cap * 2 : 128;
        CcEntry *grown = (CcEntry *)realloc(g_entries, (size_t)cap * sizeof(CcEntry));
        if (!grown) return NULL;
        g_entries = grown;
        g_entry_cap = cap;
    }
    CcEntry *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    {  return e; }
}

static CcEntry *cc_entry_add_icon(const char *label, const char *type, const char *payload, const char *icon) {
        if (!label || !label[0]) {  return NULL; }
    if (g_entry_count >= MAX_VISIBLE_ITEMS) {  return NULL; }
    CcEntry *e = cc_entry_new();
    if (!e) {  return NULL; }
    snprintf(e->label, sizeof(e->label), "%s", label);
    snprintf(e->type, sizeof(e->type), "%s", type ? type : "");
    snprintf(e->payload, sizeof(e->payload), "%s", payload ? payload : "");
    snprintf(e->icon, sizeof(e->icon), "%s", icon ? icon : "");
    {  return e; }
}

static CcEntry *cc_entry_add(const char *label, const char *type, const char *payload) {
        {  return cc_entry_add_icon(label, type, payload, NULL); }
}

static CcEntry *cc_entry_add_fn(const char *label, const char *type, void (*fn)(void)) {
        CcEntry *e = cc_entry_add(label, type, "");
    if (e) e->on_action = fn;
    {  return e; }
}

static bool cc_register_type(const char *name, CcFill fill) {
        if (!name || !fill || g_type_count >= CC_MAX_TYPES) {  return false; }
    g_type_names[g_type_count] = name;
    g_type_fills[g_type_count] = fill;
    g_type_count++;
    {  return true; }
}

static CC_MAYBE_UNUSED bool cc_add_action(const char *label, const char *type, void (*fn)(void)) {
        if (!label || !label[0] || !fn || g_action_count >= CC_MAX_ACTIONS) {  return false; }
    CcActionDef *a = &g_actions[g_action_count++];
    snprintf(a->label, sizeof(a->label), "%s", label);
    snprintf(a->type, sizeof(a->type), "%s", type ? type : "");
    a->fn = fn;
    {  return true; }
}

static bool g_cc_fallback_active = false;

static CcEntry *cc_add_fallback(void) {
        CcEntry *fallback = cc_entry_new();
    if (!fallback) {  return NULL; }
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s Search", UsedSearchEngine);
    buffer[0] = toupper((unsigned char)buffer[0]);
    snprintf(fallback->label, sizeof(fallback->label), "%s", buffer);
    snprintf(fallback->icon, sizeof(fallback->icon), "resources/%s.png", UsedSearchEngine);
    {  return fallback; }
}

static void cc_build(void) {
        g_entry_count = 0;
    for (int i = 0; i < g_type_count; i++) g_type_fills[i]();
    for (int i = 0; i < g_action_count; i++) {
        CcEntry *e = cc_entry_add(g_actions[i].label, g_actions[i].type, "");
        if (e) e->on_action = g_actions[i].fn;
    }
    for (int i = 0; i < fl_cc_entry_count(); i++) {
        const FlCcEntrySpec *s = fl_cc_entry_at(i);
        if (!s || !s->display_name || !s->display_name[0]) continue;
        CcEntry *e = cc_entry_add_icon(s->display_name, s->type ? s->type : "", "", s->icon ? s->icon : "");
        if (!e) continue;
        if (s->on_enter) e->on_enter = s->on_enter;
        if (s->on_ctrl_enter) e->on_ctrl_enter = s->on_ctrl_enter;
        if (s->on_tab) e->on_tab = s->on_tab;
        if (s->on_action) e->on_action = s->on_action;
    }
    if (g_cc_fallback_active) cc_add_fallback();
}

static const CcEntry *cc_find(const char *label) {
        if (!label || !label[0]) {  return NULL; }
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].label, label) == 0)  {
            return &g_entries[i];
        }
    }
    {  return NULL; }
}

static const char *g_cc_staged_label = NULL;
static void (*g_pending_action)(void);
static void (*g_pending_fn)(void);
static char g_pending_label[256];

static void cc_stage(const CcEntry *e, bool ctrl) {
        g_pending_action = NULL;
    g_pending_fn = NULL;
    g_pending_label[0] = '\0';
    if (!e) {  return ; }
    if (ctrl) {
        if (e->on_ctrl_enter) {
            g_pending_fn = e->on_ctrl_enter;
            snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
        }
        return;
    }
    if (e->on_action) {
        g_pending_action = e->on_action;
        snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
    } else if (e->on_enter) {
        g_pending_fn = e->on_enter;
        snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
    }
}

static void cc_run_pending(void) {
        void (*act)(void) = g_pending_action;
    void (*pfn)(void) = g_pending_fn;
    snprintf(g_pending_label, sizeof(g_pending_label), "%s", g_pending_label);
    g_pending_action = NULL;
    g_pending_fn = NULL;
    g_cc_staged_label = g_pending_label;
    if (act) act();
    else if (pfn) pfn();
    g_cc_staged_label = NULL;
}

static const char *ListOfShitToIgnore[] = {
  "node_modules",
  "git",
  "dll",
};

typedef struct {
    const char *path;
    int max_depth;
} CcScanPath;

static const CcScanPath ListOfProgramPaths[] = {
#if defined(_WIN32)
    { "C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu", FL_DIR_WALK_UNLIMITED },
    { "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs", FL_DIR_WALK_UNLIMITED },
    { "C:\\miscellaneous", FL_DIR_WALK_UNLIMITED },
    { "shell:AppsFolder", FL_DIR_WALK_UNLIMITED },
    { "C:\\Program Files\\Epic Games", 5 },
    { "C:\\Program Files (x86)\\Epic Games", 5 },
    { "D:\\Epic Games", 5 },
    { "C:\\Games", 5 },
    { "C:\\XboxGames", 5 },
    { "C:\\Program Files (x86)\\Steam\\steamapps\\common", 5 },
    { "C:\\Program Files\\Steam\\steamapps\\common", 5 },
#else
    { "/usr/share/applications", FL_DIR_WALK_UNLIMITED },
    { "/home/dog/.local/share/applications", FL_DIR_WALK_UNLIMITED },
    { "/usr/bin", FL_DIR_WALK_UNLIMITED },
    { "/home/dog/.local/bin", FL_DIR_WALK_UNLIMITED },
    { "/home/dog/miscellaneous", FL_DIR_WALK_UNLIMITED },
#endif
=======
#include "core/flapi.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef void (*CcFill)(void);

typedef struct {
    char label[256];
    char type[64];
    char payload[MAX_PATH];
    char icon[MAX_PATH];
    void (*on_enter)(void);
    void (*on_ctrl_enter)(void);
    void (*on_tab)(void);
    void (*on_action)(void);
} CcEntry;

#define CC_MAX_TYPES 16
#define CC_FEATURE_MAX 130

#define CC_ITEM_TEXT_X 10
#define CC_ITEM_TEXT_Y 4

#define CC_ICON_GAP 8

static const char *g_type_names[CC_MAX_TYPES];
static CcFill g_type_fills[CC_MAX_TYPES];
static int g_type_count;

static CcEntry *g_entries;
static int g_entry_count;
static int g_entry_cap;

static CcEntry *cc_entry_new(void) {
        if (g_entry_count >= g_entry_cap) {
        int cap = g_entry_cap ? g_entry_cap * 2 : 128;
        CcEntry *grown = (CcEntry *)realloc(g_entries, (size_t)cap * sizeof(CcEntry));
        if (!grown) return NULL;
        g_entries = grown;
        g_entry_cap = cap;
    }
    CcEntry *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    {  return e; }
}

static CcEntry *cc_entry_add_icon(const char *label, const char *type, const char *payload, const char *icon) {
        if (!label || !label[0]) {  return NULL; }
    if (g_entry_count >= MAX_VISIBLE_ITEMS) {  return NULL; }
    CcEntry *e = cc_entry_new();
    if (!e) {  return NULL; }
    snprintf(e->label, sizeof(e->label), "%s", label);
    snprintf(e->type, sizeof(e->type), "%s", type ? type : "");
    snprintf(e->payload, sizeof(e->payload), "%s", payload ? payload : "");
    snprintf(e->icon, sizeof(e->icon), "%s", icon ? icon : "");
    {  return e; }
}

static CcEntry *cc_entry_add(const char *label, const char *type, const char *payload) {
        {  return cc_entry_add_icon(label, type, payload, NULL); }
}



static bool cc_register_type(const char *name, CcFill fill) {
        if (!name || !fill || g_type_count >= CC_MAX_TYPES) {  return false; }
    g_type_names[g_type_count] = name;
    g_type_fills[g_type_count] = fill;
    g_type_count++;
    {  return true; }
}

static bool g_cc_fallback_active = false;

static CcEntry *cc_add_fallback(void) {
        CcEntry *fallback = cc_entry_new();
    if (!fallback) {  return NULL; }
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s Search", UsedSearchEngine);
    buffer[0] = toupper((unsigned char)buffer[0]);
    snprintf(fallback->label, sizeof(fallback->label), "%s", buffer);
    snprintf(fallback->icon, sizeof(fallback->icon), "resources/%s.png", UsedSearchEngine);
    {  return fallback; }
}

static void cc_build(void) {
        g_entry_count = 0;
    for (int i = 0; i < g_type_count; i++) g_type_fills[i]();
    for (int i = 0; i < fl_cc_entry_count(); i++) {
        const FlCcEntrySpec *s = fl_cc_entry_at(i);
        if (!s || !s->display_name || !s->display_name[0]) continue;
        CcEntry *e = cc_entry_add_icon(s->display_name, s->type ? s->type : "", "", s->icon ? s->icon : "");
        if (!e) continue;
        if (s->on_enter) e->on_enter = s->on_enter;
        if (s->on_ctrl_enter) e->on_ctrl_enter = s->on_ctrl_enter;
        if (s->on_tab) e->on_tab = s->on_tab;
        if (s->on_action) e->on_action = s->on_action;
    }
    if (g_cc_fallback_active) cc_add_fallback();
}

static const CcEntry *cc_find(const char *label) {
        if (!label || !label[0]) {  return NULL; }
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].label, label) == 0)  {
            return &g_entries[i];
        }
    }
    {  return NULL; }
}

static const char *g_cc_staged_label = NULL;
static void (*g_pending_action)(void);
static void (*g_pending_fn)(void);
static char g_pending_label[256];

static void cc_stage(const CcEntry *e, bool ctrl) {
        g_pending_action = NULL;
    g_pending_fn = NULL;
    g_pending_label[0] = '\0';
    if (!e) {  return ; }
    if (ctrl) {
        if (e->on_ctrl_enter) {
            g_pending_fn = e->on_ctrl_enter;
            snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
        }
        return;
    }
    if (e->on_action) {
        g_pending_action = e->on_action;
        snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
    } else if (e->on_enter) {
        g_pending_fn = e->on_enter;
        snprintf(g_pending_label, sizeof(g_pending_label), "%s", e->label);
    }
}

static void cc_run_pending(void) {
        void (*act)(void) = g_pending_action;
    void (*pfn)(void) = g_pending_fn;
    snprintf(g_pending_label, sizeof(g_pending_label), "%s", g_pending_label);
    g_pending_action = NULL;
    g_pending_fn = NULL;
    g_cc_staged_label = g_pending_label;
    if (act) act();
    else if (pfn) pfn();
    g_cc_staged_label = NULL;
}

static const char *ListOfShitToIgnore[] = {
  "node_modules",
  "git",
  "dll",
>>>>>>> 408a6905 (removed some dead functions)
};

#define CC_APPCACHE_FILE "app_launcher_cache.txt"
#define CC_APPCACHE_VERSION "#cc3"

/* Icon paths pointing at a shell link force a multi-ms shell extraction on every
   repaint. fl_get_icon_path returns an exported PNG instead, so cached entries
   left over from before that existed must be re-resolved. */
static bool cc_icon_is_shell_only(const char *icon) {
        if (!icon || !icon[0]) return false;
    const char *dot = strrchr(icon, '.');
    if (!dot) return false;
    return (strcmp(dot, ".lnk") == 0 || strcmp(dot, ".exe") == 0 || strcmp(dot, ".url") == 0);
}

static ListOfFiles g_apps = {0};

static char (*g_app_icons)[MAX_PATH] = NULL;
static size_t g_app_icons_cap = 0;

static bool cc_app_icons_reserve(size_t n) {
        if (n <= g_app_icons_cap) {  return true; }
    size_t cap = g_app_icons_cap ? g_app_icons_cap : 64;
    while (cap < n) cap *= 2;
    char (*grown)[MAX_PATH] = (char (*)[MAX_PATH])realloc(g_app_icons, cap * MAX_PATH);
    if (!grown) {  return false; }
    memset(grown + g_app_icons_cap, 0, (cap - g_app_icons_cap) * MAX_PATH);
    g_app_icons = grown;
    g_app_icons_cap = cap;
    {  return true; }
}

static void cc_app_icons_clear(void) {
        free(g_app_icons);
    g_app_icons = NULL;
    g_app_icons_cap = 0;
}

static void cc_resolve_app_icon(size_t idx) {
        if (idx >= g_app_icons_cap) {  return ; }
    g_app_icons[idx][0] = '\0';
    char *icon = fl_get_icon_path(g_apps.files[idx].path);
    if (!icon) {  return ; }
    snprintf(g_app_icons[idx], MAX_PATH, "%s", icon);
    free(icon);
}

static const char *cc_app_icon(size_t idx) {
        if (idx >= g_app_icons_cap) {  return ""; }
    {  return g_app_icons[idx]; }
}

static bool cc_app_cache_grow(ListOfFiles *l) {
        size_t new_cap = l->capacity ? l->capacity * 2 : 64;
    FileItem *grown = (FileItem *)realloc(l->files, new_cap * sizeof(FileItem));
    if (!grown) {  return false; }
    l->files = grown;
    l->capacity = new_cap;
    {  return true; }
}

static void cc_save_app_cache(void) {
        size_t n = g_apps.count;
    if (n == 0) {  return ; }
    if (!cc_app_icons_reserve(n)) {  return ; }
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += strlen(g_apps.files[i].name) + 1;
        total += strlen(g_apps.files[i].path) + 1;
        total += strlen(cc_app_icon(i)) + 2;
    }
    total += sizeof(CC_APPCACHE_VERSION);
    const char **lines = (const char **)malloc((n + 1) * sizeof(char *));
    char *store = (char *)malloc(total);
    if (!lines || !store) { free(lines); free(store); return; }
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
        const char *nm = g_apps.files[i].name;
        const char *pth = g_apps.files[i].path;
        if (!nm[0] || !pth[0]) continue;
        int k = snprintf(store + off, total - off, "%s;%s;%s", nm, pth, cc_app_icon(i));
        if (k <= 0 || (size_t)k >= total - off) continue;
        lines[written++] = store + off;
        off += (size_t)k + 1;
    }
    fl_write_to_file(CC_APPCACHE_FILE, lines, (int)written);
    free(store);
    free(lines);
}

static bool g_cc_reexported = false;

static bool cc_load_app_cache(void) {
        FlStringList lines = fl_load_file_list(CC_APPCACHE_FILE);
    if (!lines.items || lines.count <= 0) {
        if (lines.items) fl_free_file_list(&lines);
        return false;
    }
    if (strcmp(lines.items[0], CC_APPCACHE_VERSION) != 0) {
        fl_free_file_list(&lines);
        return false;
    }
    ListOfFiles tmp = {0};
    char (*icons)[MAX_PATH] = NULL;
    size_t icons_cap = 0;
    if (lines.count > 0) {
        icons_cap = 64;
        icons = (char (*)[MAX_PATH])calloc(icons_cap, MAX_PATH);
        if (!icons) { fl_free_file_list(&lines); return false; }
    }
    for (int i = 1; i < lines.count; i++) {
        char *entry = lines.items[i];
        if (!entry) continue;
        char *first = strchr(entry, ';');
        if (!first || !first[1]) continue;
        char *second = strchr(first + 1, ';');
        const char *name = entry;
        const char *path = first + 1;
        const char *icon = "";
        *first = '\0';
        if (second) {
            *second = '\0';
            icon = second + 1;
        }
        if (!path[0]) continue;
        if (tmp.count >= tmp.capacity && !cc_app_cache_grow(&tmp)) break;
        while (tmp.count >= icons_cap) {
            size_t grown_cap = icons_cap * 2;
            char (*grown)[MAX_PATH] = (char (*)[MAX_PATH])realloc(icons, grown_cap * MAX_PATH);
            if (!grown) break;
            memset(grown + icons_cap, 0, (grown_cap - icons_cap) * MAX_PATH);
            icons = grown;
            icons_cap = grown_cap;
        }
        if (tmp.count >= icons_cap) break;
        strncpy(tmp.files[tmp.count].name, name, sizeof(tmp.files[tmp.count].name) - 1);
        tmp.files[tmp.count].name[sizeof(tmp.files[tmp.count].name) - 1] = '\0';
        strncpy(tmp.files[tmp.count].path, path, sizeof(tmp.files[tmp.count].path) - 1);
        tmp.files[tmp.count].path[sizeof(tmp.files[tmp.count].path) - 1] = '\0';
        snprintf(icons[tmp.count], MAX_PATH, "%s", icon);
        tmp.count++;
    }
    fl_free_file_list(&lines);
    if (tmp.count == 0) {
        free(tmp.files);
        free(icons);
        return false;
    }
    bool reexported = false;
    g_cc_reexported = false;
    for (size_t i = 0; i < tmp.count; i++) {
        if (icons[i][0] == '\0' || cc_icon_is_shell_only(icons[i])) {
            char *resolved = fl_get_icon_path(tmp.files[i].path);
            if (resolved) {
                if (strcmp(resolved, icons[i]) != 0) reexported = true;
                snprintf(icons[i], MAX_PATH, "%s", resolved);
                free(resolved);
            }
        }
    }
    g_cc_reexported = reexported;
    if (g_apps.files) free(g_apps.files);
    g_apps = tmp;
    cc_app_icons_clear();
    if (icons) {
        g_app_icons = icons;
        g_app_icons_cap = icons_cap;
    }
    {  return true; }
}

static const char **g_xdg_paths = NULL;
static size_t g_xdg_path_count = 0;
static int *g_xdg_depths = NULL;

static void cc_build_xdg_paths(void) {
        if (g_xdg_paths) {  return ; }
    g_xdg_paths = calloc(ARRAY_LEN(ListOfProgramPaths) + 16, sizeof(char *));
    g_xdg_depths = calloc(ARRAY_LEN(ListOfProgramPaths) + 16, sizeof(int));
    if (!g_xdg_paths || !g_xdg_depths) {  return ; }
    for (size_t i = 0; i < ARRAY_LEN(ListOfProgramPaths); i++) {
        g_xdg_paths[i] = ListOfProgramPaths[i].path;
        g_xdg_depths[i] = ListOfProgramPaths[i].max_depth;
    }
    g_xdg_path_count = ARRAY_LEN(ListOfProgramPaths);
#if !defined(_WIN32)
    const char *data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs || !data_dirs[0]) data_dirs = "/usr/local/share:/usr/share";
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", data_dirs);
    char *save = buf;
    for (char *tok = strtok_r(buf, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
        char *dir = malloc(strlen(tok) + 16);
        if (!dir) continue;
        snprintf(dir, strlen(tok) + 16, "%s/applications", tok);
        g_xdg_paths[g_xdg_path_count] = dir;
        g_xdg_depths[g_xdg_path_count] = FL_DIR_WALK_UNLIMITED;
        g_xdg_path_count++;
    }
#endif
}

static void cc_scan_app_dirs(void) {
    #if defined(_WIN32)
    char *allowed[] = {"lnk", "exe", "url", NULL};
#else
    char *allowed[] = {"desktop", NULL};
#endif
    ListOfFiles tmp = {0};
    cc_build_xdg_paths();
    for (size_t i = 0; i < g_xdg_path_count; i++) {
        const char *p = g_xdg_paths[i];
        if (!p || p[0] == '\0') continue;
        fl_get_all_files_from_dir((char *)p, allowed, &tmp, (char **)ListOfShitToIgnore, true, g_xdg_depths[i]);
    }
    for (size_t i = 0; i < tmp.count; ) {
        bool skip = false;
        char display_name[256] = {0};
        if (strstr(tmp.files[i].path, ".desktop")) {
            bool nodisplay = false;
            fl_parse_desktop_file(tmp.files[i].path, display_name, sizeof(display_name), &nodisplay);
            if (nodisplay) skip = true;
        }
        if (skip) {
            for (size_t k = i; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k + 1];
            tmp.count--;
            continue;
        }
        i++;
    }
    for (size_t i = 0; i < tmp.count; i++) {
        for (size_t j = i + 1; j < tmp.count; ) {
            if (fl_string_contains(tmp.files[i].name, tmp.files[j].name)) {
                for (size_t k = j; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k + 1];
                tmp.count--;
            } else {
                j++;
            }
        }
    }
    if (g_apps.files) free(g_apps.files);
    g_apps = tmp;
    cc_app_icons_clear();
    if (cc_app_icons_reserve(g_apps.count)) {
        for (size_t i = 0; i < g_apps.count; i++) cc_resolve_app_icon(i);
    }
}

static FlMutex g_app_cache_mtx = NULL;
static bool g_app_cache_ready = false;

/* Icon extraction costs several ms per app, so the cache is built on a worker
   thread during startup. Callers block on the mutex until it is available. */
static void cc_ensure_app_cache(void) {
    if (!g_app_cache_mtx) return;
    fl_mutex_lock(g_app_cache_mtx);
    if (!g_app_cache_ready) {
        int loaded = cc_load_app_cache();
        if (!loaded) {
            cc_scan_app_dirs();
            cc_save_app_cache();
        } else if (g_cc_reexported) {
            cc_save_app_cache();
        }
        g_app_cache_ready = true;
    }
    fl_mutex_unlock(g_app_cache_mtx);
}

static void* cc_prewarm_app_cache(void* arg) {
    (void)arg;
    cc_ensure_app_cache();
    return NULL;
}

static bool cc_app_ext_allowed(const char *name) {
    #if defined(_WIN32)
    static const char *exts[] = {"lnk", "exe", "url", NULL};
#else
    static const char *exts[] = {"desktop", NULL};
#endif
    const char *dot = strrchr(name, '.');
    if (!dot || !dot[1]) {  return false; }
    for (int i = 0; exts[i]; i++) {
        if (strcasecmp(dot + 1, exts[i]) == 0) return true;
    }
    {  return false; }
}

static void cc_app_cache_drop(const char *full_path) {
        for (size_t i = 0; i < g_apps.count; i++) {
        if (strcmp(g_apps.files[i].path, full_path) == 0) {
            for (size_t k = i; k + 1 < g_apps.count; k++) {
                g_apps.files[k] = g_apps.files[k + 1];
                if (k + 1 < g_app_icons_cap) memcpy(g_app_icons[k], g_app_icons[k + 1], MAX_PATH);
            }
            g_apps.count--;
            return;
        }
    }
}

static void cc_app_cache_add(const char *full_path) {
        for (size_t i = 0; i < g_apps.count; i++) {
        if (strcmp(g_apps.files[i].path, full_path) == 0) return;
    }
    const char *base = strrchr(full_path, '/');
    if (!base) base = strrchr(full_path, '\\');
    base = base ? base + 1 : full_path;
    if (!cc_app_ext_allowed(base)) {  return ; }
    size_t before = g_apps.count;
    ListOfFiles tmp = {0};
    fl_get_all_files_from_dir(full_path, NULL, &tmp, NULL, true, FL_DIR_WALK_UNLIMITED);
    if (tmp.count > 0) {
        if (g_apps.count >= g_apps.capacity && !cc_app_cache_grow(&g_apps)) { free(tmp.files); return; }
        if (!cc_app_icons_reserve(g_apps.count + 1)) { free(tmp.files); return; }
        g_apps.files[g_apps.count++] = tmp.files[0];
        cc_resolve_app_icon(g_apps.count - 1);
        tmp.count = 0;
    }
    if (tmp.files) free(tmp.files);
    if (g_apps.count != before) {
        cc_save_app_cache();
        printf("[control_center] incremental add '%s' count=%zu\n", base, g_apps.count);
        fflush(stdout);
    }
}

static void cc_rescan_app_cache(const char *full_path, int added) {
        if (!full_path || !g_app_cache_ready) {  return ; }
    if (added) cc_app_cache_add(full_path);
    else cc_app_cache_drop(full_path);
}

/* Name -> path lookup for application entries. Lets every callback keep the
 * uniform void(void) signature; the staged entry name resolves the path here. */
#define CC_APP_MAP_SLOTS 2048
static char g_cc_app_map_key[CC_APP_MAP_SLOTS][128];
static char g_cc_app_map_path[CC_APP_MAP_SLOTS][MAX_PATH];

static unsigned cc_map_hash(const char *s) {
        unsigned h = 2166136261u;
    while (s && *s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    {  return h; }
}

static void cc_app_map_clear(void) {
        memset(g_cc_app_map_key, 0, sizeof(g_cc_app_map_key));
    memset(g_cc_app_map_path, 0, sizeof(g_cc_app_map_path));
}

static void cc_app_map_put(const char *name, const char *path) {
        if (!name || !name[0] || !path || !path[0]) {  return ; }
    unsigned i = cc_map_hash(name) % CC_APP_MAP_SLOTS;
    for (unsigned n = 0; n < CC_APP_MAP_SLOTS; n++) {
        unsigned slot = (i + n) % CC_APP_MAP_SLOTS;
        if (g_cc_app_map_key[slot][0] == '\0' || strcmp(g_cc_app_map_key[slot], name) == 0) {
            snprintf(g_cc_app_map_key[slot], sizeof(g_cc_app_map_key[slot]), "%s", name);
            snprintf(g_cc_app_map_path[slot], sizeof(g_cc_app_map_path[slot]), "%s", path);
            return;
        }
    }
}

static const char *cc_app_map_get(const char *name) {
        if (!name || !name[0]) {  return NULL; }
    unsigned i = cc_map_hash(name) % CC_APP_MAP_SLOTS;
    for (unsigned n = 0; n < CC_APP_MAP_SLOTS; n++) {
        unsigned slot = (i + n) % CC_APP_MAP_SLOTS;
        if (g_cc_app_map_key[slot][0] == '\0') return NULL;
        if (strcmp(g_cc_app_map_key[slot], name) == 0) return g_cc_app_map_path[slot];
    }
    {  return NULL; }
}

static void cc_launch_app(void) {
        const char *path = g_cc_staged_label ? cc_app_map_get(g_cc_staged_label) : NULL;
    if (path && path[0]) fl_launch_with_args((char *)path, NULL);
}

static void cc_launch_app_as_admin(void) {
        const char *path = g_cc_staged_label ? cc_app_map_get(g_cc_staged_label) : NULL;
    if (path && path[0]) fl_launch_with_args_as_admin((char *)path, NULL);
}

static void cc_fill_applications(void) {
        cc_ensure_app_cache();
    cc_app_map_clear();
    for (size_t i = 0; i < g_apps.count; i++) {
        CcEntry *e = cc_entry_add_icon(g_apps.files[i].name, "Application",
                                       g_apps.files[i].path, cc_app_icon(i));
        if (!e) break;
        cc_app_map_put(g_apps.files[i].name, g_apps.files[i].path);
        e->on_enter = cc_launch_app;
        e->on_ctrl_enter = cc_launch_app_as_admin;
    }
}

static const char *cc_feature_names[CC_FEATURE_MAX];
static void (*cc_feature_cbs[CC_FEATURE_MAX])(void);

static void cc_fill_features(void) {
        int n = fl_get_feature_list(cc_feature_names, cc_feature_cbs, CC_FEATURE_MAX);
    for (int i = 0; i < n; i++) {
        if (!cc_entry_add_fn(cc_feature_names[i], "Feature", cc_feature_cbs[i])) break;
    }
}

static const char **g_names = NULL;
static const char **g_no_results = NULL;

static void cc_refresh(const char *filter, ListBoxInfo *lb) {
        cc_build();
    if (!g_names || !g_no_results) {
        g_names = (const char **)malloc(sizeof(char *) * MAX_VISIBLE_ITEMS);
        g_no_results = (const char **)malloc(sizeof(char *) * MAX_VISIBLE_ITEMS);
        if (!g_names) return;
        if (!g_no_results) return;
    }
    int n = g_entry_count;
    for (int i = 0; i < n; i++) g_names[i] = g_entries[i].label;
    fl_populate_listbox(lb, filter, g_names, n);
    if (lb->total_matches > 0) g_cc_fallback_active = false;
    if (lb->total_matches == 0) {
       CcEntry *fallback = cc_add_fallback();
       if (fallback) {
           g_names[n] = fallback->label;
           fl_populate_listbox(lb, "", &g_names[n], 1);
           g_cc_fallback_active = true;
       }
    }
}

static unsigned long cc_type_color(void) {
        unsigned int h = Color_Lerp(rgb_to_hex(COLOR_TEXT), rgb_to_hex(COLOR_BG), 0.45f);
    {  return RGB((h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF); }
}

static bool cc_icon_is_glyph(const char *icon) {
        if (!icon || !icon[0]) {  return false; }
    unsigned char c = (unsigned char)icon[0];
    int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (len <= 0) {  return false; }
    const char *tail = icon + len;
    if (tail[0] == '\0') {  return true; }
    if ((unsigned char)tail[0] == 0xEF && (unsigned char)tail[1] == 0xB8 && (unsigned char)tail[2] == 0x8F && tail[3] == '\0') {  return true; }
    {  return false; }
}

static void cc_render(PromptState *state) {
        ListBoxInfo *lb = &state->listbox;
    FlListSpec spec = { .lb = lb };
    fl_place_editbox();
    if (!fl_listbox_begin(&spec)) {  return ; }
    int type_x = state->width - CC_ITEM_TEXT_X;
    int icon_size = FontSize;
    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        int name_x = ctx.x + CC_ITEM_TEXT_X + icon_size + CC_ICON_GAP;
        const CcEntry *e = cc_find(ctx.text);
        if (e && e->icon[0]) {
            int icon_x = ctx.x + CC_ITEM_TEXT_X;
            int icon_y = ctx.y + (ctx.h - icon_size) / 2;
            if (cc_icon_is_glyph(e->icon)) {
                int gh = fl_get_glyph_ink_height(e->icon, icon_size);
                if (gh <= 0) gh = icon_size;
                int gw = fl_get_text_width(e->icon, icon_size);
                fl_draw_text(.x = icon_x + (icon_size - gw) / 2,
                             .y = ctx.y + (ctx.h - gh) / 2,
                             .text = e->icon, .align = "LEFT", .pad = 0,
                             .fontsize = icon_size);
            } else {
                fl_draw_image(.path = e->icon,
                              .x = icon_x, .y = icon_y,
                              .w = icon_size, .h = icon_size);
            }
        }
        fl_draw_text(.x = name_x, .y = ctx.y + CC_ITEM_TEXT_Y,
                     .text = ctx.text, .align = "LEFT", .pad = name_x);
        if (e && e->type[0]) {
            fl_draw_text(.x = type_x, .y = ctx.y + CC_ITEM_TEXT_Y,
                         .text = e->type, .align = "RIGHT", .color = cc_type_color());
        }
    }
    fl_listbox_end();
}

static bool cc_enter(const char *selection, ListBoxInfo *lb) {
        if (!fl_is_selected(lb)) {
        fl_query_browser(selection);
    }
    cc_stage(cc_find(selection), false);
    {  return true; }
}

static bool cc_ctrl_enter(const char *selection, ListBoxInfo *lb) {
        if (!fl_is_selected(lb)) {
        fl_query_browser(selection);
    }
    cc_stage(cc_find(selection), true);
    {  return true; }
}

static void cc_on_close(void) {
        cc_run_pending();
}

static void cc_tab(ListBoxInfo *lb) {
        const CcEntry *e = cc_find(ListBoxInfo_GetSelectedText(lb));
    if (e && e->on_tab) e->on_tab();
}

static void TriggerControl(void) {
        cc_build_xdg_paths();
    for (size_t i = 0; i < g_xdg_path_count; i++) {
        const char *p = g_xdg_paths[i];
        if (p) fl_on_dir_changed(p, cc_rescan_app_cache);
    }
    PROMPT(.title = "⚡ Control",
           .refresh = cc_refresh,
           .enter = cc_enter,
           .ctrl_enter = cc_ctrl_enter,
           .tab = cc_tab,
           .render = cc_render,
           .on_close = cc_on_close);
}

FLINT_FEATURE_INIT {
  g_app_cache_mtx = fl_mutex_create();
  if (g_app_cache_mtx) fl_thread_create(cc_prewarm_app_cache, NULL);
  cc_register_type("Application", cc_fill_applications);
  cc_register_type("Feature", cc_fill_features);
  fl_register_hotkey((char *)ControlHotkey, TriggerControl,"Fortnite");
}
