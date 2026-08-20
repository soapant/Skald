#include <windows.h>
#include <shlobj.h>
#include "helpers.h"
#include "platform/platform.h"
#include "config.h"
#include <shellapi.h>
#include <gdiplus.h>

char* Platform_QueryZoxidePath(const char* query) {
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
        SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_ERROR);
        return NULL;
    }
    return (strlen(path) > 0) ? path : NULL;
}

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = (async ? SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK : SEE_MASK_FLAG_NO_UI);
    sei.lpVerb = verb ? verb : "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExA(&sei);
}

bool Platform_LaunchApp(const char* path) {
    if (!path) return false;

    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return Platform_ShellExecuteWithVerb(expanded, "open", true);
}

bool Platform_SendX11Command(const char* window_title, const char* atom_name, long command) {
    (void)window_title;
    (void)atom_name;
    (void)command;
    return false;
}

bool Platform_SetX11Property(const char* window_title, const char* atom_name, long value) {
    (void)window_title;
    (void)atom_name;
    (void)value;
    return false;
}

bool Platform_SetX11StringProperty(const char* window_title, const char* atom_name, const char* value) {
    (void)window_title;
    (void)atom_name;
    (void)value;
    return false;
}

bool Platform_LaunchAppAsAdmin(const char* path) {
    if (!path) return false;

    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) return false;
    return Platform_ShellExecuteWithVerb(expanded, "runas", true);
}

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
bool Platform_IsCtrlPressed(void) {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

char Platform_GetLastChar(MappedFile *file) {
    return file->pBuffer[file->fileSize - 1];
}

int Platform_FileInit(const char *filePath, MappedFile *mf) {
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

void Platform_CleanUpFile(MappedFile *mf, NewlineInfo *newlines, size_t newlineCount) {
    if (!newlines) return;
    for (size_t i = 0; i < newlineCount; i++) {
        mf->pBuffer[newlines[i].index] = newlines[i].original;
    }
    free(newlines);
}

void Platform_SanitizeFile(const char *filePath) {
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

static ULONGLONG g_timer_start_ms = 0;

void Platform_StartUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void Platform_ResetUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    LONGLONG ms_offset = (LONGLONG)(seconds_to_add * 1000.0);
    g_timer_start_ms -= ms_offset; 
    printf("[TIMER] Modified baseline by %.2f seconds.\n", seconds_to_add);
}

double Platform_GetUniversalTimeSeconds() {
    ULONGLONG current_ms = GetTickCount64();
    
    if (current_ms < g_timer_start_ms) {
        return 0.0;
    }
    
    ULONGLONG elapsed_ms = current_ms - g_timer_start_ms;
    return (double)elapsed_ms / 1000.0;
}

int Platform_CopyToClipboard(const char* text) {
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
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_OF_SELECTION);
    CloseClipboard();
    
    return 1;
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

void Platform_SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    int width = abs(x2 - x1);
    int height = abs(y2 - y1);
    if (width == 0 || height == 0) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, (x1 < x2 ? x1 : x2), (y1 < y2 ? y1 : y2), SRCCOPY);

    GpBitmap* pBitmap = NULL;
    GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);

    if (pBitmap != NULL) {
        CLSID pngClsid;
        UINT num = 0, size = 0;
        GdipGetImageEncodersSize(&num, &size); 
        
        ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
        if (pImageCodecInfo != NULL) {
            GdipGetImageEncoders(num, size, pImageCodecInfo); 
            for (UINT j = 0; j < num; ++j) {
                if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0) {
                    pngClsid = pImageCodecInfo[j].Clsid;
                    break;
                }
            }
            free(pImageCodecInfo);
        }

        wchar_t wPath[512];
        MultiByteToWideChar(CP_ACP, 0, fullPath, -1, wPath, 512);

        GdipSaveImageToFile((GpImage*)pBitmap, wPath, &pngClsid, NULL);
        
        GdipDisposeImage((GpImage*)pBitmap);
    }

    
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}
