#ifndef FILE_SEARCH_H
#define FILE_SEARCH_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlwapi.h>
#define MAX_FILES 2000

typedef struct {
    char name[256];
    char path[MAX_PATH];
} FileItem;

static FileItem G_FILES[MAX_FILES];
static int G_FILE_COUNT = 0;

static char* SearchFileInDirectory(const char* directory, const char* search_string) {
    if (!directory || !search_string || strlen(search_string) == 0) {
        return NULL;
    }

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", directory);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    static char found_path[MAX_PATH];  // Static so we can return it safely

    do {
        // Skip directories and hidden files
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        // Check if the filename contains the search string (case-insensitive)
        if (StrStrIA(find_data.cFileName, search_string) != NULL) {
            snprintf(found_path, sizeof(found_path), "%s\\%s", directory, find_data.cFileName);
            FindClose(hFind);
            return found_path;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return NULL;
}
static char* SearchFileRecursive(const char* directory, const char* search_string) {
    if (!directory || !search_string || strlen(search_string) == 0) {
        return NULL;
    }

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", directory);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    static char found_path[MAX_PATH];

    do {
        // Skip . and ..
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", directory, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into subdirectories
            char* result = SearchFileRecursive(full_path, search_string);
            if (result != NULL) {
                FindClose(hFind);
                return result;
            }
        } else {
            // Check if the filename contains the search string
            if (StrStrIA(find_data.cFileName, search_string) != NULL) {
                snprintf(found_path, sizeof(found_path), "%s", full_path);
                FindClose(hFind);
                return found_path;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return NULL;
}

typedef struct {
    char name[512];
    char path[MAX_PATH];
    FILETIME last_write;
} FileItemWithTime;

static FileItemWithTime G_FILES_WITH_TIME[MAX_FILES];

static void GetFilesRecursiveWithTime(const char* dir_path, bool show_folder) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (StrCmpIA(find_data.cFileName, "node_modules") == 0 ||
                StrCmpIA(find_data.cFileName, ".git") == 0 ||
                StrCmpIA(find_data.cFileName, "bin") == 0 ||
                StrCmpIA(find_data.cFileName, "obj") == 0 ||
                StrCmpIA(find_data.cFileName, "build") == 0) {
                continue;
            }
            GetFilesRecursiveWithTime(full_path, show_folder);
        } else {
            if (G_FILE_COUNT < MAX_FILES) {
                char clean_name[256];
                strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                PathRemoveExtensionA(clean_name);

                // Store the name with optional folder prefix
                if (show_folder) {
                    char folder_name[MAX_PATH];
                    strncpy(folder_name, dir_path, sizeof(folder_name) - 1);
                    char* last_backslash = strrchr(folder_name, '\\');
                    if (last_backslash != NULL) {
                        snprintf(G_FILES_WITH_TIME[G_FILE_COUNT].name, sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name), 
                                 "%s/%s", last_backslash + 1, clean_name);
                    } else {
                        strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].name, clean_name, 
                                sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name) - 1);
                    }
                } else {
                    strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].name, clean_name, 
                            sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name) - 1);
                }

                strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].path, full_path, 
                        sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].path) - 1);
                
                // Store the last write time
                G_FILES_WITH_TIME[G_FILE_COUNT].last_write = find_data.ftLastWriteTime;
                G_FILE_COUNT++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_FILE_COUNT < MAX_FILES);

    FindClose(hFind);
}
static int CompareFileByTime(const void* a, const void* b) {
    const FileItemWithTime* fa = (const FileItemWithTime*)a;
    const FileItemWithTime* fb = (const FileItemWithTime*)b;
    
    // Compare FILETIME (most recent first)
    // Returns -1 if a is newer, 1 if b is newer, 0 if same
    __int64 time_a = ((__int64)fa->last_write.dwHighDateTime << 32) + fa->last_write.dwLowDateTime;
    __int64 time_b = ((__int64)fb->last_write.dwHighDateTime << 32) + fb->last_write.dwLowDateTime;
    
    if (time_a > time_b) return -1;
    if (time_a < time_b) return 1;
    return 0;
}

static void SortFilesByTime(void) {
    qsort(G_FILES_WITH_TIME, G_FILE_COUNT, sizeof(FileItemWithTime), CompareFileByTime);
}

static void GetFilesRecursive(const char* dir_path, bool show_folder) {
    G_FILE_COUNT = 0;
    GetFilesRecursiveWithTime(dir_path, show_folder);
    SortFilesByTime();
    
    for (int i = 0; i < G_FILE_COUNT && i < MAX_FILES; i++) {
        strncpy(G_FILES[i].name, G_FILES_WITH_TIME[i].name, sizeof(G_FILES[i].name) - 1);
        G_FILES[i].name[sizeof(G_FILES[i].name) - 1] = '\0';
        strncpy(G_FILES[i].path, G_FILES_WITH_TIME[i].path, sizeof(G_FILES[i].path) - 1);
        G_FILES[i].path[sizeof(G_FILES[i].path) - 1] = '\0';
    }
}
static void GetFilesByExtension(const char* dir_path, const char* extensions[], int ext_count) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (StrCmpIA(find_data.cFileName, "node_modules") == 0 ||
                StrCmpIA(find_data.cFileName, ".git") == 0 ||
                StrCmpIA(find_data.cFileName, "bin") == 0 ||
                StrCmpIA(find_data.cFileName, "obj") == 0 ||
                StrCmpIA(find_data.cFileName, "build") == 0) {
                continue;
            }
            GetFilesByExtension(full_path, extensions, ext_count);
        } else {
            // Check if the file has a matching extension
            char* ext = PathFindExtensionA(find_data.cFileName);
            for (int i = 0; i < ext_count; i++) {
                if (StrCmpIA(ext, extensions[i]) == 0) {
                    if (G_FILE_COUNT < MAX_FILES) {
                        strncpy(G_FILES[G_FILE_COUNT].name, find_data.cFileName, sizeof(G_FILES[G_FILE_COUNT].name) - 1);
                        strncpy(G_FILES[G_FILE_COUNT].path, full_path, sizeof(G_FILES[G_FILE_COUNT].path) - 1);
                        G_FILE_COUNT++;
                    }
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_FILE_COUNT < MAX_FILES);

    FindClose(hFind);
}
// Add this to file_search.h:
static FileItemWithTime* GetFileListWithTime(void) {
    return G_FILES_WITH_TIME;
}
// Clear the file list
static void ClearFileList(void) {
    G_FILE_COUNT = 0;
}

// Get the file list
static FileItem* GetFileList(void) {
    return G_FILES;
}

// Get the file count
static int GetFileCount(void) {
    return G_FILE_COUNT;
}

#endif
