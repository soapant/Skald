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
        DWORD wait_res = WaitForSingleObject(pi.hProcess, 5000); 
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

    
    if (mf->fileSize + len + 1 > mf->capacity) {
        
        if (mf->pBuffer) {
            UnmapViewOfFile(mf->pBuffer);
            mf->pBuffer = NULL;
        }
        if (mf->hMapping) {
            CloseHandle(mf->hMapping);
            mf->hMapping = NULL;
        }

        
        size_t newCapacity = mf->fileSize + len + 1;
        LARGE_INTEGER mapSizeLI;
        mapSizeLI.QuadPart = newCapacity;

        
        LARGE_INTEGER newFileSize;
        newFileSize.QuadPart = newCapacity;
        SetFilePointerEx(mf->hFile, newFileSize, NULL, FILE_BEGIN);
        SetEndOfFile(mf->hFile);

        
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

    
    memcpy(mf->pBuffer + mf->fileSize, text, len);
    mf->fileSize += len;
    mf->pBuffer[mf->fileSize] = '\0';

    
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

typedef struct {
    char* flag;
    int index;
} FoundFlag;

// Parse time string like "5m", "10h", "20s" into seconds
static inline int ParseTimeToSeconds(const char* input) {
    if (!input || strlen(input) == 0) return 0;
    
    int total = 0;
    char buffer[64];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* token = strtok(buffer, " ");
    while (token != NULL) {
        int value = 0;
        char unit = 's';
        int len = strlen(token);
        
        if (len > 1) {
            char last = tolower(token[len - 1]);
            if (last == 's' || last == 'm' || last == 'h' || last == 'd') {
                token[len - 1] = '\0';
                value = atoi(token);
                unit = last;
            } else {
                value = atoi(token);
            }
        } else {
            value = atoi(token);
        }
        
        switch (unit) {
            case 's': total += value; break;
            case 'm': total += value * 60; break;
            case 'h': total += value * 3600; break;
            case 'd': total += value * 86400; break;
            default: total += value; break;
        }
        
        token = strtok(NULL, " ");
    }
    
    return total;
}
typedef void (*ActionFn)(const char* parameter);

typedef bool (*ConditionFn)(const char* parameter);

typedef void (*TimerFn)(const char* parameter);

typedef struct { char* str; ActionFn    func; } ActionDef;
typedef struct { char* str; ConditionFn func; } ConditionDef;
typedef struct { char* str; TimerFn     func; } TimerDef;

typedef struct {
    char* name;
    ConditionFn func;
} ConditionParamDef;

static inline int SearchFlagsInText(const char* text, void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    
    int match_count = 0;
    char* text_copy = _strdup(text);
    if (!text_copy) return 0;
    
    // Convert to lowercase for case-insensitive search
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    for (int i = 0; i < flag_count && match_count < max_matches; i++) {
        char* flag_str = NULL;
        
        // Extract flag string based on flag_size
        if (flag_size == sizeof(char*)) {
            flag_str = ((char**)flag_array)[i];
        } else if (flag_size == sizeof(ActionDef)) {
            flag_str = ((ActionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(ConditionDef)) {
            flag_str = ((ConditionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(TimerDef)) {
            flag_str = ((TimerDef*)flag_array)[i].str;
        }
        
        if (!flag_str) continue;
        
        char* found = strstr(text_copy, flag_str);
        if (found) {
            // === EXACT MATCH CHECK ===
            // Make sure this is a whole word, not part of another flag
            
            // Check if character before match is start of string or space
            int before_ok = (found == text_copy || *(found - 1) == ' ');
            
            // Check if character after match is end of string, space, or not a letter
            int after_ok = 1;
            char* after = found + strlen(flag_str);
            if (*after != '\0' && *after != ' ' && *after != '\t') {
                after_ok = 0;
            }
            
            if (before_ok && after_ok) {
                matches[match_count].flag = flag_str;
                matches[match_count].index = found - text_copy;
                match_count++;
            }
        }
    }
    
    free(text_copy);
    return match_count;
}

static inline char* GetFlagParameter(const char* text, int flag_index) {
    if (!text || flag_index < 0) return NULL;
    
    static char param[256];
    param[0] = '\0';
    
    // Skip past the flag
    int pos = flag_index;
    while (text[pos] && text[pos] != ' ') pos++;
    while (text[pos] && text[pos] == ' ') pos++;
    
    // Find next flag (must be at start of word, with space before or at start)
    int start = pos;
    while (text[pos]) {
        // Check if this is the start of a flag
        if (text[pos] == '-' && (pos == 0 || text[pos-1] == ' ')) {
            break;
        }
        pos++;
    }
    
    int len = pos - start;
    if (len > 0) {
        strncpy(param, text + start, len);
        param[len] = '\0';
        
        // Trim trailing spaces
        char* end = param + strlen(param) - 1;
        while (end > param && *end == ' ') end--;
        *(end + 1) = '\0';
        
        return param;
    }
    
    return NULL;
}

// Get the text without any flags (clean text)
static inline char* GetCleanText(const char* text) {
    if (!text) return NULL;
    
    static char clean[256];
    clean[0] = '\0';
    
    // List of all flags to skip
    const char* flags[] = {"-f", "-cg", "-trw", "-tr", "-t"};
    int flag_count = sizeof(flags) / sizeof(flags[0]);
    
    char* text_copy = _strdup(text);
    if (!text_copy) return NULL;
    
    char* result = text_copy;
    char* write_ptr = text_copy;
    
    while (*result) {
        int skip = 0;
        for (int i = 0; i < flag_count; i++) {
            if (strncmp(result, flags[i], strlen(flags[i])) == 0) {
                // Skip this flag and its parameter
                skip = 1;
                result += strlen(flags[i]);
                while (*result && *result == ' ') result++;
                while (*result && *result != ' ') result++;
                while (*result && *result == ' ') result++;
                break;
            }
        }
        
        if (!skip) {
            *write_ptr = *result;
            write_ptr++;
            result++;
        }
    }
    
    *write_ptr = '\0';
    
    // Trim leading/trailing spaces
    char* start = text_copy;
    while (*start == ' ') start++;
    char* end = text_copy + strlen(text_copy) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    strncpy(clean, start, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    
    free(text_copy);
    return clean;
}

static bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
        if (StrStrIA(window_title, blacklist[i]) != NULL) {
            return true;
        }
    }
    return false;
}

static ULONGLONG g_timer_start_ms = 0;

void StartUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void ResetUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void ModifyUniversalTimer(double seconds_to_add) {
    LONGLONG ms_offset = (LONGLONG)(seconds_to_add * 1000.0);
    g_timer_start_ms -= ms_offset; 
    printf("[TIMER] Modified baseline by %.2f seconds.\n", seconds_to_add);
}

double GetUniversalTimeSeconds() {
    ULONGLONG current_ms = GetTickCount64();
    
    if (current_ms < g_timer_start_ms) {
        return 0.0;
    }
    
    ULONGLONG elapsed_ms = current_ms - g_timer_start_ms;
    return (double)elapsed_ms / 1000.0;
}

#endif
