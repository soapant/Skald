#include "wm/i3_wm.h"
#include "deps/log.h"
#include "platform/platform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glob.h>
#include <strings.h>
#include <sys/stat.h>

static bool i3_sock_live(const char* path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0;
}

static int g_i3_probe = 0;
static char g_i3_sock[512];

static bool i3_ensure_sock(void) {
    if (g_i3_probe != 0) return g_i3_probe == 1;
    g_i3_probe = -1;
    const char* cur = getenv("I3SOCK");
    if (i3_sock_live(cur)) {
        snprintf(g_i3_sock, sizeof(g_i3_sock), "%s", cur);
        g_i3_probe = 1;
        return true;
    }
    const char* rtd = getenv("XDG_RUNTIME_DIR");
    if (!rtd || !rtd[0]) return false;
    char pat[512];
    snprintf(pat, sizeof(pat), "%s/i3/ipc-socket.*", rtd);
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        snprintf(g_i3_sock, sizeof(g_i3_sock), "%s", g.gl_pathv[g.gl_pathc - 1]);
        if (i3_sock_live(g_i3_sock)) {
            setenv("I3SOCK", g_i3_sock, 1);
            log_info("discovered I3SOCK='%s'", g_i3_sock);
            g_i3_probe = 1;
        }
    }
    globfree(&g);
    return g_i3_probe == 1;
}

bool i3_is_active(void) {
    return i3_ensure_sock();
}

static bool i3_succeeded(char* output) {
    if (!output) return false;
    if (output[0] == '\0') return false;
    bool ok = true;
    if (strstr(output, "\"success\":false") || strstr(output, "\"error\"")) ok = false;
    if (!ok) log_error("output='%s'", output);
    free(output);
    return ok;
}

static bool i3_run(const char* command) {
    char* out = Platform_RunTerminalCommand((char*)command, false);
    return i3_succeeded(out);
}

static const char* i3_skip_ws(const char* p) {
    if (!p) return NULL;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* i3_skip_string(const char* p) {
    if (!p || *p != '"') return NULL;
    p++;
    while (*p) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return NULL;
}

static const char* i3_skip_container(const char* p) {
    int depth = 0;
    while (p && *p) {
        if (*p == '"') {
            const char* n = i3_skip_string(p);
            if (!n) return NULL;
            p = n;
            continue;
        }
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') {
            depth--;
            if (depth == 0) return p + 1;
        }
        p++;
    }
    return NULL;
}

static const char* i3_skip_value(const char* p) {
    p = i3_skip_ws(p);
    if (!p) return NULL;
    if (*p == '"') return i3_skip_string(p);
    if (*p == '{' || *p == '[') return i3_skip_container(p);
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\t' && *p != '\r') p++;
    return p;
}

static const char* i3_obj_end(const char* obj) {
    return i3_skip_container(i3_skip_ws(obj));
}

static const char* i3_key(const char* obj, const char* end, const char* key) {
    const char* p = i3_skip_ws(obj);
    if (!p || *p != '{') return NULL;
    p++;
    size_t klen = strlen(key);
    while (p && p < end) {
        p = i3_skip_ws(p);
        if (!p || *p == '}' || *p == ',') return NULL;
        if (*p != '"') return NULL;
        const char* ke = i3_skip_string(p);
        if (!ke || ke > end) return NULL;
        if ((size_t)(ke - p - 2) == klen && strncmp(p + 1, key, klen) == 0) {
            const char* c = i3_skip_ws(ke);
            if (c && *c == ':') return i3_skip_ws(c + 1);
        }
        const char* nx = i3_skip_value(ke);
        if (!nx || nx > end) return NULL;
        p = i3_skip_ws(nx);
        if (p && *p == ',') { p++; continue; }
        return NULL;
    }
    return NULL;
}

static bool i3_str_val(const char* v, char* out, size_t cap) {
    if (!v || *v != '"' || !out || cap == 0) return false;
    const char* s = v + 1;
    const char* e = i3_skip_string(v);
    if (!e) return false;
    size_t len = (size_t)(e - s - 1);
    if (len == 0) return false;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len);
    out[len] = '\0';
    return true;
}

static bool i3_bool_val(const char* v, bool* out) {
    if (!v || !out) return false;
    if (strncmp(v, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(v, "false", 5) == 0) { *out = false; return true; }
    return false;
}

typedef struct {
    int  id;
    bool focused;
    char name[512];
    char app_id[256];
} I3Leaf;

typedef void (*I3LeafVisitor)(const I3Leaf* leaf, void* user);

static void i3_walk_array(const char* arr, const char* end, I3LeafVisitor visit, void* user);
static void i3_walk_node(const char* obj, const char* end, I3LeafVisitor visit, void* user);

static void i3_walk_node(const char* obj, const char* end, I3LeafVisitor visit, void* user) {
    char type[64] = {0};
    bool is_leaf = i3_str_val(i3_key(obj, end, "type"), type, sizeof(type)) && strcmp(type, "leaf") == 0;
    if (is_leaf) {
        I3Leaf leaf;
        memset(&leaf, 0, sizeof(leaf));
        const char* idv = i3_key(obj, end, "id");
        leaf.id = idv ? atoi(idv) : 0;
        i3_bool_val(i3_key(obj, end, "focused"), &leaf.focused);
        i3_str_val(i3_key(obj, end, "name"), leaf.name, sizeof(leaf.name));
        if (!i3_str_val(i3_key(obj, end, "app_id"), leaf.app_id, sizeof(leaf.app_id))) {
            const char* wp = i3_key(obj, end, "window_properties");
            const char* wpend = wp ? i3_obj_end(wp) : NULL;
            if (wp && wpend) i3_str_val(i3_key(wp, wpend, "class"), leaf.app_id, sizeof(leaf.app_id));
        }
        visit(&leaf, user);
        return;
    }
    i3_walk_array(i3_key(obj, end, "nodes"), end, visit, user);
    i3_walk_array(i3_key(obj, end, "floating_nodes"), end, visit, user);
}

static void i3_walk_array(const char* arr, const char* end, I3LeafVisitor visit, void* user) {
    const char* p = i3_skip_ws(arr);
    if (!p || *p != '[') return;
    const char* aend = i3_skip_container(p);
    if (!aend) return;
    p++;
    while (p && p < aend) {
        p = i3_skip_ws(p);
        if (!p || *p == ']' || p >= aend) return;
        if (*p == '{') {
            const char* oe = i3_obj_end(p);
            if (!oe) return;
            i3_walk_node(p, oe, visit, user);
            p = oe;
        } else {
            const char* ne = i3_skip_value(p);
            if (!ne) return;
            p = ne;
        }
        p = i3_skip_ws(p);
        if (p && *p == ',') { p++; continue; }
        return;
    }
}

static bool i3_get_tree(char** out) {
    if (!i3_ensure_sock()) return false;
    char* json = Platform_RunTerminalCommand("i3-msg -t get_tree", false);
    if (!json || json[0] == '\0') { log_error("i3 get_tree empty"); free(json); return false; }
    *out = json;
    return true;
}

typedef struct {
    char buffer[1024];
    bool found;
} I3TitleSearch;

static void i3_collect_focused(const I3Leaf* leaf, void* user) {
    I3TitleSearch* s = (I3TitleSearch*)user;
    if (!leaf->focused || !leaf->name[0] || s->found) return;
    snprintf(s->buffer, sizeof(s->buffer), "%s", leaf->name);
    s->found = true;
}

bool get_title_i3(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) { log_error("buffer='%s' buffer_size=%d", buffer ? buffer : "null", buffer_size); return false; }
    buffer[0] = '\0';
    if (!i3_ensure_sock()) return false;
    char* json = NULL;
    if (!i3_get_tree(&json)) return false;
    I3TitleSearch search;
    memset(&search, 0, sizeof(search));
    const char* end = json + strlen(json);
    i3_walk_array(i3_key(json, end, "nodes"), end, i3_collect_focused, &search);
    free(json);
    if (!search.found) return false;
    strncpy(buffer, search.buffer, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return buffer[0] != '\0';
}

static void i3_collect_app_id(const I3Leaf* leaf, void* user) {
    I3TitleSearch* s = (I3TitleSearch*)user;
    if (!leaf->focused || !leaf->app_id[0] || s->found) return;
    snprintf(s->buffer, sizeof(s->buffer), "%s", leaf->app_id);
    s->found = true;
}

bool get_app_id_i3(char *buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) return false;
    buffer[0] = '\0';
    if (!i3_ensure_sock()) return false;
    char* json = NULL;
    if (!i3_get_tree(&json)) return false;
    I3TitleSearch search;
    memset(&search, 0, sizeof(search));
    const char* end = json + strlen(json);
    i3_walk_array(i3_key(json, end, "nodes"), end, i3_collect_app_id, &search);
    free(json);
    if (!search.found) return false;
    strncpy(buffer, search.buffer, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return buffer[0] != '\0';
}

char* paste_i3(char* content) {
    if (!content) { log_error("content is NULL"); return NULL; }
    if (!i3_ensure_sock()) return NULL;
    log_error("paste over i3 IPC is not supported");
    return NULL;
}

typedef struct {
    const char* title;
    int id;
    int lower_id;
} I3FocusSearch;

static void i3_collect_title(const I3Leaf* leaf, void* user) {
    I3FocusSearch* s = (I3FocusSearch*)user;
    if (!leaf->name[0] || !leaf->id) return;
    if (!strcmp(leaf->name, s->title) && !s->id) { s->id = leaf->id; return; }
    if (!strcasecmp(leaf->name, s->title) && !s->lower_id) s->lower_id = leaf->id;
}

bool focus_window_title_i3(char* title) {
    if (!title) { log_error("title is NULL"); return false; }
    if (!i3_ensure_sock()) return false;
    char* trimmed = strdup(title);
    if (!trimmed) return false;
    char* a = trimmed;
    while (*a && (iscntrl((unsigned char)*a) || isspace((unsigned char)*a))) a++;
    char* b = a + strlen(a);
    while (b > a && (iscntrl((unsigned char)b[-1]) || isspace((unsigned char)b[-1]))) *--b = '\0';
    if (!*a) { free(trimmed); log_error("title empty after trim"); return false; }

    char* json = NULL;
    if (!i3_get_tree(&json)) { free(trimmed); return false; }
    I3FocusSearch search;
    memset(&search, 0, sizeof(search));
    search.title = a;
    const char* end = json + strlen(json);
    i3_walk_array(i3_key(json, end, "nodes"), end, i3_collect_title, &search);
    free(json);
    free(trimmed);

    int con_id = search.id ? search.id : search.lower_id;
    if (!con_id) { log_error("title='%s'", title); return false; }
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "i3-msg [con_id=%d] focus", con_id);
    return i3_run(cmd);
}

typedef struct {
    bool names;
    Fl_WindowList list;
} I3ListBuild;

static void i3_collect_list(const I3Leaf* leaf, void* user) {
    I3ListBuild* b = (I3ListBuild*)user;
    const char* value = b->names ? leaf->app_id : leaf->name;
    if (!value[0]) return;
    if (!strcasecmp(value, "window") || !strcasecmp(value, "colored")) return;
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s", value);
    if (b->names) buffer[0] = toupper((unsigned char)buffer[0]);
    char** tmp = realloc(b->list.windows, (b->list.count + 1) * sizeof(char*));
    if (!tmp) return;
    b->list.windows = tmp;
    b->list.windows[b->list.count++] = strdup(buffer);
}

static Fl_WindowList i3_build_list(bool names) {
    Fl_WindowList list = { .windows = NULL, .count = 0 };
    if (!i3_ensure_sock()) return list;
    char* json = NULL;
    if (!i3_get_tree(&json)) return list;
    I3ListBuild build;
    build.names = names;
    build.list.windows = NULL;
    build.list.count = 0;
    const char* end = json + strlen(json);
    i3_walk_array(i3_key(json, end, "nodes"), end, i3_collect_list, &build);
    free(json);
    return build.list;
}

Fl_WindowList get_window_titles_i3() {
    return i3_build_list(false);
}

Fl_WindowList get_window_names_i3() {
    return i3_build_list(true);
}

Fl_Pos get_cursor_position_i3(void) {
    if (!i3_ensure_sock()) return (Fl_Pos){0, 0};
    return (Fl_Pos){0, 0};
}

bool close_active_window_i3(void) {
    if (!i3_ensure_sock()) return false;
    return i3_run("i3-msg kill");
}

bool toggle_fullscreen_active_i3(void) {
    if (!i3_ensure_sock()) return false;
    return i3_run("i3-msg fullscreen toggle");
}

bool move_active_window_to_monitor_i3(bool right) {
    if (!i3_ensure_sock()) return false;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "i3-msg move container to output %s", right ? "right" : "left");
    return i3_run(cmd);
}

bool focus_window_left_i3(void) {
    if (!i3_ensure_sock()) return false;
    return i3_run("i3-msg focus left");
}

bool focus_window_right_i3(void) {
    if (!i3_ensure_sock()) return false;
    return i3_run("i3-msg focus right");
}

bool move_active_window_left_i3(void) {
    move_active_window_to_monitor_i3(false);
    return focus_window_left_i3();
}

bool move_active_window_right_i3(void) {
    move_active_window_to_monitor_i3(true);
    return focus_window_right_i3();
}


bool minimize_active_window_i3(void) {
    if (!i3_ensure_sock()) return false;
    if (i3_run("i3-msg move container to scratchpad")) return true;
    return i3_run("i3-msg scratchpad show");
}
