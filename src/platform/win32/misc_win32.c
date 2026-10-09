#include <windows.h>
#include "deps/log.h"
#include <tlhelp32.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "platform/platform.h"
#include "platform/platform.h"
#include "config.h"
#include <time.h>

static bool FolderToPath(int csidl, const char* subdir, char* out, size_t out_size) {
    char folder[MAX_PATH];
    if (SHGetFolderPathA(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, folder) != S_OK) return false;
    if (subdir != NULL) {
        snprintf(out, out_size, "%s\\%s", folder, subdir);
    } else {
        snprintf(out, out_size, "%s", folder);
    }
    return true;
}

static bool ResolvePathToken(const char* token, char* out, size_t out_size) {
    for (unsigned int i = 0; i < PATH_VAR_COUNT; i++) {
        if (strcmp(G_PATH_VARS[i].name, token) == 0) {
            const char* value = G_PATH_VARS[i].win32_value;
            if (value == NULL || value[0] == '\0') return false;
            snprintf(out, out_size, "%s", value);
            return true;
        }
    }

    if (strcmp(token, "HOME") == 0) return FolderToPath(CSIDL_PROFILE, NULL, out, out_size);
    if (strcmp(token, "APPDATA") == 0) return FolderToPath(CSIDL_APPDATA, NULL, out, out_size);
    if (strcmp(token, "PICTURES") == 0) return FolderToPath(CSIDL_MYPICTURES, NULL, out, out_size);
    if (strcmp(token, "DESKTOP") == 0) return FolderToPath(CSIDL_DESKTOPDIRECTORY, NULL, out, out_size);
    if (strcmp(token, "DOCUMENTS") == 0) return FolderToPath(CSIDL_MYDOCUMENTS, NULL, out, out_size);
    if (strcmp(token, "DOWNLOADS") == 0) return FolderToPath(CSIDL_PROFILE, "Downloads", out, out_size);
    if (strcmp(token, "PROGRAM_FILES") == 0) return FolderToPath(CSIDL_PROGRAM_FILES, NULL, out, out_size);
    return false;
}

bool Platform_ExpandPath(const char* template_path, char* out, size_t out_size) {
    if (template_path == NULL || out_size == 0) { log_error("operation failed, returning error template_path='%s' out='%s' out_size=%d", template_path ? template_path : "null", out ? out : "null", out_size); return false; }

    size_t o = 0;
    const char* p = template_path;
    while (*p != '\0' && o < out_size - 1) {
        if (*p == '{') {
            const char* close = strchr(p + 1, '}');
            if (close != NULL) {
                size_t token_len = (size_t)(close - p - 1);
                if (token_len > 0 && token_len < 64) {
                    char token[64];
                    memcpy(token, p + 1, token_len);
                    token[token_len] = '\0';
                    char resolved[1024];
                    if (ResolvePathToken(token, resolved, sizeof(resolved))) {
                        size_t rlen = strlen(resolved);
                        size_t room = out_size - 1 - o;
                        size_t copy = (rlen < room) ? rlen : room;
                        memcpy(out + o, resolved, copy);
                        o += copy;
                        p = close + 1;
                        continue;
                    }
                    log_info("'%s' is not set, ignoring: %s", token, template_path);
                    return false;
                }
            }
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
log_info("Platform_ExpandPath template_path='%s' out='%s' out_size=%d", template_path ? template_path : "", out ? out : "", out_size);;
    return true;
}


int Platform_FileAppend(MappedFile *mf, const char *text) {
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

log_info("Platform_FileAppend mf=%p text=%p", mf, text);;
    return 1;
}

void Platform_FileClose(MappedFile *mf) {
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
log_info("Platform_FileClose mf=%p", mf);;
}

Line* Platform_FileGetLines(MappedFile *mf, size_t *outLineCount) {
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
                size_t newMax = maxLines * 2;
                Line *tmp = (Line *)realloc(list, newMax * sizeof(Line));
                if (!tmp) { free(list); *outLineCount = 0; return NULL; }
                list = tmp;
                maxLines = newMax;
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
log_info("Platform_FileGetLines mf=%p outLineCount=%d", mf, outLineCount);;
    return list;
}

char** Platform_FileGetStrings(MappedFile *mf, size_t *outLineCount, NewlineInfo **outNewlines, size_t *outNewlineCount) {
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
                size_t newMax = maxNewlines * 2;
                NewlineInfo *tmp = (NewlineInfo *)realloc(newlines, newMax * sizeof(NewlineInfo));
                if (!tmp) { free(stringList); free(newlines); *outLineCount = 0; *outNewlines = NULL; *outNewlineCount = 0; return NULL; }
                newlines = tmp;
                maxNewlines = newMax;
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };
        }
        else if (mf->pBuffer[i] == '\n') {
            char orig = mf->pBuffer[i];
            mf->pBuffer[i] = '\0';

            if (newlineCount >= maxNewlines) {
                size_t newMax = maxNewlines * 2;
                NewlineInfo *tmp = (NewlineInfo *)realloc(newlines, newMax * sizeof(NewlineInfo));
                if (!tmp) { free(stringList); free(newlines); *outLineCount = 0; *outNewlines = NULL; *outNewlineCount = 0; return NULL; }
                newlines = tmp;
                maxNewlines = newMax;
            }
            newlines[newlineCount++] = (NewlineInfo){ .index = i, .original = orig };

            if (i + 1 < totalBytes) {
                if (count >= maxLines) {
                    size_t newMaxL = maxLines * 2;
                    char **tmp = (char **)realloc(stringList, newMaxL * sizeof(char*));
                    if (!tmp) { free(stringList); free(newlines); *outLineCount = 0; *outNewlines = NULL; *outNewlineCount = 0; return NULL; }
                    stringList = tmp;
                    maxLines = newMaxL;
                }
                stringList[count++] = &mf->pBuffer[i + 1];
            }
        }
    }

    *outLineCount = count;
    *outNewlines = newlines;
    *outNewlineCount = newlineCount;
log_info("Platform_FileGetStrings mf=%p outLineCount=%d outNewlines=%p outNewlineCount=%d", mf, outLineCount, outNewlines, outNewlineCount);;
    return stringList;
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    if (!newlines) return;
    for (size_t i = 0; i < newlineCount; i++) {
        mf->pBuffer[newlines[i].index] = newlines[i].original;
    }
    free(newlines);
log_info("Platform_CleanUpFile mf=%p newlines=%p newlineCount=%d", mf, newlines, newlineCount);;
}

bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle) { log_error("operation failed, returning error haystack='%s' needle='%s'", haystack ? haystack : "null", needle ? needle : "null"); return false; }
    return StrStrIA(haystack, needle) != NULL;
}

bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix) {
    if (!str || !prefix) { log_error("operation failed, returning error str='%s' prefix='%s'", str ? str : "null", prefix ? prefix : "null"); return false; }
    return StrStrIA(str, prefix) == str;
}

long long Platform_GetWallClockMs(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    return (long long)((t.QuadPart - 116444736000000000ULL) / 10000ULL);
}


bool Platform_TerminateProcessByName(const char* exe_name) {
    if (!exe_name || !*exe_name) {
        log_error("operation failed, returning error exe_name='%s'", exe_name ? exe_name : "null");
        return false;
    }

    char cmd[512];
    const char *ext = strrchr(exe_name, '.');
    
    if (!ext) {
        snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s.exe*\"", exe_name);
    } else {
        snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", exe_name);
    }

    Platform_RunTerminalCommand(cmd, false);
    log_info("Platform_TerminateProcessByName exe_name='%s'", exe_name);
    
    return true;
}

int Platform_ReplaceLineInMappedFile(MappedFile* mf, int line_index, const char* new_text) {
    if (!mf || !mf->pBuffer || !new_text || line_index < 0) return 0;

    size_t lineCount = 0;
    Line* lines = Platform_FileGetLines(mf, &lineCount);
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
log_info("Platform_ReplaceLineInMappedFile mf=%p line_index=%d new_text='%s'", mf, line_index, new_text ? new_text : "");;
    return 1;
}

bool Platform_LaunchBrowserAtUrl(char* url, bool new_window) {
    char full_url[2048] = {0};
    if (!url || !url[0]) full_url[0] = '\0';
    else if (strstr(url, "://")) snprintf(full_url, sizeof(full_url), "%s", url);
    else snprintf(full_url, sizeof(full_url), "https://%s", url);
    if (full_url[0] == '\0') {
        if (UsedBrowser && UsedBrowser[0]) {
            char browser_path[MAX_PATH] = {0};
            char* exe = NULL;
            if (SearchPathA(NULL, UsedBrowser, ".exe", MAX_PATH, browser_path, NULL) || SearchPathA(NULL, UsedBrowser, NULL, MAX_PATH, browser_path, NULL)) exe = browser_path;
            else { char with_exe[MAX_PATH]; snprintf(with_exe, sizeof(with_exe), "%s.exe", UsedBrowser); if (SearchPathA(NULL, with_exe, NULL, MAX_PATH, browser_path, NULL)) exe = browser_path; }
            if (exe && exe[0]) { HINSTANCE r = ShellExecuteA(NULL, "open", exe, NULL, NULL, SW_SHOWNORMAL); if ((intptr_t)r > 32) { log_info("Platform_LaunchBrowserAtUrl via %s (no url)", exe); return true; } }
            HINSTANCE r2 = ShellExecuteA(NULL, "open", UsedBrowser, NULL, NULL, SW_SHOWNORMAL);
            if ((intptr_t)r2 > 32) { log_info("Platform_LaunchBrowserAtUrl via UsedBrowser (no url)"); return true; }
        }
        log_info("Platform_LaunchBrowserAtUrl no url and no browser, refusing to launch");
        return false;
    }
    if (new_window && UsedBrowser && UsedBrowser[0]) {
        char browser_path[MAX_PATH] = {0};
        char* exe = NULL;
        if (SearchPathA(NULL, UsedBrowser, ".exe", MAX_PATH, browser_path, NULL) || SearchPathA(NULL, UsedBrowser, NULL, MAX_PATH, browser_path, NULL)) exe = browser_path;
        else { char with_exe[MAX_PATH]; snprintf(with_exe, sizeof(with_exe), "%s.exe", UsedBrowser); if (SearchPathA(NULL, with_exe, NULL, MAX_PATH, browser_path, NULL)) exe = browser_path; }
        if (exe && exe[0]) {
            char params[4096];
            snprintf(params, sizeof(params), "--new-window \"%s\"", full_url);
            HINSTANCE r = ShellExecuteA(NULL, "open", exe, params, NULL, SW_SHOWNORMAL);
            if ((intptr_t)r > 32) { log_info("Platform_LaunchBrowserAtUrl via %s new_window url='%s'", exe, full_url); return true; }
        }
        char params2[4096];
        snprintf(params2, sizeof(params2), "--new-window \"%s\"", full_url);
        HINSTANCE r2 = ShellExecuteA(NULL, "open", UsedBrowser, params2, NULL, SW_SHOWNORMAL);
        if ((intptr_t)r2 > 32) { log_info("Platform_LaunchBrowserAtUrl via UsedBrowser new_window url='%s'", full_url); return true; }
    }
    HINSTANCE r3 = ShellExecuteA(NULL, "open", full_url, NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)r3 > 32) { log_info("Platform_LaunchBrowserAtUrl via default url='%s' new_window=%d", full_url, new_window); return true; }
    char command[2048];
    snprintf(command, sizeof(command), "start \"\" \"%s\"", full_url);
    log_info("Platform_LaunchBrowserAtUrl fallback url='%s' new_window=%d", full_url, new_window);
    return Platform_RunTerminalCommand(command,false);
}

bool Platform_QueryBrowser(char* query) {
    if (!query || !query[0]) { log_error("operation failed, returning error query='%s'", query ? query : "null"); return false; }
    
    char encoded_query[2048] = {0};
    char* src = query;
    char* dst = encoded_query;
    while (*src && (dst - encoded_query) < sizeof(encoded_query) - 1) {
        if (*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
        } else if (*src == '&') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '6';
        } else if (*src == '?') {
            *dst++ = '%';
            *dst++ = '3';
            *dst++ = 'F';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    char url[4096];
    
    if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "google")) {
        snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "duckduckgo")) {
        snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "bing")) {
        snprintf(url, sizeof(url), "https://www.bing.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "brave")) {
        snprintf(url, sizeof(url), "https://search.brave.com/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "startpage")) {
        snprintf(url, sizeof(url), "https://www.startpage.com/sp/search?query=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "yandex")) {
        snprintf(url, sizeof(url), "https://yandex.com/search/?text=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "ecosia")) {
        snprintf(url, sizeof(url), "https://www.ecosia.org/search?q=%s", encoded_query);
    } else if (Platform_StringContainsCaseInsensitive(UsedSearchEngine, "qwant")) {
        snprintf(url, sizeof(url), "https://www.qwant.com/?q=%s", encoded_query);
    } else {
        snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
    }
    
    char command[4096];
    snprintf(command, sizeof(command), "start \"\" \"%s\"", url);
log_info("Platform_QueryBrowser query='%s'", query ? query : "");;
    return Platform_RunTerminalCommand(command,false);
}

char* Platform_GetClipboardEntry(void) {
    char* result = NULL;

    if (!OpenClipboard(NULL)) { log_error("operation failed, returning error"); return NULL; }

    HANDLE hClipData = GetClipboardData(CF_UNICODETEXT);
    if (hClipData == NULL) {
        CloseClipboard();
        return NULL;
    }

    wchar_t* pClipText = (wchar_t*)GlobalLock(hClipData);
    if (pClipText == NULL) {
        CloseClipboard();
        return NULL;
    }

    int wideCharLen = wcslen(pClipText);
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pClipText, wideCharLen, NULL, 0, NULL, NULL);

    if (utf8Len > 0) {
        result = (char*)malloc(utf8Len + 1);
        if (result != NULL) {
            WideCharToMultiByte(CP_UTF8, 0, pClipText, wideCharLen, result, utf8Len, NULL, NULL);
            result[utf8Len] = '\0';
        }
    }

    GlobalUnlock(hClipData);
    CloseClipboard();

    return result;
}

bool Platform_PasteContent(char* content) {
    if (!content || !content[0]) { log_error("operation failed, returning error content='%s'", content ? content : "null"); return false; }

    size_t len = strlen(content);
    INPUT* inputs = (INPUT*)calloc(len, sizeof(INPUT));
    if (!inputs) return false;

    for (size_t i = 0; i < len; i++) {
        char c = content[i];
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = 0;           
        inputs[i].ki.wScan = c;         
        inputs[i].ki.dwFlags = KEYEVENTF_SCANCODE;  
        inputs[i].ki.time = 0;
        inputs[i].ki.dwExtraInfo = 0;
    }

    UINT result = SendInput((UINT)len, inputs, sizeof(INPUT));

    bool success = (result == len);
    if (success) {
        INPUT* up_inputs = (INPUT*)calloc(len, sizeof(INPUT));
        if (!up_inputs) {
            free(inputs);
            return false;
        }

        for (size_t i = 0; i < len; i++) {
            up_inputs[i] = inputs[i];
            up_inputs[i].ki.dwFlags |= KEYEVENTF_KEYUP;
        }

        result = SendInput((UINT)len, up_inputs, sizeof(INPUT));
        success = (result == len);
        free(up_inputs);
    }

    free(inputs);
log_info("Platform_PasteContent content='%s'", content ? content : "");;
    return success;
}
const char* Platform_GetRealHome(void) {
    static char buf[512] = {0};

    const char* profile = getenv("USERPROFILE");
    if (profile && profile[0]) {
        strncpy(buf, profile, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    const char* drive = getenv("HOMEDRIVE");
    const char* path  = getenv("HOMEPATH");
    if (drive && drive[0] && path && path[0]) {
        snprintf(buf, sizeof(buf), "%s%s", drive, path);
        return buf;
    }

    char user_profile[MAX_PATH];
    if (GetEnvironmentVariableA("USERPROFILE", user_profile, MAX_PATH) > 0) {
        strncpy(buf, user_profile, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    return buf;
}
