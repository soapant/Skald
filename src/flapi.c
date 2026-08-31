#include "core/flapi.h"

int fl_copy_into_clipboard(const char* text) {
    if (text == NULL) return 0;
    return fl_copy_into_clipboard_platform(text);
}

void fl_get_all_files_from_dir(const char* dir_path, char** list_of_allowed_extensions, ListOfFiles* list_of_files,char** list_of_names_to_ignore) {
    if (!dir_path || !list_of_files) return;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) return;
    dir_path = expanded_dir;

    fl_get_all_files_from_dir_platform(dir_path, list_of_allowed_extensions, list_of_files, list_of_names_to_ignore);
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
    char temp[100][512];
    int count = ReadFile(temp, filepath);
    if (count > max) count = max;
    for (int i = 0; i < count; i++) {
        char* dup = strdup(temp[i]);
        if (!dup) {
            for (int j = 0; j < i; j++) free((void*)out[j]);
            return 0;
        }
        out[i] = dup;
    }
    return count;
}

void fl_free_file_list(const char* list[], int count) {
    if (!list || count <= 0) return;
    for (int i = 0; i < count; i++) free((void*)list[i]);
}

void fl_register_hotkey(char* hotkey_string,void (*on_hotkey_trigger)(void),bool (*optional_condition_for_hotkey)(void),const char** optional_blacklistedwindows) {
    HotkeyBinding binding = convert_into_hotkey_binding(hotkey_string,
                                                        on_hotkey_trigger,
                                                        optional_condition_for_hotkey,
                                                        optional_blacklistedwindows);
}

