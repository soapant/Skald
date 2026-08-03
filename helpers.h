#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>  
#include <string.h> 
#include <shlwapi.h>
#include <stdlib.h>
#include <assert.h>
#include "types.h"
#include "config.h"
#include "common_ui.h"
#define MAX_EXE_CACHE 128

static inline void LOG(const char* fmt, ...) {
    if (!GetConsoleWindow()) return;
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

typedef void (*HotkeyHandler)(void);

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;
    int blacklist_count;
    HotkeyHandler handler;
    const char* name;
    bool condition;
} HotkeyBinding;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,COLORREF color) {
    if (MessagesEnabled) {
        if (color == COLOR_OF_SELECTION) {
            if (UI_IsListBoxSelected()) {
              DisplayMessageText = COLOR_OF_SELECTION;
            } else {
              DisplayMessageText = COLOR_TEXT;
            }
        }
        else if (color) {
            DisplayMessageText = color;
        }
        else {
            DisplayMessageText = COLOR_TEXT;
        }
        if (hPopupWnd != NULL) {
          MSG_TOP(buffer,msg_height,timeout);
        }else {
          MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout);
        }
        DisplayMessageText = COLOR_TEXT;
    }
}

static inline bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;
        
        if (!b->condition) {
            continue;
        }
        
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
        
        if (IsPopupActive(hPopupWnd)) return false;
        
        if (b->handler) {
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


    char command[512];
    snprintf(command, sizeof(command), "zoxide query %s", clean_query);

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe, hWritePipe;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return NULL;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {0};

    DWORD proc_start = GetTickCount();
    if (CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);

        DWORD wait_res = WaitForSingleObject(pi.hProcess, 5000); 

        if (wait_res == WAIT_TIMEOUT) {
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
        CloseHandle(hWritePipe);
    }

    CloseHandle(hReadPipe);
    if (strstr(path, "no match found")) {
        char buffer[256];
        snprintf(buffer,sizeof(buffer),"Query {%s} Doesnt Have A Zoxide Path",query);
        DisplayMessageText = RGB(255,0,0);
        MSG_BOTTOM(buffer,140,1);
        return NULL;
    }
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

    mf->hFile = CreateFileA(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->hFile == INVALID_HANDLE_VALUE) {
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
            return 0;
        }

        mf->pBuffer = (char *)MapViewOfFile(mf->hMapping, FILE_MAP_WRITE, 0, 0, 0);
        if (!mf->pBuffer) {
            CloseHandle(mf->hMapping);
            mf->hMapping = NULL;
            return 0;
        }
        mf->capacity = newCapacity;
    }

    
    memcpy(mf->pBuffer + mf->fileSize, text, len);
    mf->fileSize += len;
    mf->pBuffer[mf->fileSize] = '\0';

    
    LARGE_INTEGER fs;
    fs.QuadPart = mf->fileSize;
    SetFilePointerEx(mf->hFile, fs, NULL, FILE_BEGIN);
    SetEndOfFile(mf->hFile);

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

static inline int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    
    int match_count = 0;
    char* text_copy = _strdup(text);
    if (!text_copy) return 0;
    
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    for (int i = 0; i < flag_count && match_count < max_matches; i++) {
        const char* flag_str = NULL;
        
        if (flag_size == sizeof(char*)) {
            flag_str = ((char**)flag_array)[i];
        } else if (flag_size == sizeof(ActionDef)) {
            flag_str = ((ActionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(ConditionDef)) {
            flag_str = ((ConditionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(TimerDef)) {
            flag_str = ((TimerDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(GenericFlagEntry)) {
            flag_str = ((const GenericFlagEntry*)flag_array)[i].flag;
        }
        
        if (!flag_str) continue;
        
        char* found = strstr(text_copy, flag_str);
        if (found) {
            int before_ok = (found == text_copy || *(found - 1) == ' ');
            
            int after_ok = 1;
            char* after = found + strlen(flag_str);
            if (*after != '\0' && *after != ' ' && *after != '\t') {
                after_ok = 0;
            }
            
            if (before_ok && after_ok) {
                matches[match_count].flag = (char*)flag_str;
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
    
    int pos = flag_index;
    while (text[pos] && text[pos] != ' ') pos++;
    while (text[pos] && text[pos] == ' ') pos++;
    
    int start = pos;
    while (text[pos]) {
        if (text[pos] == '-' && (pos == 0 || text[pos-1] == ' ')) {
            break;
        }
        pos++;
    }
    
    int len = pos - start;
    if (len > 0) {
        strncpy(param, text + start, len);
        param[len] = '\0';
        
        char* end = param + strlen(param) - 1;
        while (end > param && *end == ' ') end--;
        *(end + 1) = '\0';
        
        return param;
    }
    
    return NULL;
}

static inline char* GetCleanText(const char* text) {
    if (!text) return NULL;
    
    static char clean[256];
    clean[0] = '\0';
    
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

int CopyToClipboard(const char* text) {
    if (text == NULL) return 0;

    size_t len = strlen(text);
    if (len == 0) return 0;

    char* clean_text = (char*)malloc(len + 1);
    if (clean_text == NULL) return 0;

    size_t write_idx = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '\r') {
            clean_text[write_idx++] = text[i];
        }
    }
    clean_text[write_idx] = '\0';

    len = write_idx;
    if (len == 0) {
        free(clean_text);
        return 0;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (hMem == NULL) {
        free(clean_text);
        return 0;
    }

    char* pMem = (char*)GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        free(clean_text);
        return 0;
    }
    
    memcpy(pMem, clean_text, len + 1); 
    GlobalUnlock(hMem);
    free(clean_text);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hMem);
        return 0;
    }

    if (!EmptyClipboard()) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    if (SetClipboardData(CF_TEXT, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }
    char buffer[256];
    
    snprintf(buffer,sizeof(buffer),"Copied %s",text);
    SubjectiveMessageDisplay(buffer,140,1,COLOR_OF_SELECTION);
    CloseClipboard();
    
    return 1;
}
static unsigned int hash(const char* key, int size) {
    unsigned int h = 0;
    while (*key) {
        h = (h * 31) ^ (unsigned char)*key;
        key++;
    }
    return h % size;
}

static inline HashMap* HashMap_Create(int initial_size) {
    HashMap* map = malloc(sizeof(HashMap));
    if (!map) return NULL;
    
    map->size = initial_size ? initial_size : 32;
    map->count = 0;
    map->buckets = calloc(map->size, sizeof(HashMapEntry*));
    
    return map;
}

static inline void HashMap_Destroy(HashMap* map) {
    if (!map) return;
    
    for (int i = 0; i < map->size; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

static inline void HashMap_Put(HashMap* map, const char* key, void* value) {
    if (!map || !key) return;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    HashMapEntry* new_entry = malloc(sizeof(HashMapEntry));
    new_entry->key = _strdup(key);
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->count++;
}

static inline void* HashMap_Get(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

static inline bool HashMap_Contains(HashMap* map, const char* key) {
    return HashMap_Get(map, key) != NULL;
}

static inline void* HashMap_Remove(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash(key, map->size);
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            
            void* value = entry->value;
            free(entry->key);
            free(entry);
            map->count--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return NULL;
}

static inline char** HashMap_GetKeys(HashMap* map, int* count) {
    if (!map) return NULL;
    
    char** keys = malloc(map->count * sizeof(char*));
    int idx = 0;
    
    for (int i = 0; i < map->size; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            keys[idx++] = entry->key;
            entry = entry->next;
        }
    }
    
    *count = map->count;
    return keys;
}
static inline char** StringList_Append(char** list, int* count, const char* str) {
    if (!str) return list;
    
    list = realloc(list, (*count + 2) * sizeof(char*));
    if (!list) return NULL;
    
    list[*count] = _strdup(str);
    (*count)++;
    list[*count] = NULL;  
    
    return list;
}

static inline void StringList_Free(char** list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

static inline int HashMap_Size(HashMap* map) {
    return map ? map->count : 0;
}

static inline int UI_LookupTrueIndex(const char* name) {
    if (!g_item_index_map || !name) return -1;
    size_t value = (size_t)HashMap_Get(g_item_index_map, name);
    return value > 0 ? (int)(value - 1) : -1;
}

static inline void SaveFeatureHistory(char* string) {
    char** HistoryList = NULL;
    int count = 0;
    
    HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    while (HistoryList && HistoryList[count]) count++;
    
    HistoryList = StringList_Append(HistoryList, &count, string);
    
    HashMap_Put(feature_history, g_current_feature, HistoryList);
}

static inline struct feature_data* GetFeatureData(const char* name) {
    if (!feature_data) {
        feature_data = HashMap_Create(32);
    }
    
    struct feature_data* data = (struct feature_data*)HashMap_Get(feature_data, name);
    
    if (!data) {
        data = malloc(sizeof(struct feature_data));
        if (data) {
            memset(data, 0, sizeof(struct feature_data));
            data->name = _strdup(name);
            HashMap_Put(feature_data, name, data);
            
            struct feature_data* verify = (struct feature_data*)HashMap_Get(feature_data, name);
        }
    }
    return data;
}

static inline void GetFeatureCallbacks(const char* name,
                                       RefreshCallback* refresh,
                                       EnterCallback* enter,
                                       CtrlEnterCallback* ctrl_enter,
                                       TabCallback* tab,
                                       RenderCallback* render,
                                       EditCallback* edit,
                                       FilterCallback* filter) {
    struct feature_data* data = GetFeatureData(name);
    if (data) {
        if (refresh) *refresh = data->refresh_callback;
        if (enter) *enter = data->enter_callback;
        if (ctrl_enter) *ctrl_enter = data->ctrl_enter_callback;
        if (tab) *tab = data->tab_callback;
        if (render) *render = data->render_callback;
        if (edit) *edit = data->edit_callback;
        if (filter) *filter = data->filter_callback;
    }
}
static int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text) {
    if (!filepath || !new_text || line_index < 0) {
        return 0;
    }
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return 0;
    }
    
    char lines[100][512];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        count++;
    }
    fclose(f);
    
    if (line_index >= count) {
        return 0;
    }
    
    
    strncpy(lines[line_index], new_text, sizeof(lines[line_index]) - 1);
    lines[line_index][sizeof(lines[line_index]) - 1] = '\0';
    
    f = fopen(filepath, "w");
    if (!f) {
        return 0;
    }
    
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    
    return 1;
}
static int ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
    if (!mf || !mf->pBuffer || !new_text || line_index < 0) return 0;
    
    size_t lineCount = 0;
    Line* lines = FileGetLines(mf, &lineCount);
    if (!lines) return 0;
    
    if (line_index >= lineCount) {
        free(lines);
        return 0;
    }
    
    size_t new_size = 0;
    for (size_t i = 0; i < lineCount; i++) {
        if (i == line_index) {
            new_size += strlen(new_text) + 1; 
        } else {
            new_size += lines[i].length + 1; 
        }
    }
    
    char* new_buffer = (char*)malloc(new_size + 1);
    if (!new_buffer) {
        free(lines);
        return 0;
    }
    
    char* ptr = new_buffer;
    for (size_t i = 0; i < lineCount; i++) {
        if (i == line_index) {
            int len = strlen(new_text);
            memcpy(ptr, new_text, len);
            ptr += len;
        } else {
            memcpy(ptr, lines[i].ptr, lines[i].length);
            ptr += lines[i].length;
        }
        *ptr = '\n';
        ptr++;
    }
    *ptr = '\0';
    
    HANDLE hFile = mf->hFile;
    
    if (mf->pBuffer) {
        FlushViewOfFile(mf->pBuffer, mf->fileSize);
        UnmapViewOfFile(mf->pBuffer);
        mf->pBuffer = NULL;
    }
    if (mf->hMapping) {
        CloseHandle(mf->hMapping);
        mf->hMapping = NULL;
    }
    
    LARGE_INTEGER newFileSize;
    newFileSize.QuadPart = new_size;
    SetFilePointerEx(hFile, newFileSize, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    
    LARGE_INTEGER mapSizeLI;
    mapSizeLI.QuadPart = new_size;
    mf->hMapping = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 
                                       mapSizeLI.HighPart, mapSizeLI.LowPart, NULL);
    if (!mf->hMapping) {
        free(lines);
        free(new_buffer);
        return 0;
    }
    
    mf->pBuffer = (char*)MapViewOfFile(mf->hMapping, FILE_MAP_WRITE, 0, 0, 0);
    if (!mf->pBuffer) {
        CloseHandle(mf->hMapping);
        mf->hMapping = NULL;
        free(lines);
        free(new_buffer);
        return 0;
    }
    
    memcpy(mf->pBuffer, new_buffer, new_size);
    mf->fileSize = new_size;
    
    free(lines);
    free(new_buffer);
    return 1;
}
static void WriteToCacheIndex(int index, const char* text) {
    if (index < 0 || index >= MAX_ITEMS || !text) return;
    
    strncpy(g_populated_cache[index], text, sizeof(g_populated_cache[index]) - 1);
    g_populated_cache[index][sizeof(g_populated_cache[index]) - 1] = '\0';
}

int ReadMdFile(char lines[100][512], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), f) && count < 100) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (strlen(buffer) > 0) {
            strncpy(lines[count], buffer, 511);
            lines[count][511] = '\0';
            count++;
        }
    }
    fclose(f);
    return count;
}
void StripCheckboxes(char* str) {
    char* read_ptr = str;
    char* write_ptr = str;
    bool is_first = true;

    while (*read_ptr != '\0') {
        if (strncmp(read_ptr, "- [ ] ", 6) == 0) {
            if (!is_first) {
                *write_ptr = ','; 
                write_ptr++;
            }
            is_first = false; 
            read_ptr += 6;
        } else {
            *write_ptr = *read_ptr;
            write_ptr++;
            read_ptr++;
        }
    }
    *write_ptr = '\0';
}

void WriteMdFile(char lines[100][512], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}
HashMap* history_indices = NULL;

bool GetFromHistory() {
    char** HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    if (HistoryList) {
        int* idx_ptr = (int*)HashMap_Get(history_indices, g_current_feature);
        int idx = idx_ptr ? *idx_ptr : 0;
        
        int count = 0;
        while (HistoryList[count]) count++;
        
        if (count > 0) {
            idx = (idx - 1 + count) % count;
            printf("IDX IS %i \n",idx);
            for (int i = 0;i < idx ;i++) {
              printf("CURRENT TEXT IS %s \n",HistoryList[idx]);
            }
            
            UI_SetText(hEditControl, HistoryList[idx]);
            UI_SetCursorPosition(hEditControl,strlen(HistoryList[idx]));

            if (!IsNormalMode()) {
                if (g_refresh_callback != NULL) {
                    g_refresh_callback(HistoryList[idx], hListBox);
                }
            }
            if (!idx_ptr) {
                idx_ptr = malloc(sizeof(int));
                HashMap_Put(history_indices, g_current_feature, idx_ptr);
            }
            *idx_ptr = idx;
            return true;
        } 
    }
    return false;
}

int convert_string_to_int(const char *str) {
    if (*str == '\0') return 0;
    int num = 0;
    for (int i = 0;i < strlen(str);i++) {
        num *= 10;
        num += str[i] - '0';
    }
    return num;
}

void HandleCommandExecution(char* string) {
    int num = convert_string_to_int(string);
    printf("SET SELECTON TO %i \n",num);
    UI_SetSelection(hListBox,num - 1,g_SelectedIndex);
    UI_LB_SetCaretIndex(hListBox, num, TRUE);
}

#endif
