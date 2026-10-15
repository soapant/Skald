#include <stdint.h> 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "core/context.h"
#include "core/timer.h"
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "platform/input_state.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/clipboard_history.h"
#include "core/flapi.h"
#include "core/feature_loader.h"
#include "core/cc_registry.h"
#include "core/cap_registry.h"
#include "helpers/helpers_owner.h"
#include "platform/flapi_platform.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "platform/widget.h"
#include "platform/render_thread.h"
#include "platform/lib.h"
#include "core/widget_registry.h"
#include "deps/stb_ds.h"

size_t fl_clipboard_history_count(void) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_clipboard_history_count"); return clipboard_history_count_platform(); }
}

const FlClipboardEntry* fl_clipboard_history_get(size_t index) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_clipboard_history_get"); return clipboard_history_get_platform(index); }
}

const FlClipboardEntry* fl_clipboard_latest(void) {
    PROFILE_START(_pt);
    size_t n = clipboard_history_count_platform();
    if (n == 0) { PROFILE_END_VAR(_pt, "fl_clipboard_latest"); return NULL; }
    { PROFILE_END_VAR(_pt, "fl_clipboard_latest"); return clipboard_history_get_platform(n - 1); }
}

void fl_clipboard_clear_history(void) {
        PROFILE_START(_pt);
clipboard_clear_history_platform();
}

bool fl_copy_to_clipboard(const char* text) {
    PROFILE_START(_pt);
    if (text == NULL) { PROFILE_END_VAR(_pt, "fl_copy_to_clipboard"); return 0; }
    bool ok = Platform_CopyToClipboard(text);
    if (ok) clipboard_history_push_text_platform(text);
    { PROFILE_END_VAR(_pt, "fl_copy_to_clipboard"); return ok; }
}

bool fl_copy_file_path(const char* path) {
    PROFILE_START(_pt);
    if (!path) { PROFILE_END_VAR(_pt, "fl_copy_file_path"); return false; }
    { PROFILE_END_VAR(_pt, "fl_copy_file_path"); return clipboard_set_path_platform(path); }
}

bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore, bool strip_extension, int max_depth) {
    PROFILE_START(_pt);
    if (!dir_path || !list_of_files) { PROFILE_END_VAR(_pt, "fl_get_all_files_from_dir"); return false; }

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) { PROFILE_END_VAR(_pt, "fl_get_all_files_from_dir"); return false; }
    dir_path = expanded_dir;

    { PROFILE_END_VAR(_pt, "fl_get_all_files_from_dir"); return Platform_GetAllFilesFromDir(dir_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore, strip_extension, max_depth); }
}

char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path){
    PROFILE_START(_pt);
    if (!directory) { PROFILE_END_VAR(_pt, "fl_search_file_in_dir"); return NULL; }

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(directory, expanded_dir, sizeof(expanded_dir))) { PROFILE_END_VAR(_pt, "fl_search_file_in_dir"); return NULL; }
    directory = expanded_dir;

    { PROFILE_END_VAR(_pt, "fl_search_file_in_dir"); return fl_search_file_in_dir_platform(directory,search_string,output_path); }
}

bool fl_launch_with_args(const char* path, const char* args) {
    PROFILE_START(_pt);
    if (!path) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_with_args"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_with_args"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    bool r = fl_launch_with_args_platform(expanded, args);
    PROFILE_END_VAR(_pt, "api:fl_launch_with_args");
    return r;
}

bool fl_launch_with_args_as_admin(const char* path, const char* args) {
    PROFILE_START(_pt);
    if (!path) { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin"); return false; }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin"); return false; }
    { PROFILE_END_VAR(_pt, "fl_launch_with_args_as_admin"); return fl_launch_with_args_as_admin_platform(expanded, args); }
}

typedef struct {
    char key[FL_CAP_KEY_LEN];
    char owner[512];
    unsigned abi;
    void *fn;
} FlCap;

static FlCap g_caps[FL_CAP_MAX];
static int g_cap_count = 0;
static unsigned g_cap_generation = 1;

unsigned fl_cap_generation(void) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_cap_generation"); return g_cap_generation; } }

static void fl_add_provides(const char *owner, const char *key) {
    FeatureLoader_ManifestAddProvides(owner, key);
}

bool fl_add_dep_impl(const char *dep_name, const char *owner_file) {
    if (!dep_name || !dep_name[0] || !owner_file || !owner_file[0]) return false;
    char dep[64];
    snprintf(dep, sizeof(dep), "%s", dep_name);
    char *dot = strrchr(dep, '.');
    if (dot) *dot = '\0';
    if (!dep[0]) return false;
    FeatureLoader_ManifestAddDepends(owner_file, dep);
    return true;
}

void fl_cap_register_abi(const char *key, unsigned abi, void *fn) {
    if (!key || !key[0] || !fn) return;

    char owner[OWNER_PATH_MAX];
    Owner_ResolveForFunc(fn, owner, sizeof(owner));

    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].fn == fn) return;
        log_info("fl_cap '%s' replaced (was abi=%u now abi=%u)", key, g_caps[i].abi, abi);
        snprintf(g_caps[i].key, FL_CAP_KEY_LEN, "%s", key);
        snprintf(g_caps[i].owner, sizeof(g_caps[i].owner), "%s", owner);
        g_caps[i].abi = abi;
        g_caps[i].fn = fn;
g_cap_generation++;
        if (owner[0]) fl_add_provides(owner, key);
        return;
    }

    if (g_cap_count >= FL_CAP_MAX) {
    PROFILE_START(_pt);
        log_error("fl_cap table full, dropping '%s'", key);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
    FlCap *c = &g_caps[g_cap_count++];
    snprintf(c->key, FL_CAP_KEY_LEN, "%s", key);
    snprintf(c->owner, sizeof(c->owner), "%s", owner);
    c->abi = abi;
    c->fn = fn;
    g_cap_generation++;
    log_info("fl_cap '%s' registered abi=%u owner='%s'", key, abi, owner[0] ? owner : "?");
    if (owner[0]) fl_add_provides(owner, key);
}

void *fl_cap_resolve_abi(const char *key, unsigned abi) {
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].abi != abi) {
    PROFILE_START(_pt);
            log_warn("fl_cap '%s' abi mismatch want=%u have=%u", key, abi, g_caps[i].abi);
            { PROFILE_END_VAR(_pt, "f"); return NULL; }
        }
        return g_caps[i].fn;
    }
    return NULL;
}

void fl_cap_bind_declare(const char *key, void *consumer) {
    PROFILE_START(_pt);
    if (!key || !key[0] || !consumer) { PROFILE_END_VAR(_pt, "fl_cap_bind_declare"); return ; }

    char consumer_path[OWNER_PATH_MAX];
    Owner_ResolveForFunc(consumer, consumer_path, sizeof(consumer_path));
    if (!consumer_path[0]) { PROFILE_END_VAR(_pt, "fl_cap_bind_declare"); return ; }

    for (int i = 0; i < g_cap_count; i++) {
        if (strcmp(g_caps[i].key, key) != 0) continue;
        if (g_caps[i].abi != FL_CAP_ABI_V1) return;
        if (!g_caps[i].owner[0]) return;
        char provider[64];
        FeatureLoader_OwnerFeatureName(g_caps[i].owner, provider, sizeof(provider));
        if (!provider[0]) return;
        FeatureLoader_ManifestAddDepends(consumer_path, provider);
        return;
    }
}

void fl_cap_purge_owner(const char *path_a, const char *path_b) {
    PROFILE_START(_pt);
    if ((!path_a || !path_a[0]) && (!path_b || !path_b[0])) { PROFILE_END_VAR(_pt, "fl_cap_purge_owner"); return ; }

    int n = 0;
    for (int i = 0; i < g_cap_count; i++) {
        if (Owner_ModuleMatches(g_caps[i].owner, path_a, path_b)) {
            log_info("fl_cap '%s' purged (owner unloaded)", g_caps[i].key);
            continue;
        }
        g_caps[n++] = g_caps[i];
    }
    int dropped = g_cap_count - n;
    g_cap_count = n;
    if (dropped > 0) {
        g_cap_generation++;
        log_info("fl_cap purged %d entr%s", dropped, dropped == 1 ? "y" : "ies");
    }
}

void fl_cap_dump(void) {
        PROFILE_START(_pt);
log_info("fl_cap table: %d entries", g_cap_count);
    for (int i = 0; i < g_cap_count; i++) {
        char ob[OWNER_PATH_MAX];
        Owner_ModuleBasename(g_caps[i].owner, ob, sizeof(ob));
        log_info("  '%s' abi=%u fn=%p owner=%s", g_caps[i].key, g_caps[i].abi, g_caps[i].fn, ob);
    }
}

FlStringList fl_load_file_list(const char* filepath) {
    FlStringList list = {0};
    if (!filepath) return list;
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full, sizeof(full))) {
    PROFILE_START(_pt);
        log_warn("fl_load_file_list resolve failed name='%s' feature='%s'", filepath, FlintContext_Get()->window.current_feature ? FlintContext_Get()->window.current_feature : "(null)");
        { PROFILE_END_VAR(_pt, "f"); return list; }
    }
    long budget = (long)MAX_FILE_LINES * (long)(MAX_FILE_LINE_LENGTH + 2);
    list = File_ReadLines(full, budget);
    if (list.count > MAX_FILE_LINES) {
        list.count = MAX_FILE_LINES;
        list.items[list.count] = NULL;
    }
    return list;
}

long fl_load_file(const char* filepath, char* buffer, long buffer_size) {
    if (!filepath || !buffer || buffer_size <= 0) return -1;
    buffer[0] = '\0';
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full, sizeof(full))) {
    PROFILE_START(_pt);
        log_warn("fl_load_file resolve failed name='%s' feature='%s'", filepath, FlintContext_Get()->window.current_feature ? FlintContext_Get()->window.current_feature : "(null)");
        { PROFILE_END_VAR(_pt, "f"); return -1; }
    }
    return File_ReadAll(full, buffer, buffer_size);
}

bool fl_free_list(const char* list[], int count) {
    PROFILE_START(_pt);
    if (!list || count <= 0) { PROFILE_END_VAR(_pt, "fl_free_list"); return false; }
    for (int i = 0; i < count; i++) free((void*)list[i]);
    { PROFILE_END_VAR(_pt, "fl_free_list"); return true; }
}
void fl_free_file_list(FlStringList* list) {
    PROFILE_START(_pt);
    if (!list) { PROFILE_END_VAR(_pt, "fl_free_file_list"); return ; }
    if (list->items) {
        for (int i = 0; i < list->count; i++) free(list->items[i]);
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
}
bool fl_free_window_list(PlWindowList *list) {
    PROFILE_START(_pt);
    if (!list) { PROFILE_END_VAR(_pt, "fl_free_window_list"); return false; }
    if (list->windows) {
        for (int i = 0; i < list->count; i++) free(list->windows[i]);
        free(list->windows);
        list->windows = NULL;
        list->count = 0;
    }
    { PROFILE_END_VAR(_pt, "fl_free_window_list"); return true; }
}

bool fl_append_to_file(const char* filepath, const char* line) {
    if (!filepath || !line) return false;
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full, sizeof(full))) return false;
    filepath = full;
    FILE* f = fopen(filepath, "a");
    if (!f) {
    PROFILE_START(_pt);
        f = fopen(filepath, "w");
        if (!f) { PROFILE_END_VAR(_pt, "f"); return false; }
    }
    fclose(f);
    f = fopen(filepath, "r");
    bool need_newline = false;
    if (f) {
        if (fseek(f, 0, SEEK_END) == 0) {
            long sz = ftell(f);
            if (sz > 0) {
                if (fseek(f, -1, SEEK_END) == 0) {
                    int c = fgetc(f);
                    if (c != '\n' && c != '\r') need_newline = true;
                }
            }
        }
        fclose(f);
    }
    f = fopen(filepath, "a");
    if (!f) return false;
    if (need_newline) fputc('\n', f);
    fputs(line, f);
    fputc('\n', f);
    fclose(f);
    return true;
}

bool fl_write_to_file(const char* filepath, const char* lines[], int count) {
    PROFILE_START(_pt);
    if (!filepath) { PROFILE_END_VAR(_pt, "fl_write_to_file"); return false; }
    if (count < 0) { PROFILE_END_VAR(_pt, "fl_write_to_file"); return false; }
    if (count > 0 && !lines) { PROFILE_END_VAR(_pt, "fl_write_to_file"); return false; }
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(__builtin_return_address(0), filepath, full, sizeof(full))) { PROFILE_END_VAR(_pt, "fl_write_to_file"); return false; }
    filepath = full;
    FILE* f = fopen(filepath, "w");
    if (!f) { PROFILE_END_VAR(_pt, "fl_write_to_file"); return false; }
    for (int i = 0; i < count; i++) {
        if (lines[i]) fprintf(f, "%s\n", lines[i]);
        else fprintf(f, "\n");
    }
    fclose(f);
    { PROFILE_END_VAR(_pt, "fl_write_to_file"); return true; }
}

bool fl_write_temp_file(const char* name, const void* data, size_t len, char* out_path, size_t out_len) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_write_temp_file"); return Platform_WriteTempFile(name, data, len, out_path, out_len); }
}

char* fl_read_file_to_string(const char* path, size_t* out_len) {
    if (!path) return NULL;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END)!=0) {     PROFILE_START(_pt);
fclose(f); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
    long sz = ftell(f);
    if (sz < 0) sz = 0;
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {     PROFILE_START(_pt);
fclose(f); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
    size_t read = fread(buf, 1, (size_t)sz, f);
    buf[read] = '\0';
    fclose(f);
    if (out_len) *out_len = read;
    return buf;
}

long fl_read_entire_file(const char* full_path, char* buffer, long buffer_size) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_read_entire_file"); return File_ReadAll(full_path, buffer, buffer_size); }
}
bool fl_write_string_to_file(const char* path, const char* content) {
    PROFILE_START(_pt);
    if (!path || !content) { PROFILE_END_VAR(_pt, "fl_write_string_to_file"); return false; }
    char full[MAX_PATH];
    if (Platform_ResolveDataPath(__builtin_return_address(0), path, full, sizeof(full))) {
        path = full;
    } else {
        char expanded[MAX_PATH];
        if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
        path = expanded;
    }
    FILE* f = fopen(path, "w");
    if (!f) { PROFILE_END_VAR(_pt, "fl_write_string_to_file"); return false; }
    fputs(content, f);
    fclose(f);
    { PROFILE_END_VAR(_pt, "fl_write_string_to_file"); return true; }
}
bool fl_file_exists(const char* path) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (f) {     PROFILE_START(_pt);
fclose(f); { PROFILE_END_VAR(_pt, "f"); return true; } }
    return false;
}
bool fl_append_unique_line(const char* path, const char* line) {
    PROFILE_START(_pt);
    if (!path || !line) { PROFILE_END_VAR(_pt, "fl_append_unique_line"); return false; }
    FlStringList files = fl_load_file_list(path);
    bool ok = fl_append_to_file(path, line);
    fl_free_file_list(&files);
    { PROFILE_END_VAR(_pt, "fl_append_unique_line"); return ok; }
}
bool fl_remove_line(const char* path, const char* line) {
    PROFILE_START(_pt);
    if (!path || !line) { PROFILE_END_VAR(_pt, "fl_remove_line"); return false; }
    FlStringList files = fl_load_file_list(path);
    const char* kept[1024]; int k=0;
    for (int i=0;i<files.count && k<1024;i++) if (strcmp(files.items[i], line)!=0) kept[k++]=files.items[i]; else { free(files.items[i]); files.items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    fl_free_file_list(&files);
    { PROFILE_END_VAR(_pt, "fl_remove_line"); return ok; }
}
bool fl_remove_line_contains(const char* path, const char* substr) {
    PROFILE_START(_pt);
    if (!path || !substr) { PROFILE_END_VAR(_pt, "fl_remove_line_contains"); return false; }
    FlStringList files = fl_load_file_list(path);
    const char* kept[1024]; int k=0;
    for (int i=0;i<files.count && k<1024;i++) if (!strstr(files.items[i], substr)) kept[k++]=files.items[i]; else { free(files.items[i]); files.items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    fl_free_file_list(&files);
    { PROFILE_END_VAR(_pt, "fl_remove_line_contains"); return ok; }
}
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay) {
    PROFILE_START(_pt);
    if (!path || !out_name) { PROFILE_END_VAR(_pt, "fl_parse_desktop_file"); return false; }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "r");
    if (!f) { PROFILE_END_VAR(_pt, "fl_parse_desktop_file"); return false; }
    char line[1024]; bool found=false;
    if (out_nodisplay) *out_nodisplay=false;
    while(fgets(line,sizeof(line),f)){
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(Platform_StringContainsCaseInsensitive(p, "NoDisplay=true") || Platform_StringContainsCaseInsensitive(p, "Hidden=true")){
            if(out_nodisplay) *out_nodisplay=true;
        }
        if(!found && Platform_StringContainsCaseInsensitive(p, "Name=")){
            p+=5; while(*p==' '||*p=='\t') p++;
            p[strcspn(p,"\r\n")]='\0';
            strncpy(out_name, p, n-1); out_name[n-1]='\0'; found=true;
        }
    }
    fclose(f);
    { PROFILE_END_VAR(_pt, "fl_parse_desktop_file"); return found; }
}

bool fl_register_remapping_impl(const char* src, const char* dst, bool whitelist, const char** windows) {
    PROFILE_START(_pt);
    if (!src || !dst) { PROFILE_END_VAR(_pt, "fl_register_remapping_impl"); return false; }
    { PROFILE_END_VAR(_pt, "fl_register_remapping_impl"); return remap_register_internal(src, dst, whitelist, windows); }
}

static void fl_snippet_fire(int idx, int del) {
    PROFILE_START(_pt);
    const SnippetBinding* b = snippet_get_binding(idx);
    if (!b || !b->trigger) { PROFILE_END_VAR(_pt, "fl_snippet_fire"); return ; }
    for (int i = 0; i < del; i++) {
        Platform_EmitKey(VK_BACK, 1);
        Platform_EmitKey(VK_BACK, 0);
    }
    if (b->text) fl_type_text(b->text);
    else if (b->func) b->func();
}

bool fl_register_snippet_impl(const char* trigger, const char* text, FlSnippetFunc fn, bool whitelist, const char** windows) {
    PROFILE_START(_pt);
    if (!trigger || (!text && !fn)) { PROFILE_END_VAR(_pt, "fl_register_snippet_impl"); return false; }
    snippet_set_fire_fn(fl_snippet_fire);
    { PROFILE_END_VAR(_pt, "fl_register_snippet_impl"); return snippet_register_internal(trigger, text, fn, whitelist, windows, (void*)__builtin_return_address(0)); }
}

typedef struct { char *key; void *value; } FlintMapEnt;
typedef struct { char name[128]; void (*cb)(void); char icon[512]; } FeatureReg;
static FlintMap* g_feature_map = NULL;
static FeatureReg** g_feature_order = NULL;
static int g_feature_order_count = 0;
static int g_feature_order_cap = 0;

static FlintMap* ensure_feature_map(void) {
    PROFILE_START(_pt);
    if (!g_feature_map) { g_feature_map = NULL; sh_new_strdup(g_feature_map); }
    { PROFILE_END_VAR(_pt, "ensure_feature_map"); return g_feature_map; }
}

void fl_register_feature(const char* name, void (*cb)(void)) {
        PROFILE_START(_pt);
fl_register_feature_with_icon(name, cb, NULL);
}
void fl_register_feature_with_icon(const char* name, void (*cb)(void), const char* icon_path) {
    if (!name || !cb) return;
    ensure_feature_map();
    ptrdiff_t idx = shgeti(g_feature_map, (char*)name);
    FeatureReg* existing = idx >= 0 ? (FeatureReg*)g_feature_map[idx].value : NULL;
    if (existing) {
    PROFILE_START(_pt);
        existing->cb = cb;
        if (icon_path) {
            strncpy(existing->icon, icon_path, sizeof(existing->icon)-1);
            existing->icon[sizeof(existing->icon)-1] = '\0';
        } else {
            existing->icon[0] = '\0';
        }
        hotkey_mark_feature_toggles(cb);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
    FeatureReg* reg = (FeatureReg*)calloc(1, sizeof(FeatureReg));
    if (!reg) return;
    strncpy(reg->name, name, sizeof(reg->name)-1);
    reg->name[sizeof(reg->name)-1] = '\0';
    reg->cb = cb;
    if (icon_path) {
        strncpy(reg->icon, icon_path, sizeof(reg->icon)-1);
        reg->icon[sizeof(reg->icon)-1] = '\0';
    }
    shput(g_feature_map, reg->name, reg);
    if (g_feature_order_count >= g_feature_order_cap) {
        int new_cap = g_feature_order_cap ? g_feature_order_cap * 2 : 32;
        FeatureReg** new_order = (FeatureReg**)realloc(g_feature_order, new_cap * sizeof(FeatureReg*));
        if (!new_order) {     PROFILE_START(_pt);
shdel(g_feature_map, reg->name); free(reg); { PROFILE_END_VAR(_pt, "f"); return ; } }
        g_feature_order = new_order;
        g_feature_order_cap = new_cap;
    }
    g_feature_order[g_feature_order_count++] = reg;
    hotkey_mark_feature_toggles(cb);
}

bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows) {
    PROFILE_START(_pt);
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows,
                                                        whitelist);
    binding.is_feature_toggle = false;
    binding.hold = false;
    if (on_hotkey_trigger && g_feature_map) {
        for (int i = 0; i < g_feature_order_count; i++) {
            if (g_feature_order[i] && g_feature_order[i]->cb == on_hotkey_trigger) { binding.is_feature_toggle = true; break; }
        }
    }
    { PROFILE_END_VAR(_pt, "fl_register_hotkey_impl"); return hotkey_register_internal(binding); }
}

int fl_get_feature_list(const char** out_names, void (**out_cbs)(void), int max) {
    PROFILE_START(_pt);
    int n = g_feature_order_count < max ? g_feature_order_count : max;
    for (int i=0;i<n;i++) {
        if (out_names) out_names[i] = g_feature_order[i]->name;
        if (out_cbs) out_cbs[i] = g_feature_order[i]->cb;
    }
    { PROFILE_END_VAR(_pt, "fl_get_feature_list"); return n; }
}

int fl_get_feature_list_with_icons(const char** out_names, void (**out_cbs)(void), const char*** out_icons, int max) {
    PROFILE_START(_pt);
    int n = g_feature_order_count < max ? g_feature_order_count : max;
    for (int i=0;i<n;i++) {
        if (out_names) out_names[i] = g_feature_order[i]->name;
        if (out_cbs) out_cbs[i] = g_feature_order[i]->cb;
    }
    static const char** icon_ptrs = NULL;
    static int icon_ptrs_cap = 0;
    if (n > icon_ptrs_cap) {
        const char** new_ptrs = (const char**)realloc(icon_ptrs, n * sizeof(const char*));
        if (new_ptrs) {
            icon_ptrs = new_ptrs;
            icon_ptrs_cap = n;
        }
    }
    if (icon_ptrs) {
        for (int i=0;i<n;i++) icon_ptrs[i] = g_feature_order[i]->icon[0] ? g_feature_order[i]->icon : NULL;
        if (out_icons) *out_icons = icon_ptrs;
    } else {
        if (out_icons) *out_icons = NULL;
    }
    { PROFILE_END_VAR(_pt, "fl_get_feature_list_with_icons"); return n; }
}
char* fl_get_icon_path(const char* app_path_path) {
    PROFILE_START(_pt);
    if (!app_path_path) { PROFILE_END_VAR(_pt, "fl_get_icon_path"); return NULL; }
    { PROFILE_END_VAR(_pt, "fl_get_icon_path"); return Platform_GetIconPath((char*)app_path_path); }
}
int fl_get_icon_size(const char* icon_path) {
    PROFILE_START(_pt);
    if (!icon_path) { PROFILE_END_VAR(_pt, "fl_get_icon_size"); return 20; }
    { PROFILE_END_VAR(_pt, "fl_get_icon_size"); return Platform_GetIconSize((char*)icon_path); }
}

char* fl_run_terminal_command(char* command) {
  PROFILE_START(_pt);
  if (!command) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_run_terminal_command"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
  char* r = Platform_RunTerminalCommand(command,false);
  PROFILE_END_VAR(_pt, "api:fl_run_terminal_command");
  return r;
}

char* fl_run_argv(const char* const argv[], const char* stdin_data, int* out_status) {
    PROFILE_START(_pt);
  if (!argv || !argv[0]) { PROFILE_END_VAR(_pt, "fl_run_argv"); return NULL; }
  { PROFILE_END_VAR(_pt, "fl_run_argv"); return Platform_RunArgvCapture(argv, stdin_data, out_status); }
}

char* fl_run_terminal_command_blocking(char* command) {
    PROFILE_START(_pt);
  if (!command) { PROFILE_END_VAR(_pt, "fl_run_terminal_command_blocking"); return NULL; }
  { PROFILE_END_VAR(_pt, "fl_run_terminal_command_blocking"); return Platform_RunCommandAndGetOutput(command); }
}

char* fl_run_terminal_command_and_open(char* command) {
  PROFILE_START(_pt);
  if (!command) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_run_terminal_command_and_open"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
  char* r = Platform_RunTerminalCommand(command,true);
  PROFILE_END_VAR(_pt, "api:fl_run_terminal_command_and_open");
  return r;
}

bool fl_launch_detached(char* command) {
  PROFILE_START(_pt);
  if (!command) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_detached"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool r = Platform_LaunchDetached(command);
  PROFILE_END_VAR(_pt, "api:fl_launch_detached");
  return r;
}

bool fl_launch_terminal_at_path(char* path,char* args) {
  PROFILE_START(_pt);
  bool r = false;
  if (!path) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_terminal_at_path"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_terminal_at_path"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    r = Platform_LaunchTerminalAtPath(expanded,args);
  PROFILE_END_VAR(_pt, "api:fl_launch_terminal_at_path");
  return r;
}

bool fl_launch_file_manager_at_path(char* path,char* args) {
  PROFILE_START(_pt);
  bool r = false;
  if (!path) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_file_manager_at_path"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_file_manager_at_path"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    r = Platform_LaunchFileManagerAtPath(expanded,args);
  PROFILE_END_VAR(_pt, "api:fl_launch_file_manager_at_path");
  return r;
}

char* fl_get_zoxide_match(char* query) {
    PROFILE_START(_pt);
  if (!query) { PROFILE_END_VAR(_pt, "fl_get_zoxide_match"); return NULL; }
  { PROFILE_END_VAR(_pt, "fl_get_zoxide_match"); return Platform_QueryZoxidePath(query); }
}

bool fl_terminate_process_by_name(const char* exe_name) {
    PROFILE_START(_pt);
    if (!exe_name) { PROFILE_END_VAR(_pt, "fl_terminate_process_by_name"); return false; }
    { PROFILE_END_VAR(_pt, "fl_terminate_process_by_name"); return Platform_TerminateProcessByName(exe_name); }
}
bool fl_string_contains(const char* haystack, const char* needle) {
    PROFILE_START(_pt);
    if (!needle || !haystack) { PROFILE_END_VAR(_pt, "fl_string_contains"); return false; }
    { PROFILE_END_VAR(_pt, "fl_string_contains"); return Platform_StringContainsCaseInsensitive(needle,haystack); }
}
char* fl_string_cut_at(const char* str, char delim) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_cut_at"); return String_CutAt(str, delim); } }
char* fl_string_cut_before(const char* str, char delim) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_cut_before"); return String_CutBefore(str, delim); } }
char* fl_string_cut_after(const char* str, char delim) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_cut_after"); return String_CutAfter(str, delim); } }
char* fl_string_between(const char* str, char open, char close) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_between"); return String_Between(str, open, close); } }
char* fl_string_inside(const char* str, char delim) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_inside"); return String_Inside(str, delim); } }
char* fl_string_trim(const char* str) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_trim"); return String_Trim(str); } }
char* fl_string_to_lower(const char* str) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_to_lower"); return String_ToLower(str); } }
char* fl_string_to_upper(const char* str) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_to_upper"); return String_ToUpper(str); } }
bool fl_string_starts_with(const char* str, const char* prefix) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_starts_with"); return String_StartsWith(str, prefix); } }
bool fl_string_ends_with(const char* str, const char* suffix) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_ends_with"); return String_EndsWith(str, suffix); } }
char* fl_string_replace(const char* str, const char* from, const char* to) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_replace"); return String_Replace(str, from, to); } }
char* fl_string_slice(const char* str, int start, int end) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_string_slice"); return String_Slice(str, start, end); } }

StringList fl_string_split(const char* str, const char* delimiter) {
    PROFILE_START(_pt);
    StringList empty = {NULL, 0};
    if (!str || !delimiter || !delimiter[0]) { PROFILE_END_VAR(_pt, "fl_string_split"); return empty; }
    { PROFILE_END_VAR(_pt, "fl_string_split"); return String_Split(str, delimiter); }
}

void fl_string_list_free(StringList* list) {
        PROFILE_START(_pt);
StringList_Free(list);
}

void fl_populate_listbox(ListBoxInfo* lb, const char* filter, const char** items, int count) {
        PROFILE_START(_pt);
PopulateListBox(lb, filter, items, count, NULL);
}

bool fl_is_selected(ListBoxInfo* lb) {
    PROFILE_START(_pt);
    if (!lb) { PROFILE_END_VAR(_pt, "fl_is_selected"); return false; }
    { PROFILE_END_VAR(_pt, "fl_is_selected"); return UI_IsListBoxSelected(lb); }
}

void fl_set_edit_text(const char* text) {
    PROFILE_START(_pt);
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) { PROFILE_END_VAR(_pt, "fl_set_edit_text"); return ; }
    UI_SetText(text ? text : "");
}

void fl_set_edit_cursor(int position) {
    PROFILE_START(_pt);
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) { PROFILE_END_VAR(_pt, "fl_set_edit_cursor"); return ; }
    UI_SetCursorPosition(position);
}

void fl_get_edit_text(char* buffer, size_t cap) {
    PROFILE_START(_pt);
    if (!buffer || cap == 0) { PROFILE_END_VAR(_pt, "fl_get_edit_text"); return ; }
    buffer[0] = '\0';
    FlintContext* ctx = FlintContext_Get();
    if (!ctx) { PROFILE_END_VAR(_pt, "fl_get_edit_text"); return ; }
    UI_GetCurrentTextEdit(buffer, cap);
}

static void fl_hover_popup_frame(void *ud) {
    PROFILE_START(_pt);
    FlHoverPopupCtx *p = (FlHoverPopupCtx *)ud;
    if (!p || !p->render) { PROFILE_END_VAR(_pt, "fl_hover_popup_frame"); return ; }
    p->frame++;
    FlWidgetState st = {0};
    st.width = p->w;
    st.height = p->h;
    st.frame = p->frame;
    st.elapsed = fl_end_timer_platform(p->created_at);
    st.hovered = true;
    Platform_SetDrawOffset(0, 0);
    Platform_BeginMeasure();
    p->render();
    int mw = 0, mh = 0;
    Platform_EndMeasure(&mw, &mh);
    if (mw > 0 && mh > 0) {
        p->w = mw + p->pad;
        p->h = mh + p->pad;
    }
}

static FlHoverPopupCtx *fl_hover_popup_ctx_new(const FlMessageSpec *spec) {
    PROFILE_START(_pt);
    FlHoverPopupCtx *p = (FlHoverPopupCtx *)calloc(1, sizeof(*p));
    if (!p) { PROFILE_END_VAR(_pt, "fl_hover_popup_ctx_new"); return NULL; }
    p->render = spec->render;
    p->w = spec->w > 0 ? spec->w : FL_WIDGET_PROVISIONAL_W;
    p->h = spec->h > 0 ? spec->h : FL_WIDGET_PROVISIONAL_H;
    p->created_at = fl_start_timer_platform();
    p->pad = FL_WIDGET_DEFAULT_PAD;
    { PROFILE_END_VAR(_pt, "fl_hover_popup_ctx_new"); return p; }
}

static void fl_hover_popup_ctx_free(void *ud) {
        PROFILE_START(_pt);
FlHoverPopupCtx *p = (FlHoverPopupCtx *)ud;
    if (p) free(p);
}

void fl_message_impl(const FlMessageSpec* spec) {
    if (!MessagesEnabled) return;
    int w = spec->w > 0 ? spec->w : 0;
    int h = spec->h > 0 ? spec->h : 0;
    int mw = spec->mw > 0 ? spec->mw : 0;
    int mh = spec->mh > 0 ? spec->mh : 0;
    double timeout = spec->timeout > 0 ? spec->timeout : (double)TimeOutForMessages;
    ColorRef color = spec->color_text ? spec->color_text : COLOR_TEXT;
    if (spec->hover_key && spec->hover_key[0]) {
        if (spec->render) {
    PROFILE_START(_pt);
            FlHoverPopupCtx *ctx = fl_hover_popup_ctx_new(spec);
            if (ctx) {
                Platform_HoverWidgetShow(spec->hover_key, fl_hover_popup_frame, ctx,
                                         spec->x, spec->y, ctx->w, ctx->h,
                                         spec->fps > 0 ? spec->fps : 1,
                                         spec->no_bg,
                                         spec->color_bg ? rgb_to_hex(spec->color_bg) : 0UL,
                                         fl_hover_popup_ctx_free, ctx);
            }
            { PROFILE_END_VAR(_pt, "f"); return ; }
        }
        Platform_HoverMessage(spec->hover_key, spec->message, spec->x, spec->y, w, h, mw, mh, color,spec->no_bg);
        return;
    }
    MessageSlot slot = spec->slot;
    if (slot == AUTO) {
    PROFILE_START(_pt);
        slot = IsPopupActive(WH_NULL) ? TOP_CENTER_RELATIVE : BOTTOM_CENTER_RELATIVE;
        Platform_DisplayMessage(spec->message, spec->x, spec->y,spec->no_bg,w,h,mw, mh, timeout, color,spec->render ? spec->render : NULL);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    } else if (slot == FREE || spec->x != 0 || spec->y != 0) {
        Platform_DisplayMessage(spec->message, spec->x, spec->y,spec->no_bg,w,h,mw, mh, timeout, color,spec->render ? spec->render : NULL);
        return;
    }
    DisplayMessageSlot(spec->message, slot, mw, mh, spec->prompt_gap, timeout, color);
}
bool fl_launch_browser(char* url) {
    PROFILE_START(_pt);
    if (!url) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_launch_browser"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    bool r = Platform_LaunchBrowserAtUrl(url,false);
    PROFILE_END_VAR(_pt, "api:fl_launch_browser");
    return r;
}
bool fl_query_browser(const char* query) {
    PROFILE_START(_pt);
    if (!query) { PROFILE_END_VAR(_pt, "fl_query_browser"); return false; }
    { PROFILE_END_VAR(_pt, "fl_query_browser"); return Platform_QueryBrowser(query); }
}

char* fl_on_dir_changed(const char* dir_path, DirChangeCallback callback) {
    PROFILE_START(_pt);
    if (!dir_path) { PROFILE_END_VAR(_pt, "fl_on_dir_changed"); return NULL; }
    { PROFILE_END_VAR(_pt, "fl_on_dir_changed"); return Platform_RegisterDirWatcher(dir_path, callback); }
}

char* fl_get_last_clipboard_entry() {
    PROFILE_START(_pt);
    char* current = Platform_GetClipboardEntry();
    if (current) {
        if (current[0]) {
            const FlClipboardEntry* latest = fl_clipboard_latest();
            if (!latest || latest->type != FL_CLIPBOARD_TEXT || !latest->text || strcmp(latest->text, current) != 0)
                clipboard_history_push_text_platform(current);
        }
        free(current);
    }
    for (size_t i = clipboard_history_count_platform(); i > 0; i--) {
        const FlClipboardEntry* e = clipboard_history_get_platform(i - 1);
        if (e->type == FL_CLIPBOARD_TEXT && e->text) return strdup(e->text);
    }
    { PROFILE_END_VAR(_pt, "fl_get_last_clipboard_entry"); return NULL; }
}
bool fl_paste_from_clipboard() {
    PROFILE_START(_pt);
    char* content = Platform_GetClipboardEntry();
   { PROFILE_END_VAR(_pt, "fl_paste_from_clipboard"); return Platform_PasteContent(content); }
}

bool fl_paste(char* content) {
    PROFILE_START(_pt);
    if (!content) { PROFILE_END_VAR(_pt, "fl_paste"); return false; }
   { PROFILE_END_VAR(_pt, "fl_paste"); return Platform_PasteContent(content); }
}

static void fl_edit_blocking_confirm(const char* text, void* user_data) {
        PROFILE_START(_pt);
char** out = (char**)user_data;
    if (out && text) *out = strdup(text);
}

char* fl_edit_text_popup_impl(const FlEditPopupSpec* spec) {
    PROFILE_START(_pt);
    const char* initial_text = spec ? spec->initial_text : NULL;
    int x = spec->x ? spec->x : 960;
    int y = spec->y ? spec->y : 540;
    int w = spec->w > 0 ? spec->w : 1000;
    int h = spec->h > 0 ? spec->h : 50;
    const char* align = spec->align ? spec->align : "LEFT";
    ListBoxInfo* lb = spec ? spec->lb : NULL;
    int* out_index = spec ? spec->out_index : NULL;
    if (out_index) *out_index = -1;
    if (!lb) { PROFILE_END_VAR(_pt, "fl_edit_text_popup_impl"); return NULL; }
    if (!initial_text) initial_text = "";
    int idx = lb->selected_index;
    int true_idx = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    if (!wm) { PROFILE_END_VAR(_pt, "fl_edit_text_popup_impl"); return NULL; }
    char* result = NULL;
    Popup* popup = EditPopup_Create(wm, initial_text, x, y, w, h, align, fl_edit_blocking_confirm, &result);
    if (!popup) { PROFILE_END_VAR(_pt, "fl_edit_text_popup_impl"); return NULL; }
    WindowManager_PushPopup(wm, popup);
    while (wm->running && popup->manager) {
        Platform_WM_ProcessEvents(wm);
    }
    bool was_confirmed = ((WMEditPopupData*)popup)->confirmed;
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (lb && idx >= 0) {
        int keep_idx = idx < lb->populated_count ? idx : lb->populated_count - 1;
        if (keep_idx >= 0) lb->selected_index = keep_idx;
    }
    if (!was_confirmed) {
        free(result);
        result = NULL;
    }
    if (!result) { PROFILE_END_VAR(_pt, "fl_edit_text_popup_impl"); return NULL; }
    if (out_index) *out_index = true_idx;
    { PROFILE_END_VAR(_pt, "fl_edit_text_popup_impl"); return result; }
}

bool fl_replace_line_in_file(const char* filepath, int line_index, const char* new_text) {
    if (!filepath || !new_text || line_index < 0) return false;
    FlStringList files = fl_load_file_list(filepath);
    if (line_index >= files.count) {
    PROFILE_START(_pt);
        fl_free_file_list(&files);
        { PROFILE_END_VAR(_pt, "f"); return false; }
    }
    free(files.items[line_index]);
    files.items[line_index] = NULL;
    files.items[line_index] = strdup(new_text);
    if (!files.items[line_index]) {
    PROFILE_START(_pt);
        fl_free_file_list(&files);
        { PROFILE_END_VAR(_pt, "f"); return false; }
    }
    bool ok = fl_write_to_file(filepath, (const char**)files.items, files.count);
    fl_free_file_list(&files);
    return ok;
}

bool fl_focus_window_by_title(char* title) {
  PROFILE_START(_pt);
  if (!title) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_focus_window_by_title"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool r = Platform_FocusWindow(title);
  PROFILE_END_VAR(_pt, "api:fl_focus_window_by_title");
  return r;
}

bool fl_close_active_window(void) {
  PROFILE_START(_pt);
  bool r = Platform_CloseActiveWindow();
  PROFILE_END_VAR(_pt, "api:fl_close_active_window");
  return r;
}

bool fl_dismiss_message(void) {
  PROFILE_START(_pt);
  bool r = fl_dismiss_message_platform();
  PROFILE_END_VAR(_pt, "api:fl_dismiss_message");
  return r;
}

bool fl_toggle_fullscreen_active(void) {
  PROFILE_START(_pt);
  bool r = Platform_ToggleFullscreenActive();
  PROFILE_END_VAR(_pt, "api:fl_toggle_fullscreen_active");
  return r;
}

bool fl_move_active_window_to_monitor(bool right) {
  PROFILE_START(_pt);
  bool r = Platform_MoveActiveWindowToMonitor(right);
  PROFILE_END_VAR(_pt, "api:fl_move_active_window_to_monitor");
  return r;
}

bool fl_move_active_window_left(void) {
  PROFILE_START(_pt);
  bool r = Platform_MoveActiveWindowLeft();
  PROFILE_END_VAR(_pt, "api:fl_move_active_window_left");
  return r;
}

bool fl_move_active_window_right(void) {
  PROFILE_START(_pt);
  bool r = Platform_MoveActiveWindowRight();
  PROFILE_END_VAR(_pt, "api:fl_move_active_window_right");
  return r;
}

bool fl_focus_window_left(void) {
  PROFILE_START(_pt);
  bool r = Platform_FocusWindowLeft();
  PROFILE_END_VAR(_pt, "api:fl_focus_window_left");
  return r;
}

bool fl_focus_window_right(void) {
  PROFILE_START(_pt);
  bool r = Platform_FocusWindowRight();
  PROFILE_END_VAR(_pt, "api:fl_focus_window_right");
  return r;
}

bool fl_toggle_minimized_active(void) {
  PROFILE_START(_pt);
  bool r = Platform_MinimizeActiveWindow();
  PROFILE_END_VAR(_pt, "api:fl_toggle_minimized_active");
  return r;
}

char* fl_popup_impl(const FlPopupSpec *spec) {
    if (!spec) return NULL;
    const char* initial_text = spec->initial_text ? spec->initial_text : "";
    int x = spec->x ? spec->x : 0;
    int y = spec->y ? spec->y : 0;
    int w = spec->w > 0 ? spec->w : 500;
    int h = spec->h > 0 ? spec->h : 70;
    const char* align = "LEFT";
    WindowManager* wm = WindowManager_Get();
    if (!wm) {     PROFILE_START(_pt);
log_error("fl_popup: no WindowManager"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
    char* result = NULL;
    Popup* popup = EditPopup_Create(wm, initial_text, x, y, w, h, align, fl_edit_blocking_confirm, &result);
    if (!popup) {     PROFILE_START(_pt);
log_error("fl_popup: EditPopup_Create failed w=%d h=%d", w, h); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
    EditPopup_SetPlaceholder(popup, spec->placeholder);

    WindowManager_PushPopup(wm, popup);
    while (wm->running && popup->manager) {
        Platform_WM_ProcessEvents(wm);
    }
    bool was_confirmed = ((WMEditPopupData*)popup)->confirmed;
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (!was_confirmed) {
        free(result);
        result = NULL;
    }
    return result;
}

unsigned long fl_listbox_get_current_standard_color(void);

bool fl_draw_text_impl(const FlTextSpec *spec) {
    PROFILE_START(_pt);
    if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_text"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    const char *real_text = spec->text;
    char tmp[2] = {0};
    if ((uintptr_t)real_text < 256) {
        if (!real_text) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_text"); { PROFILE_END_VAR(_pt, "f"); return false; } }
        tmp[0] = (char)(uintptr_t)real_text;
        tmp[1] = '\0';
        real_text = tmp;
    }
    if (!real_text) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_text"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    unsigned long col = spec->color;
    if (col == 0) {
        unsigned long cur = fl_listbox_get_current_standard_color();
        col = cur ? cur : rgb_to_hex(COLOR_TEXT);
    }else {
        col = rgb_to_hex(col);
    }
    const char *align = spec->align ? spec->align : "CENTER";
    FontHandle fh = spec->font;
    bool r = Platform_DrawTextRaw(spec->x, spec->y, real_text, col, fh, align, spec->pad,spec->fontsize);
    PROFILE_END_VAR(_pt, "api:fl_draw_text");
    return r;
}

typedef int (*FlWrapMeasureFn)(void* ctx, const char* s, int n);
typedef void (*FlWrapDrawFn)(void* ctx, int x, int y, const char* s, int n);
typedef struct { unsigned long color; int fontsize; } FlWrapCtx;

static int FlUtf8LeadLen(unsigned char c) {
    PROFILE_START(_pt);
    if ((c & 0x80) == 0) { PROFILE_END_VAR(_pt, "FlUtf8LeadLen"); return 1; }
    if ((c & 0xE0) == 0xC0) { PROFILE_END_VAR(_pt, "FlUtf8LeadLen"); return 2; }
    if ((c & 0xF0) == 0xE0) { PROFILE_END_VAR(_pt, "FlUtf8LeadLen"); return 3; }
    if ((c & 0xF8) == 0xF0) { PROFILE_END_VAR(_pt, "FlUtf8LeadLen"); return 4; }
    { PROFILE_END_VAR(_pt, "FlUtf8LeadLen"); return 1; }
}

static void FlWrapRecord(FlCharPos* out, int maxout, int* count, int x, int y) {
        PROFILE_START(_pt);
int i = *count;
    (*count)++;
    if (out && i < maxout) {
        out[i].x = x;
        out[i].y = y;
    }
}

static int FlWrapPlaceWord(FlWrapMeasureFn measure, FlWrapDrawFn draw, void* ctx, const char* text, int ws, int we, int wx, int rowtop, FlCharPos* out, int maxout, int* count) {
    PROFILE_START(_pt);
    int i = ws;
    while (i < we) {
        int j = i + FlUtf8LeadLen((unsigned char)text[i]);
        if (j > we) j = we;
        int px = wx + measure(ctx, text + ws, i - ws);
        for (int m = i; m < j; m++) FlWrapRecord(out, maxout, count, px, rowtop);
        i = j;
    }
    draw(ctx, wx, rowtop, text + ws, we - ws);
    { PROFILE_END_VAR(_pt, "FlWrapPlaceWord"); return measure(ctx, text + ws, we - ws); }
}

static int FlWrapLayout(const char* text, int x0, int y0, int max_width, int line_height, FlWrapMeasureFn measure, FlWrapDrawFn draw, void* ctx, FlCharPos* out, int maxout) {
    PROFILE_START(_pt);
    int count = 0;
    int len = (int)strlen(text);
    int avail = max_width;
    if (avail < 40) avail = 40;
    int spaceW = measure(ctx, " ", 1);
    int row = 0;
    int p = 0;
    int lw = 0;
    bool has = false;
    while (p < len) {
        if (text[p] == '\n') {
            FlWrapRecord(out, maxout, &count, x0, y0 + (row + 1) * line_height);
            row++;
            lw = 0;
            has = false;
            p++;
            continue;
        }
        int ws = p;
        while (ws < len && (text[ws] == ' ' || text[ws] == '\t')) ws++;
        for (int m = p; m < ws; m++) FlWrapRecord(out, maxout, &count, x0 + (has ? lw : 0), y0 + row * line_height);
        if (ws >= len) break;
        if (text[ws] == '\n') {
            p = ws;
            continue;
        }
        int we = ws;
        while (we < len && text[we] != ' ' && text[we] != '\t' && text[we] != '\n') we++;
        int ww = measure(ctx, text + ws, we - ws);
        int rowtop = y0 + row * line_height;
        if (ww > avail) {
            if (has) {
                row++;
                rowtop = y0 + row * line_height;
                lw = 0;
                has = false;
            }
            int c = ws;
            while (c < we) {
                int lo = 0, hi = we - c;
                while (lo < hi) {
                    int mid = (lo + hi + 1) / 2;
                    if (measure(ctx, text + c, mid) <= avail) lo = mid;
                    else hi = mid - 1;
                }
                int n = lo;
                while (n > 0 && (((unsigned char)text[c + n] & 0xC0) == 0x80)) n--;
                if (n <= 0) {
                    n = FlUtf8LeadLen((unsigned char)text[c]);
                    if (n > we - c) n = we - c;
                }
                if (c + n >= we) {
                    lw = FlWrapPlaceWord(measure, draw, ctx, text, c, we, x0, rowtop, out, maxout, &count);
                    has = true;
                    c = we;
                    break;
                }
                FlWrapPlaceWord(measure, draw, ctx, text, c, c + n, x0, rowtop, out, maxout, &count);
                row++;
                rowtop = y0 + row * line_height;
                c += n;
            }
            p = we;
            continue;
        }
        int add = ww + (has ? spaceW : 0);
        if (has && lw + add > avail) {
            row++;
            rowtop = y0 + row * line_height;
            lw = 0;
            has = false;
        }
        int wx = x0 + (has ? lw + spaceW : 0);
        int pxw = FlWrapPlaceWord(measure, draw, ctx, text, ws, we, wx, rowtop, out, maxout, &count);
        lw = has ? lw + spaceW + pxw : pxw;
        has = true;
        p = we;
    }
    { PROFILE_END_VAR(_pt, "FlWrapLayout"); return count; }
}

static int FlWrapMeasureThunk(void* ctx, const char* s, int n) {
    PROFILE_START(_pt);
    FlWrapCtx* c = (FlWrapCtx*)ctx;
    { PROFILE_END_VAR(_pt, "FlWrapMeasureThunk"); return Platform_MeasureTextWidth(s, n, c->fontsize); }
}

static void FlWrapDrawThunk(void* ctx, int x, int y, const char* s, int n) {
        PROFILE_START(_pt);
FlWrapCtx* c = (FlWrapCtx*)ctx;
    Platform_DrawTextLine(x, y, s, n, c->color, c->fontsize);
}

static void FlWrapNoDraw(void* ctx, int x, int y, const char* s, int n) {
        PROFILE_START(_pt);
(void)ctx;
    (void)x;
    (void)y;
    (void)s;
    (void)n;
}

int fl_measure_text_wrapped_impl(const FlMeasureSpec *spec) {
    PROFILE_START(_pt);
    if (!spec || !spec->text) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_measure_text_wrapped"); { PROFILE_END_VAR(_pt, "f"); return 0; } }
    int fontsize = spec->fontsize;
    if (fontsize <= 0) fontsize = FontSize;
    int line_height = spec->line_height;
    if (line_height <= 0) line_height = fontsize + 4;
    FlWrapCtx ctx;
    ctx.color = 0;
    ctx.fontsize = fontsize;
    int cap = (int)strlen(spec->text) + 1;
    int r = FlWrapLayout(spec->text, spec->x, spec->y, spec->max_width, line_height, FlWrapMeasureThunk, FlWrapNoDraw, &ctx, spec->out_positions, cap);
    PROFILE_END_VAR(_pt, "api:fl_measure_text_wrapped");
    return r;
}

int fl_draw_text_wrapped_ex(int x, int y, int max_width, int line_height, const char* text, int fontsize, unsigned long color, FlCharPos* out_positions, int max_positions) {
    PROFILE_START(_pt);
    if (!text) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_text_wrapped_ex"); { PROFILE_END_VAR(_pt, "f"); return 0; } }
    if (fontsize <= 0) fontsize = FontSize;
    if (line_height <= 0) line_height = fontsize + 4;
    if (color == 0) color = rgb_to_hex(COLOR_TEXT);
    FlWrapCtx ctx;
    ctx.color = color;
    ctx.fontsize = fontsize;
    int r = FlWrapLayout(text, x, y, max_width, line_height, FlWrapMeasureThunk, FlWrapDrawThunk, &ctx, out_positions, max_positions);
    PROFILE_END_VAR(_pt, "api:fl_draw_text_wrapped_ex");
    return r;
}

bool fl_place_editbox_impl(const FlEditSpec *spec) {
  PROFILE_START(_pt);
  if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_place_editbox"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  FlintContext_Get()->window.editbox_placed = true;
  bool r = fl_place_editbox_platform(spec);
  PROFILE_END_VAR(_pt, "api:fl_place_editbox");
  return r;
}
bool fl_draw_image_impl(const FlImageSpec *spec) {
  PROFILE_START(_pt);
  if (!spec || !spec->path || !spec->path[0]) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_image"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  int w = spec->w > 0 ? spec->w : 0;
  int h = spec->h > 0 ? spec->h : 0;
  bool r = Platform_DrawImageAt(spec->path, spec->x, spec->y, w, h);
  PROFILE_END_VAR(_pt, "api:fl_draw_image");
  return r;
}
bool fl_draw_rect_impl(const FlRectSpec *spec) {
  PROFILE_START(_pt);
  if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_rect"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  unsigned long col = spec->color;
  bool transparent = spec->transparent ? spec->transparent : true;
  if (col == 0) col = COLOR_TEXT;
  col = rgb_to_hex(col);
  bool r = Platform_DrawRectRaw(spec->x, spec->y, spec->w, spec->h, col,spec->radius ? spec->radius : 0,spec->rounded ? spec->rounded : RoundedCorners,!transparent,spec->thickness,spec->filled);
  PROFILE_END_VAR(_pt, "api:fl_draw_rect");
  return r;
}

int fl_rand_num_between(int min, int max) {
    PROFILE_START(_pt);
  if (max < min) { int t = min; min = max; max = t; }
  unsigned long span = (unsigned long)((long)max - (long)min) + 1UL;
  if (span == 0UL) { PROFILE_END_VAR(_pt, "fl_rand_num_between"); return min; }
  unsigned long limit = (unsigned long)RAND_MAX + 1UL;
  unsigned long r = (unsigned long)rand();
  if (span > limit) {
    r = (r << 31) | (unsigned long)rand();
  }
  { PROFILE_END_VAR(_pt, "fl_rand_num_between"); return min + (int)(r % span); }
}

int fl_get_text_height(const char *text) {
  PROFILE_START(_pt);
  (void)text;
  int lh = 0, rh = 0, lt = 0;
  Platform_GetTextLayoutMetrics(&lh, &rh, &lt);
  int r = lh > 0 ? lh : FontSize;
  PROFILE_END_VAR(_pt, "api:fl_get_text_height");
  return r;
}
int fl_get_text_width(const char *text,int fontsize) {
    return Platform_MeasureTextWidth(text,strlen(text), fontsize);
}
int fl_get_glyph_ink_height(const char *text, int fontsize) {
  PROFILE_START(_pt);
  int r = Platform_GetGlyphInkHeight(text, fontsize);
  PROFILE_END_VAR(_pt, "api:fl_get_glyph_ink_height");
  return r;
}

char *fl_http_get(const char *url) {
  PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_http_get"); return Platform_HttpGet(url); }
}

char *fl_http_get_limited(const char *url, size_t max_bytes, size_t *out_len) {
  PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_http_get_limited"); return Platform_HttpGetLimited(url, max_bytes, out_len); }
}

bool fl_http_download(const char *url, const char *save_path) {
  PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_http_download"); return Platform_HttpDownload(url, save_path); }
}

char *fl_http_post(const char *url, const char *content_type, const char *body) {
  PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_http_post"); return Platform_HttpPost(url, content_type, body); }
}

void fl_http_get_async(const char *url, FlHttpCallback cb, void *userdata) {
  PROFILE_START(_pt);
  Platform_HttpGetAsync(url, cb, userdata);
  { PROFILE_END_VAR(_pt, "fl_http_get_async"); return ; }
}

bool fl_draw_line_impl(const FlLineSpec *spec) {
  PROFILE_START(_pt);
  if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_line"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  unsigned long col = spec->color;
  col = rgb_to_hex(col);
  if (col == 0) col = COLOR_TEXT;
  bool r = Platform_DrawLineRaw(spec->x1, spec->y1, spec->x2, spec->y2, col, spec->thickness);
  PROFILE_END_VAR(_pt, "api:fl_draw_line");
  return r;
}
bool fl_draw_circle_impl(const FlCircleSpec *spec) {
  PROFILE_START(_pt);
  if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_draw_circle"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  unsigned long col = spec->color;
  col = rgb_to_hex(col);
  if (col == 0) col = COLOR_TEXT;
  bool r = Platform_DrawCircleRaw(spec->x, spec->y, spec->radius, col, spec->thickness);
  PROFILE_END_VAR(_pt, "api:fl_draw_circle");
  return r;
}
bool fl_listbox_begin(const FlListSpec *spec) {
    PROFILE_START(_pt);
    if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_listbox_begin"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    bool r = fl_listbox_begin_platform(spec);
    PROFILE_END_VAR(_pt, "api:fl_listbox_begin");
    return r;
}
bool fl_listbox_next(FlListItemCtx *out) {
    PROFILE_START(_pt);
    if (!out) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_listbox_next"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    bool r = fl_listbox_next_platform(out);
    PROFILE_END_VAR(_pt, "api:fl_listbox_next");
    return r;
}
void fl_listbox_end(void) {
    PROFILE_START(_pt);
    fl_listbox_end_platform();
    PROFILE_END_VAR(_pt, "api:fl_listbox_end");
}
bool fl_place_listbox_impl(const FlListSpec *spec) {
    PROFILE_START(_pt);
    if (!spec) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_place_listbox"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    if (!fl_listbox_begin(spec)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_place_listbox"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        fl_draw_text(.x=ctx.x + 10, .y=ctx.y + 4, .text=ctx.text, .align="LEFT", .pad=ctx.x + 10);
    }
    fl_listbox_end();
    PROFILE_END_VAR(_pt, "api:fl_place_listbox");
    return true;
}
typedef struct {
    char name[128];
    bool checked;
    bool hovered;
    bool was_down;
    void (*on_click)(const char *name, bool checked);
    void (*on_hover)(const char *name, bool hovered);
} FlCheckboxState;

#define FL_CHECKBOX_MAX 64
static FlCheckboxState g_checkboxes[FL_CHECKBOX_MAX];
static int g_checkbox_count = 0;

static struct {
    bool valid;
    int mx, my;
    bool down;
} g_cb_mouse;

void fl_checkbox_frame_begin(void) {
      PROFILE_START(_pt);
      g_cb_mouse.valid = false;
}

static void fl_checkbox_mouse(int *out_x, int *out_y, bool *out_down) {
        PROFILE_START(_pt);
if (!g_cb_mouse.valid) {
        bool have = Platform_GetMousePos(&g_cb_mouse.mx, &g_cb_mouse.my);
        if (have) {
            int wx = 0, wy = 0, dx = 0, dy = 0;
            WindowManager* wm = WindowManager_Get();
            Popup* top = wm ? WindowManager_GetTopPopup(wm) : NULL;
            if (top && Platform_WindowToRoot(top->handle, &wx, &wy)) {
                g_cb_mouse.mx -= wx;
                g_cb_mouse.my -= wy;
            }
            Platform_GetDrawOffset(&dx, &dy);
            g_cb_mouse.mx -= dx;
            g_cb_mouse.my -= dy;
        } else {
            g_cb_mouse.mx = 0;
            g_cb_mouse.my = 0;
        }
        g_cb_mouse.down = Platform_MouseStateDown(MOUSE_BTN_LEFT);
        g_cb_mouse.valid = true;
    }
    if (out_x) *out_x = g_cb_mouse.mx;
    if (out_y) *out_y = g_cb_mouse.my;
    if (out_down) *out_down = g_cb_mouse.down;
}

static FlCheckboxState* fl_checkbox_find(const char* name) {
        PROFILE_START(_pt);
for (int i = 0; i < g_checkbox_count; i++) {
        if (strcmp(g_checkboxes[i].name, name) == 0) return &g_checkboxes[i];
    }
    { PROFILE_END_VAR(_pt, "fl_checkbox_find"); return NULL; }
}

static FlCheckboxState* fl_checkbox_intern(const char* name, bool seed) {
        PROFILE_START(_pt);
FlCheckboxState* cb = fl_checkbox_find(name);
    if (cb) { PROFILE_END_VAR(_pt, "fl_checkbox_intern"); return cb; }
    if (g_checkbox_count >= FL_CHECKBOX_MAX) { PROFILE_END_VAR(_pt, "fl_checkbox_intern"); return NULL; }
    cb = &g_checkboxes[g_checkbox_count++];
    memset(cb, 0, sizeof(*cb));
    snprintf(cb->name, sizeof(cb->name), "%s", name);
    cb->checked = seed;
    { PROFILE_END_VAR(_pt, "fl_checkbox_intern"); return cb; }
}

bool fl_checkbox_is_checked(const char *name) {
    PROFILE_START(_pt);
    FlCheckboxState* cb = name ? fl_checkbox_find(name) : NULL;
    bool r = cb ? cb->checked : false;
    PROFILE_END_VAR(_pt, "api:fl_checkbox_is_checked");
    return r;
}

bool fl_checkbox_is_hovered(const char *name) {
    PROFILE_START(_pt);
    FlCheckboxState* cb = name ? fl_checkbox_find(name) : NULL;
    bool r = cb ? cb->hovered : false;
    PROFILE_END_VAR(_pt, "api:fl_checkbox_is_hovered");
    return r;
}

#define FL_CHECKBOX_BASE 16
#define FL_CHECKBOX_GLYPH "\xe2\x9c\x93"

static int fl_checkbox_line_height(int fontsize) {
        PROFILE_START(_pt);
int ref_h = fl_get_text_height("");
    int ref_fs = FontSize > 0 ? FontSize : fontsize;
    if (ref_h <= 0 || ref_fs <= 0) { PROFILE_END_VAR(_pt, "fl_checkbox_line_height"); return fontsize; }
    { PROFILE_END_VAR(_pt, "fl_checkbox_line_height"); return (int)((long)ref_h * fontsize / ref_fs); }
}

void fl_draw_checkbox(int x, int y, int w, int h, bool checked, bool hovered) {
    PROFILE_START(_pt);
    if (w <= 0 || h <= 0) { PROFILE_END_VAR(_pt, "api:fl_draw_checkbox"); return; }

    unsigned long border = hovered ? FL_WHITE : FL_GRAY;

    Platform_DrawRectRaw(x, y, w, h, FL_DARK_GRAY,0, false, true, 1, true);
    Platform_DrawRectRaw(x, y, w, h, border, 0, false, true, 2, false);

    if (checked) {
        int fontsize = w * 3 / 4;
        if (fontsize < 1) fontsize = 1;
        int tw = fl_get_text_width(FL_CHECKBOX_GLYPH,fontsize);
        int th = fl_checkbox_line_height(fontsize);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - th) / 2;
        fl_draw_text(.text = FL_CHECKBOX_GLYPH,
                     .x = tx,
                     .y = ty - 2,
                     .fontsize = fontsize,
                     .color = FL_WHITE,
                     .align = "LEFT");
    }

    PROFILE_END_VAR(_pt, "api:fl_draw_checkbox");
}

bool fl_place_checkbox_impl(const FlCheckboxSpec *spec) {
    PROFILE_START(_pt);
    if (!spec || !spec->name || !spec->name[0]) {
        PROFILE_END_VAR(_pt, "api:fl_place_checkbox");
        return false;
    }

    int x = spec->x;
    int y = spec->y;
    int scale = spec->scale > 0 ? spec->scale : 1;
    int side = FL_CHECKBOX_BASE * scale;

    FlCheckboxState* cb = fl_checkbox_intern(spec->name, spec->checked);
    if (!cb) { PROFILE_END_VAR(_pt, "api:fl_place_checkbox"); return false; }
    cb->on_click = spec->on_click;
    cb->on_hover = spec->on_hover;

    int mx = 0, my = 0;
    bool down = false;
    fl_checkbox_mouse(&mx, &my, &down);

    bool hovered = (mx >= x && mx < x + side && my >= y && my < y + side);
    bool hover_changed = (hovered != cb->hovered);
    cb->hovered = hovered;

    bool clicked = false;
    if (down && !cb->was_down && hovered) {
        cb->checked = !cb->checked;
        clicked = true;
    }
    cb->was_down = down;

    fl_draw_checkbox(x, y, side, side, cb->checked, cb->hovered);

    if (clicked && cb->on_click) cb->on_click(cb->name, cb->checked);
    if (hover_changed && cb->on_hover) cb->on_hover(cb->name, cb->hovered);

    PROFILE_END_VAR(_pt, "api:fl_place_checkbox");
    return true;
}

PlWindowList fl_get_all_window_titles() {
   PROFILE_START(_pt);
   PlWindowList l = Platform_GetWindowTitles();
   PROFILE_END_VAR(_pt, "api:fl_get_all_window_titles");
   return l;
}
PlWindowList fl_get_all_window_names() {
  PROFILE_START(_pt);
  PlWindowList l = Platform_GetWindowNames();
  PROFILE_END_VAR(_pt, "api:fl_get_all_window_names");
  return l;
}

void fl_toggle_play_pause(void) {
    PROFILE_START(_pt);
  fl_toggle_play_pause_platform();
  { PROFILE_END_VAR(_pt, "fl_toggle_play_pause"); return ; }
}

PlColor fl_get_pixel_color(PlPos pos) {
  PROFILE_START(_pt);
  PlColor c = Platform_GetPixelColor(pos.x, pos.y);
  PROFILE_END_VAR(_pt, "flapi:fl_get_pixel_color");
  return c;
}
bool fl_move_to_pos(PlPos pos) {
  PROFILE_START(_pt);
  bool ok = Platform_MoveToPos(pos.x, pos.y);
  PROFILE_END_VAR(_pt, "flapi:fl_move_to_pos");
  return ok;
}
bool fl_click_at_pos(PlPos pos) {
  PROFILE_START(_pt);
  bool ok = fl_move_to_pos(pos) && fl_click();
  PROFILE_END_VAR(_pt, "flapi:fl_click_at_pos");
  return ok;
}
bool fl_left_click(void) {
  PROFILE_START(_pt);
  bool ok = Platform_ClickButton(1);
  PROFILE_END_VAR(_pt, "flapi:fl_left_click");
  return ok;
}
bool fl_right_click(void) {
  PROFILE_START(_pt);
  bool ok = Platform_ClickButton(2);
  PROFILE_END_VAR(_pt, "flapi:fl_right_click");
  return ok;
}
bool fl_emit_mouse_button(int button, bool down) {
  PROFILE_START(_pt);
  bool ok = Platform_MouseButtonDown(button, down);
  PROFILE_END_VAR(_pt, "flapi:fl_emit_mouse_button");
  return ok;
}
bool fl_emit_mouse_button_at(PlPos pos, int button, bool down) {
  PROFILE_START(_pt);
  bool moved = fl_move_to_pos(pos);
  bool ok = moved && Platform_MouseButtonDown(button, down);
  PROFILE_END_VAR(_pt, "flapi:fl_emit_mouse_button_at");
  return ok;
}
bool fl_click_button_at(PlPos pos, int button) {
  PROFILE_START(_pt);
  bool moved = fl_move_to_pos(pos);
  bool down = moved && Platform_MouseButtonDown(button, true);
  if (!down) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_click_button_at"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  fl_sleep(0.02);
  bool up = Platform_MouseButtonDown(button, false);
  PROFILE_END_VAR(_pt, "flapi:fl_click_button_at");
  return up;
}
bool fl_mouse_scroll(int notches) {
  PROFILE_START(_pt);
  bool ok = Platform_MouseScroll(notches);
  PROFILE_END_VAR(_pt, "flapi:fl_mouse_scroll");
  return ok;
}

static bool fl_rel_to_abs(int rx, int ry, FlPos *out) {
  PROFILE_START(_pt);
  if (!out) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_rel_to_abs"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  int vx = 0, vy = 0, vw = 0, vh = 0;
  if (!Platform_GetVirtualBounds(&vx, &vy, &vw, &vh)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_rel_to_abs"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  out->x = vx + rx;
  out->y = vy + ry;
  PROFILE_END_VAR(_pt, "flapi:fl_rel_to_abs");
  return true;
}

bool fl_move_to_pos_rel(int rx, int ry) {
  PROFILE_START(_pt);
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_move_to_pos_rel"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool ok = fl_move_to_pos(abs);
  PROFILE_END_VAR(_pt, "flapi:fl_move_to_pos_rel");
  return ok;
}

bool fl_click_at_pos_rel(int rx, int ry) {
  PROFILE_START(_pt);
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_click_at_pos_rel"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool ok = fl_click_at_pos(abs);
  PROFILE_END_VAR(_pt, "flapi:fl_click_at_pos_rel");
  return ok;
}

bool fl_emit_mouse_button_at_rel(int rx, int ry, int button, bool down) {
  PROFILE_START(_pt);
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_emit_mouse_button_at_rel"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool ok = fl_emit_mouse_button_at(abs, button, down);
  PROFILE_END_VAR(_pt, "flapi:fl_emit_mouse_button_at_rel");
  return ok;
}

bool fl_click_button_at_rel(int rx, int ry, int button) {
  PROFILE_START(_pt);
  FlPos abs;
  if (!fl_rel_to_abs(rx, ry, &abs)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_click_button_at_rel"); { PROFILE_END_VAR(_pt, "f"); return false; } }
  bool ok = fl_click_button_at(abs, button);
  PROFILE_END_VAR(_pt, "flapi:fl_click_button_at_rel");
  return ok;
}

void fl_emit_key(int vk, bool down) {
  PROFILE_START(_pt);
  Platform_EmitKey(vk, down ? 1 : 0);
  PROFILE_END_VAR(_pt, "flapi:fl_emit_key");
}

bool fl_replay_hotkey(int vk, int mods) {
  PROFILE_START(_pt);
  bool matched = hotkey_process(vk, mods);
  PROFILE_END_VAR(_pt, "flapi:fl_replay_hotkey");
  return matched;
}
int fl_get_current_volume(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_get_current_volume"); return fl_get_current_volume_platform(); }
}
bool fl_set_current_volume(int volume) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_set_current_volume"); return fl_set_volume_platform(volume); }
}
bool fl_increment_volume_by(int increment) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_increment_volume_by"); return fl_set_current_volume(fl_get_current_volume() + increment); }
}

bool fl_decrement_volume_by(int decrement) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_decrement_volume_by"); return fl_set_current_volume(fl_get_current_volume() - decrement); }
}
bool fl_type_text(const char *s) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_type_text"); return fl_type_text_platform(s); }
}

void fl_sleep(double seconds) {
    PROFILE_START(_pt);
fl_sleep_platform(seconds);
}

FlThread fl_thread_create(FlThreadFunc fn, void* arg) {
    PROFILE_START(_pt);
  if (!fn) { PROFILE_END_VAR(_pt, "fl_thread_create"); return NULL; }
  { PROFILE_END_VAR(_pt, "fl_thread_create"); return (FlThread)fl_thread_create_platform((void* (*)(void*))fn, arg); }
}

void fl_thread_join(FlThread thread) {
    PROFILE_START(_pt);
  if (!thread) { PROFILE_END_VAR(_pt, "fl_thread_join"); return ; }
  fl_thread_join_platform((void*)thread);
}

FlMutex fl_mutex_create(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_mutex_create"); return (FlMutex)fl_mutex_create_platform(); }
}

void fl_mutex_destroy(FlMutex mutex) {
    PROFILE_START(_pt);
  if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_destroy"); return ; }
  fl_mutex_destroy_platform((void*)mutex);
}

void fl_mutex_lock(FlMutex mutex) {
    PROFILE_START(_pt);
  if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_lock"); return ; }
  fl_mutex_lock_platform((void*)mutex);
}

void fl_mutex_unlock(FlMutex mutex) {
    PROFILE_START(_pt);
  if (!mutex) { PROFILE_END_VAR(_pt, "fl_mutex_unlock"); return ; }
  fl_mutex_unlock_platform((void*)mutex);
}

void fl_send_hotkey(const char* hotkey_string) {
    PROFILE_START(_pt);
  if (!hotkey_string) { PROFILE_END_VAR(_pt, "fl_send_hotkey"); return ; }
  remap_send_combo(hotkey_string);
}

bool fl_is_key_down(int vk){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_is_key_down"); return Platform_IsKeyDown(vk); } 
}
bool fl_is_key_pressed(int vk){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_is_key_pressed"); return Platform_WasKeyPressed(vk); } 
}
bool fl_is_key_released(int vk){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_is_key_released"); return Platform_WasKeyReleased(vk); } 
}
bool fl_ctrl_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_ctrl_pressed"); return Platform_IsCtrlPressed(); }
}
bool fl_shift_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_shift_pressed"); return Platform_IsShiftPressed(); }
}
bool fl_alt_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_alt_pressed"); return Platform_IsAltPressed(); }
}
bool fl_super_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_super_pressed"); return Platform_IsSuperPressed(); }
}
void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud){
    PROFILE_START(_pt);
Platform_SetKeyCallback(cb,ud);
}
int fl_get_key_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_get_key_pressed"); return Platform_GetPressedKey(); }
}
char fl_get_char_pressed(void){
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_get_char_pressed"); return Platform_GetPressedChar(); }
}

bool fl_poll_key(FlKeyEvent *out) {
    PROFILE_START(_pt);
  if (!out) { PROFILE_END_VAR(_pt, "fl_poll_key"); return false; }
  PlatformKeyEvent ev;
  if (!Platform_GlobalKey_Pop(&ev)) { PROFILE_END_VAR(_pt, "fl_poll_key"); return false; }
  out->vk = ev.vk;
  out->scancode = ev.scancode;
  out->mods = ev.mods;
  out->down = ev.down;
  out->repeat = ev.repeat;
  out->t_us = ev.t_us;
  { PROFILE_END_VAR(_pt, "fl_poll_key"); return true; }
}

bool fl_poll_mouse(FlMouseEvent *out) {
    PROFILE_START(_pt);
  if (!out) { PROFILE_END_VAR(_pt, "fl_poll_mouse"); return false; }
  PlatformMouseEvent ev;
  if (!Platform_GlobalMouse_Pop(&ev)) { PROFILE_END_VAR(_pt, "fl_poll_mouse"); return false; }
  out->button = ev.button;
  out->x = ev.x;
  out->y = ev.y;
  out->mods = ev.mods;
  out->down = ev.down;
  out->repeat = ev.repeat;
  out->t_us = ev.t_us;
  { PROFILE_END_VAR(_pt, "fl_poll_mouse"); return true; }
}

int fl_mouse_queue_len(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_mouse_queue_len"); return Platform_GlobalMouse_Pending(); }
}

unsigned long fl_mouse_queue_dropped(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_mouse_queue_dropped"); return Platform_GlobalMouse_Dropped(); }
}

void fl_clear_mouse_queue(void) {
    PROFILE_START(_pt);
Platform_GlobalMouse_Clear();
}

FlPos fl_get_cursor_pos() {
    PROFILE_START(_pt);
  int x, y;
  Platform_GetMousePos(&x, &y);
  { PROFILE_END_VAR(_pt, "fl_get_cursor_pos"); return (FlPos){x, y}; }
}

bool fl_mouse_pressed(int button) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_mouse_pressed"); return Platform_MouseStateDown(button); }
}

int fl_key_queue_len(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_key_queue_len"); return Platform_GlobalKey_Pending(); }
}

unsigned long fl_key_queue_dropped(void) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_key_queue_dropped"); return Platform_GlobalKey_Dropped(); }
}

void fl_clear_key_queue(void) {
    PROFILE_START(_pt);
Platform_GlobalKey_Clear();
}

int fl_char_for_key(int vk, int scancode, int mods) {
    PROFILE_START(_pt);
  { PROFILE_END_VAR(_pt, "fl_char_for_key"); return Platform_CharForKey(vk, scancode, mods); }
}

bool fl_is_flag_prefix(char c) {     PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_is_flag_prefix"); return c == '-' || c == '/'; } }

size_t fl_extract_flag_name(char *p, char *out, size_t cap) {
    PROFILE_START(_pt);
    size_t n = 0;
    while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '=') n++;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n); out[n] = '\0';
    { PROFILE_END_VAR(_pt, "fl_extract_flag_name"); return n; }
}

bool fl_extract_quoted(char **pp, char *out, size_t cap) {
    PROFILE_START(_pt);
    char *p = *pp;
    char q = *p++;
    char *s = p;
    while (*p && *p != q) p++;
    size_t len = p - s;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len); out[len] = '\0';
    if (*p == q) p++;
    *pp = p;
    { PROFILE_END_VAR(_pt, "fl_extract_quoted"); return true; }
}
bool fl_extract_value(char **pp, char *out, size_t cap) {
    PROFILE_START(_pt);
    char *p = *pp;
    if (*p == '"' || *p == '\'') { PROFILE_END_VAR(_pt, "fl_extract_value"); return fl_extract_quoted(pp, out, cap); }
    char *s = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    size_t len = p - s;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len); out[len] = '\0';
    *pp = p;
    { PROFILE_END_VAR(_pt, "fl_extract_value"); return len > 0; }
}
bool fl_next_flag(char *p) {
    PROFILE_START(_pt);
    while (*p && isspace((unsigned char)*p)) p++;
    { PROFILE_END_VAR(_pt, "fl_next_flag"); return *p == '-' || *p == '/'; }
}
Fl_FlagList fl_parse_flags(const char* input) {
    Fl_FlagList out = {0};
    if (!input || !*input) return out;
    char *copy = strdup(input);
    if (!copy) return out;
    size_t cap = 8;
    Fl_Flag *arr = (Fl_Flag*)calloc(cap, sizeof(Fl_Flag));
    if (!arr) {     PROFILE_START(_pt);
free(copy); { PROFILE_END_VAR(_pt, "f"); return out; } }
    char *p = copy;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (!fl_is_flag_prefix(*p)) { while (*p && !isspace((unsigned char)*p)) p++; continue; }
        p++; if (*p == '-') p++;
        char name[64] = {0};
        size_t nlen = fl_extract_flag_name(p, name, sizeof(name));
        p += nlen;
        char valbuf[256] = {0};
        bool has_val = false;
        if (*p == '=') { p++; has_val = fl_extract_value(&p, valbuf, sizeof(valbuf)); }
        else {
            char *save = p;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p && !fl_is_flag_prefix(*p)) {
                has_val = fl_extract_value(&p, valbuf, sizeof(valbuf));
                if (has_val && fl_next_flag(p)) { has_val = false; p = save; while (*p && isspace((unsigned char)*p)) p++; }
            }
        }
        if (out.count >= cap) {
            cap *= 2;
            Fl_Flag *narr = (Fl_Flag*)realloc(arr, cap * sizeof(Fl_Flag));
            if (!narr) break;
            arr = narr;
        }
        Fl_Flag *f = &arr[out.count++];
        memset(f, 0, sizeof(*f));
        strncpy(f->name, name, sizeof(f->name)-1);
        if (has_val) { strncpy(f->value, valbuf, sizeof(f->value)-1); f->has_value = true; }
    }
    free(copy);
    out.flags = arr;
    if (out.count == 0) { free(arr); out.flags = NULL; }
    return out;
}

void fl_free_flags(Fl_FlagList* list) {
    PROFILE_START(_pt);
    if (!list) { PROFILE_END_VAR(_pt, "fl_free_flags"); return ; }
    free(list->flags);
    list->flags = NULL;
    list->count = 0;
}

int fl_time_unit_mult(const char *unit) {
    PROFILE_START(_pt);
    if (!unit || !*unit) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 1; }
    if (strcmp(unit,"ms")==0 || strcmp(unit,"millis")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 0; }
    if (strcmp(unit,"s")==0 || strcmp(unit,"sec")==0 || strcmp(unit,"secs")==0 || strcmp(unit,"second")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 1; }
    if (strcmp(unit,"m")==0 || strcmp(unit,"min")==0 || strcmp(unit,"mins")==0 || strcmp(unit,"minute")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 60; }
    if (strcmp(unit,"h")==0 || strcmp(unit,"hr")==0 || strcmp(unit,"hrs")==0 || strcmp(unit,"hour")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 3600; }
    if (strcmp(unit,"d")==0 || strcmp(unit,"day")==0 || strcmp(unit,"days")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 86400; }
    if (strcmp(unit,"w")==0 || strcmp(unit,"week")==0 || strcmp(unit,"weeks")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 604800; }
    if (strcmp(unit,"mo")==0 || strcmp(unit,"month")==0 || strcmp(unit,"months")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 2592000; }
    if (strcmp(unit,"y")==0 || strcmp(unit,"yr")==0 || strcmp(unit,"year")==0) { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 31536000; }
    { PROFILE_END_VAR(_pt, "fl_time_unit_mult"); return 1; }
}

int fl_parse_time(const char* time_str) {
    PROFILE_START(_pt);
    if (!time_str || !*time_str) { PROFILE_END_VAR(_pt, "fl_parse_time"); return 0; }
    int total = 0;
    const char *p = time_str;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char *end = NULL;
        long num = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        p = end;
        while (*p && isspace((unsigned char)*p)) p++;
        char unit[16] = {0};
        int ui = 0;
        while (*p && isalpha((unsigned char)*p) && ui < 15) unit[ui++] = tolower((unsigned char)*p++);
        unit[ui]='\0';
        int mult = fl_time_unit_mult(unit);
        if (mult == 0) total += num / 1000;
        else total += num * mult;
    }
    { PROFILE_END_VAR(_pt, "fl_parse_time"); return total; }
}

void fl_run_after_time(double seconds, void (*callback)(void)) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_run_after_time"); return fl_run_after_time_platform(seconds,callback); }
}

FlTimer fl_start_timer(void) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_start_timer"); return fl_start_timer_platform(); }
}

double fl_end_timer(FlTimer t) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_end_timer"); return fl_end_timer_platform(t); }
}

char* fl_get_active_window_title(void) {
  PROFILE_START(_pt);
  char* title = Platform_GetActiveWindowTitle();
  if (!title || !title[0]) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_get_active_window_title"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
  char* r = strdup(title);
  PROFILE_END_VAR(_pt, "api:fl_get_active_window_title");
  return r;
}

char* fl_get_active_window_app_id(void) {
  PROFILE_START(_pt);
  char* id = Platform_GetActiveWindowAppId();
  if (!id || !id[0]) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_get_active_window_app_id"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
  char* r = strdup(id);
  PROFILE_END_VAR(_pt, "api:fl_get_active_window_app_id");
  return r;
}

bool fl_is_window_active(char* title) {
  PROFILE_START(_pt);
  PlWindowList l = Platform_GetWindowTitles();
  for (int i = 0; i < l.count;i++) {
      if (Platform_StringContainsCaseInsensitive(l.windows[i],title)) {
    PROFILE_START(_pt);
        PROFILE_END_VAR(_pt, "api:fl_is_window_active");
        { PROFILE_END_VAR(_pt, "f"); return true; }
      }
  }
  PROFILE_END_VAR(_pt, "api:fl_is_window_active");
  return false;
}
char* fl_get_working_dir() {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_get_working_dir"); return Platform_GetWorkingDir(); }
}

void fl_hashmap_init(FlHashMap** map) {
    PROFILE_START(_pt);
    if (!map) { PROFILE_END_VAR(_pt, "fl_hashmap_init"); return ; }
    sh_new_strdup(*map);
}

void fl_hashmap_free(FlHashMap** map) {
    PROFILE_START(_pt);
    if (!map) { PROFILE_END_VAR(_pt, "fl_hashmap_free"); return ; }
    shfree(*map);
}

void fl_hashmap_put(FlHashMap** map, const char* key, long value) {
    PROFILE_START(_pt);
    if (!map || !key) { PROFILE_END_VAR(_pt, "fl_hashmap_put"); return ; }
    shput(*map, (char*)key, value);
}

void fl_hashmap_add(FlHashMap** map, const char* key, long delta) {
    PROFILE_START(_pt);
    if (!map || !key) { PROFILE_END_VAR(_pt, "fl_hashmap_add"); return ; }
    ptrdiff_t idx = shgeti(*map, (char*)key);
    if (idx >= 0) (*map)[idx].value += delta;
    else shput(*map, (char*)key, delta);
}

long fl_hashmap_get(FlHashMap* map, const char* key, long def) {
    PROFILE_START(_pt);
    if (!map || !key) { PROFILE_END_VAR(_pt, "fl_hashmap_get"); return def; }
    ptrdiff_t idx = shgeti(map, (char*)key);
    if (idx < 0) { PROFILE_END_VAR(_pt, "fl_hashmap_get"); return def; }
    { PROFILE_END_VAR(_pt, "fl_hashmap_get"); return map[idx].value; }
}

bool fl_hashmap_has(FlHashMap* map, const char* key) {
    PROFILE_START(_pt);
    if (!map || !key) { PROFILE_END_VAR(_pt, "fl_hashmap_has"); return false; }
    { PROFILE_END_VAR(_pt, "fl_hashmap_has"); return shgeti(map, (char*)key) >= 0; }
}

bool fl_hashmap_del(FlHashMap** map, const char* key) {
    PROFILE_START(_pt);
    if (!map || !*map || !key) { PROFILE_END_VAR(_pt, "fl_hashmap_del"); return false; }
    { PROFILE_END_VAR(_pt, "fl_hashmap_del"); return shdel(*map, (char*)key) != 0; }
}

size_t fl_hashmap_len(FlHashMap* map) {
    PROFILE_START(_pt);
    if (!map) { PROFILE_END_VAR(_pt, "fl_hashmap_len"); return 0; }
    { PROFILE_END_VAR(_pt, "fl_hashmap_len"); return shlenu(map); }
}

const FlHashMap* fl_hashmap_at(FlHashMap* map, size_t index) {
    PROFILE_START(_pt);
    if (!map || index >= shlenu(map)) { PROFILE_END_VAR(_pt, "fl_hashmap_at"); return NULL; }
    { PROFILE_END_VAR(_pt, "fl_hashmap_at"); return &map[index]; }
}

FlTimer fl_profile_start() {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_profile_start"); return fl_start_timer_platform(); }
}

void fl_profile_end(FlTimer timer) {
        PROFILE_START(_pt);
printf("[profile] %f s\n", fl_end_timer_platform(timer) / 1000);
}


static FlWidget *g_fl_widgets[FL_WIDGET_MAX];
static int g_fl_widget_count = 0;
static int g_fl_widget_seq = 0;
static unsigned g_fl_widget_gen = 1;

static FlTimer g_mon_at;
static bool g_mon_valid = false;
static int g_mon_x, g_mon_y, g_mon_w, g_mon_h;

static void fl_widget_monitor_geometry(int *x, int *y, int *w, int *h) {
        PROFILE_START(_pt);
if (!g_mon_valid || fl_end_timer_platform(g_mon_at) > 2000) {
        Platform_WM_GetMonitorGeometry(&g_mon_x, &g_mon_y, &g_mon_w, &g_mon_h);
        g_mon_at = fl_start_timer_platform();
        g_mon_valid = true;
    }
    *x = g_mon_x;
    *y = g_mon_y;
    *w = g_mon_w;
    *h = g_mon_h;
}

unsigned fl_widget_bump_generation(void) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_widget_bump_generation"); return ++g_fl_widget_gen; }
}

unsigned fl_widget_current_generation(void) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_widget_current_generation"); return g_fl_widget_gen; }
}

static FlWidget *fl_widget_find_by_name(const char *name) {
    PROFILE_START(_pt);
    if (!name || !name[0]) { PROFILE_END_VAR(_pt, "fl_widget_find_by_name"); return NULL; }
    for (int i = 0; i < g_fl_widget_count; i++) {
        if (g_fl_widgets[i] && strcmp(g_fl_widgets[i]->name, name) == 0) return g_fl_widgets[i];
    }
    { PROFILE_END_VAR(_pt, "fl_widget_find_by_name"); return NULL; }
}

static void fl_widget_relayout(void);

static PlPos g_fl_hover_pos = {0, 0};
static FlTimer g_fl_hover_at = {0};
static bool g_fl_hover_valid = false;

static void fl_widget_hover_poll(void) {
    PROFILE_START(_pt);
    FlTimer now = fl_start_timer_platform();
    if (g_fl_hover_valid && fl_end_timer_platform(g_fl_hover_at) < 400.0) {
    PROFILE_START(_pt);
        PROFILE_END_VAR(_pt, "flapi:fl_widget_hover_poll");
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
    g_fl_hover_pos = fl_get_cursor_pos();
    g_fl_hover_at = now;
    g_fl_hover_valid = true;
    PROFILE_END_VAR(_pt, "flapi:fl_widget_hover_poll");
}

static void *fl_widget_hover_worker(void *ud) {
    PROFILE_START(_pt);
    FlWidget *w = (FlWidget *)ud;
    if (w && w->on_hover) w->on_hover();
    if (w) w->hover_running = false;
    { PROFILE_END_VAR(_pt, "fl_widget_hover_worker"); return NULL; }
}

static void fl_widget_render_frame(void *ud) {
    PROFILE_START(_pt);
    FlWidget *widget = (FlWidget *)ud;
    if (!widget || !widget->render || widget->dead) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:widget[null]"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    widget->frame++;
    bool hoverable = (widget->on_hover != NULL);
    if (hoverable) {
        fl_widget_hover_poll();
        bool inside = (widget->x >= 0x7fffffff) ? false
            : (g_fl_hover_pos.x >= widget->x && g_fl_hover_pos.x < widget->x + widget->w &&
               g_fl_hover_pos.y >= widget->y && g_fl_hover_pos.y < widget->y + widget->h);
        if (inside && !widget->hovered && widget->on_hover && !widget->hover_running) {
            widget->hover_running = true;
            widget->hover_thread = fl_thread_create(fl_widget_hover_worker, widget);
            if (!widget->hover_thread) widget->hover_running = false;
        }
        widget->hovered = inside;
        if (!inside) {
            Platform_HoverMessageRelease(widget->name);
            Platform_HoverWidgetHide(widget->name);
        }
    }
    FlWidgetState st;
    st.width = widget->w;
    st.height = widget->h;
    st.frame = widget->frame;
    st.elapsed = fl_end_timer_platform(widget->created_at);
    st.hovered = widget->hovered;

    Platform_SetDrawOffset(-widget->origin_x, 0);
    Platform_BeginMeasure();
    widget->render(&st);
    int mw = 0, mh = 0, mx = 0, my = 0;
    Platform_EndMeasure(&mw, &mh);
    Platform_GetLastMeasureOrigin(&mx, &my);
    mx += widget->origin_x;

    if (widget->origin_x != mx || widget->origin_y != my) {
        widget->origin_x = mx;
        widget->origin_y = my;
        Platform_Widget_Invalidate(widget->win);
    }

    if (widget->auto_w || widget->auto_h) {
        int nw = (widget->auto_w && mw > 0) ? mw + widget->pad : widget->w;
        int nh = (widget->auto_h && mh > 0) ? mh + widget->pad : widget->h;
        
        if (nw != widget->w || nh != widget->h) {
            widget->w = nw;
            widget->h = nh;
            if (Platform_Widget_IsValid(widget->win)) {
                Platform_Widget_Resize(widget->win, nw, nh);
            }
            fl_widget_relayout();
        }
    }
    char prof_label[80];
    snprintf(prof_label, sizeof(prof_label), "flapi:widget[%s]", widget->name);
    PROFILE_END_VAR(_pt, prof_label);
}

static void fl_widget_owner_path(void *addr, char *out, size_t n) {
        PROFILE_START(_pt);
Platform_GetModuleOwnerPath(addr, out, n);
}

static const char* fl_widget_owner_basename(const char* p, char* out, size_t n) {
    if (!p || !p[0]) {     PROFILE_START(_pt);
if (n) out[0] = '\0'; { PROFILE_END_VAR(_pt, "f"); return out; } }
    const char* s = p;
    for (const char* q = p; *q; q++) if (*q == '/' || *q == '\\') s = q + 1;
    snprintf(out, n, "%s", s);
    return out;
}

static bool fl_widget_owner_matches(const FlWidget *widget, const char *a, const char *b) {
    if (!widget->owner) return false;
    if (a && a[0] && strcmp(widget->owner, a) == 0) return true;
    if (b && b[0] && strcmp(widget->owner, b) == 0) return true;
    char ab[512], bb[512], wb[512];
    fl_widget_owner_basename(widget->owner, wb, sizeof(wb));
    if (a && a[0]) {     PROFILE_START(_pt);
fl_widget_owner_basename(a, ab, sizeof(ab)); if (strcmp(wb, ab) == 0) { PROFILE_END_VAR(_pt, "f"); return true; } }
    if (b && b[0]) {     PROFILE_START(_pt);
fl_widget_owner_basename(b, bb, sizeof(bb)); if (strcmp(wb, bb) == 0) { PROFILE_END_VAR(_pt, "f"); return true; } }
    return false;
}

static void fl_widget_destroy_window(void *ud) {
    PROFILE_START(_pt);
    FlWidget *widget = (FlWidget *)ud;
    if (!widget) { PROFILE_END_VAR(_pt, "fl_widget_destroy_window"); return ; }
    widget->hovered = false;
    Platform_HoverMessageRelease(widget->name);
    Platform_HoverWidgetHide(widget->name);
    if (widget->hover_thread) {
        fl_thread_join(widget->hover_thread);
        widget->hover_thread = NULL;
        widget->hover_running = false;
    }
    if (Platform_Widget_IsValid(widget->win)) {
        Platform_Widget_Destroy(widget->win);
    }
    widget->win = NULL;
}
void fl_widget_purge_owner(const char *path_a, const char *path_b, unsigned generation) {
    PROFILE_START(_pt);
    if ((!path_a || !path_a[0]) && (!path_b || !path_b[0])) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_widget_purge_owner"); { PROFILE_END_VAR(_pt, "f"); return ; } }

    for (int i = 0; i < g_fl_widget_count; i++) {
        FlWidget *w = g_fl_widgets[i];
        if (!w) continue;
        if (!fl_widget_owner_matches(w, path_a, path_b)) continue;
        if (w->gen > generation) continue;
        w->dead = true;
        Platform_HoverMessageRelease(w->name);
        Platform_HoverWidgetHide(w->name);
        Platform_RenderThread_RemoveFrameCallback(fl_widget_render_frame, w);
        Platform_RenderThread_Post(fl_widget_destroy_window, w);
        log_info("fl_widget '%s' purged (owner unloaded)", w->name);
    }

    Platform_RenderThread_Sync();

    int n = 0;
    for (int i = 0; i < g_fl_widget_count; i++) {
        FlWidget *w = g_fl_widgets[i];
        if (!w) continue;
        if (fl_widget_owner_matches(w, path_a, path_b) && w->gen <= generation) {
            if (w->owner) free(w->owner);
            free(w);
            continue;
        }
        g_fl_widgets[n++] = w;
    }
    g_fl_widget_count = n;
    if (g_fl_widget_count > 0) fl_widget_relayout();
    PROFILE_END_VAR(_pt, "flapi:fl_widget_purge_owner");
}

bool fl_widget_owner_active(const char *path) {
    PROFILE_START(_pt);
    if (!path || !path[0]) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_widget_owner_active"); { PROFILE_END_VAR(_pt, "f"); return false; } }
    for (int i = 0; i < g_fl_widget_count; i++) {
        FlWidget *w = g_fl_widgets[i];
        if (w && fl_widget_owner_matches(w, path, NULL)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_widget_owner_active"); { PROFILE_END_VAR(_pt, "f"); return true; } }
    }
    PROFILE_END_VAR(_pt, "flapi:fl_widget_owner_active");
    return false;
}

static void fl_widget_attach_window(void *ud) {
    PROFILE_START(_pt);
    FlWidget *widget = (FlWidget *)ud;
    if (!widget || widget->dead) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "flapi:fl_widget_attach_window"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    bool created = false;
    if (!Platform_Widget_IsValid(widget->win)) {
        Platform_SetDrawBgColor(widget->has_bg ? widget->bg_hex : 0);
        widget->win = Platform_Widget_Create(widget->x, widget->y, widget->w, widget->h, widget->fps, widget->no_bg, widget->on_click, widget->name);
        Platform_SetDrawBgColor(0);
        created = (widget->win != NULL);
    } else {
        Platform_Widget_Move(widget->win, widget->x, widget->y);
    }
    if (widget->win) {
        Platform_Widget_SetBgColor(widget->win, widget->has_bg ? widget->bg_hex : 0);
        Platform_Widget_SetNoBg(widget->win, widget->no_bg);
        Platform_Widget_SetFps(widget->win, widget->fps);
        if (created) Platform_Widget_SetFrameCallback(widget->win, fl_widget_render_frame, widget);
    }
    PROFILE_END_VAR(_pt, "flapi:fl_widget_attach_window");
}

static void fl_widget_apply_geometry(FlWidget *widget, int x, int y) {
    widget->x = x;
    widget->y = y;
    if (Platform_RenderThread_IsCurrentThread()) {
    PROFILE_START(_pt);
        fl_widget_attach_window(widget);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
    if (!Platform_RenderThread_EnsureStarted(widget->fps)) {
    PROFILE_START(_pt);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
    if (!Platform_RenderThread_Post(fl_widget_attach_window, widget)) {
    PROFILE_START(_pt);
        { PROFILE_END_VAR(_pt, "f"); return ; }
    }
}

static int fl_widget_sort_key(const FlWidget *widget) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_widget_sort_key"); return widget->order_set ? widget->order : widget->seq; }
}

static void fl_widget_relayout(void) {
    PROFILE_START(_pt);
    int mx = 0, my = 0, mw = 1920, mh = 1080;
    fl_widget_monitor_geometry(&mx, &my, &mw, &mh);

    FlWidget *sorted[FL_WIDGET_MAX];
    int n = 0;
    for (int i = 0; i < g_fl_widget_count; i++) {
        if (g_fl_widgets[i] && !g_fl_widgets[i]->free_pos) sorted[n++] = g_fl_widgets[i];
    }
    for (int i = 1; i < n; i++) {
        FlWidget *key = sorted[i];
        int k = i - 1;
        while (k >= 0 && fl_widget_sort_key(sorted[k]) > fl_widget_sort_key(key)) {
            sorted[k + 1] = sorted[k];
            k--;
        }
        sorted[k + 1] = key;
    }

    for (int s = 0; s < 4; s++) {
        bool from_right = (s == FL_WIDGET_SLOT_TOP_RIGHT || s == FL_WIDGET_SLOT_BOTTOM_RIGHT);
        bool from_top = (s == FL_WIDGET_SLOT_TOP_LEFT || s == FL_WIDGET_SLOT_TOP_RIGHT);
        int cursor = from_right ? mx + mw : mx;
        for (int i = 0; i < n; i++) {
            FlWidget *w = sorted[i];
            if (w->slot != s) continue;
            int g = w->gap_override > 0 ? w->gap_override : WidgetGap;
            int x = from_right ? cursor - w->w : cursor;
            int y = from_top ? my : my + mh - w->h;
            fl_widget_apply_geometry(w, x, y);
            cursor += from_right ? -(w->w + g) : (w->w + g);
        }
    }
    PROFILE_END_VAR(_pt, "flapi:fl_widget_relayout");
}

FlWidget *fl_widget_create(const FlWidgetSpec *spec) {
    PROFILE_START(_pt);
    if (!spec || !spec->name || !spec->name[0]) {
    PROFILE_START(_pt);
        log_warn("fl_widget_create: name is required");
        PROFILE_END_VAR(_pt, "api:fl_widget_create");
        { PROFILE_END_VAR(_pt, "f"); return NULL; }
    }
    FlWidget *widget = fl_widget_find_by_name(spec->name);
    bool fresh = (widget == NULL);
    if (fresh) {
        if (g_fl_widget_count >= FL_WIDGET_MAX) {
    PROFILE_START(_pt);
            log_warn("fl_widget_create: widget table full");
            PROFILE_END_VAR(_pt, "api:fl_widget_create");
            { PROFILE_END_VAR(_pt, "f"); return NULL; }
        }
        widget = (FlWidget *)calloc(1, sizeof(*widget));
        if (!widget) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_create"); { PROFILE_END_VAR(_pt, "f"); return NULL; } }
        snprintf(widget->name, FL_WIDGET_NAME_MAX, "%s", spec->name);
        widget->seq = g_fl_widget_seq++;
        g_fl_widgets[g_fl_widget_count++] = widget;
    }

    {
        char owner_path[512];
        fl_widget_owner_path(__builtin_return_address(0), owner_path, sizeof(owner_path));
        if (owner_path[0]) {
            if (widget->owner) free(widget->owner);
            widget->owner = strdup(owner_path);
        }
    }

    widget->gen = g_fl_widget_gen;
    widget->has_bg = spec->color != 0;
    widget->bg_hex = widget->has_bg ? rgb_to_hex(spec->color) : 0;
    widget->no_bg = spec->no_bg;
    widget->auto_w = spec->w <= 0;
    widget->auto_h = spec->h <= 0;
    widget->pad = spec->pad > 0 ? spec->pad : FL_WIDGET_DEFAULT_PAD;
    if (fresh || !widget->auto_w) widget->w = widget->auto_w ? FL_WIDGET_PROVISIONAL_W : spec->w;
    if (fresh || !widget->auto_h) widget->h = widget->auto_h ? FL_WIDGET_PROVISIONAL_H : spec->h;
    widget->gap_override = spec->gap > 0 ? spec->gap : 0;
    widget->fps = spec->fps ? spec->fps : 1;
    widget->slot = (spec->slot >= FL_WIDGET_SLOT_TOP_LEFT && spec->slot <= FL_WIDGET_SLOT_BOTTOM_RIGHT) ? spec->slot : FL_WIDGET_SLOT_BOTTOM_LEFT;
    widget->order = spec->order;
    widget->order_set = spec->order > 0;
    widget->render = spec->render;
    widget->on_click = spec->on_click;
    widget->on_hover = spec->on_hover;
    widget->free_pos = (spec->slot == FL_WIDGET_FREE);
    widget->frame = 0;
    widget->created_at = fl_start_timer_platform();

    if (widget->free_pos) {
        int x = spec->x;
        int y = spec->y;
        if (x < 0 && y < 0) {
            int mx = 0, my = 0, mw = 1920, mh = 1080;
            fl_widget_monitor_geometry(&mx, &my, &mw, &mh);
            x = mx + (mw - widget->w) / 2;
            y = my + (mh - widget->h) / 2;
        }
        fl_widget_apply_geometry(widget, x, y);
        Platform_Widget_SetFrameCallback(widget->win, fl_widget_render_frame, widget);
    } else {
        fl_widget_relayout();
        Platform_Widget_SetFrameCallback(widget->win, fl_widget_render_frame, widget);
    }

    if (!Platform_Widget_IsValid(widget->win)) {
    }
    PROFILE_END_VAR(_pt, "api:fl_widget_create");
    return widget;
}

bool fl_widget_valid(FlWidget *widget) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_widget_valid"); return widget != NULL; }
}

PlPos fl_get_widget_pos(const char *name) {
    PROFILE_START(_pt);
    PlPos r = {0, 0};
    FlWidget *w = fl_widget_find_by_name(name);
    if (w && w->x < 0x7fffffff) {
        r.x = w->x + w->w / 2;
        r.y = w->y + w->h / 2;
    }
    PROFILE_END_VAR(_pt, "api:fl_get_widget_pos");
    return r;
}

static void fl_widget_apply_show(void *ud) {
    PROFILE_START(_pt);
    FlWidget *widget = (FlWidget *)ud;
    if (widget) Platform_Widget_Show(widget->win);
    PROFILE_END_VAR(_pt, "flapi:fl_widget_apply_show");
}

static void fl_widget_apply_hide(void *ud) {
    PROFILE_START(_pt);
    FlWidget *widget = (FlWidget *)ud;
    if (widget) Platform_Widget_Hide(widget->win);
    PROFILE_END_VAR(_pt, "flapi:fl_widget_apply_hide");
}

void fl_widget_show(FlWidget *widget) {
    PROFILE_START(_pt);
    if (!fl_widget_valid(widget)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_show"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    if (widget->mapped) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_show"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    widget->mapped = true;
    Platform_RenderThread_Post(fl_widget_apply_show, widget);
    PROFILE_END_VAR(_pt, "api:fl_widget_show");
}

void fl_widget_hide(FlWidget *widget) {
    PROFILE_START(_pt);
    if (!fl_widget_valid(widget)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_hide"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    if (!widget->mapped) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_hide"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    widget->mapped = false;
    Platform_RenderThread_Post(fl_widget_apply_hide, widget);
    PROFILE_END_VAR(_pt, "api:fl_widget_hide");
}

void fl_widget_set_render(FlWidget *widget, FlWidgetRenderFn render) {
    PROFILE_START(_pt);
    if (!fl_widget_valid(widget)) {     PROFILE_START(_pt);
PROFILE_END_VAR(_pt, "api:fl_widget_set_render"); { PROFILE_END_VAR(_pt, "f"); return ; } }
    widget->render = render;
    if (Platform_Widget_IsValid(widget->win)) {
        Platform_Widget_Invalidate(widget->win);
    }
    PROFILE_END_VAR(_pt, "api:fl_widget_set_render");
}

void fl_format_bytes(unsigned long long bytes, char* buf, size_t buflen) {
        PROFILE_START(_pt);
const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
    const int   count   = sizeof(units) / sizeof(units[0]);

    double value = (double)bytes;
    int    i     = 0;

    while (value >= 1024.0 && i < count - 1) {
        value /= 1024.0;
        i++;
    }

    if (i == 0) {
        snprintf(buf, buflen, "%llu %s", bytes, units[i]);
    } else {
        snprintf(buf, buflen, "%.1f %s", value, units[i]);
    }
}
FlNetStatus fl_net_status(void) {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_net_status"); return fl_net_status_platform(); }
}

unsigned long long fl_net_rx_bytes() {
    PROFILE_START(_pt);
    { PROFILE_END_VAR(_pt, "fl_net_rx_bytes"); return fl_net_rx_bytes_platform(); }
}


unsigned long long fl_net_rx_bytes_over(double seconds) {
    PROFILE_START(_pt);
    if (seconds <= 0) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_over"); return 0; }

    unsigned long long before = fl_net_rx_bytes_platform();
    if (before == 0) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_over"); return 0; }

    fl_sleep(seconds);

    unsigned long long after = fl_net_rx_bytes_platform();
    if (after < before) { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_over"); return 0; }
    { PROFILE_END_VAR(_pt, "fl_net_rx_bytes_over"); return after - before; }
}
FlRect fl_get_monitor_geometry() {
    PROFILE_START(_pt);
    int x, y, w, h;
    WindowManager_GetMainMonitorGeometry(&x,&y,&w,&h);
    { PROFILE_END_VAR(_pt, "fl_get_monitor_geometry"); return (PlRect){x,y,w,h}; }
}

int fl_get_screen_width() {
    PROFILE_START(_pt);
    int x, y, w, h;
    WindowManager_GetMainMonitorGeometry(&x,&y,&w,&h);
    { PROFILE_END_VAR(_pt, "fl_get_screen_width"); return w; }
}

int fl_get_screen_height() {
    PROFILE_START(_pt);
    int x, y, w, h;
    WindowManager_GetMainMonitorGeometry(&x,&y,&w,&h);
    { PROFILE_END_VAR(_pt, "fl_get_screen_height"); return h; }
}

#define FL_CC_MAX 128
#define FL_CC_STR_MAX 512
#define FL_CC_DEP_FEATURE "control_center.c"

typedef struct {
    FlCcEntrySpec pub;
    char display_name[FL_CC_STR_MAX];
    char internal_name[FL_CC_STR_MAX];
    char icon[FL_CC_STR_MAX];
    char type[128];
    char owner[OWNER_PATH_MAX];
} FlCcEntry;

static int g_cc_entry_count = 0;
static FlCcEntry g_cc_entries[FL_CC_MAX];

#define FL_CC_DEP_OWNERS 64
static char g_cc_dep_owners[FL_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int g_cc_dep_owner_count = 0;

static bool fl_cc_dep_seen(const char *owner) {
    for (int i = 0; i < g_cc_dep_owner_count; i++)
        if (strcmp(g_cc_dep_owners[i], owner) == 0) return true;
    return false;
}

static void fl_cc_dep_mark(const char *owner) {
    if (g_cc_dep_owner_count >= FL_CC_DEP_OWNERS) return;
    snprintf(g_cc_dep_owners[g_cc_dep_owner_count++], OWNER_PATH_MAX, "%s", owner);
}

static const char *fl_cc_dup(char *dst, size_t cap, const char *src) {
    snprintf(dst, cap, "%s", src ? src : "");
    return dst;
}

bool fl_add_to_cc_impl(const FlCcEntrySpec *spec) {
    if (!spec || g_cc_entry_count >= FL_CC_MAX) return false;
    FlCcEntry *e = &g_cc_entries[g_cc_entry_count];
    memset(e, 0, sizeof(*e));
    e->pub.on_enter = spec->on_enter;
    e->pub.on_ctrl_enter = spec->on_ctrl_enter;
    e->pub.on_tab = spec->on_tab;
    e->pub.on_action = spec->on_action;
    e->pub.display_name = fl_cc_dup(e->display_name, sizeof(e->display_name), spec->display_name);
    e->pub.icon = fl_cc_dup(e->icon, sizeof(e->icon), spec->icon);
    e->pub.type = fl_cc_dup(e->type, sizeof(e->type), spec->type);
    if (spec->internal_name) {
        e->pub.internal_name = fl_cc_dup(e->internal_name, sizeof(e->internal_name), spec->internal_name);
    } else {
        e->pub.internal_name = fl_cc_dup(e->internal_name, sizeof(e->internal_name), spec->display_name);
    }
    void *anchor = (void*)(uintptr_t)spec->on_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_ctrl_enter;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_tab;
    if (!anchor) anchor = (void*)(uintptr_t)spec->on_action;
    Owner_ResolveForFunc(anchor, e->owner, sizeof(e->owner));
    if (e->owner[0] && !fl_cc_dep_seen(e->owner)) {
        fl_cc_dep_mark(e->owner);
        fl_add_dep_impl(FL_CC_DEP_FEATURE, e->owner);
    }
    g_cc_entry_count++;
    return true;
}

int fl_cc_entry_count(void) {
    return g_cc_entry_count;
}

const FlCcEntrySpec *fl_cc_entry_at(int index) {
    if (index < 0 || index >= g_cc_entry_count) return NULL;
    return &g_cc_entries[index].pub;
}

int fl_cc_registry_count(void) {
    return g_cc_entry_count;
}

const FlCcEntrySpec *fl_cc_registry_at(int index) {
    return fl_cc_entry_at(index);
}

void fl_cc_registry_purge_owner(const char *path_a, const char *path_b) {
    if (!path_a && !path_b) return;
    int out = 0;
    for (int i = 0; i < g_cc_entry_count; i++) {
        if (Owner_ModuleMatches(g_cc_entries[i].owner, path_a, path_b)) continue;
        if (out != i) g_cc_entries[out] = g_cc_entries[i];
        out++;
    }
    g_cc_entry_count = out;
}

void fl_cc_registry_reset(void) {
    g_cc_entry_count = 0;
}
