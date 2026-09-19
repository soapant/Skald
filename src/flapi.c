#include <stdint.h>
#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/input_state.h"
#include "config.h"
#include "core/platform.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/flapi.h"
#include "core/flapi_platform.h"
#include "platform/wm_platform.h"
#include "platform/draw.h"
#include "core/hotkey_manager.h"
#include "core/remap_manager.h"

bool fl_copy_to_clipboard(const char* text) {
    if (text == NULL) return 0;
    return Platform_CopyToClipboard(text);
}

static const unsigned char FL_PNG_MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

bool fl_copy_image_to_clipboard(const unsigned char* png, size_t len) {
    if (png == NULL || len < 8) return 0;
    if (memcmp(png, FL_PNG_MAGIC, 8) != 0) return 0;
    return Platform_CopyImageToClipboard(png, len);
}

static bool fl_clipboard_set_png_path(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (32 * 1024 * 1024)) {
        fclose(f);
        return 0;
    }
    unsigned char* buf = malloc((size_t)n);
    if (!buf) {
        fclose(f);
        return 0;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    bool ok = 0;
    if (got == (size_t)n) ok = fl_copy_image_to_clipboard(buf, (size_t)n);
    free(buf);
    remove(path);
    return ok;
}

bool fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {
    if (!dir_path || !list_of_files) return false;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) return false;
    dir_path = expanded_dir;

    return fl_get_all_files_from_dir_platform(dir_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
}

char* fl_search_file_in_dir(const char* directory, const char* search_string,char* output_path){
    if (!directory) return NULL;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(directory, expanded_dir, sizeof(expanded_dir))) return NULL;
    directory = expanded_dir;

    return fl_search_file_in_dir_platform(directory,search_string,output_path);
}

bool fl_launch_with_args(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return fl_launch_with_args_platform(expanded, args);
}

bool fl_launch_with_args_as_admin(const char* path, const char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return fl_launch_with_args_as_admin_platform(expanded, args);
}

int fl_load_file_list(const char* filepath, const char* out[], int max) {
    if (!filepath || !out || max <= 0) return 0;
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(filepath, full, sizeof(full))) return 0;
    filepath = full;
    FILE* f = fopen(filepath, "r");
    if (!f) return 0;
    int count = 0;
    char buffer[MAX_FILE_LINE_LENGTH];
    while (count < max && fgets(buffer, sizeof(buffer), f)) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') continue;
        char* dup = strdup(buffer);
        if (!dup) {
            for (int j = 0; j < count; j++) free((void*)out[j]);
            fclose(f);
            return 0;
        }
        out[count++] = dup;
    }
    fclose(f);
    return count;
}
bool fl_free_list(const char* list[], int count) {
    if (!list || count <= 0) return false;
    for (int i = 0; i < count; i++) free((void*)list[i]);
    return true;
}
bool fl_free_file_list(const char* list[], int count) {
    return fl_free_list(list, count);
}
bool fl_free_window_list(Fl_WindowList *list) {
    if (!list) return false;
    if (list->windows) {
        for (int i = 0; i < list->count; i++) free(list->windows[i]);
        free(list->windows);
        list->windows = NULL;
        list->count = 0;
    }
    return true;
}

bool fl_append_to_file(const char* filepath, const char* line) {
    if (!filepath || !line) return false;
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(filepath, full, sizeof(full))) return false;
    filepath = full;
    FILE* f = fopen(filepath, "a");
    if (!f) {
        f = fopen(filepath, "w");
        if (!f) return false;
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
    if (!filepath) return false;
    if (count < 0) return false;
    if (count > 0 && !lines) return false;
    char full[MAX_PATH];
    if (!Platform_ResolveDataPath(filepath, full, sizeof(full))) return false;
    filepath = full;
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    for (int i = 0; i < count; i++) {
        if (lines[i]) fprintf(f, "%s\n", lines[i]);
        else fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

char* fl_read_file_to_string(const char* path, size_t* out_len) {
    if (!path) return NULL;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END)!=0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) sz = 0;
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)sz, f);
    buf[read] = '\0';
    fclose(f);
    if (out_len) *out_len = read;
    return buf;
}
bool fl_write_string_to_file(const char* path, const char* content) {
    if (!path || !content) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}
bool fl_file_exists(const char* path) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "rb");
    if (f) { fclose(f); return true; }
    return false;
}
bool fl_append_unique_line(const char* path, const char* line) {
    if (!path || !line) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    bool ok = fl_append_to_file(path, line);
    fl_free_file_list(items, n);
    return ok;
}
bool fl_remove_line(const char* path, const char* line) {
    if (!path || !line) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    const char* kept[1024]; int k=0;
    for (int i=0;i<n;i++) if (strcmp(items[i], line)!=0) kept[k++]=items[i]; else { free((void*)items[i]); items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    for (int i=0;i<n;i++) if (items[i]) free((void*)items[i]);
    return ok;
}
bool fl_remove_line_contains(const char* path, const char* substr) {
    if (!path || !substr) return false;
    const char* items[1024]; int n = fl_load_file_list(path, items, 1024);
    const char* kept[1024]; int k=0;
    for (int i=0;i<n;i++) if (!strstr(items[i], substr)) kept[k++]=items[i]; else { free((void*)items[i]); items[i]=NULL; }
    bool ok = fl_write_to_file(path, kept, k);
    for (int i=0;i<n;i++) if (items[i]) free((void*)items[i]);
    return ok;
}
bool fl_parse_desktop_file(const char* path, char* out_name, size_t n, bool* out_nodisplay) {
    if (!path || !out_name) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) strncpy(expanded, path, sizeof(expanded)-1);
    FILE* f = fopen(expanded, "r");
    if (!f) return false;
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
    return found;
}


bool fl_register_hotkey_impl(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),bool whitelist,const char** optional_blacklistedwindows) {
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows,
                                                        whitelist);
    return hotkey_register_internal(binding);
}

bool fl_register_remapping(const char* src, const char* dst) {
    if (!src || !dst) return false;
    return remap_register_internal(src, dst);
}

typedef struct { char *key; void *value; } FlintMapEnt;
typedef struct { char name[128]; void (*cb)(void); char icon[512]; } FeatureReg;
static FlintMap* g_feature_map = NULL;
static FeatureReg** g_feature_order = NULL;
static int g_feature_order_count = 0;
static int g_feature_order_cap = 0;

static FlintMap* ensure_feature_map(void) {
    if (!g_feature_map) { g_feature_map = NULL; sh_new_strdup(g_feature_map); }
    return g_feature_map;
}

void fl_register_feature(const char* name, void (*cb)(void)) {
    fl_register_feature_with_icon(name, cb, NULL);
}
void fl_register_feature_with_icon(const char* name, void (*cb)(void), const char* icon_path) {
    if (!name || !cb) return;
    ensure_feature_map();
    ptrdiff_t idx = shgeti(g_feature_map, (char*)name);
    FeatureReg* existing = idx >= 0 ? (FeatureReg*)g_feature_map[idx].value : NULL;
    if (existing) {
        existing->cb = cb;
        if (icon_path) {
            strncpy(existing->icon, icon_path, sizeof(existing->icon)-1);
            existing->icon[sizeof(existing->icon)-1] = '\0';
        } else {
            existing->icon[0] = '\0';
        }
        return;
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
        if (!new_order) { shdel(g_feature_map, reg->name); free(reg); return; }
        g_feature_order = new_order;
        g_feature_order_cap = new_cap;
    }
    g_feature_order[g_feature_order_count++] = reg;
}

int fl_get_feature_list(const char** out_names, void (**out_cbs)(void), int max) {
    int n = g_feature_order_count < max ? g_feature_order_count : max;
    for (int i=0;i<n;i++) {
        if (out_names) out_names[i] = g_feature_order[i]->name;
        if (out_cbs) out_cbs[i] = g_feature_order[i]->cb;
    }
    return n;
}

int fl_get_feature_list_with_icons(const char** out_names, void (**out_cbs)(void), const char*** out_icons, int max) {
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
    return n;
}
char* fl_get_icon_path(const char* app_path_path) {
    if (!app_path_path) return NULL;
    return Platform_GetIconPath((char*)app_path_path);
}
int fl_get_icon_size(const char* icon_path) {
    if (!icon_path) return 20;
    return Platform_GetIconSize((char*)icon_path);
}

char* fl_run_terminal_command(char* command) {
  if (!command) return NULL;
  return Platform_RunTerminalCommand(command,false);
}

char* fl_run_terminal_command_and_open(char* command) {
  if (!command) return NULL;
  return Platform_RunTerminalCommand(command,true);
}

bool fl_launch_terminal_at_path(char* path,char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))return false;
    return Platform_LaunchTerminalAtPath(expanded,args);
}

bool fl_launch_file_manager_at_path(char* path,char* args) {
    if (!path) return false;
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded)))return false;
    return Platform_LaunchFileManagerAtPath(expanded,args);
}

char* fl_get_zoxide_match(char* query) {
  if (!query) return NULL;
  return Platform_QueryZoxidePath(query);
}

int fl_collect_running_processes(char process_list[][128], int max_count) {
    return Platform_CollectRunningProcesses(process_list, max_count);
}

bool fl_terminate_process_by_name(const char* exe_name) {
    if (!exe_name) return false;
    return Platform_TerminateProcessByName(exe_name);
}
bool fl_string_match_insensitive(char* needle,char* haystack) {
    if (!needle || !haystack) return false;
    return Platform_StringContainsCaseInsensitive(needle,haystack);
}

static char fl__matching_close(char open) {
    switch (open) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '<': return '>';
        case '"': return '"';
        case '\'': return '\'';
        case '`': return '`';
        default: return open;
    }
}

char* fl_string_cut_at(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return NULL;
    return strdup(p);
}

char* fl_string_cut_before(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return strdup(str);
    size_t len = (size_t)(p - str);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, str, len);
    out[len] = '\0';
    return out;
}

char* fl_string_cut_after(const char* str, char delim) {
    if (!str) return NULL;
    const char* p = strchr(str, delim);
    if (!p) return NULL;
    return strdup(p + 1);
}

char* fl_string_between(const char* str, char open, char close) {
    if (!str) return NULL;
    const char* start = strchr(str, open);
    if (!start) return NULL;
    start++;
    if (open == close) {
        const char* end = strchr(start, close);
        if (!end) return NULL;
        size_t len = (size_t)(end - start);
        char* out = (char*)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, start, len);
        out[len] = '\0';
        return out;
    }
    int depth = 1;
    const char* cur = start;
    while (*cur) {
        if (*cur == open) depth++;
        else if (*cur == close) {
            depth--;
            if (depth == 0) {
                size_t len = (size_t)(cur - start);
                char* out = (char*)malloc(len + 1);
                if (!out) return NULL;
                memcpy(out, start, len);
                out[len] = '\0';
                return out;
            }
        }
        cur++;
    }
    return NULL;
}

char* fl_string_inside(const char* str, char delim) {
    if (!str) return NULL;
    char close = fl__matching_close(delim);
    if (delim == close) {
        const char* p = str;
        char* last = NULL;
        while ((p = strchr(p, delim)) != NULL) {
            const char* start = p + 1;
            const char* end = strchr(start, close);
            if (!end) break;
            size_t len = (size_t)(end - start);
            char* cur = (char*)malloc(len + 1);
            if (!cur) { free(last); return NULL; }
            memcpy(cur, start, len);
            cur[len] = '\0';
            free(last);
            last = cur;
            p = end + 1;
        }
        return last;
    } else {
        const char* p = str;
        char* last = NULL;
        while ((p = strchr(p, delim)) != NULL) {
            const char* start = p + 1;
            int depth = 1;
            const char* cur = start;
            while (*cur) {
                if (*cur == delim) depth++;
                else if (*cur == close) {
                    depth--;
                    if (depth == 0) break;
                }
                cur++;
            }
            if (*cur != close) break;
            size_t len = (size_t)(cur - start);
            char* cur_out = (char*)malloc(len + 1);
            if (!cur_out) { free(last); return NULL; }
            memcpy(cur_out, start, len);
            cur_out[len] = '\0';
            free(last);
            last = cur_out;
            p = cur + 1;
        }
        return last;
    }
}

char* fl_string_trim(const char* str) {
    if (!str) return NULL;
    const char* s = str;
    while (*s && isspace((unsigned char)*s)) s++;
    const char* e = str + strlen(str);
    while (e > s && isspace((unsigned char)*(e - 1))) e--;
    size_t len = (size_t)(e - s);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

char* fl_string_to_lower(const char* str) {
    if (!str) return NULL;
    char* out = strdup(str);
    if (!out) return NULL;
    for (char* p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

char* fl_string_to_upper(const char* str) {
    if (!str) return NULL;
    char* out = strdup(str);
    if (!out) return NULL;
    for (char* p = out; *p; p++) *p = (char)toupper((unsigned char)*p);
    return out;
}

bool fl_string_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t n = strlen(prefix);
    return strncmp(str, prefix, n) == 0;
}

bool fl_string_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t ls = strlen(str);
    size_t lx = strlen(suffix);
    if (lx > ls) return false;
    return strcmp(str + ls - lx, suffix) == 0;
}

char* fl_string_replace(const char* str, const char* from, const char* to) {
    if (!str || !from || !to) return NULL;
    size_t lf = strlen(from);
    if (lf == 0) return strdup(str);
    size_t lt = strlen(to);
    size_t count = 0;
    const char* p = str;
    while ((p = strstr(p, from))) { count++; p += lf; }
    size_t ls = strlen(str);
    size_t lo = ls + count * (lt - lf);
    char* out = (char*)malloc(lo + 1);
    if (!out) return NULL;
    char* dst = out;
    const char* src = str;
    while ((p = strstr(src, from))) {
        size_t n = (size_t)(p - src);
        memcpy(dst, src, n); dst += n;
        memcpy(dst, to, lt); dst += lt;
        src = p + lf;
    }
    strcpy(dst, src);
    return out;
}

char* fl_string_slice(const char* str, int start, int end) {
    if (!str) return NULL;
    int len = (int)strlen(str);
    if (start < 0) start = len + start;
    if (end < 0) end = len + end;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (end < start) end = start;
    size_t n = (size_t)(end - start);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, str + start, n);
    out[n] = '\0';
    return out;
}
bool fl_launch_browser(char* url) {
    if (!url) return false;
    return Platform_LaunchBrowserAtUrl(url,false);
}
bool fl_query_browser(const char* query) {
    if (!query) return false;
    return Platform_QueryBrowser(query);
}

char* fl_on_dir_changed(char* dir_path, void (*callback)(void)) {
    if (!dir_path) return NULL;
    return Platform_RegisterDirWatcher(dir_path, callback);
}

char* fl_adb_tap(int x,int y) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell input tap %i %i",x,y);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_type_text(char* text) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell input text %s",text);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_get_devices() {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb devices");
  return Platform_RunTerminalCommand(buffer,false);
}

char* fl_adb_install(char* app_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb install %s",app_path);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_uninstall(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb uninstall %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_force_close(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell am force-stop %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_clear_app_data(char* app_name) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb shell pm clear %s",app_name);
  return Platform_RunTerminalCommand(buffer,false);
}
char* fl_adb_get_file(char* filepath,char* output_file_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb pull %s %s",filepath,output_file_path);
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_adb_put_file(char* filepath,char* output_file_path) {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb push %s %s",filepath,output_file_path);
  return Platform_RunTerminalCommand(buffer,false);

}

char* fl_adb_reboot() {
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"adb reboot");
  return Platform_RunTerminalCommand(buffer,false);

}
char* fl_get_last_clipboard_entry() {
   return Platform_GetLastClipboardEntry();
}
bool fl_paste_from_clipboard() {
   char* content = Platform_GetLastClipboardEntry();
   return Platform_PasteContent(content);
}

bool fl_paste(char* content) {
    if (!content) return false;
   return Platform_PasteContent(content);
}

static void fl_edit_blocking_confirm(const char* text, void* user_data) {
    char** out = (char**)user_data;
    if (out && text) *out = strdup(text);
}

char* fl_edit_text_popup(const char* initial_text, const char* title, int x, int y, ListBoxInfo* lb, int* out_index) {
    if (out_index) *out_index = -1;
    if (!lb) return NULL;
    if (!title) title = "Edit";
    if (!initial_text) initial_text = "";
    int idx = lb->selected_index;
    int true_idx = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    WindowManager* wm = WindowManager_Get();
    if (!wm) return NULL;
    char* result = NULL;
    Popup* popup = EditPopup_Create(wm, initial_text, title, x, y, fl_edit_blocking_confirm, &result);
    if (!popup) return NULL;
    WindowManager_PushPopup(wm, popup);
    while (wm->running && popup->manager) {
        Platform_WM_ProcessEvents(wm);
    }
    if (popup->manager == NULL) {
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    } else {
        WindowManager_PopPopup(wm);
        if (popup->vtable && popup->vtable->destroy) popup->vtable->destroy(popup);
    }
    if (!result) return NULL;
    if (out_index) *out_index = true_idx;
    return result;
}

bool fl_replace_line_in_file(const char* filepath, int line_index, const char* new_text) {
    if (!filepath || !new_text || line_index < 0) return false;
    const char* items[MAX_FILE_LINES];
    int n = fl_load_file_list(filepath, items, MAX_FILE_LINES);
    if (line_index >= n) {
        fl_free_file_list(items, n);
        return false;
    }
    free((void*)items[line_index]);
    items[line_index] = NULL;
    items[line_index] = strdup(new_text);
    if (!items[line_index]) {
        fl_free_file_list(items, n);
        return false;
    }
    bool ok = fl_write_to_file(filepath, items, n);
    fl_free_file_list(items, n);
    return ok;
}

bool fl_focus_window_by_title(char* title) {
  if (!title) return false;
  return Platform_FocusWindow(title);
}

char* fl_popup_impl(const FlPopupSpec *spec) {
    const char *initial = spec && spec->initial_text ? spec->initial_text : "";
    int x = spec->x ? spec->x : 0;
    int y = spec->y ? spec->y : 0;
    int w = spec->w ? spec->w : 500;
    int h = spec->h ? spec->h : 70;
    SimplePromptSpec s = {0};
    s.prompt = NULL;
    s.initial_text = initial;
    s.x = x; s.y = y; s.w = w; s.h = h;
    WindowManager *wm = WindowManager_Get();
    if (!wm) return NULL;
    char *res = WindowManager_ShowSimplePrompt(wm, &s);
    if (!res) return NULL;
    return strdup(res);
}

unsigned long fl_listbox_get_current_standard_color(void);

bool fl_place_text_impl(const FlTextSpec *spec) {
    if (!spec) return false;
    const char *real_text = spec->text;
    char tmp[2] = {0};
    if ((uintptr_t)real_text < 256) {
        if (!real_text) return false;
        tmp[0] = (char)(uintptr_t)real_text;
        tmp[1] = '\0';
        real_text = tmp;
    }
    if (!real_text) return false;
    unsigned long col = spec->color;
    if (col == 0) {
        unsigned long cur = fl_listbox_get_current_standard_color();
        col = cur ? cur : rgb_to_hex(COLOR_TEXT);
    }else {
        col = rgb_to_hex(col);
    }
    const char *align = spec->align ? spec->align : "CENTER";
    FontHandle fh = spec->font;
    return Platform_DrawTextRaw(spec->x, spec->y, real_text, col, fh, align, spec->pad,spec->fontsize);
}

bool fl_place_editbox_impl(const FlEditSpec *spec) {
  if (!spec) return false;
  return fl_place_editbox_platform(spec);
}
bool fl_place_image_impl(const FlImageSpec *spec) {
  (void)spec; return false; 
}
bool fl_draw_rect_impl(const FlRectSpec *spec) {
  if (!spec) return false;
  unsigned long col = spec->color;
  if (col == 0) col = COLOR_TEXT;
  return Platform_DrawRectRaw(spec->x, spec->y, spec->w, spec->h, col);
}

bool fl_draw_line(int x1, int y1, int x2, int y2, unsigned long color) {
  (void)x1; (void)y1; (void)x2; (void)y2; (void)color; return false; 
}
bool fl_draw_circle(int x, int y, int radius, unsigned long color) {
  (void)x; (void)y; (void)radius; (void)color; return false; 
}
bool fl_draw_rounded_rect(int x, int y, int w, int h, int radius, unsigned long color) {
  (void)x; (void)y; (void)w; (void)h; (void)radius; (void)color; return false; 
}

bool fl_listbox_begin(const FlListSpec *spec) {
    if (!spec) return false;
    return fl_listbox_begin_platform(spec);
}
bool fl_listbox_next(FlListItemCtx *out) {
    if (!out) return false;
    return fl_listbox_next_platform(out);
}
void fl_listbox_end(void) {
    fl_listbox_end_platform();
}
bool fl_place_listbox_impl(const FlListSpec *spec) {
    if (!spec) return false;
    if (!fl_listbox_begin(spec)) return false;
    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        fl_place_text(.x=ctx.x + 10, .y=ctx.y + 4, .text=ctx.text, .align="LEFT", .pad=ctx.x + 10);
    }
    fl_listbox_end();
    return true;
}

Fl_WindowList fl_get_all_window_titles() {
  Fl_WindowList l = Platform_GetWindowTitles();
  return l;
}
Fl_WindowList fl_get_all_window_names() {
  Fl_WindowList l = Platform_GetWindowNames();
  return l;
}

void fl_toggle_play_pause(void) {
  fl_toggle_play_pause_platform();
  return;
}

Fl_Pos fl_get_cursor_pos(void) {
  return Platform_GetCursorPos();
}
int fl_get_current_volume(void) {
  return fl_get_current_volume_platform();
}
bool fl_set_current_volume(int volume) {
  return fl_set_volume_platform(volume);
}
bool fl_increment_volume_by(int increment) {
  return fl_increment_volume_by_platform(increment);
}

bool fl_decrement_volume_by(int decrement) {
  return fl_decrement_volume_by_platform(decrement);
}
bool fl_increment_volume_by_percent(int increment_percent) {
  return fl_increment_volume_by_percent_platform(increment_percent);
}

bool fl_decrement_volume_by_percent(int decrement_percent) {
  return fl_decrement_volume_by_percent_platform(decrement_percent);
}

bool fl_type_text(const char *s) {
  return fl_type_text_platform(s);
}

bool fl_is_key_down(int vk){
  return Platform_IsKeyDown(vk); 
}
bool fl_is_key_pressed(int vk){
  return Platform_WasKeyPressed(vk); 
}
bool fl_is_key_released(int vk){
  return Platform_WasKeyReleased(vk); 
}
bool fl_is_key_held(int vk){
  return Platform_IsKeyDown(vk); 
}
void fl_on_key(void (*cb)(int vk, bool down, int mods, void *ud), void *ud){
  Platform_SetKeyCallback(cb,ud);
}
int fl_get_key_pressed(void){
  return Platform_GetPressedKey();
}
char fl_get_char_pressed(void){
  return Platform_GetPressedChar();
}

bool fl_is_flag_prefix(char c) { return c == '-' || c == '/'; }

size_t fl_extract_flag_name(char *p, char *out, size_t cap) {
    size_t n = 0;
    while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '=') n++;
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n); out[n] = '\0';
    return n;
}

bool fl_extract_quoted(char **pp, char *out, size_t cap) {
    char *p = *pp;
    char q = *p++;
    char *s = p;
    while (*p && *p != q) p++;
    size_t len = p - s;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len); out[len] = '\0';
    if (*p == q) p++;
    *pp = p;
    return true;
}
bool fl_extract_value(char **pp, char *out, size_t cap) {
    char *p = *pp;
    if (*p == '"' || *p == '\'') return fl_extract_quoted(pp, out, cap);
    char *s = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    size_t len = p - s;
    if (len >= cap) len = cap - 1;
    memcpy(out, s, len); out[len] = '\0';
    *pp = p;
    return len > 0;
}
bool fl_next_is_flag(char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return *p == '-' || *p == '/';
}
Fl_FlagList fl_parse_flags(const char* input) {
    Fl_FlagList out = {0};
    if (!input || !*input) return out;
    char *copy = strdup(input);
    if (!copy) return out;
    size_t cap = 8;
    Fl_Flag *arr = (Fl_Flag*)calloc(cap, sizeof(Fl_Flag));
    if (!arr) { free(copy); return out; }
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
                if (has_val && fl_next_is_flag(p)) { has_val = false; p = save; while (*p && isspace((unsigned char)*p)) p++; }
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
    if (!list) return;
    free(list->flags);
    list->flags = NULL;
    list->count = 0;
}

int fl_time_unit_mult(const char *unit) {
    if (!unit || !*unit) return 1;
    if (strcmp(unit,"ms")==0 || strcmp(unit,"millis")==0) return 0;
    if (strcmp(unit,"s")==0 || strcmp(unit,"sec")==0 || strcmp(unit,"secs")==0 || strcmp(unit,"second")==0) return 1;
    if (strcmp(unit,"m")==0 || strcmp(unit,"min")==0 || strcmp(unit,"mins")==0 || strcmp(unit,"minute")==0) return 60;
    if (strcmp(unit,"h")==0 || strcmp(unit,"hr")==0 || strcmp(unit,"hrs")==0 || strcmp(unit,"hour")==0) return 3600;
    if (strcmp(unit,"d")==0 || strcmp(unit,"day")==0 || strcmp(unit,"days")==0) return 86400;
    if (strcmp(unit,"w")==0 || strcmp(unit,"week")==0 || strcmp(unit,"weeks")==0) return 604800;
    if (strcmp(unit,"mo")==0 || strcmp(unit,"month")==0 || strcmp(unit,"months")==0) return 2592000;
    if (strcmp(unit,"y")==0 || strcmp(unit,"yr")==0 || strcmp(unit,"year")==0) return 31536000;
    return 1;
}

int fl_parse_time(const char* time_str) {
    if (!time_str || !*time_str) return 0;
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
    return total;
}

void fl_run_after_time(double seconds, void (*callback)(void)) {
    return fl_run_after_time_platform(seconds,callback);
}

bool fl_is_window_active(char* title) {
  Fl_WindowList l = Platform_GetWindowTitles();
  for (int i = 0; i < l.count;i++) {
      if (Platform_StringContainsCaseInsensitive(l.windows[i],title)) {
        return true;
      }
  }
  return false;
}
char* fl_get_working_dir() {
    return Platform_GetWorkingDir();
}

void fl_screenshot_copy_area(void) {
    char cmd[512];
    char* wd = fl_get_working_dir();
    char path[MAX_PATH];

    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");
    snprintf(cmd, sizeof(cmd),
             "%s/external/grim -g \"$(%s/external/slurp)\" -t png %s",
             wd, wd, path);

    fl_run_terminal_command(cmd);
    fl_clipboard_set_png_path(path);
}

void fl_screenshot_copy(void) {
    char cmd[512];
    char* wd = fl_get_working_dir();
    char path[MAX_PATH];

    snprintf(path, sizeof(path), "%s/flint_clip.png", wd ? wd : ".");
    snprintf(cmd, sizeof(cmd),
             "%s/external/grim -t png %s",
             wd, path);

    fl_run_terminal_command(cmd);
    fl_clipboard_set_png_path(path);
}

void fl_screenshot_save_area(char* save_path) {
    char cmd[512];
    char* wd = fl_get_working_dir();
    
    snprintf(cmd, sizeof(cmd),
             "%s/external/grim -g \"$(%s/external/slurp)\" %s.png",
             wd, wd,save_path);
    
    fl_run_terminal_command(cmd);
}

void fl_screenshot_save(char* save_path) {
    char cmd[512];
    char* wd = fl_get_working_dir();
    
    snprintf(cmd, sizeof(cmd),
             "%s/external/grim %s.png",
             wd, wd,save_path);
    
    fl_run_terminal_command(cmd);
}

void fl_ocr_area_to_clipboard(void) {
    char cmd[512];
    char* wd = fl_get_working_dir();

    snprintf(cmd, sizeof(cmd),
             "%s/external/grim -g \"$(%s/external/slurp)\" -t png - | tesseract stdin stdout -l eng",
             wd, wd);

    char* out = Platform_RunTerminalCommand(cmd, false);
    if (!out || !out[0]) {
        free(out);
        return;
    }
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    if (out[0]) Platform_CopyToClipboard(out);
    free(out);
}
