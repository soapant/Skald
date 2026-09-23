#include <windows.h>
#include <psapi.h>
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
#include "helpers/helpers_hotkey.h"
#include "platform/win32/win32_state.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "core/platform.h"
#include "config.h"
#include <shellapi.h>
#include <gdiplus.h>
#include <ctype.h>
#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "../../../nob.h"

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
    ULONGLONG startMs = GetTickCount64();
    ULONGLONG deadline = startMs + 350;
    bool got_data = false;
    bool exited = false;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if (now >= deadline) break;
        DWORD waitSlice = (DWORD)(deadline - now);
        DWORD w = WaitForSingleObject(pi.hProcess, waitSlice);
        if (w == WAIT_OBJECT_0) exited = true;
        bool read_any = false;
        for (;;) {
            DWORD avail = 0;
            if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL)) break;
            if (avail == 0) break;
            if (len + avail + 1 > cap) {
                size_t ncap = cap;
                while (len + avail + 1 > ncap) ncap *= 2;
                char* n = (char*)realloc(out, ncap);
                if (!n) break;
                out = n;
                cap = ncap;
            }
            DWORD chunk = avail;
            if (chunk > (DWORD)(cap - len - 1)) chunk = (DWORD)(cap - len - 1);
            DWORD bytesRead = 0;
            if (!ReadFile(hRead, out + len, chunk, &bytesRead, NULL) || bytesRead == 0) break;
            len += bytesRead;
            read_any = true;
        }
        if (read_any && !got_data) { got_data = true; }
        if (read_any) { deadline = GetTickCount64() + 5000; }
        if (exited) {
            for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL)) break;
                if (avail == 0) break;
                if (len + avail + 1 > cap) {
                    size_t ncap = cap;
                    while (len + avail + 1 > ncap) ncap *= 2;
                    char* n = (char*)realloc(out, ncap);
                    if (!n) break;
                    out = n;
                    cap = ncap;
                }
                DWORD chunk = avail;
                if (chunk > (DWORD)(cap - len - 1)) chunk = (DWORD)(cap - len - 1);
                DWORD bytesRead = 0;
                if (!ReadFile(hRead, out + len, chunk, &bytesRead, NULL) || bytesRead == 0) break;
                len += bytesRead;
            }
            DWORD avail = 0;
            if (PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail == 0) break;
            if (!read_any) break;
        }
    }
    out[len] = '\0';
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    log_info("ran terminal command '%s' open_terminal=%d", command, open_terminal);
    return out;
}

bool Platform_LaunchDetached(char* command) {
    if (!command || !command[0]) return false;
    char cmdLine[4096];
    snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi);
    if (!ok) return false;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

char* Platform_RunCommandAndGetOutput(char* command) {
    if (command == NULL || strlen(command) == 0) return NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return strdup("");
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
    if (!ok) { CloseHandle(hRead); return strdup(""); }
    WaitForSingleObject(pi.hProcess, INFINITE);
    size_t cap = 8192;
    size_t len = 0;
    char* out = (char*)malloc(cap);
    if (!out) { CloseHandle(hRead); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return strdup(""); }
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
    return out;
}

bool Platform_LaunchFileManagerAtPath(char* path,char* args) {
    if (!path || !path[0]) { log_error("path is NULL args='%s'", args ? args : "null"); return false; }
    //temporarily not being used and just doiung explorere on widnwos no diea if this is the right way to do it but it works for now
    HINSTANCE res = ShellExecuteA(NULL, "open", "explorer.exe", path ? path : "", NULL, SW_SHOWNORMAL);
    if ((intptr_t)res <= 32) { log_error("ShellExecute failed for filemanager '%s' path='%s' err=%ld", buffer, path, (long)(intptr_t)res); return false; }
    (void)args;
log_info("Platform_LaunchFileManagerAtPath path='%s' args='%s'", path ? path : "", args ? args : "");;
    return true;
}

char* Platform_QueryZoxidePath(const char* query) {
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

int Platform_CopyImageToClipboard(const unsigned char* png, size_t len) {
    if (!png || len == 0) return 0;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    strcat(tmp, "flint_clip_up.png");
    FILE* f = fopen(tmp, "wb");
    if (!f) return 0;
    size_t w = fwrite(png, 1, len, f);
    fclose(f);
    if (w != len) { remove(tmp); return 0; }
    wchar_t wtmp[512];
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, wtmp, 512);
    GpBitmap* bmp = NULL;
    if (GdipCreateBitmapFromFile(wtmp, &bmp) != 0 || !bmp) { remove(tmp); return 0; }
    HBITMAP hbm = NULL;
    if (GdipCreateHBITMAPFromBitmap((GpImage*)bmp, &hbm, 0) != 0 || !hbm) { GdipDisposeImage((GpImage*)bmp); remove(tmp); return 0; }
    GdipDisposeImage((GpImage*)bmp);
    remove(tmp);
    BITMAP bm;
    memset(&bm, 0, sizeof(bm));
    if (GetObjectA(hbm, sizeof(bm), &bm) == 0) { DeleteObject(hbm); return 0; }
    int width = bm.bmWidth;
    int height = abs(bm.bmHeight);
    if (width <= 0 || height <= 0) { DeleteObject(hbm); return 0; }
    int stride = ((width * 32 + 31) / 32) * 4;
    size_t img = (size_t)stride * (size_t)height;
    HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + img);
    if (!hDib) { DeleteObject(hbm); return 0; }
    BITMAPINFOHEADER* hdr = (BITMAPINFOHEADER*)GlobalLock(hDib);
    if (!hdr) { GlobalFree(hDib); DeleteObject(hbm); return 0; }
    memset(hdr, 0, sizeof(*hdr));
    hdr->biSize = sizeof(*hdr);
    hdr->biWidth = width;
    hdr->biHeight = bm.bmHeight;
    hdr->biPlanes = 1;
    hdr->biBitCount = 32;
    hdr->biCompression = BI_RGB;
    hdr->biSizeImage = (DWORD)img;
    HDC hdc = GetDC(NULL);
    int lines = GetDIBits(hdc, hbm, 0, (UINT)height, (BYTE*)hdr + sizeof(*hdr), (BITMAPINFO*)hdr, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);
    GlobalUnlock(hDib);
    DeleteObject(hbm);
    if (lines == 0) { GlobalFree(hDib); return 0; }
    if (!OpenClipboard(NULL)) { GlobalFree(hDib); return 0; }
    EmptyClipboard();
    if (!SetClipboardData(CF_DIB, hDib)) { CloseClipboard(); GlobalFree(hDib); return 0; }
    CloseClipboard();
    log_info("Platform_CopyImageToClipboard ok %dx%d", width, height);
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
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
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
                HFONT hf2 = CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fallbacks[i]);
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
    return FH_FROM_HFONT(hf);
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

char* Platform_WatchForCFileChanges(const char* dir_path) {
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
    return true;
}

typedef struct {
    const char* target_title;
    HWND found_hwnd;
} FindWindowData;

static BOOL CALLBACK EnumWindowsProcTitle(HWND hwnd, LPARAM lParam) {
    FindWindowData* data = (FindWindowData*)lParam;

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    char window_title[512];
    if (GetWindowTextA(hwnd, window_title, sizeof(window_title)) == 0) return TRUE;
    if (window_title[0] == '\0') return TRUE;

    if (Platform_StringContainsCaseInsensitive(window_title, data->target_title)) {
        data->found_hwnd = hwnd;
        return FALSE;
    }

    return TRUE;
}

bool Platform_FocusWindow(const char* title) {
    if (!title || !title[0]) { log_error("operation failed, returning error title='%s'", title ? title : "null"); return false; }

    char trimmed[512];
    const char* s = title;
    while (*s && isspace((unsigned char)*s)) s++;
    const char* e = s + strlen(s);
    while (e > s && isspace((unsigned char)*(e - 1))) e--;
    size_t len = (size_t)(e - s);
    if (len == 0) return false;
    if (len >= sizeof(trimmed)) len = sizeof(trimmed) - 1;
    memcpy(trimmed, s, len);
    trimmed[len] = '\0';

    HWND hwnd = FindWindowA(NULL, trimmed);

    if (hwnd == NULL) {
        FindWindowData data;
        data.target_title = trimmed;
        data.found_hwnd = NULL;

        EnumWindows(EnumWindowsProcTitle, (LPARAM)&data);
        hwnd = data.found_hwnd;
    }

    if (hwnd == NULL) {
        log_error("Platform_FocusWindow no window for '%s' (trimmed '%s')", title, trimmed);
        return false;
    }

    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    HWND fg = GetForegroundWindow();
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD targetTid = GetWindowThreadProcessId(hwnd, NULL);
    DWORD curTid = GetCurrentThreadId();
    if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, TRUE);
    if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, TRUE);
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);
    if (fgTid && fgTid != curTid) AttachThreadInput(fgTid, curTid, FALSE);
    if (targetTid && targetTid != curTid) AttachThreadInput(targetTid, curTid, FALSE);

    FLASHWINFO fwi;
    fwi.cbSize = sizeof(FLASHWINFO);
    fwi.hwnd = hwnd;
    fwi.dwFlags = FLASHW_CAPTION | FLASHW_TRAY;
    fwi.uCount = 3;
    fwi.dwTimeout = 0;
    FlashWindowEx(&fwi);

    log_info("Platform_FocusWindow title='%s' trimmed='%s' hwnd=%p", title ? title : "", trimmed, hwnd);
    return true;
}

static HWND Win32_ForegroundTarget(void) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindow(hwnd)) return NULL;
    char cls[64];
    cls[0] = '\0';
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "QuickSwitcherModalClass") == 0) return NULL;
    if (strcmp(cls, "DisplayMessageModalClass") == 0) return NULL;
    if (strcmp(cls, "EditPopupClass") == 0) return NULL;
    if (strcmp(cls, "SearchPopupClass") == 0) return NULL;
    if (strcmp(cls, "SettingsPopupClass") == 0) return NULL;
    return hwnd;
}

bool Platform_CloseActiveWindow(void) {
    HWND hwnd = Win32_ForegroundTarget();
    if (!hwnd) return false;
    PostMessageA(hwnd, WM_CLOSE, 0, 0);
    return true;
}

static BOOL g_win32_fs_active = FALSE;
static HWND g_win32_last_min = NULL;
static RECT g_win32_fs_rect;
static LONG g_win32_fs_style;
static LONG g_win32_fs_exstyle;

bool Platform_ToggleFullscreenActive(void) {
    HWND hwnd = Win32_ForegroundTarget();
    if (!hwnd) return false;
    if (!g_win32_fs_active) {
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        g_win32_fs_style = GetWindowLongA(hwnd, GWL_STYLE);
        g_win32_fs_exstyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
        GetWindowRect(hwnd, &g_win32_fs_rect);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoA(mon, &mi)) return false;
        SetWindowLongA(hwnd, GWL_STYLE, (g_win32_fs_style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowLongA(hwnd, GWL_EXSTYLE, g_win32_fs_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE));
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_win32_fs_active = TRUE;
        return true;
    }
    SetWindowLongA(hwnd, GWL_STYLE, g_win32_fs_style);
    SetWindowLongA(hwnd, GWL_EXSTYLE, g_win32_fs_exstyle);
    SetWindowPos(hwnd, HWND_NOTOPMOST, g_win32_fs_rect.left, g_win32_fs_rect.top,
        g_win32_fs_rect.right - g_win32_fs_rect.left, g_win32_fs_rect.bottom - g_win32_fs_rect.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    g_win32_fs_active = FALSE;
    return true;
}

typedef struct {
    RECT rects[16];
    int count;
} Win32_MonList;

static BOOL CALLBACK Win32_MonEnum(HMONITOR hMon, HDC hdc, LPRECT rc, LPARAM lp) {
    (void)hMon; (void)hdc;
    Win32_MonList* out = (Win32_MonList*)lp;
    if (!out || out->count >= 16 || !rc) return TRUE;
    out->rects[out->count++] = *rc;
    return TRUE;
}

bool Platform_MoveActiveWindowToMonitor(bool right) {
    HWND hwnd = Win32_ForegroundTarget();
    if (!hwnd) return false;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    RECT wr;
    if (!GetWindowRect(hwnd, &wr)) return false;
    int cx = wr.left + (wr.right - wr.left) / 2;
    int cy = wr.top + (wr.bottom - wr.top) / 2;
    Win32_MonList list;
    list.count = 0;
    EnumDisplayMonitors(NULL, NULL, Win32_MonEnum, (LPARAM)&list);
    if (list.count < 2) return false;
    for (int i = 0; i < list.count - 1; i++) {
        for (int j = i + 1; j < list.count; j++) {
            if (list.rects[j].left < list.rects[i].left) {
                RECT t = list.rects[i]; list.rects[i] = list.rects[j]; list.rects[j] = t;
            }
        }
    }
    int cur = 0;
    for (int i = 0; i < list.count; i++) {
        if (cx >= list.rects[i].left && cx < list.rects[i].right &&
            cy >= list.rects[i].top && cy < list.rects[i].bottom) { cur = i; break; }
        if (list.rects[i].left <= cx) cur = i;
    }
    int dst = right ? cur + 1 : cur - 1;
    if (dst < 0 || dst >= list.count) return false;
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    int nx = list.rects[dst].left + (cx - list.rects[cur].left);
    int ny = list.rects[dst].top + (wr.top - list.rects[cur].top);
    int dw = list.rects[dst].right - list.rects[dst].left;
    int dh = list.rects[dst].bottom - list.rects[dst].top;
    if (w > dw) w = dw;
    if (h > dh) h = dh;
    if (nx < list.rects[dst].left) nx = list.rects[dst].left;
    if (ny < list.rects[dst].top) ny = list.rects[dst].top;
    if (nx + w > list.rects[dst].right) nx = list.rects[dst].right - w;
    if (ny + h > list.rects[dst].bottom) ny = list.rects[dst].bottom - h;
    SetWindowPos(hwnd, HWND_TOP, nx, ny, w, h, SWP_SHOWWINDOW);
    return true;
}

bool Platform_MinimizeActiveWindow(void) {
    HWND hwnd = Win32_ForegroundTarget();
    if (hwnd && !IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_MINIMIZE);
        g_win32_last_min = hwnd;
        return true;
    }
    if (hwnd && IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        if (g_win32_last_min == hwnd) g_win32_last_min = NULL;
        return true;
    }
    if (g_win32_last_min && IsWindow(g_win32_last_min) && IsIconic(g_win32_last_min)) {
        ShowWindow(g_win32_last_min, SW_RESTORE);
        SetForegroundWindow(g_win32_last_min);
        g_win32_last_min = NULL;
        return true;
    }
    g_win32_last_min = NULL;
    return false;
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

char* Platform_GetActiveWindowAppId(void) {
    static char appId[256];
    appId[0] = '\0';
    HWND hForeground = GetForegroundWindow();
    if (hForeground == NULL) return appId;
    DWORD pid = 0;
    GetWindowThreadProcessId(hForeground, &pid);
    if (pid == 0) return appId;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return appId;
    char exe[MAX_PATH];
    DWORD sz = sizeof(exe);
    if (QueryFullProcessImageNameA(h, 0, exe, &sz) && sz > 0) {
        const char* base = strrchr(exe, '\\');
        base = base ? base + 1 : exe;
        strncpy(appId, base, sizeof(appId) - 1);
        appId[sizeof(appId) - 1] = '\0';
        size_t n = strlen(appId);
        if (n > 4 && _stricmp(appId + n - 4, ".exe") == 0) appId[n - 4] = '\0';
    }
    CloseHandle(h);
    return appId;
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
        "gcc -I include -DCOBJMACROS -O2 -shared -o \"%s\" \"%s\" \"build/main.lib.a\" -lcomctl32 -lgdiplus -lgdi32 -lshlwapi -lshell32 -lwinmm -lole32 -luuid -lmsimg32 -ldwmapi -lpsapi -lcrypt32 -lcomdlg32 -ladvapi32 -ldxva2 -luser32 -lwinhttp 2>&1",
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

typedef struct { Fl_WindowList *titles; Fl_WindowList *names; } WinPair;

static BOOL CALLBACK EnumWindowsProcPair(HWND hwnd, LPARAM lParam) {
    WinPair *pair = (WinPair*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    char cls[256];
    if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0) {
        if (strcmp(cls, "Progman") == 0 || strcmp(cls, "WorkerW") == 0) return TRUE;
    }
    char title[512];
    if (GetWindowTextA(hwnd, title, sizeof(title)) == 0 || title[0] == '\0') return TRUE;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return TRUE;
    char exe[MAX_PATH] = {0};
    char exePath[MAX_PATH] = {0};
    DWORD sz = sizeof(exePath);
    DWORD len = 0;
    if (QueryFullProcessImageNameA(h, 0, exePath, &sz) && sz > 0) {
        char *base = strrchr(exePath, '\\');
        if (base) { strncpy(exe, base + 1, sizeof(exe)-1); len = strlen(exe); }
        else { strncpy(exe, exePath, sizeof(exe)-1); len = strlen(exe); }
    } else {
        len = GetModuleBaseNameA(h, NULL, exe, sizeof(exe));
    }
    CloseHandle(h);
    if (len == 0 || exe[0] == '\0') return TRUE;
    char **t1 = realloc(pair->titles->windows, (pair->titles->count + 1) * sizeof(char*));
    char **t2 = realloc(pair->names->windows, (pair->names->count + 1) * sizeof(char*));
    if (!t1 || !t2) { free(t1); free(t2); return TRUE; }
    pair->titles->windows = t1;
    pair->names->windows = t2;
    pair->titles->windows[pair->titles->count++] = strdup(title);
    pair->names->windows[pair->names->count++] = strdup(exe);
    return TRUE;
}

static void win32_collect_pair(Fl_WindowList *out_titles, Fl_WindowList *out_names) {
    WinPair p = { out_titles, out_names };
    EnumWindows(EnumWindowsProcPair, (LPARAM)&p);
}

Fl_WindowList Platform_GetWindowTitles() {
    Fl_WindowList titles={0}, names={0};
    win32_collect_pair(&titles, &names);
    for(int i=0;i<names.count;i++) free(names.windows[i]);
    free(names.windows);
    log_info("Platform_GetWindowTitles count=%d", titles.count);
    return titles;
}
Fl_WindowList Platform_GetWindowNames() {
    Fl_WindowList titles={0}, names={0};
    win32_collect_pair(&titles, &names);
    for(int i=0;i<titles.count;i++) free(titles.windows[i]);
    free(titles.windows);
    log_info("Platform_GetWindowNames count=%d", names.count);
    return names;
}

static HANDLE g_singleton_mtx = NULL;

bool Platform_TryAcquireSingleInstance(void) {
    g_singleton_mtx = CreateMutexA(NULL, TRUE, "flint_single_instance_mutex");
    if (g_singleton_mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        log_info("Another instance is already running. Exiting.");
        CloseHandle(g_singleton_mtx);
        g_singleton_mtx = NULL;
        return false;
    }
    return true;
}

bool Platform_InitDisplaySafe(void) {
    return true;
}

void Platform_CleanupDisplaySafe(void) {
}

void Platform_InitMainThreadContext(void) {
    g_win32_main_thread_id = GetCurrentThreadId();
}

void Platform_FixWorkingDirectory(void) {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        char* slash = strrchr(buf, '\\');
        if (slash) *slash = '\0';
        for (int depth = 0; depth < 4; ++depth) {
            char probe[MAX_PATH];
            snprintf(probe, sizeof(probe), "%s\\data", buf);
            DWORD attr = GetFileAttributesA(probe);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                SetCurrentDirectoryA(buf);
                break;
            }
            slash = strrchr(buf, '\\');
            if (!slash) break;
            *slash = '\0';
        }
    }
}

void Platform_CleanupPlatformResources(void) {
    if (g_win32_hMainFont) {
        DeleteObject(g_win32_hMainFont);
        g_win32_hMainFont = NULL;
    }
}

Fl_Pos Platform_GetCursorPos(void) {
    POINT point;
    
    if (GetCursorPos(&point)) {
        return (Fl_Pos){(int)point.x, (int)point.y};
    }
    
    return (Fl_Pos){0, 0};
}

char* Platform_GetWorkingDir() {
  return Platform_RunTerminalCommand("cd",false);
}

void Platform_InitDataFolder() {
    char dir_path[200];
    snprintf(dir_path,sizeof(dir_path),"%s\\AppData\\Roaming\\Flint",Platform_GetRealHome());
    nob_mkdir_if_not_exists(dir_path);
}

bool Platform_GetFeaturePath(const char* feature, const char* name, char* out, size_t out_len) {
    if (!feature || !feature[0] || !name || !name[0] || !out || out_len == 0) return false;
    char dir_path[256];
    snprintf(dir_path,sizeof(dir_path),"%s\\AppData\\Roaming\\Flint",Platform_GetRealHome());
    nob_mkdir_if_not_exists(dir_path);
    char feat_path[320];
    snprintf(feat_path,sizeof(feat_path),"%s\\%s",dir_path,feature);
    nob_mkdir_if_not_exists(feat_path);
    snprintf(out,out_len,"%s\\%s",feat_path,name);
    return true;
}

bool Platform_ResolveDataPath(const char* name, char* out, size_t out_len) {
    if (!name || !name[0] || !out || out_len == 0) return false;
    for (const char* p = name; *p; p++) {
        if (*p == '/' || *p == '\\') return false;
    }
    if (strstr(name, "..")) return false;
    const char* feat = FlintContext_Get()->window.current_feature;
    if (!feat || !feat[0]) return false;
    char safe[128];
    size_t w = 0;
    for (const char* p = feat; *p && w + 1 < sizeof(safe); p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') safe[w++] = (char)(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') safe[w++] = c;
        else if (c == ' ') safe[w++] = '-';
    }
    safe[w] = '\0';
    if (w == 0 || strcmp(safe, ".") == 0 || strcmp(safe, "..") == 0) return false;
    return Platform_GetFeaturePath(safe, name, out, out_len);
}
