// helpers.h
#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>  
#include <string.h> 
#include <shlwapi.h>
#include <stdlib.h>
#include "types.h"
#include "config.h"
#include "common_ui.h"

// ============================================================
// HOTKEY SYSTEM - Single entry point for all hotkeys
// ============================================================

struct MappedFile;

typedef void (*HotkeyHandler)(void);

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    HotkeyHandler handler;
    const char* name;
} HotkeyBinding;

// helpers.h - ProcessHotkey uses exact modifier matching
static inline bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        // Exact match on both key AND modifiers
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;  // Exact match
        
        // Check blacklist
        if (b->blacklist && b->blacklist_count > 0) {
            bool blacklisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
                if (StrStrIA(current_title, b->blacklist[j]) != NULL) {
                    blacklisted = true;
                    break;
                }
            }
            if (blacklisted) return false;
        }
        
        // Don't trigger if popup is active
        extern HWND hPopupWnd;
        if (IsPopupActive(hPopupWnd)) return false;
        
        // Execute the handler
        if (b->handler) {
            printf("[HOTKEY] %s triggered (mods: 0x%X)\n", b->name, b->modifiers);
            b->handler();
            return true;
        }
    }
    return false;
}
// ============================================================
// ZOXIDE HELPER
// ============================================================

typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

static inline char* QueryZoxidePath(const char* query) {
    char command[512];
    snprintf(command, sizeof(command), "zoxide query %s", query);
    
    static char path[1024];
    path[0] = '\0';
    FILE* pipe = popen(command, "r");
    if (pipe) {
        fgets(path, sizeof(path), pipe);
        pclose(pipe);
        path[strcspn(path, "\r\n")] = '\0';
    }
    return (strlen(path) > 0) ? path : NULL;
}

static inline void TriggerZoxideWithOptions(const ZoxideOptions* options) {
    extern HWND hPopupWnd;
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    const char* prompt[1] = { options->prompt_title };
    char* query = PromptUser(prompt, options->icon, 1, 220, 150, 32, 
                             NULL, NULL, NULL, NULL, NULL);
    
    if (!query || strlen(query) == 0) return;
    
    char* path = QueryZoxidePath(query);
    if (!path) {
        printf("[ZOXIDE] No match found for: %s\n", query);
        return;
    }
    
    if (options->open_neovide) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start neovide \"%s\"", path);
        system(cmd);
        printf("[ZOXIDE] Opened Neovide in: %s\n", path);
    } else if (options->open_yazi) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\" -e yazi", path);
        system(cmd);
        printf("[ZOXIDE] Opened Yazi in: %s\n", path);
    } else if (options->open_explorer) {
        ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
        printf("[ZOXIDE] Opened Explorer in: %s\n", path);
    } else {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\"", path);
        system(cmd);
        printf("[ZOXIDE] Opened Alacritty in: %s\n", path);
    }
}

// ============================================================
// SHELL HELPER
// ============================================================

static inline bool ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = (async ? SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK : SEE_MASK_FLAG_NO_UI);
    sei.lpVerb = verb ? verb : "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExA(&sei);
}

static inline bool LaunchApp(const char* path) {
    return ShellExecuteWithVerb(path, "open", true);
}

static inline bool LaunchAppAsAdmin(const char* path) {
    return ShellExecuteWithVerb(path, "runas", true);
}
static inline bool IsCtrlPressed(void) {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}



static inline int FileInit(const char *filePath, MappedFile *mf, size_t initialPadding) {
    mf->hFile = CreateFileA(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->hFile == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER fs;
    GetFileSizeEx(mf->hFile, &fs);
    mf->fileSize = (size_t)fs.QuadPart;
    mf->capacity = mf->fileSize + initialPadding; 

    LARGE_INTEGER mapSize;
    mapSize.QuadPart = mf->capacity;

    mf->hMapping = CreateFileMappingA(mf->hFile, NULL, PAGE_READWRITE, mapSize.HighPart, mapSize.LowPart, NULL);
    if (mf->hMapping == NULL) {
        CloseHandle(mf->hFile);
        return 0;
    }

    mf->pBuffer = (char *)MapViewOfFile(mf->hMapping, FILE_MAP_WRITE, 0, 0, 0);
    if (mf->pBuffer == NULL) {
        CloseHandle(mf->hMapping);
        CloseHandle(mf->hFile);
        return 0;
    }
    return 1;
}

static inline int FileAppend(MappedFile *mf, const char *text) {
    if (!mf || !text) return 0;
    
    size_t len = strlen(text);
    if (mf->fileSize + len > mf->capacity) {
        // Need to expand - we'll handle this by just not appending if too big
        printf("[FILE] Warning: Not enough space to append data!\n");
        return 0;
    }
    
    memcpy(mf->pBuffer + mf->fileSize, text, len);
    mf->fileSize += len;
    return 1;
}

static inline Line* FileGetLines(MappedFile *mf, size_t *outLineCount) {
    size_t maxLines = 1000;
    size_t count = 0;
    Line *list = (Line *)malloc(maxLines * sizeof(Line));
    if (list == NULL) return NULL;

    char *lineStart = mf->pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        if (mf->pBuffer[i] == '\n' || i == totalBytes - 1) {
            if (count >= maxLines) {
                maxLines *= 2;
                list = (Line *)realloc(list, maxLines * sizeof(Line));
            }
            size_t currentLineLen = &mf->pBuffer[i] - lineStart;
            if (i == totalBytes - 1) currentLineLen++; 
            list[count].ptr = lineStart;
            list[count].length = currentLineLen;
            count++;
            lineStart = &mf->pBuffer[i + 1];
        }
    }
    *outLineCount = count;
    return list;
}

void FileClose(MappedFile *mf) {
    if (mf->pBuffer) UnmapViewOfFile(mf->pBuffer);
    if (mf->hMapping) CloseHandle(mf->hMapping);
    if (mf->hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fs;
        fs.QuadPart = mf->fileSize;
        SetFilePointerEx(mf->hFile, fs, NULL, FILE_BEGIN);
        SetEndOfFile(mf->hFile); 
        CloseHandle(mf->hFile);
    }
}


static inline char** FileGetStrings(MappedFile *mf, size_t *outLineCount, size_t **outNewlineIndices, size_t *outNewlineCount) {
    size_t maxLines = 1000;
    size_t count = 0;
    
    char **stringList = (char **)malloc(maxLines * sizeof(char*));
    if (stringList == NULL) return NULL;

    // Allocate an array to remember EXACTLY which indices were modified
    size_t maxNewlines = 2000;
    size_t newlineCount = 0;
    size_t *newlineIndices = (size_t *)malloc(maxNewlines * sizeof(size_t));

    stringList[count++] = mf->pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        // Track and remember Windows \r characters
        if (mf->pBuffer[i] == '\r') {
            mf->pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlineIndices = (size_t *)realloc(newlineIndices, maxNewlines * sizeof(size_t));
            }
            // Save index, flag it as a \r by using a tracking format or handling it at \n
            newlineIndices[newlineCount++] = i; 
        }
        else if (mf->pBuffer[i] == '\n') {
            mf->pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlineIndices = (size_t *)realloc(newlineIndices, maxNewlines * sizeof(size_t));
            }
            newlineIndices[newlineCount++] = i; // Save exact index of \n

            if (i + 1 < totalBytes) {
                if (count >= maxLines) {
                    maxLines *= 2;
                    stringList = (char **)realloc(stringList, maxLines * sizeof(char*));
                }
                stringList[count++] = &mf->pBuffer[i + 1];
            }
        }
    }
    
    *outLineCount = count;
    *outNewlineIndices = newlineIndices; // Pass the map back to main
    *outNewlineCount = newlineCount;
    return stringList;
}

void CleanUpFile(MappedFile *mf, size_t *newlineIndices, size_t newlineCount) {
    if (!newlineIndices) return;

    // Read the map backwards or forwards and put the exact characters back
    for (size_t i = 0; i < newlineCount; i++) {
        size_t idx = newlineIndices[i];
        
        if (idx + 1 < mf->fileSize && mf->pBuffer[idx + 1] == '\0') {
            mf->pBuffer[idx] = '\r';
        } else {
            mf->pBuffer[idx] = '\n';
        }
    }
    free(newlineIndices);
}

#endif 
