#include <stdio.h>
#include "common.h"
#include "features/file_search.h"
#include "core/platform.h"
#include "config.h"
#if defined(_WIN32)
#include <shlwapi.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

static FileItem G_FILES[MAX_FILES];
static int G_FILE_COUNT = 0;

FileItemWithTime G_FILES_WITH_TIME[MAX_FILES];

AppItem G_APPS[MAX_APPS];
int G_APP_COUNT = 0;

char* SearchFileInDirectory(const char* directory, const char* search_string) {
    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(directory, expanded_dir, sizeof(expanded_dir))) return NULL;
    directory = expanded_dir;
#if defined(_WIN32)
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
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        
        if (StrStrIA(find_data.cFileName, search_string) != NULL) {
            snprintf(found_path, sizeof(found_path), "%s\\%s", directory, find_data.cFileName);
            FindClose(hFind);
            return found_path;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return NULL;
#else
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
#endif
}
static void GetFilesRecursiveWithTime(const char* dir_path, bool show_folder, char** list_of_allowed_extentions) {
    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) return;
    dir_path = expanded_dir;
#if defined(_WIN32)
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
            // Check if directory should be ignored
            bool should_ignore = false;
            for (int i = 0; i < sizeof(ListOfShitToIgnore) / sizeof(ListOfShitToIgnore[0]); i++) {
                if (StrCmpIA(find_data.cFileName, ListOfShitToIgnore[i]) == 0) {
                    should_ignore = true;
                    break;
                }
            }
            if (!should_ignore) {
                GetFilesRecursiveWithTime(full_path, show_folder, list_of_allowed_extentions);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extentions) {
                allowed = false;
                const char* ext = strrchr(find_data.cFileName, '.');
                if (ext) {
                    ext++;
                    int ext_count = 0;
                    while (list_of_allowed_extentions[ext_count] != NULL) {
                        ext_count++;
                    }
                    for (int i = 0; i < ext_count; i++) {
                        if (list_of_allowed_extentions[i] && StrCmpIA(ext, list_of_allowed_extentions[i]) == 0) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }
            
            if (allowed && G_FILE_COUNT < MAX_FILES) {
                char clean_name[256];
                strncpy(clean_name, find_data.cFileName, sizeof(clean_name) - 1);
                PathRemoveExtensionA(clean_name);

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
                
                G_FILES_WITH_TIME[G_FILE_COUNT].last_write = find_data.ftLastWriteTime;
                G_FILE_COUNT++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_FILE_COUNT < MAX_FILES);

    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && G_FILE_COUNT < MAX_FILES) {
        if (strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st)!=0) continue;
        if (S_ISDIR(st.st_mode)) {
            bool should_ignore = false;
            for (int i = 0; i < sizeof(ListOfShitToIgnore) / sizeof(ListOfShitToIgnore[0]); i++) {
                if (strcasecmp(entry->d_name, ListOfShitToIgnore[i]) == 0) {
                    should_ignore = true;
                    break;
                }
            }
            if (!should_ignore) {
                GetFilesRecursiveWithTime(full_path, show_folder, list_of_allowed_extentions);
            }
        } else {
            bool allowed = true;
            if (list_of_allowed_extentions) {
                allowed = false;
                const char* ext = strrchr(entry->d_name, '.');
                if (ext) {
                    ext++;
                    int ext_count = 0;
                    while (list_of_allowed_extentions[ext_count] != NULL) {
                        ext_count++;
                    }
                    for (int i = 0; i < ext_count; i++) {
                        if (list_of_allowed_extentions[i] && strcasecmp(ext, list_of_allowed_extentions[i]) == 0) {
                            allowed = true;
                            break;
                        }
                    }
                }
            }
            
            if (allowed) {
                if (G_FILE_COUNT >= MAX_FILES) break;
                char clean_name[256];
                strncpy(clean_name, entry->d_name, sizeof(clean_name)-1);
                clean_name[sizeof(clean_name)-1]='\0';
                char* dot = strrchr(clean_name, '.');
                if (dot) *dot='\0';
                if (show_folder) {
                    char folder_name[MAX_PATH];
                    strncpy(folder_name, dir_path, sizeof(folder_name)-1);
                    folder_name[sizeof(folder_name)-1]='\0';
                    char* last_slash = strrchr(folder_name, '/');
                    char* last_bs = strrchr(folder_name, '\\');
                    if (last_bs && (!last_slash || last_bs > last_slash)) last_slash = last_bs;
                    if (last_slash) snprintf(G_FILES_WITH_TIME[G_FILE_COUNT].name, sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name), "%s/%s", last_slash+1, clean_name);
                    else strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].name, clean_name, sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name)-1);
                } else {
                    strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].name, clean_name, sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].name)-1);
                }
                strncpy(G_FILES_WITH_TIME[G_FILE_COUNT].path, full_path, sizeof(G_FILES_WITH_TIME[G_FILE_COUNT].path)-1);
                G_FILES_WITH_TIME[G_FILE_COUNT].last_write.dwLowDateTime = (uint32_t)(st.st_mtime & 0xFFFFFFFF);
                G_FILES_WITH_TIME[G_FILE_COUNT].last_write.dwHighDateTime = (uint32_t)((st.st_mtime >> 32) & 0xFFFFFFFF);
                G_FILE_COUNT++;
            }
        }
    }
    closedir(dir);
#endif
}
static int CompareFileByTime(const void* a, const void* b) {
    const FileItemWithTime* fa = (const FileItemWithTime*)a;
    const FileItemWithTime* fb = (const FileItemWithTime*)b;
    int64_t time_a = ((int64_t)fa->last_write.dwHighDateTime << 32) + fa->last_write.dwLowDateTime;
    int64_t time_b = ((int64_t)fb->last_write.dwHighDateTime << 32) + fb->last_write.dwLowDateTime;
    if (time_a > time_b) return -1;
    if (time_a < time_b) return 1;
    return 0;
}

static void SortFilesByTime(void) {
    qsort(G_FILES_WITH_TIME, G_FILE_COUNT, sizeof(FileItemWithTime), CompareFileByTime);
}

void GetFilesRecursive(const char* dir_path, bool show_folder,char** list_of_allowed_extentions) {
    G_FILE_COUNT = 0;
    GetFilesRecursiveWithTime(dir_path, show_folder,list_of_allowed_extentions);
    SortFilesByTime();
    
    for (int i = 0; i < G_FILE_COUNT && i < MAX_FILES; i++) {
        strncpy(G_FILES[i].name, G_FILES_WITH_TIME[i].name, sizeof(G_FILES[i].name) - 1);
        G_FILES[i].name[sizeof(G_FILES[i].name) - 1] = '\0';
        strncpy(G_FILES[i].path, G_FILES_WITH_TIME[i].path, sizeof(G_FILES[i].path) - 1);
        G_FILES[i].path[sizeof(G_FILES[i].path) - 1] = '\0';
    }
}
void ClearFileList(void) {
    G_FILE_COUNT = 0;
}


int GetFileCount(void) {
    return G_FILE_COUNT;
}

void scan_directory_recursive(const char* dir_path) {
    if (dir_path == NULL) return;

    char expanded_dir[MAX_PATH];
    if (!Platform_ExpandPath(dir_path, expanded_dir, sizeof(expanded_dir))) return;
    dir_path = expanded_dir;
#if defined(_WIN32)
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return;

    // Pre-compute ignore list size
    static int ignore_count = 0;
    static bool ignore_init = false;
    if (!ignore_init) {
        ignore_count = sizeof(ListOfShitToIgnore) / sizeof(ListOfShitToIgnore[0]);
        ignore_init = true;
    }

    do {
        if (find_data.cFileName[0] == '.' && 
            (find_data.cFileName[1] == '\0' || 
             (find_data.cFileName[1] == '.' && find_data.cFileName[2] == '\0'))) {
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Check ignore list
            bool skip = false;
            for (int i = 0; i < ignore_count; i++) {
                if (StrCmpIA(find_data.cFileName, ListOfShitToIgnore[i]) == 0) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);
            scan_directory_recursive(full_path);
        } else {
            // Check extension first (cheap)
            const char* ext = PathFindExtensionA(find_data.cFileName);
            if ((ext[0] == '.' && ext[1] == 'l' && ext[2] == 'n' && ext[3] == 'k' && ext[4] == '\0') ||
                (ext[0] == '.' && ext[1] == 'e' && ext[2] == 'x' && ext[3] == 'e' && ext[4] == '\0') ||
                (ext[0] == '.' && ext[1] == 'u' && ext[2] == 'r' && ext[3] == 'l' && ext[4] == '\0')) {
                
                if (G_APP_COUNT >= MAX_APPS) break;

                char clean_name[128];
                int name_len = strlen(find_data.cFileName);
                int ext_len = strlen(ext);
                int base_len = name_len - ext_len;
                
                // Copy only the base name (before extension)
                if (base_len >= sizeof(clean_name)) base_len = sizeof(clean_name) - 1;
                memcpy(clean_name, find_data.cFileName, base_len);
                clean_name[base_len] = '\0';

                // Check duplicates (only if we have items to check)
                bool is_duplicate = false;
                for (int i = 0; i < G_APP_COUNT; i++) {
                    if (StrCmpIA(G_APPS[i].name, clean_name) == 0) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (is_duplicate) continue;

                char full_path[MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

                strncpy(G_APPS[G_APP_COUNT].name, clean_name, sizeof(G_APPS[G_APP_COUNT].name) - 1);
                strncpy(G_APPS[G_APP_COUNT].path, full_path, sizeof(G_APPS[G_APP_COUNT].path) - 1);
                G_APP_COUNT++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) && G_APP_COUNT < MAX_APPS);

    FindClose(hFind);
#else
    if (strlen(dir_path)==0) return;
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    static int ignore_count = 0;
    static bool ignore_init = false;
    if (!ignore_init) {
        ignore_count = sizeof(ListOfShitToIgnore)/sizeof(ListOfShitToIgnore[0]);
        ignore_init = true;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && G_APP_COUNT < MAX_APPS) {
        if (entry->d_name[0]=='.' && (entry->d_name[1]=='\0' || (entry->d_name[1]=='.' && entry->d_name[2]=='\0'))) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st)!=0) continue;
        if (S_ISDIR(st.st_mode)) {
            bool skip=false;
            for (int i=0;i<ignore_count;i++) if (ListOfShitToIgnore[i] && strcasecmp(entry->d_name, ListOfShitToIgnore[i])==0) {skip=true;break;}
            if (skip) continue;
            scan_directory_recursive(full_path);
        } else {
            const char* dot = strrchr(entry->d_name, '.');
            bool is_app = false;
            if (dot) {
                if (strcasecmp(dot, ".desktop")==0 || strcasecmp(dot, ".lnk")==0 || strcasecmp(dot, ".exe")==0 || strcasecmp(dot, ".url")==0 || strcasecmp(dot, ".sh")==0) is_app = true;
            } else {
                if (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) is_app = true;
            }
            if (!is_app) continue;
            if (G_APP_COUNT >= MAX_APPS) break;
            char clean_name[128];
            int name_len = strlen(entry->d_name);
            int base_len = dot ? (int)(dot - entry->d_name) : name_len;
            if (dot && strcasecmp(dot, ".desktop")==0) base_len = name_len - 8;
            else if (dot && (strcasecmp(dot, ".lnk")==0 || strcasecmp(dot, ".exe")==0 || strcasecmp(dot, ".url")==0 || strcasecmp(dot, ".sh")==0)) base_len = name_len - (int)strlen(dot);
            if (base_len >= (int)sizeof(clean_name)) base_len = sizeof(clean_name)-1;
            memcpy(clean_name, entry->d_name, base_len);
            clean_name[base_len]='\0';
            bool dup=false;
            for (int i=0;i<G_APP_COUNT;i++) if (strcasecmp(G_APPS[i].name, clean_name)==0) {dup=true;break;}
            if (dup) continue;
            strncpy(G_APPS[G_APP_COUNT].name, clean_name, sizeof(G_APPS[G_APP_COUNT].name)-1);
            G_APPS[G_APP_COUNT].name[sizeof(G_APPS[G_APP_COUNT].name)-1]='\0';
            strncpy(G_APPS[G_APP_COUNT].path, full_path, sizeof(G_APPS[G_APP_COUNT].path)-1);
            G_APPS[G_APP_COUNT].path[sizeof(G_APPS[G_APP_COUNT].path)-1]='\0';
            G_APP_COUNT++;
        }
    }
    closedir(dir);
#endif
}
