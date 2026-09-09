#include <windows.h>
#include <fileapi.h>
#include <handleapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/context.h"
#include "core/modal.h"
#include "helpers.h"
#include "core/platform.h"
#include "config.h"
#include <shellapi.h>
#include <gdiplus.h>

bool Platform_LaunchTerminalAtPath(const char* working_dir, const char* extra_args){
      char terminal[512];
      snprintf(terminal,sizeof(terminal),"%s.exe",UsedTerminal);
      ShellExecuteA(NULL, "open", terminal, NULL, working_dir, SW_SHOWNORMAL);
}

char* Platform_RunTerminalCommand(char* command,bool open_terminal) {
    if (command == NULL || strlen(command) == 0) return NULL;
    
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
    ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
    char buffer[100];
    snprintf(buffer,sizeof(buffer),"%s.exe",UsedFileManager);
    ShellExecuteA(NULL, "open",buffer, path, NULL, SW_SHOWNORMAL);
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
    return dup;
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

bool Platform_IsCtrlPressed(void) {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    CloseClipboard();
    
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

FontHandle Platform_CreateFont(int size, const char* face_name) {
    return FH_FROM_HFONT(CreateFontA(
        size, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        face_name
        ));
}

void Platform_DestroyFont(FontHandle font) {
    if (FH_TO_HFONT(font)) {
        DeleteObject(FH_TO_HFONT(font));
    }
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
}

long Platform_GetFileSize(char *filepath) {
    FILE *fp = fopen(filepath, "rb"); 
    if (fp == NULL) {
        return -1; 
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long size = ftell(fp);

    fclose(fp);
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
    return NULL;
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
}

bool Platform_FocusWindow(const char* title) {

}

bool Platform_EmitHotkey(int vk_code, int modifiers) {
    INPUT inputs[10] = {0};
    int idx = 0;
    // press modifiers
    if (modifiers & HOTKEY_MOD_CTRL) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_CONTROL;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_ALT) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_MENU;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_SHIFT) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_SHIFT;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_SUPER) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_LWIN;
        idx++;
    }
    // press main key
    inputs[idx].type = INPUT_KEYBOARD;
    inputs[idx].ki.wVk = vk_code;
    idx++;
    // release main key
    inputs[idx].type = INPUT_KEYBOARD;
    inputs[idx].ki.wVk = vk_code;
    inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
    idx++;
    // release modifiers in reverse
    if (modifiers & HOTKEY_MOD_SUPER) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_LWIN;
        inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_SHIFT) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_SHIFT;
        inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_ALT) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_MENU;
        inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
        idx++;
    }
    if (modifiers & HOTKEY_MOD_CTRL) {
        inputs[idx].type = INPUT_KEYBOARD;
        inputs[idx].ki.wVk = VK_CONTROL;
        inputs[idx].ki.dwFlags = KEYEVENTF_KEYUP;
        idx++;
    }
    // SendInput expects count, but we have variable idx, need to handle up to 10
    // For simplicity, send in two batches if needed
    if (idx > 6) {
        // fallback: send first 6, then remaining
        SendInput(6, inputs, sizeof(INPUT));
        SendInput(idx - 6, inputs + 6, sizeof(INPUT));
    } else {
        SendInput(idx, inputs, sizeof(INPUT));
    }
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
    return fgTitle;
}
