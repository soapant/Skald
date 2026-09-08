#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "helpers.h"
#include "core/platform.h"
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
            const char* value =
#if defined(_WIN32)
                G_PATH_VARS[i].win32_value;
#else
                G_PATH_VARS[i].linux_value;
#endif
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
    if (template_path == NULL || out_size == 0) return false;

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
                    fprintf(stderr, "[PATH] '%s' is not set, ignoring: %s\n", token, template_path);
                    return false;
                }
            }
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return true;
}


char Platform_GetLastChar(MappedFile *file) {
    return file->pBuffer[file->fileSize - 1];
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
    return stringList;
}

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    if (!newlines) return;
    for (size_t i = 0; i < newlineCount; i++) {
        mf->pBuffer[newlines[i].index] = newlines[i].original;
    }
    free(newlines);
}

bool Platform_StringContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    return StrStrIA(haystack, needle) != NULL;
}

bool Platform_StringStartsWithCaseInsensitive(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    return StrStrIA(str, prefix) == str;
}

int Platform_CollectRunningProcesses(char process_list[][128], int max_count) {
    if (!process_list || max_count <= 0) return 0;
    int count = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe)) {
        do {
            bool exists = false;
            for (int i = 0; i < count; i++) {
                if (StrCmpIA(process_list[i], pe.szExeFile) == 0) { exists = true; break; }
            }
            if (!exists && count < max_count) {
                strncpy(process_list[count], pe.szExeFile, 127);
                process_list[count][127] = '\0';
                count++;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return count;
}

bool Platform_TerminateProcessByName(const char* exe_name) {
    if (!exe_name || !*exe_name) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "taskkill /f /im \"%s*\"", exe_name);
    Platform_RunTerminalCommand(cmd, false);
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
    return 1;
}

bool Platform_LaunchBrowserAtUrl(char* url, bool new_window) {
    char command[2048];
    
    if (new_window) {
        snprintf(command, sizeof(command), "start \"\" \"%s\" --new-window \"%s\"",UsedBrowser,url);
    } else {
        snprintf(command, sizeof(command), "start \"\" \"%s\"", url);
    }
    
    return system(command) == 0;
}

bool Platform_QueryBrowser(char* query) {
    if (!query || !query[0]) return false;
    
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
    
    snprintf(url, sizeof(url), "https://duckduckgo.com/?q=%s", encoded_query);
    
    char command[4096];
    snprintf(command, sizeof(command), "start \"\" \"%s\"", url);
    return system(command) == 0;
}

char* Platform_GetLastClipboardEntry(void) {
    char* result = NULL;

    if (!OpenClipboard(NULL)) {
        return NULL;
    }

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
    if (!content || !content[0]) return false;

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
    return success;
}
