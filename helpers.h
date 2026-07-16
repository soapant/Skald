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

// Helper to write timestamped logs to a file since we are in -mwindows
static inline void WriteDebugLog(const char* format, ...) {
    FILE* logFile = fopen("D:\\Projects\\C Code\\zoxide_debug.log", "a");
    if (!logFile) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);

    fclose(logFile);
}

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

static inline bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;
        
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
        
        extern HWND hPopupWnd;
        if (IsPopupActive(hPopupWnd)) return false;
        
        if (b->handler) {
            WriteDebugLog("[HOTKEY] %s triggered\n", b->name);
            b->handler();
            return true;
        }
    }
    return false;
}

// ============================================================
// ZOXIDE HELPER WITH HIGH-PRECISION TIMING LOGS
// ============================================================

typedef struct {
    const char* prompt_title;
    const char* icon;
    bool open_neovide;
    bool open_yazi;
    bool open_explorer;
} ZoxideOptions;

static inline char* QueryZoxidePath(const char* query) {
    DWORD start_time = GetTickCount();

    static char path[1024];
    path[0] = '\0';

    char clean_query[512];
    strncpy(clean_query, query, sizeof(clean_query) - 1);
    clean_query[sizeof(clean_query) - 1] = '\0';
    clean_query[strcspn(clean_query, "\r\n")] = '\0';

    WriteDebugLog("  [ZOXIDE QUERY START] Query: '%s'\n", clean_query);

    char command[512];
    snprintf(command, sizeof(command), "zoxide query %s", clean_query);

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        WriteDebugLog("  [ZOXIDE QUERY ERROR] CreatePipe failed!\n");
        return NULL;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {0};

    WriteDebugLog("  [ZOXIDE QUERY] Spawning zoxide process...\n");
    DWORD proc_start = GetTickCount();
    if (CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        WriteDebugLog("  [ZOXIDE QUERY] Waiting for process to return...\n");
        DWORD wait_res = WaitForSingleObject(pi.hProcess, 5000); // 5-second max fail-safe
        WriteDebugLog("  [ZOXIDE QUERY] Process wait finished. Code: %lu (Time taken: %lu ms)\n", 
                      wait_res, GetTickCount() - proc_start);

        if (wait_res == WAIT_TIMEOUT) {
            WriteDebugLog("  [ZOXIDE QUERY WARNING] Zoxide timed out!\n");
            TerminateProcess(pi.hProcess, 0);
        }

        DWORD bytesRead = 0;
        if (ReadFile(hReadPipe, path, sizeof(path) - 1, &bytesRead, NULL)) {
            path[bytesRead] = '\0';
            path[strcspn(path, "\r\n")] = '\0';
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        WriteDebugLog("  [ZOXIDE QUERY ERROR] CreateProcessA failed!\n");
        CloseHandle(hWritePipe);
    }

    CloseHandle(hReadPipe);
    WriteDebugLog("  [ZOXIDE QUERY END] Found path: '%s' (Total query time: %lu ms)\n", path, GetTickCount() - start_time);
    return (strlen(path) > 0) ? path : NULL;
}

static inline void TriggerZoxideWithOptions(const ZoxideOptions* options) {
    DWORD start_time = GetTickCount();
    WriteDebugLog("[TRIGGER ZOXIDE] Trigger initiated for: %s\n", options->prompt_title);

    extern HWND hPopupWnd;
    if (CheckAndBlockIfPopupActive(hPopupWnd)) {
        WriteDebugLog("[TRIGGER ZOXIDE] Blocked: Popup already active.\n");
        return;
    }
    
    const char* prompt[1] = { options->prompt_title };
    
    WriteDebugLog("[TRIGGER ZOXIDE] Opening user text input prompt...\n");
    DWORD prompt_start = GetTickCount();
    char* query = PromptUser(prompt, options->icon, 1, 220, 150, 32, 
                               NULL, NULL, NULL, NULL, NULL);
    WriteDebugLog("[TRIGGER ZOXIDE] Prompt closed. User entered: '%s' (User took: %lu ms)\n", 
                  query ? query : "NULL", GetTickCount() - prompt_start);
    
    if (!query || strlen(query) == 0) {
        WriteDebugLog("[TRIGGER ZOXIDE] Cancelled: Query is empty.\n");
        return;
    }
    
    char* path = QueryZoxidePath(query);
    if (!path) {
        WriteDebugLog("[TRIGGER ZOXIDE] Ended: No path match returned from zoxide.\n");
        return;
    }
    
    DWORD exec_start = GetTickCount();
    if (options->open_neovide) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start neovide \"%s\"", path);
        WriteDebugLog("[TRIGGER ZOXIDE] Executing system command: %s\n", cmd);
        system(cmd);
    } else if (options->open_yazi) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\" -e yazi", path);
        WriteDebugLog("[TRIGGER ZOXIDE] Executing system command: %s\n", cmd);
        system(cmd);
    } else if (options->open_explorer) {
        WriteDebugLog("[TRIGGER ZOXIDE] Executing ShellExecuteA for explorer on: %s\n", path);
        ShellExecuteA(NULL, "open", "explorer.exe", path, NULL, SW_SHOWNORMAL);
    } else {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "start alacritty --working-directory \"%s\"", path);
        WriteDebugLog("[TRIGGER ZOXIDE] Executing system command: %s\n", cmd);
        system(cmd);
    }
    
    WriteDebugLog("[TRIGGER ZOXIDE] App launch command executed. (Launch execution took: %lu ms)\n", GetTickCount() - exec_start);
    WriteDebugLog("[TRIGGER ZOXIDE] Finished completely. (Total cycle time: %lu ms)\n", GetTickCount() - start_time);
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

// ============================================================
// MEMORY MAPPED FILE SYSTEM WITH TIMING LOGS
// ============================================================

typedef struct {
    size_t index;
    char original;
} NewlineInfo;

static inline int FileInit(const char *filePath, MappedFile *mf, size_t initialPadding) {
    WriteDebugLog("  [FILE INIT START] File: %s\n", filePath);

    mf->hFile = CreateFileA(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->hFile == INVALID_HANDLE_VALUE) {
        WriteDebugLog("  [FILE INIT ERROR] CreateFileA failed!\n");
        return 0;
    }

    LARGE_INTEGER fs;
    GetFileSizeEx(mf->hFile, &fs);
    mf->fileSize = (size_t)fs.QuadPart;
    
    // CRITICAL: Only map the ACTUAL file size, NOT the padded size
    // If file is empty, map 1 byte so we can write
    size_t mapSize = (mf->fileSize > 0) ? mf->fileSize : 1;
    mf->capacity = mapSize;

    LARGE_INTEGER mapSizeLI;
    mapSizeLI.QuadPart = mapSize;

    mf->hMapping = CreateFileMappingA(mf->hFile, NULL, PAGE_READWRITE, mapSizeLI.HighPart, mapSizeLI.LowPart, NULL);
    if (!mf->hMapping) {
        WriteDebugLog("  [FILE INIT ERROR] CreateFileMappingA failed!\n");
        CloseHandle(mf->hFile);
        return 0;
    }

    mf->pBuffer = (char *)MapViewOfFile(mf->hMapping, FILE_MAP_WRITE, 0, 0, 0);
    if (!mf->pBuffer) {
        CloseHandle(mf->hMapping);
        CloseHandle(mf->hFile);
        return 0;
    }

    // If file is empty, initialize with null terminator
    if (mf->fileSize == 0) {
        mf->pBuffer[0] = '\0';
    }

    WriteDebugLog("  [FILE INIT END] File: %s, Size: %zu bytes, Mapped: %zu\n", filePath, mf->fileSize, mapSize);
    return 1;
}

static inline int FileAppend(MappedFile *mf, const char *text) {
    if (!mf || !text) return 0;
    
    size_t len = strlen(text);
    if (len == 0) return 0;

    // Check if we need to expand the mapping
    if (mf->fileSize + len + 1 > mf->capacity) {
        // Unmap the current view
        if (mf->pBuffer) {
            UnmapViewOfFile(mf->pBuffer);
            mf->pBuffer = NULL;
        }
        if (mf->hMapping) {
            CloseHandle(mf->hMapping);
            mf->hMapping = NULL;
        }

        // New capacity: current file size + new text + 1 for null terminator
        size_t newCapacity = mf->fileSize + len + 1;
        LARGE_INTEGER mapSizeLI;
        mapSizeLI.QuadPart = newCapacity;

        // Expand the file on disk FIRST
        LARGE_INTEGER newFileSize;
        newFileSize.QuadPart = newCapacity;
        SetFilePointerEx(mf->hFile, newFileSize, NULL, FILE_BEGIN);
        SetEndOfFile(mf->hFile);

        // Create new mapping
        mf->hMapping = CreateFileMappingA(mf->hFile, NULL, PAGE_READWRITE, mapSizeLI.HighPart, mapSizeLI.LowPart, NULL);
        if (!mf->hMapping) {
            WriteDebugLog("  [FILE APPEND ERROR] Failed to expand mapping!\n");
            return 0;
        }

        mf->pBuffer = (char *)MapViewOfFile(mf->hMapping, FILE_MAP_WRITE, 0, 0, 0);
        if (!mf->pBuffer) {
            CloseHandle(mf->hMapping);
            mf->hMapping = NULL;
            return 0;
        }
        mf->capacity = newCapacity;
        WriteDebugLog("  [FILE APPEND] Expanded capacity to %zu\n", newCapacity);
    }

    // Write the text to the buffer
    memcpy(mf->pBuffer + mf->fileSize, text, len);
    mf->fileSize += len;
    mf->pBuffer[mf->fileSize] = '\0';

    // CRITICAL: Update file size on disk AFTER writing
    LARGE_INTEGER fs;
    fs.QuadPart = mf->fileSize;
    SetFilePointerEx(mf->hFile, fs, NULL, FILE_BEGIN);
    SetEndOfFile(mf->hFile);

    WriteDebugLog("  [FILE APPEND] Wrote %zu bytes. Total Size: %zu\n", len, mf->fileSize);
    return 1;
}

static inline void FileClose(MappedFile *mf) {
    if (!mf) return;
    
    if (mf->pBuffer) {
        FlushViewOfFile(mf->pBuffer, mf->fileSize);
        UnmapViewOfFile(mf->pBuffer);
        mf->pBuffer = NULL;
    }
    
    if (mf->hMapping) {
        CloseHandle(mf->hMapping);
        mf->hMapping = NULL;
    }
    
    if (mf->hFile != INVALID_HANDLE_VALUE) {
        // Truncate to actual data size
        LARGE_INTEGER fs;
        fs.QuadPart = mf->fileSize;
        SetFilePointerEx(mf->hFile, fs, NULL, FILE_BEGIN);
        SetEndOfFile(mf->hFile);
        CloseHandle(mf->hFile);
        mf->hFile = INVALID_HANDLE_VALUE;
    }
}
static inline Line* FileGetLines(MappedFile *mf, size_t *outLineCount) {
    DWORD start = GetTickCount();
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
    WriteDebugLog("  [FILE GET LINES] Extracted %zu lines (Time: %lu ms)\n", count, GetTickCount() - start);
    return list;
}

static inline char** FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
    DWORD start = GetTickCount();
    size_t maxLines = 1000;
    size_t count = 0;
    
    char **stringList = (char **)malloc(maxLines * sizeof(char*));
    if (stringList == NULL) return NULL;

    size_t maxNewlines = 2000;
    size_t newlineCount = 0;
    NewlineInfo *newlines = (NewlineInfo *)malloc(maxNewlines * sizeof(NewlineInfo));
    if (newlines == NULL) {
        free(stringList);
        return NULL;
    }

    stringList[count++] = mf->pBuffer;
    size_t totalBytes = mf->fileSize;

    for (size_t i = 0; i < totalBytes; i++) {
        if (mf->pBuffer[i] == '\r') {
            char orig = mf->pBuffer[i];
            mf->pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlines = (NewlineInfo *)realloc(newlines, maxNewlines * sizeof(NewlineInfo));
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };
        }
        else if (mf->pBuffer[i] == '\n') {
            char orig = mf->pBuffer[i];
            mf->pBuffer[i] = '\0';
            
            if (newlineCount >= maxNewlines) {
                maxNewlines *= 2;
                newlines = (NewlineInfo *)realloc(newlines, maxNewlines * sizeof(NewlineInfo));
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };

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
    *outNewlines = newlines; 
    *outNewlineCount = newlineCount;
    WriteDebugLog("  [FILE GET STRINGS] Processed strings. Count: %zu (Time: %lu ms)\n", count, GetTickCount() - start);
    return stringList;
}

void CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    if (!newlines) return;
    for (size_t i = 0; i < newlineCount; i++) {
        mf->pBuffer[newlines[i].index] = newlines[i].original;
    }
    free(newlines);
}

static inline void SanitizeFile(const char *filePath) {
    HANDLE hFile = CreateFileA(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER fs;
    GetFileSizeEx(hFile, &fs);
    DWORD fileSize = (DWORD)fs.QuadPart;
    if (fileSize == 0) { CloseHandle(hFile); return; }

    char *buf = (char *)malloc(fileSize);
    if (!buf) { CloseHandle(hFile); return; }

    DWORD bytesRead = 0;
    ReadFile(hFile, buf, fileSize, &bytesRead, NULL);

    DWORD cleanCount = 0;
    for (DWORD i = 0; i < bytesRead; i++) {
        if (buf[i] != '\0') {
            buf[cleanCount++] = buf[i];
        }
    }

    if (cleanCount < bytesRead) {
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        WriteFile(hFile, buf, cleanCount, &bytesRead, NULL);
        SetEndOfFile(hFile);
        printf("[SANITIZE] Cleaned %lu null bytes from %s\n", bytesRead - cleanCount, filePath);
    }

    free(buf);
    CloseHandle(hFile);
}


#endif
