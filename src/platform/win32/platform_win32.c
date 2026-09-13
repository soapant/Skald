#include <windows.h>
#include "deps/log.h"
#include <fileapi.h>
#include <handleapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/platform.h"
#include "config.h"
#include <shellapi.h>
#include <gdiplus.h>

bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args){
      if (!working_dir) { log_error("working_dir is NULL extra_args='%s'", extra_args ? extra_args : "null"); return false; }
      if (!UsedTerminal || !UsedTerminal[0]) { log_error("UsedTerminal not configured working_dir='%s' extra_args='%s'", working_dir ? working_dir : "null", extra_args ? extra_args : "null"); return false; }
      char terminal[512];
      snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
      HINSTANCE res = ShellExecuteA(NULL, "open", terminal, extra_args, working_dir, SW_SHOWNORMAL);
      if ((intptr_t)res <= 32) {
          log_error("ShellExecute failed for terminal '%s' (extra_args='%s', dir='%s') err=%ld - check UsedTerminal config (foot vs alacritty, ensure .exe exists in PATH)", terminal, extra_args ? extra_args : "", working_dir, (long)(intptr_t)res);
          return false;
      }
log_info("Platform_LaunchTerminalAtPath working_dir='%s' extra_args='%s'", working_dir ? working_dir : "", extra_args ? extra_args : "");;
      return true;
}

char* Platform_RunTerminalCommand(char* command,bool open_terminal) {
    if (command == NULL || strlen(command) == 0) { log_error("operation failed, returning error command='%s' open_terminal=%d", command ? command : "null", open_terminal); return NULL; }
    if (open_terminal) {
        char clean_cmd[512];
        strncpy(clean_cmd, command, sizeof(clean_cmd) - 1);
        clean_cmd[sizeof(clean_cmd) - 1] = '\0';
        char temp_file[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_file);
        strcat(temp_file, "temp_cmd.sh");
        FILE* f = fopen(temp_file, "w");
        if (f) {
            fprintf(f, "#!/bin/%s\n",UsedShell);
            fprintf(f, "%s\n", clean_cmd);
            fclose(f);
        }
        char terminal_exe[512];
        snprintf(terminal_exe, sizeof(terminal_exe), "%s.exe", UsedTerminal);
        char args[2048];
        snprintf(args, sizeof(args), "-e %s \"%s\"",UsedShell,temp_file);
        HINSTANCE res2 = ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
        if ((intptr_t)res2 <= 32) { log_error("Platform_RunTerminalCommand(open_terminal): ShellExecute failed for '%s' args='%s' err=%ld - check UsedTerminal", terminal_exe, args, (long)(intptr_t)res2); }
        return strdup("");
    }
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) { log_error("CreatePipe failed err=%lu", GetLastError()); return strdup(""); }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi = {0};
    char cmdLine[4096];
    snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
    BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { log_error("CreateProcess failed for '%s' err=%lu", cmdLine, GetLastError()); CloseHandle(hRead); return strdup(""); }
    WaitForSingleObject(pi.hProcess, 10000);
    char* out = NULL;
    size_t cap = 8192;
    size_t len = 0;
    out = (char*)malloc(cap);
    if (!out) {
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return strdup("");
    }
    DWORD bytesRead = 0;
    while (ReadFile(hRead, out + len, (DWORD)(cap - len - 1), &bytesRead, NULL) && bytesRead > 0) {
        len += bytesRead;
        if (len + 1024 > cap) {
            cap *= 2;
            char* n = (char*)realloc(out, cap);
            if (!n) break;
            out = n;
        }
    }
    out[len] = '\0';
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    log_info("ran terminal command '%s' open_terminal=%d", command, open_terminal);
    return out;
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
    if (!path || !path[0]) { log_error("path is NULL args='%s'", args ? args : "null"); return false; }
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"%s.exe",UsedFileManager);
    HINSTANCE res = ShellExecuteA(NULL, "open", buffer, path ? path : "", NULL, SW_SHOWNORMAL);
    if ((intptr_t)res <= 32) { log_error("ShellExecute failed for filemanager '%s' path='%s' err=%ld", buffer, path, (long)(intptr_t)res); return false; }
    //temporarily not being used
    (void)args;
log_info("Platform_LaunchFileManagerAtPath path='%s' args='%s'", path ? path : "", args ? args : "");;
    return true;
}

char* Platform_QueryZoxidePath(const char* query) {
    DWORD start_time = GetTickCount();

    char path[1024];
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
                log_error("operation failed, returning error query='%s'", query ? query : "null");
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
    if (strlen(path) == 0) return NULL;
    char* dup = _strdup(path);
log_info("Platform_QueryZoxidePath query='%s'", query ? query : "");;
    return dup;
}

bool Platform_ShellExecuteWithVerb(const char* path, const char* verb, bool async) {
    if (!path || !path[0]) { log_error("path is NULL/empty verb='%s' async=%d", verb ? verb : "null", async); return false; }
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOA);
    sei.fMask = (async ? SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK : SEE_MASK_FLAG_NO_UI);
    sei.lpVerb = verb ? verb : "open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    BOOL ok = ShellExecuteExA(&sei);
    if (!ok) {
        log_error("ShellExecuteEx failed for '%s' verb='%s' err=%lu", path, verb ? verb : "open", GetLastError());
        return false;
    }
log_info("Platform_ShellExecuteWithVerb path='%s' verb='%s' async=%d", path ? path : "", verb ? verb : "", async);;
    return true;
}

bool Platform_LaunchApp(const char* path) {
    if (!path) { log_error("path is NULL"); return false; }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { log_error("Platform_ExpandPath failed for '%s'", path); return false; }
    if (!Platform_ShellExecuteWithVerb(expanded, "open", true)) { log_error("ShellExecute failed for '%s' (expanded '%s')", path, expanded); return false; }
log_info("Platform_LaunchApp path='%s'", path ? path : "");;
    return true;
}

bool Platform_LaunchAppAsAdmin(const char* path) {
    if (!path) { log_error("path is NULL"); return false; }
    char expanded[MAX_PATH];
    if (!Platform_ExpandPath(path, expanded, sizeof(expanded))) { log_error("Platform_ExpandPath failed for '%s'", path); return false; }
    if (!Platform_ShellExecuteWithVerb(expanded, "runas", true)) { log_error("ShellExecute (runas) failed for '%s' (expanded '%s')", path, expanded); return false; }
log_info("Platform_LaunchAppAsAdmin path='%s'", path ? path : "");;
    return true;
}

bool Platform_IsCtrlPressed(void) {
log_info("Platform_IsCtrlPressed completed");
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

static ULONGLONG g_timer_start_ms = 0;

void Platform_StartUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
log_info("Platform_StartUniversalTimer completed");
}

void Platform_ResetUniversalTimer() {
    g_timer_start_ms = GetTickCount64();
log_info("Platform_ResetUniversalTimer completed");
}

void Platform_ModifyUniversalTimer(double seconds_to_add) {
    LONGLONG ms_offset = (LONGLONG)(seconds_to_add * 1000.0);
    g_timer_start_ms -= ms_offset; 
    log_info("Modified baseline by %.2f seconds.", seconds_to_add);
log_info("Platform_ModifyUniversalTimer seconds_to_add=%p", seconds_to_add);;
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
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    CloseClipboard();
    
log_info("Platform_CopyToClipboard text='%s'", text ? text : "");;
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
    log_info("Platform_SaveRegionToPNG x1=%d y1=%d x2=%d y2=%d fullPath='%s'", x1, y1, x2, y2, fullPath ? fullPath : "");;
}

FontHandle Platform_CreateFont(int size, const char* face_name) {
    int h = -size;
    static bool s_font_loaded = false;
    if (!s_font_loaded) {
        s_font_loaded = true;
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("resources\\*.ttf", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char path[MAX_PATH]; snprintf(path, sizeof(path), "resources\\%s", fd.cFileName);
                AddFontResourceExA(path, FR_PRIVATE, 0);
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
        hFind = FindFirstFileA("data\\*.ttf", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char path[MAX_PATH]; snprintf(path, sizeof(path), "data\\%s", fd.cFileName);
                AddFontResourceExA(path, FR_PRIVATE, 0);
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    HFONT hf = CreateFontA(
        h, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        face_name
        );
    if (hf) {
        HDC hdc = GetDC(NULL);
        HFONT old = (HFONT)SelectObject(hdc, hf);
        char actual[64] = {0};
        GetTextFaceA(hdc, sizeof(actual), actual);
        SelectObject(hdc, old);
        ReleaseDC(NULL, hdc);
        if (actual[0] && face_name && _stricmp(actual, face_name) != 0) {
            const char* fallbacks[] = {"Cascadia Code", "Cascadia Mono", "Consolas", "Segoe UI", NULL};
            for (int i=0; fallbacks[i]; i++) {
                if (_stricmp(actual, fallbacks[i])==0) break; 
                HFONT hf2 = CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fallbacks[i]);
                if (hf2) {
                    HDC hdc2 = GetDC(NULL);
                    HFONT old2 = (HFONT)SelectObject(hdc2, hf2);
                    char act2[64]={0}; GetTextFaceA(hdc2, sizeof(act2), act2);
                    SelectObject(hdc2, old2); ReleaseDC(NULL, hdc2);
                    if (_stricmp(act2, fallbacks[i])==0) { DeleteObject(hf); hf = hf2; break; }
                    DeleteObject(hf2);
                }
            }
        }
    }
    log_info("Platform_CreateFont size=%d face_name='%s'", size, face_name ? face_name : "");;
    return FH_FROM_HFONT(hf);
}

void Platform_DestroyFont(FontHandle font) {
    if (FH_TO_HFONT(font)) {
        DeleteObject(FH_TO_HFONT(font));
    }
    log_info("Platform_DestroyFont font=%p", (void*)font);;
}

void Platform_GetTextExtents(FontHandle font, const char* text, int maxWidth, int* outWidth, int* outHeight) {
    if (!FH_TO_HFONT(font) || !text) {
        *outWidth = 0;
        *outHeight = 0;
        return;
    }
    HDC hdc = CreateCompatibleDC(NULL);
    HFONT hOldFont = (HFONT)SelectObject(hdc, FH_TO_HFONT(font));
    RECT rect = { 0, 0, maxWidth, 0x7FFFFFFF };
    DrawTextA(hdc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK);
    SelectObject(hdc, hOldFont);
    DeleteDC(hdc);
    *outWidth = rect.right;
    *outHeight = rect.bottom;
    log_info("Platform_GetTextExtents font=%p text='%s' maxWidth=%d outWidth=%d outHeight=%d", (void*)font, text ? text : "", maxWidth, outWidth, outHeight);;
}

long Platform_GetFileSize(char *filepath) {
    FILE *fp = fopen(filepath, "rb"); 
    if (fp == NULL) {
        log_error("operation failed, returning error filepath='%s'", filepath ? filepath : "null");
          return -1; 
    }

    if (fseek(fp, 0L, SEEK_END) != 0) { log_error("fseek failed for '%s' err=%s", filepath, strerror(errno)); fclose(fp); return -1; }

    long size = ftell(fp);

    fclose(fp);
    log_info("Platform_GetFileSize filepath=%p", filepath);;
    return size;
}


typedef void (*DirChangedCallback)();

typedef struct {
    char dir_path[256];
    DirChangedCallback callback;
} WatcherArgs;

DWORD WINAPI Platform_WatchWorker(LPVOID arg) {
    WatcherArgs *args = (WatcherArgs*)arg;

    HANDLE hDir = CreateFileA(
        args->dir_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        free(args);
        return 1;
    }

    BYTE buffer[4096];
    DWORD bytesReturned;

    while (1) {
        BOOL success = ReadDirectoryChangesW(
            hDir,
            &buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
            &bytesReturned,
            NULL,
            NULL
        );

        if (!success || bytesReturned == 0) {
            break;
        }

        args->callback();
    }

    CloseHandle(hDir);
    free(args);
    return 0;
}

char* Platform_RegisterDirWatcher(const char* dir_path, void (*callback)(void)) {
    WatcherArgs *args = malloc(sizeof(WatcherArgs));
    if (!args) {
        return NULL;
    }

    snprintf(args->dir_path, sizeof(args->dir_path), "%s", dir_path);
    args->callback = callback;

    HANDLE hThread = CreateThread(
        NULL,                   
        0,                      
        Platform_WatchWorker,   
        args,                   
        0,                      
        NULL                    
    );

    if (hThread != NULL) {
        CloseHandle(hThread);
        return NULL;
    }

    free(args);
    log_info("Registering Directory Watcher");
    return NULL;
}

#define MAX_C_WATCH 130
typedef struct { char path[256]; char file[256]; FILETIME lastWrite; bool inited; } CWatch;
static CWatch g_c_watches[MAX_C_WATCH];
static int g_c_watch_count = 0;
static char g_c_watch_dir[256] = {0};
static char g_last_c_file[256] = {0};
static DWORD g_last_c_tick = 0;

char* Platform_WatchForCFileChanges(const char* dir_path, void (*callback)(void)) {
    if (!dir_path) return NULL;
    const char* watch_dir = dir_path;
    if (!watch_dir || !watch_dir[0]) watch_dir = FeaturePath ? FeaturePath : "src/features";

    bool found_dir = false;
    for (int i = 0; i < g_c_watch_count; i++) if (strcmp(g_c_watches[i].path, watch_dir) == 0) { found_dir = true; break; }
    if (!found_dir && g_c_watch_count == 0) {
        strncpy(g_c_watch_dir, watch_dir, sizeof(g_c_watch_dir)-1);

        char pattern[MAX_PATH];
        snprintf(pattern, sizeof(pattern), "%s\\*.c", watch_dir);
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (g_c_watch_count >= MAX_C_WATCH) break;
                strncpy(g_c_watches[g_c_watch_count].path, watch_dir, sizeof(g_c_watches[0].path)-1);
                strncpy(g_c_watches[g_c_watch_count].file, fd.cFileName, sizeof(g_c_watches[0].file)-1);
                g_c_watches[g_c_watch_count].lastWrite = fd.ftLastWriteTime;
                g_c_watches[g_c_watch_count].inited = true;
                g_c_watch_count++;
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
        return NULL;
    }

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.c", watch_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return NULL;
    do {
        bool found = false;
        for (int i = 0; i < g_c_watch_count; i++) {
            if (strcmp(g_c_watches[i].path, watch_dir) == 0 && strcmp(g_c_watches[i].file, fd.cFileName) == 0) {
                found = true;
                if (CompareFileTime(&fd.ftLastWriteTime, &g_c_watches[i].lastWrite) != 0) {
                    DWORD now = GetTickCount();
                    bool debounce = (strcmp(g_last_c_file, fd.cFileName)==0 && (now - g_last_c_tick) < 1500);
                    g_c_watches[i].lastWrite = fd.ftLastWriteTime;
                    if (debounce) {
                        break;
                    }
                    strncpy(g_last_c_file, fd.cFileName, sizeof(g_last_c_file)-1);
                    g_last_c_tick = now;
                    if (callback) callback();
                    char* ret = _strdup(fd.cFileName);
                    FindClose(hFind);
                    return ret;
                }
                break;
            }
        }
        if (!found && g_c_watch_count < MAX_C_WATCH) {
            strncpy(g_c_watches[g_c_watch_count].path, watch_dir, sizeof(g_c_watches[0].path)-1);
            strncpy(g_c_watches[g_c_watch_count].file, fd.cFileName, sizeof(g_c_watches[0].file)-1);
            g_c_watches[g_c_watch_count].lastWrite = fd.ftLastWriteTime;
            g_c_watches[g_c_watch_count].inited = true;
            g_c_watch_count++;
            DWORD now = GetTickCount();
            if (strcmp(g_last_c_file, fd.cFileName)==0 && (now - g_last_c_tick) < 1500) {
                break;
            }
            strncpy(g_last_c_file, fd.cFileName, sizeof(g_last_c_file)-1);
            g_last_c_tick = now;
            if (callback) callback();
            char* ret = _strdup(fd.cFileName);
            FindClose(hFind);
            return ret;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return NULL;
}

void Platform_CleanupDirWatchers(void) {
    g_c_watch_count = 0;
    g_c_watch_dir[0] = '\0';
    g_last_c_file[0] = '\0';
    g_last_c_tick = 0;
    log_info("Platform_CleanupDirWatchers completed");
}

bool Platform_ClickAtPos(int x,int y) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    INPUT inputs[3] = {0};

    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dx = (x * 65535) / screenWidth;
    inputs[0].mi.dy = (y * 65535) / screenHeight;
    inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    inputs[2].type = INPUT_MOUSE;
    inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(3, inputs, sizeof(INPUT));
    log_info("Platform_ClickAtPos x=%d y=%d", x, y);;
}

typedef struct {
    const char* target_title;
    HWND found_hwnd;
} FindWindowData;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;
    
    if (!IsWindowVisible(hwnd)) {
        return TRUE; 
    }
    
    char window_title[256];
    GetWindowTextA(hwnd, window_title, sizeof(window_title));
    
    if (Platform_StringContainsCaseInsensitive(window_title, data->target_title) != NULL) {
        data->found_hwnd = hwnd;
        return FALSE;
    }
    
    return TRUE;
}

bool Platform_FocusWindow(const char* title) {
    if (!title || !title[0]) { log_error("operation failed, returning error title='%s'", title ? title : "null"); return false; }
    
    HWND hwnd = FindWindowA(NULL, title);
    
    if (hwnd == NULL) {
        FindWindowData data;
        data.target_title = title;
        data.found_hwnd = NULL;
        
        EnumWindows(EnumWindowsProc, (LPARAM)&data);
        hwnd = data.found_hwnd;
    }
    
    if (hwnd == NULL) {
        return false;
    }
    
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    
    SetForegroundWindow(hwnd);
    
    FLASHWINFO fwi;
    fwi.cbSize = sizeof(FLASHWINFO);
    fwi.hwnd = hwnd;
    fwi.dwFlags = FLASHW_CAPTION | FLASHW_TRAY;
    fwi.uCount = 3;
    fwi.dwTimeout = 0;
    FlashWindowEx(&fwi);
    
    log_info("Platform_FocusWindow title='%s'", title ? title : "");;
    return true;
}

char* Platform_GetActiveWindowTitle(void) {
    static char fgTitle[256];  
    HWND hForeground = GetForegroundWindow();
    
    if (hForeground != NULL) {
        GetWindowTextA(hForeground, fgTitle, sizeof(fgTitle));
    } else {
        fgTitle[0] = '\0';
    }
    log_info("Platform_GetActiveWindowTitle completed");
    return fgTitle;
}
char* Platform_GetIconPath(char* app_path) {
  if (!app_path || !app_path[0]) { log_error("operation failed, returning error app_path='%s'", app_path ? app_path : "null"); return NULL; }
  char expanded[MAX_PATH];
  const char* src = app_path;
  if (Platform_ExpandPath(app_path, expanded, sizeof(expanded))) {
      if (expanded[0]) src = expanded;
  }
  if (strncmp(src, "shell:", 6)==0) return NULL;
  if (GetFileAttributesA(src) != INVALID_FILE_ATTRIBUTES) {
      return _strdup(src);
  }
  char full[MAX_PATH];
  if (SearchPathA(NULL, src, NULL, MAX_PATH, full, NULL) && GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES) {
      return _strdup(full);
  }
  if (!strchr(src, '.')) {
      char withExe[MAX_PATH]; snprintf(withExe, sizeof(withExe), "%s.exe", src);
      if (SearchPathA(NULL, withExe, NULL, MAX_PATH, full, NULL) && GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES) {
          return _strdup(full);
      }
  }
  log_info("Platform_GetIconPath app_path='%s'", app_path ? app_path : "");;
  return NULL;
}
int Platform_GetIconSize(char* icon_path) {
    (void)icon_path;
    extern const int IconSize;
    int s = IconSize;
    if (s < 16) s = 16;
    if (s > 32) s = 32;
    log_info("Platform_GetIconSize icon_path='%s'", icon_path ? icon_path : "");;
    return s;
}

bool Platform_BuildFeature(const char* src, const char* out) {
    if (!src || !out) return false;
    if (GetFileAttributesA("build/main.lib.a") == INVALID_FILE_ATTRIBUTES) {
        log_error("build/main.lib.a missing, need to build main first");
        return false;
    }
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", out);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "gcc -I include -DCOBJMACROS -O2 -shared -o \"%s\" \"%s\" \"build/main.lib.a\" -lcomctl32 -lgdiplus -lgdi32 -lshlwapi -lshell32 -lwinmm -lole32 -luuid -lmsimg32 -ldbghelp -ldwmapi -lpsapi -lcrypt32 -lcomdlg32 -ladvapi32 -ldxva2 -luser32 -lwinhttp 2>&1",
        tmp, src);
    char* outStr = Platform_RunTerminalCommand(cmd, false);
    bool ok = false;
    if (outStr) {
        if (strlen(outStr)==0 || (!strstr(outStr, "error:") && !strstr(outStr, "undefined reference") && !strstr(outStr, "failed"))) ok = true;
        log_info("%s", outStr);
        free(outStr);
    } else {
        ok = true;
    }
    if (!ok) { remove(tmp); return false; }
    remove(out);
    if (rename(tmp, out) != 0) {
        if (!MoveFileExA(tmp, out, MOVEFILE_REPLACE_EXISTING)) {
            log_error("failed to move %s -> %s", tmp, out);
            return false;
        }
    }
    log_info("Platform_BuildFeature src='%s' out='%s'", src ? src : "", out ? out : "");;
    return true;
}
