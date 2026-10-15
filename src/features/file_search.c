#include "core/flapi.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

const char* ListOfFilePaths[] = {
    #if defined(_WIN32)
      "C:\\Projects",
    #else 
      "~/Programming/Projects",
      "~/miscellaneous",
    #endif
};

#define APPCACHE_FILE "file_search_cache.txt"

ListOfFiles s_files = {0};

static bool file_cache_grow(ListOfFiles* l) {
    size_t new_cap = l->capacity ? l->capacity * 2 : 64;
    FileItem* grown = (FileItem*)realloc(l->files, new_cap * sizeof(FileItem));
    if (!grown) return false;
    l->files = grown;
    l->capacity = new_cap;
    return true;
}

static void save_file_cache(void) {
    size_t n = s_files.count;
    if (n == 0) return;
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += strlen(s_files.files[i].name) + 1;
        total += strlen(s_files.files[i].path) + 2;
    }
    const char** lines = (const char**)malloc(n * sizeof(char*));
    char* store = (char*)malloc(total);
    if (!lines || !store) { free(lines); free(store); return; }
    size_t off = 0;
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        const char* nm = s_files.files[i].name;
        const char* pth = s_files.files[i].path;
        if (!nm[0] || !pth[0]) continue;
        int k = snprintf(store + off, total - off, "%s;%s", nm, pth);
        if (k <= 0 || (size_t)k >= total - off) continue;
        lines[written++] = store + off;
        off += (size_t)k + 1;
    }
    if (written > 0) fl_write_to_file(APPCACHE_FILE, lines, (int)written);
    free(lines);
    free(store);
}

static bool load_file_cache(void) {
    FlStringList lines = fl_load_file_list(APPCACHE_FILE);
    if (!lines.items || lines.count <= 0) {
        if (lines.items) fl_free_file_list(&lines);
        return false;
    }
    ListOfFiles tmp = {0};
    for (int i = 0; i < lines.count; i++) {
        char* entry = lines.items[i];
        if (!entry) continue;
        char* colon = strrchr(entry, ';');
        if (!colon || colon == entry || !colon[1]) continue;
        *colon = '\0';
        if (tmp.count >= tmp.capacity && !file_cache_grow(&tmp)) break;
        strncpy(tmp.files[tmp.count].name, entry, sizeof(tmp.files[tmp.count].name) - 1);
        tmp.files[tmp.count].name[sizeof(tmp.files[tmp.count].name) - 1] = '\0';
        strncpy(tmp.files[tmp.count].path, colon + 1, sizeof(tmp.files[tmp.count].path) - 1);
        tmp.files[tmp.count].path[sizeof(tmp.files[tmp.count].path) - 1] = '\0';
        tmp.count++;
    }
    fl_free_file_list(&lines);
    if (tmp.count == 0) {
        free(tmp.files);
        return false;
    }
    if (s_files.files) free(s_files.files);
    s_files = tmp;
    return true;
}

static void scan_app_dirs(void) {
    char* allowed[] = {"txt","png","pdf","jpg","jpeg","mp3","wav","ogg",NULL};

    ListOfFiles tmp = {0};
    for (int i = 0; i < (int)ARRAY_LEN(ListOfFilePaths); i++) {
        if (!ListOfFilePaths[i] || ListOfFilePaths[i][0] == '\0') continue;
        fl_get_all_files_from_dir(ListOfFilePaths[i], allowed, &tmp, NULL, false, FL_DIR_WALK_UNLIMITED);
    }
    for (size_t i = 0; i < tmp.count; ) {
        bool skip = false;
        char display_name[256] = {0};
        bool has_display = false;
        if (skip) {
            for (size_t k = i; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k+1];
            tmp.count--;
            continue;
        }
        if (has_display && display_name[0]) {
            snprintf(tmp.files[i].name, sizeof(tmp.files[i].name), "%s", display_name);
        }
        i++;
    }
    for (size_t i = 0; i < tmp.count; i++) {
        for (size_t j = i + 1; j < tmp.count; ) {
            if (fl_string_contains(tmp.files[i].name, tmp.files[j].name)) {
                for (size_t k = j; k + 1 < tmp.count; k++) tmp.files[k] = tmp.files[k+1];
                tmp.count--;
            } else {
                j++;
            }
        }
    }
    if (s_files.files) free(s_files.files);
    s_files = tmp;
}

static bool g_file_cache_ready = false;

void ensure_file_cache(void) {
    if (g_file_cache_ready) return;
    g_file_cache_ready = true;
    if (load_file_cache()) {
        return;
    }
    scan_app_dirs();
    save_file_cache();
}

static const char* g_file_exts[] = {"txt","png","pdf","jpg","jpeg","mp3","wav","ogg","c",NULL};

static bool file_ext_allowed(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot || !dot[1]) return false;
    for (int i = 0; g_file_exts[i]; i++) {
        if (strcasecmp(dot + 1, g_file_exts[i]) == 0) return true;
    }
    return false;
}

static void file_cache_drop(const char* full_path) {
    for (size_t i = 0; i < s_files.count; i++) {
        if (strcmp(s_files.files[i].path, full_path) == 0) {
            for (size_t k = i; k + 1 < s_files.count; k++) s_files.files[k] = s_files.files[k + 1];
            s_files.count--;
            return;
        }
    }
}

static void file_cache_add(const char* full_path) {
    for (size_t i = 0; i < s_files.count; i++) {
        if (strcmp(s_files.files[i].path, full_path) == 0) return;
    }
    const char* base = strrchr(full_path, '/');
    if (!base) base = strrchr(full_path, '\\');
    base = base ? base + 1 : full_path;
    if (!file_ext_allowed(base)) return;
    if (s_files.count >= s_files.capacity && !file_cache_grow(&s_files)) return;
    FileItem* it = &s_files.files[s_files.count];
    snprintf(it->path, sizeof(it->path), "%s", full_path);
    snprintf(it->name, sizeof(it->name), "%s", base);
    s_files.count++;
}

static void file_cache_on_change(const char* full_path, int added) {
    if (!full_path || !g_file_cache_ready) return;
    size_t before = s_files.count;
    if (added) file_cache_add(full_path);
    else file_cache_drop(full_path);
    if (s_files.count != before) {
        save_file_cache();
        printf("[file_search] incremental %s '%s' count=%zu->%zu\n",
               added ? "add" : "del", full_path, before, s_files.count);
        fflush(stdout);
    }
}

void rescan_file_cache(const char* full_path, int added) {
    g_file_cache_ready = true;
    file_cache_on_change(full_path, added);
}

void open_file_by_name(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    for (size_t i = 0; i < s_files.count; i++) {
        if (fl_string_contains(display_name,s_files.files[i].name)) {
            strncpy(path_copy, s_files.files[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    if (found) fl_launch_with_args(path_copy, NULL);
}

void open_file_as_admin(const char* display_name) {
    char path_copy[512] = {0};
    bool found = false;
    for (size_t i = 0; i < s_files.count; i++) {
        if (fl_string_contains(s_files.files[i].name, display_name)) {
            strncpy(path_copy, s_files.files[i].path, sizeof(path_copy)-1);
            found = true;
            break;
        }
    }
    if (found) fl_launch_with_args_as_admin(path_copy, NULL);
}

bool file_search_enter_callback(const char* selection, ListBoxInfo* lb) {
    open_file_by_name(selection);
    return true;
}

bool file_search_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    open_file_as_admin(selection);
    return true;
}

static const char* names[MAX_VISIBLE_ITEMS];

void file_search_refresh_callback(const char* filter, ListBoxInfo* lb) {
    ensure_file_cache();
    size_t cnt = s_files.count;
    if (cnt == 0) { fflush(stdout); return; }

    if (cnt > MAX_VISIBLE_ITEMS) cnt = MAX_VISIBLE_ITEMS;
    for (size_t i = 0; i < cnt; i++) names[i] = s_files.files[i].name;
    fl_populate_listbox(lb, filter, names, (int)cnt);
}

void file_search_copy_callback(const char* selection,int true_index,int type) {
    char* text = (char*)selection;
    switch(type) {
      case 0: {
        int start = strlen(text);
        for (int i = start; i > 0 ; i--) {
          if (text[i] == '\\') {
            for (int j = i; j < start; ++j){
                text[j] = '\0';
            }
            break;
          }
        }
        break;
      }
      case 1: {
        char buffer[512];
        strncpy(buffer, s_files.files[true_index].path, sizeof(buffer) - 1);
        text = strdup(buffer);
        break;
      }
    }
    fl_copy_to_clipboard(text);
}

void TriggerFileSearch(void) {
    for (size_t i = 0; i < ARRAY_LEN(ListOfFilePaths); i++) {
        fl_on_dir_changed(ListOfFilePaths[i], rescan_file_cache);
    }
    PROMPT(.title = "🚀 file_search",
           .refresh = file_search_refresh_callback,
           .enter = file_search_enter_callback,
           .ctrl_enter = file_search_ctrlenter_callback,
           .copy = file_search_copy_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+X", TriggerFileSearch,"Fortnite");
    fl_register_feature("File Search", TriggerFileSearch);
}
