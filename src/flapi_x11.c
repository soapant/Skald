#include "core/flapi.h"

bool fl_case_insensitive_string_match_platform(char* needle,char* haystack) {
  return strstr(needle,haystack);
}

int fl_copy_into_clipboard_platform(const char* text) {
    int status = 0;
    size_t len = strlen(text);

    char buffer[256];
    char cmd[256];

    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if ((!wayland_display || !wayland_display[0]) || (!runtime_dir || !runtime_dir[0])) {
        static char found_display[64];
        static char found_runtime[256];
        found_display[0] = '\0';
        found_runtime[0] = '\0';
        DIR *dir = opendir("/run/user");
        if (dir) {
            struct dirent *e;
            while ((e = readdir(dir)) != NULL) {
                if (e->d_name[0] == '.') continue;
                char udir[256];
                snprintf(udir, sizeof(udir), "/run/user/%s", e->d_name);
                DIR *sub = opendir(udir);
                if (!sub) continue;
                struct dirent *f;
                while ((f = readdir(sub)) != NULL) {
                    if (strncmp(f->d_name, "wayland-", 8) == 0) {
                        snprintf(found_display, sizeof(found_display), "%s", f->d_name);
                        snprintf(found_runtime, sizeof(found_runtime), "%s", udir);
                        break;
                    }
                }
                closedir(sub);
                if (found_display[0] != '\0') break;
            }
            closedir(dir);
        }
        if (!wayland_display || !wayland_display[0])
            wayland_display = found_display[0] != '\0' ? found_display : NULL;
        if (!runtime_dir || !runtime_dir[0])
            runtime_dir = found_runtime[0] != '\0' ? found_runtime : NULL;
    }

    if (wayland_display != NULL && wayland_display[0] != '\0' &&
        runtime_dir != NULL && runtime_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "XDG_RUNTIME_DIR=%s WAYLAND_DISPLAY=%s wl-copy",
                 runtime_dir, wayland_display);
    } else {
        snprintf(cmd, sizeof(cmd), "xclip -selection clipboard");
    }

    FILE *pipe = popen(cmd, "w");
    if (!pipe) {
        pipe = popen("xclip -selection clipboard", "w");
        if (!pipe) {
            pipe = popen("xsel --clipboard --input", "w");
        }
    }

    if (!pipe) {
        fprintf(stderr, "[PLATFORM] Error: Failed to find a valid Linux clipboard manager.\n");
        return 0;
    }

    size_t len2 = strlen(text);
    fprintf(pipe, "%s", text);
    
    status = pclose(pipe);
    return (status == 0) ? 1 : 0;
}

void fl_get_all_files_from_dir_platform(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {

    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            bool should_ignore = false;
            if (list_of_names_to_ignore) {
                for (int i = 0; i < sizeof(list_of_names_to_ignore) / sizeof(list_of_names_to_ignore[0]); i++) {
                    if (fl_case_insensitive_string_match_platform(list_of_files->items[i].name, list_of_names_to_ignore[i]) == 0) {
                        should_ignore = true;
                        break;
                    }
                }
            }
            if (!should_ignore) {
                fl_get_all_files_from_dir_platform(full_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extensions) {
                allowed = false;
                const char* ext = strrchr(entry->d_name, '.');
                if (ext) {
                    ext++;
                    for (int i = 0; list_of_allowed_extensions[i] != NULL; i++) {
                        if (strcasecmp(ext, list_of_allowed_extensions[i]) == 0) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }

            if (allowed) {
                if (list_of_files->count >= list_of_files->capacity) {
                    list_of_files->capacity = list_of_files->capacity == 0 ? 10 : list_of_files->capacity * 2;
                    list_of_files->items = realloc(list_of_files->items, list_of_files->capacity * sizeof(FileItem));
                }

                FileItem* item = &list_of_files->items[list_of_files->count];

                strncpy(item->path, full_path, sizeof(item->path) - 1);
                item->path[sizeof(item->path) - 1] = '\0';

                char clean_name[256];
                strncpy(clean_name, entry->d_name, sizeof(clean_name) - 1);
                clean_name[sizeof(clean_name) - 1] = '\0';
                char* dot = strrchr(clean_name, '.');
                if (dot) *dot = '\0';
                strncpy(item->name, clean_name, sizeof(item->name) - 1);
                item->name[sizeof(item->name) - 1] = '\0';

                list_of_files->count++;
            }
        }
    }
    closedir(dir);
}


char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path) {
    if (!directory || !search_string || strlen(search_string) == 0) return NULL;
    DIR* dir = opendir(directory);
    if (!dir) return NULL;
    static char found_path[MAX_PATH];
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0]=='.' && (entry->d_name[1]=='\0' || (entry->d_name[1]=='.' && entry->d_name[2]=='\0'))) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(full_path, &st)!=0) continue;
        if (S_ISDIR(st.st_mode)) continue;
        if (strcasestr(entry->d_name, search_string) != NULL) {
            snprintf(found_path, sizeof(found_path), "%s", full_path);
            closedir(dir);
            return found_path;
        }
    }
    closedir(dir);
    return NULL;
}
