#include <windows.h>
#include <winhttp.h>
#include <psapi.h>
#include <dwmapi.h>
#include "deps/log.h"
#include <fileapi.h>
#include <handleapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/context.h"
#include "core/feature_path.h"
#include "platform/platform.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/win32/bg_image_win32.h"
#include "core/prompt_geometry.h"
#include "platform/win32/input_core_win32.h"
#include <shellapi.h>
#include <gdiplus.h>
#include <ctype.h>
#include "platform/input_state.h"
#pragma comment(lib, "winhttp.lib")

bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args) {

  if (!working_dir) {
    log_error("working_dir is NULL extra_args='%s'",
              extra_args ? extra_args : "null");
    return false;
  }
  if (!UsedTerminal || !UsedTerminal[0]) {
    log_error("UsedTerminal not configured working_dir='%s' extra_args='%s'",
              working_dir ? working_dir : "null",
              extra_args ? extra_args : "null");
    return false;
  }
  char terminal[512];
  snprintf(terminal, sizeof(terminal), "%s.exe", UsedTerminal);
  HINSTANCE res = ShellExecuteA(NULL, "open", terminal, extra_args, working_dir,
                                SW_SHOWNORMAL);
  if ((intptr_t)res <= 32) {
    log_error("ShellExecute failed for terminal '%s' (extra_args='%s', "
              "dir='%s') err=%ld - check UsedTerminal config (foot vs "
              "alacritty, ensure .exe exists in PATH)",
              terminal, extra_args ? extra_args : "", working_dir,
              (long)(intptr_t)res);
    return false;
  }
  log_info("Platform_LaunchTerminalAtPath working_dir='%s' extra_args='%s'",
           working_dir ? working_dir : "", extra_args ? extra_args : "");
  ;
  return true;
}

char *Platform_RunTerminalCommand(char *command, bool open_terminal) {
  if (open_terminal) {
    char clean_cmd[512];
    strncpy(clean_cmd, command ? command : "", sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';
    char *p = clean_cmd;
    while (*p && isspace((unsigned char)*p))
      p++;
    bool empty = (*p == '\0');
    char terminal_exe[512];
    snprintf(terminal_exe, sizeof(terminal_exe), "%s.exe", UsedTerminal);
    if (empty) {
      HINSTANCE res2 =
          ShellExecuteA(NULL, "open", terminal_exe, NULL, NULL, SW_SHOWNORMAL);
      if ((intptr_t)res2 <= 32) {
        log_error("Platform_RunTerminalCommand(open_terminal): ShellExecute "
                  "failed for '%s' err=%ld - check UsedTerminal",
                  terminal_exe, (long)(intptr_t)res2);
      }
      return NULL;
    }
    char temp_file[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_file);
    strcat(temp_file, "temp_cmd.sh");
    FILE *f = fopen(temp_file, "w");
    if (f) {
      fprintf(f, "#!/bin/%s\n", UsedShell);
      fprintf(f, "%s\n", clean_cmd);
      fclose(f);
    }
    char args[2048];
    snprintf(args, sizeof(args), "-e %s \"%s\"", UsedShell, temp_file);
    HINSTANCE res2 =
        ShellExecuteA(NULL, "open", terminal_exe, args, NULL, SW_SHOWNORMAL);
    if ((intptr_t)res2 <= 32) {
      log_error("Platform_RunTerminalCommand(open_terminal): ShellExecute "
                "failed for '%s' args='%s' err=%ld - check UsedTerminal",
                terminal_exe, args, (long)(intptr_t)res2);
    }
    return NULL;
  }
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  HANDLE hRead, hWrite;
  if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
    log_error("CreatePipe failed err=%lu", GetLastError());
    return NULL;
  }
  SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA si = {0};
  si.cb = sizeof(STARTUPINFOA);
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;
  si.dwFlags = STARTF_USESTDHANDLES;
  PROCESS_INFORMATION pi = {0};
  char cmdLine[4096];
  snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
  BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi);
  CloseHandle(hWrite);
  if (!ok) {
    log_error("CreateProcess failed for '%s' err=%lu", cmdLine, GetLastError());
    CloseHandle(hRead);
    return NULL;
  }
  char *out = NULL;
  size_t cap = 8192;
  size_t len = 0;
  out = (char *)malloc(cap);
  if (!out) {
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return NULL;
  }
  ULONGLONG startMs = GetTickCount64();
  ULONGLONG deadline = startMs + 30000;
  bool exited = false;
  for (;;) {
    ULONGLONG now = GetTickCount64();
    if (now >= deadline)
      break;
    DWORD waitSlice = (DWORD)(deadline - now);
    if (waitSlice > 100)
      waitSlice = 100;
    DWORD w = WaitForSingleObject(pi.hProcess, waitSlice);
    if (w == WAIT_OBJECT_0)
      exited = true;
    bool read_any = false;
    for (;;) {
      DWORD avail = 0;
      if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL))
        break;
      if (avail == 0)
        break;
      if (len + avail + 1 > cap) {
        size_t ncap = cap;
        while (len + avail + 1 > ncap)
          ncap *= 2;
        char *n = (char *)realloc(out, ncap);
        if (!n)
          break;
        out = n;
        cap = ncap;
      }
      DWORD chunk = avail;
      if (chunk > (DWORD)(cap - len - 1))
        chunk = (DWORD)(cap - len - 1);
      DWORD bytesRead = 0;
      if (!ReadFile(hRead, out + len, chunk, &bytesRead, NULL) ||
          bytesRead == 0)
        break;
      len += bytesRead;
      read_any = true;
    }
    if (read_any) {
      deadline = GetTickCount64() + 5000;
    }
    if (exited) {
      for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL))
          break;
        if (avail == 0)
          break;
        if (len + avail + 1 > cap) {
          size_t ncap = cap;
          while (len + avail + 1 > ncap)
            ncap *= 2;
          char *n = (char *)realloc(out, ncap);
          if (!n)
            break;
          out = n;
          cap = ncap;
        }
        DWORD chunk = avail;
        if (chunk > (DWORD)(cap - len - 1))
          chunk = (DWORD)(cap - len - 1);
        DWORD bytesRead = 0;
        if (!ReadFile(hRead, out + len, chunk, &bytesRead, NULL) ||
            bytesRead == 0)
          break;
        len += bytesRead;
      }
      break;
    }
  }
  out[len] = '\0';
  CloseHandle(hRead);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  log_info("ran terminal command '%s' open_terminal=%d", command,
           open_terminal);
  return out;
}

bool Platform_LaunchDetached(char *command) {
  if (!command || !command[0])
    return false;
  char cmdLine[4096];
  snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
  STARTUPINFOA si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  BOOL ok =
      CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                     CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi);
  if (!ok)
    return false;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

char *Platform_RunCommandAndGetOutput(char *command, int priority) {
  if (command == NULL || strlen(command) == 0)
    return NULL;
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  HANDLE hRead, hWrite;
  if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    return NULL;
  SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA si = {0};
  si.cb = sizeof(STARTUPINFOA);
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;
  si.dwFlags = STARTF_USESTDHANDLES;
  PROCESS_INFORMATION pi = {0};
  char cmdLine[4096];
  snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
  DWORD creation_flags = CREATE_NO_WINDOW;
  if (priority == PLATFORM_RUN_PRIO_LOW)
    creation_flags |= BELOW_NORMAL_PRIORITY_CLASS;
  BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, creation_flags,
                           NULL, NULL, &si, &pi);
  CloseHandle(hWrite);
  if (!ok) {
    CloseHandle(hRead);
    return NULL;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  size_t cap = 8192;
  size_t len = 0;
  char *out = (char *)malloc(cap);
  if (!out) {
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return NULL;
  }
  DWORD bytesRead = 0;
  while (ReadFile(hRead, out + len, (DWORD)(cap - len - 1), &bytesRead, NULL) &&
         bytesRead > 0) {
    len += bytesRead;
    if (len + 1024 > cap) {
      cap *= 2;
      char *n = (char *)realloc(out, cap);
      if (!n)
        break;
      out = n;
    }
  }
  out[len] = '\0';
  CloseHandle(hRead);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return out;
}

char *Platform_RunArgvCapture(const char *const argv[], const char *stdin_data,
                              int *out_status) {

  if (out_status)
    *out_status = -1;
  if (!argv || !argv[0])
    return NULL;
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  HANDLE rd = NULL, wr = NULL;
  if (!CreatePipe(&rd, &wr, &sa, 0))
    return NULL;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = rd;
  si.hStdOutput = wr;
  si.hStdError = wr;
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  char cmdline[4096];
  size_t off = 0;
  cmdline[0] = '\0';
  for (int i = 0; argv[i] && i < 128; i++) {
    size_t need = strlen(argv[i]) * 2 + 3;
    if (off + need >= sizeof(cmdline))
      break;
    if (i > 0)
      cmdline[off++] = ' ';
    cmdline[off++] = '"';
    for (const char *p = argv[i]; *p; p++) {
      if (*p == '"' || *p == '\\')
        cmdline[off++] = '\\';
      cmdline[off++] = *p;
    }
    cmdline[off++] = '"';
  }
  cmdline[off] = '\0';
  CloseHandle(wr);
  HANDLE child_in = si.hStdInput;
  BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi);
  CloseHandle(rd);
  if (!ok) {
    log_error("Platform_RunArgvCapture CreateProcess failed for '%s'", argv[0]);
    return NULL;
  }
  if (stdin_data && stdin_data[0]) {
    DWORD written = 0;
    WriteFile(child_in, stdin_data, (DWORD)strlen(stdin_data), &written, NULL);
  }
  CloseHandle(child_in);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  if (out_status)
    *out_status = (int)code;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return NULL;
}

bool Platform_IsCtrlPressed(void) {
  return Platform_IsKeyDown(VK_LCONTROL) || Platform_IsKeyDown(VK_RCONTROL) ||
         Platform_IsKeyDown(VK_CONTROL);
}

bool Platform_IsShiftPressed(void) {
  return Platform_IsKeyDown(VK_LSHIFT) || Platform_IsKeyDown(VK_RSHIFT) ||
         Platform_IsKeyDown(VK_SHIFT);
}

bool Platform_IsAltPressed(void) {
  return Platform_IsKeyDown(VK_LMENU) || Platform_IsKeyDown(VK_RMENU) ||
         Platform_IsKeyDown(VK_MENU);
}

bool Platform_IsSuperPressed(void) {
  return Platform_IsKeyDown(VK_LWIN) || Platform_IsKeyDown(VK_RWIN);
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
  log_info("Platform_ModifyUniversalTimer seconds_to_add=%p", seconds_to_add);
  ;
}

double Platform_GetUniversalTimeSeconds() {
  ULONGLONG current_ms = GetTickCount64();

  if (current_ms < g_timer_start_ms) {
    return 0.0;
  }

  ULONGLONG elapsed_ms = current_ms - g_timer_start_ms;
  return (double)elapsed_ms / 1000.0;
}

FontHandle Platform_CreateFont(int size, const char *face_name) {
  int h = -size;
  static bool s_font_loaded = false;
  if (!s_font_loaded) {
    s_font_loaded = true;
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("resources\\*.ttf", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "resources\\%s", fd.cFileName);
        AddFontResourceExA(path, FR_PRIVATE, 0);
      } while (FindNextFileA(hFind, &fd));
      FindClose(hFind);
    }
    hFind = FindFirstFileA("data\\*.ttf", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "data\\%s", fd.cFileName);
        AddFontResourceExA(path, FR_PRIVATE, 0);
      } while (FindNextFileA(hFind, &fd));
      FindClose(hFind);
    }
  }
  HFONT hf =
      CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                  DEFAULT_PITCH | FF_DONTCARE, face_name);
  if (hf) {
    HDC hdc = GetDC(NULL);
    HFONT old = (HFONT)SelectObject(hdc, hf);
    char actual[64] = {0};
    GetTextFaceA(hdc, sizeof(actual), actual);
    SelectObject(hdc, old);
    ReleaseDC(NULL, hdc);
    if (actual[0] && face_name && _stricmp(actual, face_name) != 0) {
      const char *fallbacks[] = {"Cascadia Code", "Cascadia Mono", "Consolas",
                                 "Segoe UI", NULL};
      for (int i = 0; fallbacks[i]; i++) {
        if (_stricmp(actual, fallbacks[i]) == 0)
          break;
        HFONT hf2 = CreateFontA(h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_TT_PRECIS,
                                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, fallbacks[i]);
        if (hf2) {
          HDC hdc2 = GetDC(NULL);
          HFONT old2 = (HFONT)SelectObject(hdc2, hf2);
          char act2[64] = {0};
          GetTextFaceA(hdc2, sizeof(act2), act2);
          SelectObject(hdc2, old2);
          ReleaseDC(NULL, hdc2);
          if (_stricmp(act2, fallbacks[i]) == 0) {
            DeleteObject(hf);
            hf = hf2;
            break;
          }
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

void Platform_GetTextExtents(FontHandle font, const char *text, int maxWidth,
                             int *outWidth, int *outHeight) {

  if (!FH_TO_HFONT(font) || !text) {
    *outWidth = 0;
    *outHeight = 0;
    return;
  }
  HDC hdc = CreateCompatibleDC(NULL);
  HFONT hOldFont = (HFONT)SelectObject(hdc, FH_TO_HFONT(font));
  int width = 0;
  int height = 0;
  if (maxWidth > 0) {
    RECT measure = {0, 0, 0, 0};
    DrawTextA(hdc, text, -1, &measure, DT_CALCRECT);
    RECT wrapped = {0, 0, maxWidth, 0x7FFFFFFF};
    DrawTextA(hdc, text, -1, &wrapped, DT_CALCRECT | DT_WORDBREAK);
    height = wrapped.bottom - wrapped.top;
    width = measure.right - measure.left;
    if (width > maxWidth)
      width = maxWidth;
  } else {
    RECT measure = {0, 0, 0, 0};
    DrawTextA(hdc, text, -1, &measure, DT_CALCRECT);
    width = measure.right - measure.left;
    height = measure.bottom - measure.top;
  }
  SelectObject(hdc, hOldFont);
  DeleteDC(hdc);
  *outWidth = width;
  *outHeight = height;
}

long Platform_GetFileSize(char *filepath) {
  if (!filepath)
    return -1;
  FILE *fp = fopen(filepath, "rb");
  if (fp == NULL) {
    log_error("operation failed, returning error filepath='%s'",
              filepath ? filepath : "null");
    return -1;
  }

  if (fseek(fp, 0L, SEEK_END) != 0) {
    log_error("fseek failed for '%s' err=%s", filepath, strerror(errno));
    fclose(fp);
    return -1;
  }

  long size = ftell(fp);

  fclose(fp);
  log_info("Platform_GetFileSize filepath=%p", filepath);
  ;
  return size;
}

typedef struct {
  const char *target_title;
  HWND found_hwnd;
} FindWindowData;

static BOOL CALLBACK EnumWindowsProcTitle(HWND hwnd, LPARAM lParam) {

  FindWindowData *data = (FindWindowData *)lParam;

  if (!IsWindowVisible(hwnd)) {
    return TRUE;
  }

  char window_title[512];
  if (GetWindowTextA(hwnd, window_title, sizeof(window_title)) == 0)
    return TRUE;
  if (window_title[0] == '\0')
    return TRUE;

  if (Platform_StringContainsCaseInsensitive(window_title,
                                             data->target_title)) {
    data->found_hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

bool Platform_FocusWindow(const char *title) {
  if (!title || !title[0]) {
    log_error("operation failed, returning error title='%s'",
              title ? title : "null");
    return false;
  }

  char trimmed[512];
  const char *s = title;
  while (*s && isspace((unsigned char)*s))
    s++;
  const char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)*(e - 1)))
    e--;
  size_t len = (size_t)(e - s);
  if (len == 0)
    return false;
  if (len >= sizeof(trimmed))
    len = sizeof(trimmed) - 1;
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
    log_error("Platform_FocusWindow no window for '%s' (trimmed '%s')", title,
              trimmed);
    return false;
  }

  extern void Platform_SetFlintPrevFg(HWND h);
  HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    Platform_SetFlintPrevFg(hwnd);
    log_info("Platform_FocusWindow deferred via g_prev_fg hwnd=%p title='%s'",
             hwnd, trimmed);
    return true;
  }

  if (IsIconic(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  ShowWindow(hwnd, SW_SHOW);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  HWND fg = GetForegroundWindow();
  DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
  DWORD targetTid = GetWindowThreadProcessId(hwnd, NULL);
  DWORD curTid = GetCurrentThreadId();
  if (fgTid && fgTid != curTid)
    AttachThreadInput(fgTid, curTid, TRUE);
  if (targetTid && targetTid != curTid)
    AttachThreadInput(targetTid, curTid, TRUE);
  AllowSetForegroundWindow(ASFW_ANY);
  SetForegroundWindow(hwnd);
  SetActiveWindow(hwnd);
  BringWindowToTop(hwnd);
  if (fgTid && fgTid != curTid)
    AttachThreadInput(fgTid, curTid, FALSE);
  if (targetTid && targetTid != curTid)
    AttachThreadInput(targetTid, curTid, FALSE);

  FLASHWINFO fwi;
  fwi.cbSize = sizeof(FLASHWINFO);
  fwi.hwnd = hwnd;
  fwi.dwFlags = FLASHW_CAPTION | FLASHW_TRAY;
  fwi.uCount = 3;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);

  log_info("Platform_FocusWindow title='%s' trimmed='%s' hwnd=%p",
           title ? title : "", trimmed, hwnd);
  return true;
}

static HWND win32_foreground_target(void) {

  HWND hwnd = GetForegroundWindow();
  if (!hwnd || !IsWindow(hwnd))
    return NULL;
  char cls[64];
  cls[0] = '\0';
  GetClassNameA(hwnd, cls, sizeof(cls));
  if (strcmp(cls, "QuickSwitcherModalClass") == 0)
    return NULL;
  if (strcmp(cls, "DisplayMessageModalClass") == 0)
    return NULL;
  if (strcmp(cls, "SearchPopupClass") == 0)
    return NULL;
  return hwnd;
}

bool Platform_CloseActiveWindow(void) {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd || !IsWindow(hwnd))
    return false;
  char cls[64];
  cls[0] = '\0';
  GetClassNameA(hwnd, cls, sizeof(cls));
  if (strcmp(cls, "QuickSwitcherModalClass") == 0) {
    FlintContext_Get()->window.selection_complete = true;
    return true;
  }
  PostMessageA(hwnd, WM_CLOSE, 0, 0);
  return true;
}

static BOOL g_win32_fs_active = FALSE;
static char g_win32_project_root[MAX_PATH] = {0};
static HWND g_win32_last_min = NULL;
static RECT g_win32_fs_rect;
static LONG g_win32_fs_style;
static LONG g_win32_fs_exstyle;

bool Platform_ToggleFullscreenActive(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  if (!g_win32_fs_active) {
    if (IsIconic(hwnd))
      ShowWindow(hwnd, SW_RESTORE);
    g_win32_fs_style = GetWindowLongA(hwnd, GWL_STYLE);
    g_win32_fs_exstyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
    GetWindowRect(hwnd, &g_win32_fs_rect);
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoA(mon, &mi))
      return false;
    SetWindowLongA(hwnd, GWL_STYLE,
                   (g_win32_fs_style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowLongA(hwnd, GWL_EXSTYLE,
                   g_win32_fs_exstyle &
                       ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE |
                         WS_EX_STATICEDGE | WS_EX_WINDOWEDGE));
    SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    g_win32_fs_active = TRUE;
    return true;
  }
  SetWindowLongA(hwnd, GWL_STYLE, g_win32_fs_style);
  SetWindowLongA(hwnd, GWL_EXSTYLE, g_win32_fs_exstyle);
  SetWindowPos(hwnd, HWND_NOTOPMOST, g_win32_fs_rect.left, g_win32_fs_rect.top,
               g_win32_fs_rect.right - g_win32_fs_rect.left,
               g_win32_fs_rect.bottom - g_win32_fs_rect.top,
               SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  g_win32_fs_active = FALSE;
  return true;
}

typedef struct {
  RECT rects[16];
  int count;
} win32_mon_list;

static BOOL CALLBACK win32_mon_enum(HMONITOR hMon, HDC hdc, LPRECT rc,
                                    LPARAM lp) {

  (void)hMon;
  (void)hdc;
  win32_mon_list *out = (win32_mon_list *)lp;
  if (!out || out->count >= 16 || !rc)
    return TRUE;
  out->rects[out->count++] = *rc;
  return TRUE;
}

typedef struct {
  int fcx;
  int fcy;
  HWND best;
  int best_gap;
  int best_vdist;
} win32_focus_search;

static BOOL CALLBACK win32_focus_enum_left(HWND hwnd, LPARAM lp) {

  win32_focus_search *s = (win32_focus_search *)lp;
  if (!s)
    return TRUE;
  if (hwnd == GetForegroundWindow())
    return TRUE;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (IsIconic(hwnd))
    return TRUE;
  char cls[64];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0)
    if (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
        strcmp(cls, "DisplayMessageModalClass") == 0 ||
        strcmp(cls, "SearchPopupClass") == 0 ||
        strcmp(cls, "WidgetClass") == 0) {
      return TRUE;
    }
  {
    int cloaked = 0;
    HMODULE dwm = GetModuleHandleA("dwmapi.dll");
    if (dwm) {
      HRESULT(WINAPI * p)(HWND, DWORD, PVOID, DWORD) =
          (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
      if (p && SUCCEEDED(p(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked)
        return TRUE;
    }
  }
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND o = GetWindow(hwnd, GW_OWNER);
    if (o)
      return TRUE;
  }
  char title[256];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0)
    return TRUE;
  if (title[0] == '\0')
    return TRUE;
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return TRUE;
  int w = wr.right - wr.left;
  int h = wr.bottom - wr.top;
  if (w <= 0 || h <= 0)
    return TRUE;
  int cx = wr.left + w / 2;
  int cy = wr.top + h / 2;
  if (cx >= s->fcx)
    return TRUE;
  int gap = s->fcx - cx;
  int vdist = cy >= s->fcy ? cy - s->fcy : s->fcy - cy;
  if (gap < s->best_gap || (gap == s->best_gap && vdist < s->best_vdist)) {
    s->best_gap = gap;
    s->best_vdist = vdist;
    s->best = hwnd;
  }
  return TRUE;
}

static BOOL CALLBACK win32_focus_enum_right(HWND hwnd, LPARAM lp) {

  win32_focus_search *s = (win32_focus_search *)lp;
  if (!s)
    return TRUE;
  if (hwnd == GetForegroundWindow())
    return TRUE;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (IsIconic(hwnd))
    return TRUE;
  char cls[64];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0)
    if (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
        strcmp(cls, "DisplayMessageModalClass") == 0 ||
        strcmp(cls, "SearchPopupClass") == 0 ||
        strcmp(cls, "WidgetClass") == 0) {
      return TRUE;
    }
  {
    int cloaked = 0;
    HMODULE dwm = GetModuleHandleA("dwmapi.dll");
    if (dwm) {
      HRESULT(WINAPI * p)(HWND, DWORD, PVOID, DWORD) =
          (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
      if (p && SUCCEEDED(p(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked)
        return TRUE;
    }
  }
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND o = GetWindow(hwnd, GW_OWNER);
    if (o)
      return TRUE;
  }
  char title[256];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0)
    return TRUE;
  if (title[0] == '\0')
    return TRUE;
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return TRUE;
  int w = wr.right - wr.left;
  int h = wr.bottom - wr.top;
  if (w <= 0 || h <= 0)
    return TRUE;
  int cx = wr.left + w / 2;
  int cy = wr.top + h / 2;
  if (cx <= s->fcx)
    return TRUE;
  int gap = cx - s->fcx;
  int vdist = cy >= s->fcy ? cy - s->fcy : s->fcy - cy;
  if (gap < s->best_gap || (gap == s->best_gap && vdist < s->best_vdist)) {
    s->best_gap = gap;
    s->best_vdist = vdist;
    s->best = hwnd;
  }
  return TRUE;
}


static BOOL CALLBACK win32_focus_enum_up(HWND hwnd, LPARAM lp) {
  win32_focus_search *s = (win32_focus_search *)lp;
  if (!s)
    return TRUE;
  if (hwnd == GetForegroundWindow())
    return TRUE;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (IsIconic(hwnd))
    return TRUE;
  char cls[64];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0)
    if (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
        strcmp(cls, "DisplayMessageModalClass") == 0 ||
        strcmp(cls, "SearchPopupClass") == 0 ||
        strcmp(cls, "WidgetClass") == 0) {
      return TRUE;
    }
  {
    int cloaked = 0;
    HMODULE dwm = GetModuleHandleA("dwmapi.dll");
    if (dwm) {
      HRESULT(WINAPI * p)(HWND, DWORD, PVOID, DWORD) =
          (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
      if (p && SUCCEEDED(p(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked)
        return TRUE;
    }
  }
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND o = GetWindow(hwnd, GW_OWNER);
    if (o)
      return TRUE;
  }
  char title[256];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0)
    return TRUE;
  if (title[0] == '\0')
    return TRUE;
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return TRUE;
  int w = wr.right - wr.left;
  int h = wr.bottom - wr.top;
  if (w <= 0 || h <= 0)
    return TRUE;
  int cx = wr.left + w / 2;
  int cy = wr.top + h / 2;
  if (cy >= s->fcy)
    return TRUE;
  int gap = s->fcy - cy;
  int hdist = cx >= s->fcx ? cx - s->fcx : s->fcx - cx;
  if (gap < s->best_gap || (gap == s->best_gap && hdist < s->best_vdist)) {
    s->best_gap = gap;
    s->best_vdist = hdist;
    s->best = hwnd;
  }
  return TRUE;
}

static BOOL CALLBACK win32_focus_enum_down(HWND hwnd, LPARAM lp) {
  win32_focus_search *s = (win32_focus_search *)lp;
  if (!s)
    return TRUE;
  if (hwnd == GetForegroundWindow())
    return TRUE;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (IsIconic(hwnd))
    return TRUE;
  char cls[64];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0)
    if (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
        strcmp(cls, "DisplayMessageModalClass") == 0 ||
        strcmp(cls, "SearchPopupClass") == 0 ||
        strcmp(cls, "WidgetClass") == 0) {
      return TRUE;
    }
  {
    int cloaked = 0;
    HMODULE dwm = GetModuleHandleA("dwmapi.dll");
    if (dwm) {
      HRESULT(WINAPI * p)(HWND, DWORD, PVOID, DWORD) =
          (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
      if (p && SUCCEEDED(p(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked)
        return TRUE;
    }
  }
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND o = GetWindow(hwnd, GW_OWNER);
    if (o)
      return TRUE;
  }
  char title[256];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0)
    return TRUE;
  if (title[0] == '\0')
    return TRUE;
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return TRUE;
  int w = wr.right - wr.left;
  int h = wr.bottom - wr.top;
  if (w <= 0 || h <= 0)
    return TRUE;
  int cx = wr.left + w / 2;
  int cy = wr.top + h / 2;
  if (cy <= s->fcy)
    return TRUE;
  int gap = cy - s->fcy;
  int hdist = cx >= s->fcx ? cx - s->fcx : s->fcx - cx;
  if (gap < s->best_gap || (gap == s->best_gap && hdist < s->best_vdist)) {
    s->best_gap = gap;
    s->best_vdist = hdist;
    s->best = hwnd;
  }
  return TRUE;
}

bool Platform_MoveActiveWindowToMonitor(bool right) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  if (IsIconic(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  if (IsZoomed(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return false;
  int cx = wr.left + (wr.right - wr.left) / 2;
  int cy = wr.top + (wr.bottom - wr.top) / 2;
  win32_mon_list list;
  list.count = 0;
  EnumDisplayMonitors(NULL, NULL, win32_mon_enum, (LPARAM)&list);
  if (list.count < 2)
    return false;
  for (int i = 0; i < list.count - 1; i++) {
    for (int j = i + 1; j < list.count; j++) {
      if (list.rects[j].left < list.rects[i].left) {
        RECT t = list.rects[i];
        list.rects[i] = list.rects[j];
        list.rects[j] = t;
      }
    }
  }
  int cur = 0;
  for (int i = 0; i < list.count; i++) {
    if (cx >= list.rects[i].left && cx < list.rects[i].right &&
        cy >= list.rects[i].top && cy < list.rects[i].bottom) {
      cur = i;
      break;
    }
    if (list.rects[i].left <= cx)
      cur = i;
  }
  int dst = right ? cur + 1 : cur - 1;
  if (dst < 0 || dst >= list.count)
    return false;
  int w = wr.right - wr.left;
  int h = wr.bottom - wr.top;
  int nx = list.rects[dst].left + (cx - list.rects[cur].left);
  int ny = list.rects[dst].top + (wr.top - list.rects[cur].top);
  int dw = list.rects[dst].right - list.rects[dst].left;
  int dh = list.rects[dst].bottom - list.rects[dst].top;
  if (w > dw)
    w = dw;
  if (h > dh)
    h = dh;
  if (nx < list.rects[dst].left)
    nx = list.rects[dst].left;
  if (ny < list.rects[dst].top)
    ny = list.rects[dst].top;
  if (nx + w > list.rects[dst].right)
    nx = list.rects[dst].right - w;
  if (ny + h > list.rects[dst].bottom)
    ny = list.rects[dst].bottom - h;
  SetWindowPos(hwnd, HWND_TOP, nx, ny, w, h, SWP_SHOWWINDOW);
  return true;
}

bool Platform_MoveActiveWindowLeft(void) {
  return Platform_MoveActiveWindowToMonitor(false);
}

bool Platform_MoveActiveWindowRight(void) {
  return Platform_MoveActiveWindowToMonitor(true);
}

static bool win32_activate_hwnd(HWND h) {

  if (!h || !IsWindow(h))
    return false;
  if (IsIconic(h))
    ShowWindow(h, SW_RESTORE);
  ShowWindow(h, SW_SHOW);
  SetWindowPos(h, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  HWND fg = GetForegroundWindow();
  DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
  DWORD tTid = GetWindowThreadProcessId(h, NULL);
  DWORD curTid = GetCurrentThreadId();
  if (fgTid && fgTid != curTid)
    AttachThreadInput(fgTid, curTid, TRUE);
  if (tTid && tTid != curTid)
    AttachThreadInput(tTid, curTid, TRUE);
  AllowSetForegroundWindow(ASFW_ANY);
  SetForegroundWindow(h);
  SetActiveWindow(h);
  BringWindowToTop(h);
  if (fgTid && fgTid != curTid)
    AttachThreadInput(fgTid, curTid, FALSE);
  if (tTid && tTid != curTid)
    AttachThreadInput(tTid, curTid, FALSE);
  return GetForegroundWindow() == h;
}
bool Platform_FocusWindowLeft(void) {
  HWND fg = win32_foreground_target();
  if (!fg)
    fg = GetForegroundWindow();
  char cls[64] = {0};
  if (fg)
    GetClassNameA(fg, cls, sizeof(cls));
  if (fg && (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
             strcmp(cls, "DisplayMessageModalClass") == 0 ||
             strcmp(cls, "SearchPopupClass") == 0 ||
             strcmp(cls, "WidgetClass") == 0)) {
  }
  if (!fg)
    return false;
  RECT fr;
  if (!GetWindowRect(fg, &fr))
    return false;
  win32_focus_search s;
  s.fcx = fr.left + (fr.right - fr.left) / 2;
  s.fcy = fr.top + (fr.bottom - fr.top) / 2;
  s.best = NULL;
  s.best_gap = 1 << 30;
  s.best_vdist = 1 << 30;
  EnumWindows(win32_focus_enum_left, (LPARAM)&s);
  if (!s.best)
    return false;
  return win32_activate_hwnd(s.best);
}

bool Platform_FocusWindowRight(void) {
  HWND fg = win32_foreground_target();
  if (!fg)
    fg = GetForegroundWindow();
  char cls[64] = {0};
  if (fg)
    GetClassNameA(fg, cls, sizeof(cls));
  if (fg && (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
             strcmp(cls, "DisplayMessageModalClass") == 0 ||
             strcmp(cls, "SearchPopupClass") == 0 ||
             strcmp(cls, "WidgetClass") == 0)) {
  }
  if (!fg)
    return false;
  RECT fr;
  if (!GetWindowRect(fg, &fr))
    return false;
  win32_focus_search s;
  s.fcx = fr.left + (fr.right - fr.left) / 2;
  s.fcy = fr.top + (fr.bottom - fr.top) / 2;
  s.best = NULL;
  s.best_gap = 1 << 30;
  s.best_vdist = 1 << 30;
  EnumWindows(win32_focus_enum_right, (LPARAM)&s);
  if (!s.best)
    return false;
  return win32_activate_hwnd(s.best);
}


bool Platform_FocusWindowUp(void) {
  HWND fg = win32_foreground_target();
  if (!fg)
    fg = GetForegroundWindow();
  char cls[64] = {0};
  if (fg)
    GetClassNameA(fg, cls, sizeof(cls));
  if (!fg)
    return false;
  RECT fr;
  if (!GetWindowRect(fg, &fr))
    return false;
  win32_focus_search s;
  s.fcx = fr.left + (fr.right - fr.left) / 2;
  s.fcy = fr.top + (fr.bottom - fr.top) / 2;
  s.best = NULL;
  s.best_gap = 1 << 30;
  s.best_vdist = 1 << 30;
  EnumWindows(win32_focus_enum_up, (LPARAM)&s);
  if (!s.best)
    return false;
  return win32_activate_hwnd(s.best);
}

bool Platform_FocusWindowDown(void) {
  HWND fg = win32_foreground_target();
  if (!fg)
    fg = GetForegroundWindow();
  char cls[64] = {0};
  if (fg)
    GetClassNameA(fg, cls, sizeof(cls));
  if (!fg)
    return false;
  RECT fr;
  if (!GetWindowRect(fg, &fr))
    return false;
  win32_focus_search s;
  s.fcx = fr.left + (fr.right - fr.left) / 2;
  s.fcy = fr.top + (fr.bottom - fr.top) / 2;
  s.best = NULL;
  s.best_gap = 1 << 30;
  s.best_vdist = 1 << 30;
  EnumWindows(win32_focus_enum_down, (LPARAM)&s);
  if (!s.best)
    return false;
  return win32_activate_hwnd(s.best);
}

bool Platform_MoveActiveWindowUp(void) { return false; }
bool Platform_MoveActiveWindowDown(void) { return false; }

bool Platform_MinimizeActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (hwnd && !IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_MINIMIZE);
    g_win32_last_min = hwnd;
    return true;
  }
  if (hwnd && IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    if (g_win32_last_min == hwnd)
      g_win32_last_min = NULL;
    return true;
  }
  if (g_win32_last_min && IsWindow(g_win32_last_min) &&
      IsIconic(g_win32_last_min)) {
    ShowWindow(g_win32_last_min, SW_RESTORE);
    SetForegroundWindow(g_win32_last_min);
    g_win32_last_min = NULL;
    return true;
  }
  g_win32_last_min = NULL;
  return false;
}

char *Platform_GetActiveWindowTitle(void) {
  static char fgTitle[256];
  HWND hForeground = GetForegroundWindow();

  if (hForeground != NULL) {
    GetWindowTextA(hForeground, fgTitle, sizeof(fgTitle));
  } else {
    fgTitle[0] = '\0';
  }
  return fgTitle;
}

char *Platform_GetActiveWindowAppId(void) {
  static char appId[256];
  appId[0] = '\0';
  HWND hForeground = GetForegroundWindow();
  if (hForeground == NULL)
    return appId;
  DWORD pid = 0;
  GetWindowThreadProcessId(hForeground, &pid);
  if (pid == 0)
    return appId;
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h)
    return appId;
  char exe[MAX_PATH];
  DWORD sz = sizeof(exe);
  if (QueryFullProcessImageNameA(h, 0, exe, &sz) && sz > 0) {
    const char *base = strrchr(exe, '\\');
    base = base ? base + 1 : exe;
    strncpy(appId, base, sizeof(appId) - 1);
    appId[sizeof(appId) - 1] = '\0';
    size_t n = strlen(appId);
    if (n > 4 && _stricmp(appId + n - 4, ".exe") == 0)
      appId[n - 4] = '\0';
  }
  CloseHandle(h);
  return appId;
}
static bool win32_placeholder_icon(char *out, size_t out_len) {

  char cand[MAX_PATH];
  snprintf(cand, sizeof(cand), "resources\\Placeholder.png");
  if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) {
    snprintf(out, out_len, "%s", cand);
    return true;
  }
  char exe[MAX_PATH];
  DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe));
  if (n == 0 || n >= sizeof(exe))
    return false;
  char *slash = strrchr(exe, '\\');
  if (!slash)
    return false;
  *slash = '\0';
  for (int depth = 0; depth < 4; depth++) {
    snprintf(cand, sizeof(cand), "%s\\resources\\Placeholder.png", exe);
    if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) {
      snprintf(out, out_len, "%s", cand);
      return true;
    }
    slash = strrchr(exe, '\\');
    if (!slash)
      break;
    *slash = '\0';
  }
  return false;
}

static ULONG_PTR g_gdiplusToken = 0;

static bool win32_is_shell_icon(const char *p) {
  const char *dot = strrchr(p, '.');
  if (!dot)
    return false;
  return (_stricmp(dot, ".lnk") == 0 || _stricmp(dot, ".exe") == 0 ||
          _stricmp(dot, ".url") == 0);
}

static unsigned win32_icon_hash(const char *s) {
  unsigned h = 2166136261u;
  while (s && *s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

static bool win32_icon_cache_dir(char *out, size_t n) {
  char base[MAX_PATH];
  snprintf(base, sizeof(base), "%s\\AppData\\Roaming\\Flint",
           Platform_GetRealHome());
  Platform_MkdirIfNotExists(base);
  snprintf(out, n, "%s\\icon_cache", base);
  Platform_MkdirIfNotExists(out);
  return true;
}

static bool win32_icon_file_ok(const char *path) {
  WIN32_FILE_ATTRIBUTE_DATA fad;
  if (!path || !path[0])
    return false;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad))
    return false;
  if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    return false;
  DWORD hi = fad.nFileSizeHigh, lo = fad.nFileSizeLow;
  return (hi != 0 || lo != 0);
}

static bool win32_icon_png_path(const char *src, char *out, size_t n) {
  char dir[MAX_PATH];
  win32_icon_cache_dir(dir, sizeof(dir));
  snprintf(out, n, "%s\\%08x_v2.png", dir, win32_icon_hash(src));
  return true;
}

static void win32_gdiplus_ready(void) {
  if (g_gdiplusToken != 0)
    return;
  GdiplusStartupInput in;
  in.GdiplusVersion = 1;
  in.DebugEventCallback = NULL;
  in.SuppressBackgroundThread = FALSE;
  in.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&g_gdiplusToken, &in, NULL);
}

/* Shell icon extraction costs several ms and cannot be cached by the draw path,
   so flatten it to a PNG once and hand that back as the icon path. */
static bool win32_export_shell_icon_png(const char *native_path) {
  char png[MAX_PATH];
  if (!win32_icon_png_path(native_path, png, sizeof(png)))
    return false;
  if (win32_icon_file_ok(png))
    return true;

  SHFILEINFOA sfi;
  ZeroMemory(&sfi, sizeof(sfi));
  if (!SHGetFileInfoA(native_path, 0, &sfi, sizeof(sfi),
                      SHGFI_ICON | SHGFI_LARGEICON))
    return false;
  if (!sfi.hIcon)
    return false;

  win32_gdiplus_ready();
  GpBitmap *src = NULL;
  GpStatus st = GdipCreateBitmapFromHICON(sfi.hIcon, &src);
  DestroyIcon(sfi.hIcon);
  if (st != Ok || !src) {
    if (src)
      GdipDisposeImage(src);
    return false;
  }

  const int px = 32;
  unsigned char *zeros = (unsigned char *)calloc((size_t)px * (size_t)px, 4);
  GpBitmap *flat = NULL;
  if (!zeros ||
      GdipCreateBitmapFromScan0(px, px, px * 4, PixelFormat32bppARGB, zeros,
                                &flat) != Ok ||
      !flat) {
    if (zeros)
      free(zeros);
    GdipDisposeImage(src);
    return false;
  }
  GpGraphics *g = NULL;
  if (GdipGetImageGraphicsContext(flat, &g) == Ok && g) {
    GdipSetInterpolationMode(g, InterpolationModeHighQualityBicubic);
    GdipSetCompositingMode(g, CompositingModeSourceOver);
    GdipSetSmoothingMode(g, SmoothingModeHighQuality);
    GdipDrawImageRectI(g, src, 0, 0, px, px);
    GdipDeleteGraphics(g);
  }
  GdipDisposeImage(src);

  static const GUID kPng = {0x557cf406,
                            0x1a04,
                            0x11d3,
                            {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
  wchar_t wpng[MAX_PATH * 2];
  int wn = MultiByteToWideChar(CP_UTF8, 0, png, -1, wpng,
                               (int)(sizeof(wpng) / sizeof(wchar_t)));
  GpStatus save_st =
      wn > 0 ? GdipSaveImageToFile(flat, wpng, &kPng, NULL) : GenericError;
  bool ok = save_st == Ok;
  if (!ok) {
    log_error("win32_export_shell_icon_png save failed st=%d path='%s'",
              (int)save_st, png);
    DeleteFileA(png);
  } else {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    DWORD sz = 0;
    if (GetFileAttributesExA(png, GetFileExInfoStandard, &fad))
      sz = (fad.nFileSizeHigh << 16) | fad.nFileSizeLow;
    log_info("win32_export_shell_icon_png saved='%s' size=%u", png, sz);
  }

  GdipDisposeImage(flat);
  free(zeros);
  return ok;
}

char *Platform_GetIconPath(char *app_path) {
  if (!app_path || !app_path[0]) {
    log_error("operation failed, returning error app_path='%s'",
              app_path ? app_path : "null");
    return NULL;
  }
  char expanded[MAX_PATH];
  const char *src = app_path;
  if (Platform_ExpandPath(app_path, expanded, sizeof(expanded))) {
    if (expanded[0])
      src = expanded;
  }
  if (strncmp(src, "shell:", 6) == 0)
    return NULL;
  char native[MAX_PATH];
  snprintf(native, sizeof(native), "%s", src);
  for (char *p = native; *p; p++)
    if (*p == '/')
      *p = '\\';
  src = native;
  if (GetFileAttributesA(src) != INVALID_FILE_ATTRIBUTES) {
    if (win32_is_shell_icon(src)) {
      char png[MAX_PATH];
      if (win32_icon_png_path(src, png, sizeof(png)) &&
          win32_icon_file_ok(png)) {
        return _strdup(png);
      }
      if (win32_export_shell_icon_png(src)) {
        return _strdup(png);
      }
    }
    return _strdup(src);
  }
  char full[MAX_PATH];
  if (SearchPathA(NULL, src, NULL, MAX_PATH, full, NULL) &&
      GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES) {
    return _strdup(full);
  }
  if (!strchr(src, '.')) {
    char withExe[MAX_PATH];
    snprintf(withExe, sizeof(withExe), "%s.exe", src);
    if (SearchPathA(NULL, withExe, NULL, MAX_PATH, full, NULL) &&
        GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES) {
      return _strdup(full);
    }
  }
  log_info("Platform_GetIconPath app_path='%s'", app_path ? app_path : "");
  ;
  char ph[MAX_PATH];
  if (win32_placeholder_icon(ph, sizeof(ph)))
    return _strdup(ph);
  return NULL;
}
int Platform_GetIconSize(char *icon_path) {
  (void)icon_path;
  extern int IconSize;
  int s = IconSize;
  if (s < 16)
    s = 16;
  if (s > 32)
    s = 32;
  log_info("Platform_GetIconSize icon_path='%s'", icon_path ? icon_path : "");
  ;
  return s;
}

typedef struct {
  PlWindowList *titles;
  PlWindowList *names;
} WinPair;

static BOOL CALLBACK EnumWindowsProcPair(HWND hwnd, LPARAM lParam) {

  WinPair *pair = (WinPair *)lParam;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  char cls[256];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0) {
    if (strcmp(cls, "Progman") == 0 || strcmp(cls, "WorkerW") == 0)
      return TRUE;
    if (strcmp(cls, "QuickSwitcherModalClass") == 0 ||
        strcmp(cls, "DisplayMessageModalClass") == 0 ||
        strcmp(cls, "SearchPopupClass") == 0 ||
        strcmp(cls, "WidgetClass") == 0)
      return TRUE;
    if (strcmp(cls, "ApplicationFrameWindow") == 0) {
      int cloaked = 0;
      HMODULE dwm = GetModuleHandleA("dwmapi.dll");
      if (dwm) {
        HRESULT(WINAPI * pDwmGet)(HWND, DWORD, PVOID, DWORD) =
            (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
        if (pDwmGet)
          pDwmGet(hwnd, 14, &cloaked, sizeof(cloaked));
      }
      if (cloaked)
        return TRUE;
    }
  }
  {
    int cloaked = 0;
    HMODULE dwm = GetModuleHandleA("dwmapi.dll");
    if (dwm) {
      HRESULT(WINAPI * pDwmGet)(HWND, DWORD, PVOID, DWORD) =
          (void *)GetProcAddress(dwm, "DwmGetWindowAttribute");
      if (pDwmGet && SUCCEEDED(pDwmGet(hwnd, 14, &cloaked, sizeof(cloaked))) &&
          cloaked)
        return TRUE;
    }
  }
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner)
      return TRUE;
  }
  RECT wr;
  if (!GetWindowRect(hwnd, &wr))
    return TRUE;
  if (wr.right - wr.left <= 0 || wr.bottom - wr.top <= 0)
    return TRUE;
  char title[512];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0 || title[0] == '\0')
    return TRUE;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0)
    return TRUE;
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h)
    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!h)
    return TRUE;
  char exe[MAX_PATH] = {0};
  char exePath[MAX_PATH] = {0};
  DWORD sz = sizeof(exePath);
  DWORD len = 0;
  if (QueryFullProcessImageNameA(h, 0, exePath, &sz) && sz > 0) {
    char *base = strrchr(exePath, '\\');
    if (base) {
      strncpy(exe, base + 1, sizeof(exe) - 1);
      len = strlen(exe);
    } else {
      strncpy(exe, exePath, sizeof(exe) - 1);
      len = strlen(exe);
    }
  } else {
    len = GetModuleBaseNameA(h, NULL, exe, sizeof(exe));
  }
  CloseHandle(h);
  if (len == 0 || exe[0] == '\0')
    return TRUE;
  if (_stricmp(exe, "SystemSettings.exe") == 0)
    return TRUE;
  if (_stricmp(exe, "ApplicationFrameHost.exe") == 0)
    return TRUE;
  for (int i = 0; i < pair->titles->count; i++)
    if (pair->titles->windows[i] &&
        _stricmp(pair->titles->windows[i], title) == 0) {
      return TRUE;
    }
  char **t1 = realloc(pair->titles->windows,
                      (pair->titles->count + 1) * sizeof(char *));
  char **t2 =
      realloc(pair->names->windows, (pair->names->count + 1) * sizeof(char *));
  if (!t1 || !t2) {
    free(t1);
    free(t2);
    return TRUE;
  }
  pair->titles->windows = t1;
  pair->names->windows = t2;
  pair->titles->windows[pair->titles->count++] = strdup(title);
  pair->names->windows[pair->names->count++] = strdup(exe);
  return TRUE;
}

static void win32_collect_pair(PlWindowList *out_titles,
                               PlWindowList *out_names) {

  WinPair p = {out_titles, out_names};
  EnumWindows(EnumWindowsProcPair, (LPARAM)&p);
}

PlWindowList Platform_GetWindowTitles() {
  PlWindowList titles = {0}, names = {0};
  win32_collect_pair(&titles, &names);
  for (int i = 0; i < names.count; i++)
    free(names.windows[i]);
  free(names.windows);
  log_info("Platform_GetWindowTitles count=%d", titles.count);
  return titles;
}
PlWindowList Platform_GetWindowNames() {
  PlWindowList titles = {0}, names = {0};
  win32_collect_pair(&titles, &names);
  for (int i = 0; i < titles.count; i++)
    free(titles.windows[i]);
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

static HHOOK s_win32_keyboard_hook = NULL;
static HHOOK s_win32_mouse_hook = NULL;

uint32_t g_win32_main_thread_id = 0;
WindowHandle g_win32_tray_wnd = {0};
WindowHandle g_win32_message_wnd = {0};

bool Platform_InitDisplaySafe(void) { return true; }

void Platform_CleanupDisplaySafe(void) {}

void Platform_InitMainThreadContext(void) {
  g_win32_main_thread_id = GetCurrentThreadId();
}

static int win32_dir_exists(const char *path) {

  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int win32_is_project_root(const char *dir) {

  char probe[MAX_PATH];
  snprintf(probe, sizeof(probe), "%s\\config", dir);
  if (!win32_dir_exists(probe))
    return 0;
  snprintf(probe, sizeof(probe), "%s\\build", dir);
  if (!win32_dir_exists(probe))
    return 0;
  return 1;
}

void Platform_FixWorkingDirectory(void) {
  char buf[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return;
  }
  char *slash = strrchr(buf, '\\');
  if (slash)
    *slash = '\0';
  char exe_dir[MAX_PATH];
  snprintf(exe_dir, sizeof(exe_dir), "%s", buf);

  for (int depth = 0; depth < 6; ++depth) {
    char probe[MAX_PATH];
    snprintf(probe, sizeof(probe), "%s\\data", buf);
    if (win32_dir_exists(probe) || win32_is_project_root(buf)) {
      if (SetCurrentDirectoryA(buf)) {
        snprintf(g_win32_project_root, sizeof(g_win32_project_root), "%s", buf);
        return;
      }
    }
    slash = strrchr(buf, '\\');
    if (!slash)
      break;
    *slash = '\0';
  }

  char cwd[MAX_PATH];
  DWORD cwd_len = GetCurrentDirectoryA(sizeof(cwd), cwd);
  if (cwd_len == 0 || cwd_len >= MAX_PATH || !win32_is_project_root(cwd)) {
    log_warn("could not locate project root from '%s', staying in '%s'",
             exe_dir, cwd);
    SetCurrentDirectoryA(exe_dir);
  }
  GetCurrentDirectoryA(sizeof(cwd), cwd);
  snprintf(g_win32_project_root, sizeof(g_win32_project_root), "%s", cwd);
  log_info("Platform_FixWorkingDirectory cwd='%s'", cwd);
}

void Platform_CleanupPlatformResources(void) {

  {
    extern void win32_default_apps_free_cache(void);
    win32_default_apps_free_cache();
  }
}

PlColor Platform_GetPixelColor(int x, int y) {
  PlColor out = {0, 0, 0};
  HDC screen = GetDC(NULL);
  if (!screen)
    return out;
  COLORREF c = GetPixel(screen, x, y);
  ReleaseDC(NULL, screen);
  if (c == CLR_INVALID)
    return out;
  out.r = GetRValue(c);
  out.g = GetGValue(c);
  out.b = GetBValue(c);
  return out;
}

bool Platform_MoveToPos(int x, int y) {
  int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

  if (screenWidth <= 0 || screenHeight <= 0)
    return false;

  long rx = (long)x - vx;
  long ry = (long)y - vy;
  if (rx < 0)
    rx = 0;
  if (ry < 0)
    ry = 0;
  if (rx > screenWidth)
    rx = screenWidth;
  if (ry > screenHeight)
    ry = screenHeight;

  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dx = (DWORD)((rx * 65535) / screenWidth);
  input.mi.dy = (DWORD)((ry * 65535) / screenHeight);
  input.mi.dwFlags =
      MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;

  bool ok = (SendInput(1, &input, sizeof(INPUT)) == 1);

  log_info("Platform_MoveToPos x=%d y=%d", x, y);
  return ok;
}

bool Platform_ClickButton(int button) {
  DWORD down_flag, up_flag;
  if (button == 1) {
    down_flag = MOUSEEVENTF_LEFTDOWN;
    up_flag = MOUSEEVENTF_LEFTUP;
  } else if (button == 2) {
    down_flag = MOUSEEVENTF_RIGHTDOWN;
    up_flag = MOUSEEVENTF_RIGHTUP;
  } else
    return false;
  INPUT inputs[2] = {0};
  inputs[0].type = INPUT_MOUSE;
  inputs[0].mi.dwFlags = down_flag;
  inputs[1].type = INPUT_MOUSE;
  inputs[1].mi.dwFlags = up_flag;
  bool ok = (SendInput(2, inputs, sizeof(INPUT)) == 2);
  return ok;
}

bool Platform_Click(void) {
  INPUT inputs[2] = {0};

  inputs[0].type = INPUT_MOUSE;
  inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

  inputs[1].type = INPUT_MOUSE;
  inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

  bool ok = (SendInput(2, inputs, sizeof(INPUT)) == 2);

  log_info("Platform_Click");
  return ok;
}

static DWORD win32_button_flag(int button, bool down) {

  switch (button) {
  case 1:
    return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
  case 2:
    return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
  case 3:
    return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
  case 4:
    return down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
  default:
    return 0;
  }
}

bool Platform_GetVirtualBounds(int *out_x, int *out_y, int *out_w, int *out_h) {
  if (out_x)
    *out_x = 0;
  if (out_y)
    *out_y = 0;
  if (out_w)
    *out_w = 0;
  if (out_h)
    *out_h = 0;
  int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (vw <= 0 || vh <= 0)
    return false;
  if (out_x)
    *out_x = vx;
  if (out_y)
    *out_y = vy;
  if (out_w)
    *out_w = vw;
  if (out_h)
    *out_h = vh;
  return true;
}

bool Platform_MouseButtonDown(int button, bool down) {
  DWORD flag = win32_button_flag(button, down);
  if (flag == 0)
    return false;
  INPUT in = {0};
  in.type = INPUT_MOUSE;
  in.mi.dwFlags = flag;
  in.mi.mouseData = (button == 5) ? XBUTTON2 : XBUTTON1;
  bool ok = SendInput(1, &in, sizeof(INPUT)) == 1;
  return ok;
}

bool Platform_MouseScroll(int notches) {
  if (notches == 0)
    return true;
  INPUT in = {0};
  in.type = INPUT_MOUSE;
  in.mi.dwFlags = MOUSEEVENTF_WHEEL;
  in.mi.mouseData = (DWORD)((notches > 0 ? notches : -notches) * WHEEL_DELTA);
  bool ok = SendInput(1, &in, sizeof(INPUT)) == 1;
  return ok;
}

char *Platform_GetWorkingDir() {
  return Platform_RunTerminalCommand("cd", false);
}

void Platform_InitDataFolder() {
  char dir_path[200];
  snprintf(dir_path, sizeof(dir_path), "%s\\AppData\\Roaming\\Flint",
           Platform_GetRealHome());
  Platform_MkdirIfNotExists(dir_path);
}

bool Platform_GetFeaturePath(const char *feature, const char *name, char *out,
                             size_t out_len) {

  if (!feature || !feature[0] || !name || !name[0] || !out || out_len == 0)
    return false;
  char dir_path[256];
  snprintf(dir_path, sizeof(dir_path), "%s\\AppData\\Roaming\\Flint",
           Platform_GetRealHome());
  Platform_MkdirIfNotExists(dir_path);
  char feat_path[320];
  snprintf(feat_path, sizeof(feat_path), "%s\\%s", dir_path, feature);
  Platform_MkdirIfNotExists(feat_path);
  snprintf(out, out_len, "%s\\%s", feat_path, name);
  return true;
}

bool Platform_ResolveDataPath(void *caller, const char *name, char *out,
                              size_t out_len) {

  bool ok = FeaturePath_ResolveDataPath(caller, name, out, out_len);
  return ok;
}

bool Platform_ResolveAssetPath(void *caller, const char *name, char *out,
                               size_t out_len) {

  bool ok = FeaturePath_ResolveAssetPath(caller, name, out, out_len);
  return ok;
}

bool Platform_FeatureDataDirExists(const char *stem) {
  if (!stem || !stem[0])
    return false;
  char dir_path[256];
  snprintf(dir_path, sizeof(dir_path), "%s\\AppData\\Roaming\\Flint\\%s",
           Platform_GetRealHome(), stem);
  bool ok = (GetFileAttributesA(dir_path) != INVALID_FILE_ATTRIBUTES);
  return ok;
}

bool Platform_ResolveAssetFile(const char *stem, const char *rel, char *out,
                               size_t out_len) {

  if (!stem || !rel || !out || out_len == 0)
    return false;
  char feature_base[512];
  if (FeaturePath[0] && FeaturePath[1] == ':') {
    snprintf(feature_base, sizeof(feature_base), "%s", FeaturePath);
  } else if (g_win32_project_root[0]) {
    snprintf(feature_base, sizeof(feature_base), "%s\\%s", g_win32_project_root,
             FeaturePath);
  } else {
    snprintf(feature_base, sizeof(feature_base), "%s", FeaturePath);
  }
  char full[600];
  snprintf(full, sizeof(full), "%s\\%s\\%s", feature_base, stem, rel);
  DWORD attr = GetFileAttributesA(full);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
    return false;
  }
  snprintf(out, out_len, "%s", full);
  return true;
}

bool Platform_WriteTempFile(const char *name, const void *data, size_t len,
                            char *out_path, size_t out_len) {

  if (!name || !name[0] || !out_path || out_len == 0)
    return false;
  if (!data && len > 0)
    return false;
  for (const char *p = name; *p; p++) {
    if (*p == '/' || *p == '\\')
      return false;
  }
  if (strstr(name, ".."))
    return false;
  char dir[MAX_PATH];
  DWORD n = GetTempPathA(sizeof(dir), dir);
  if (n == 0 || n >= sizeof(dir))
    return false;
  size_t dl = strlen(dir);
  while (dl > 0 && (dir[dl - 1] == '\\' || dir[dl - 1] == '/'))
    dir[--dl] = '\0';
  snprintf(out_path, out_len, "%s\\%s", dir, name);
  out_path[out_len - 1] = '\0';
  FILE *f = fopen(out_path, "wb");
  if (!f)
    return false;
  bool ok = true;
  if (len > 0 && fwrite(data, 1, len, f) != len)
    ok = false;
  if (fclose(f) != 0)
    ok = false;
  if (!ok)
    remove(out_path);
  return ok;
}

void Platform_PrewarmBackgroundPixmap(void) {
  win32_bg_image_purge();
  win32_bg_image_prewarm(PROMPT_W, PROMPT_H);
}

int Platform_InitializeTrayIcon() {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (Tray) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) {
      log_error("operation failed, returning error");
      return -1;
    }

    g_win32_tray_wnd = WH_FROM_HWND(
        CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName, "TrayApp", WS_POPUP,
                        0, 0, 0, 0, NULL, NULL, hInstance, NULL));

    if (WH_TO_HWND(g_win32_tray_wnd) == NULL) {
      return -1;
    }
  }
  log_info("Platform_InitializeTrayIcon completed");
  return 0;
}

void Platform_InitUI() {
  struct GdiplusStartupInput gdiplusStartupInput;
  gdiplusStartupInput.GdiplusVersion = 1;
  gdiplusStartupInput.DebugEventCallback = NULL;
  gdiplusStartupInput.SuppressBackgroundThread = FALSE;
  gdiplusStartupInput.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

  {
    char warm[MAX_PATH];
    if (SearchPathA(NULL, "shell32.dll", NULL, MAX_PATH, warm, NULL)) {
      SHFILEINFOA sfi;
      ZeroMemory(&sfi, sizeof(sfi));
      if (SHGetFileInfoA(warm, 0, &sfi, sizeof(sfi),
                         SHGFI_ICON | SHGFI_LARGEICON) &&
          sfi.hIcon) {
        DestroyIcon(sfi.hIcon);
      }
    }
    win32_gdiplus_ready();
  }

  WNDCLASSA wc = {0};
  wc.lpfnWndProc = PopupWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(NULL_BRUSH);
  wc.lpszClassName = "QuickSwitcherModalClass";

  if (RegisterClassA(&wc)) {
    log_info("QuickSwitcherModalClass registered successfully with OS.");
  } else {
    log_error("CRITICAL WARNING: Failed to register window class! Error: %lu",
              GetLastError());
  }

  WNDCLASSA wcFade = {0};
  wcFade.lpfnWndProc = DisplayMessageProc;
  wcFade.hInstance = GetModuleHandle(NULL);
  wcFade.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcFade.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
  wcFade.lpszClassName = "DisplayMessageModalClass";
  RegisterClassA(&wcFade);

  s_win32_keyboard_hook =
      SetWindowsHookExA(WH_KEYBOARD_LL, win32_handle_global_key, NULL, 0);
  if (!s_win32_keyboard_hook) {
    log_warn("WH_KEYBOARD_LL hook FAILED to install err=%lu", GetLastError());
  } else {
    log_warn("WH_KEYBOARD_LL hook installed handle=%p",
             (void *)s_win32_keyboard_hook);
  }

  Platform_RegisterRawMouse(WH_TO_HWND(g_win32_tray_wnd));
  s_win32_mouse_hook = NULL;

  FontHandle warm = Platform_CreateFont(FontSize, UsedFont);
  log_warn("warm font size=%d face='%s' created=%d", FontSize,
           UsedFont ? UsedFont : "", FH_TO_HFONT(warm) ? 1 : 0);
  Platform_DestroyFont(warm);
  log_info("Platform_InitUI completed");
}

void Platform_ShutdownUI() {
  {
    extern void win32_img_cache_free_all(void);
    win32_img_cache_free_all();
  }
  if (s_win32_keyboard_hook) {
    UnhookWindowsHookEx((HHOOK)s_win32_keyboard_hook);
    s_win32_keyboard_hook = NULL;
  }
  if (s_win32_mouse_hook) {
    UnhookWindowsHookEx((HHOOK)s_win32_mouse_hook);
    s_win32_mouse_hook = NULL;
  }
  if (g_gdiplusToken != 0) {
    GdiplusShutdown(g_gdiplusToken);
  }
  log_info("Platform_ShutdownUI completed");
}

static void Platform_http_buf_init(HttpBuf *buf, size_t limit) {
  memset(buf, 0, sizeof(*buf));
  buf->limit = limit;
  if (limit) {
    buf->buf = (char *)malloc(limit + 1);
    if (buf->buf) {
      buf->buf[0] = '\0';
      buf->cap = limit + 1;
    }
  }
}

int Platform_http_perform(const char *url, const char *method,
                          const char *headers, const char *body,
                          HttpBuf *resp_buf, FILE *file_out,
                          long max_redirects) {
  if (!url || !url[0])
    return 0;

  HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
  int status_code = 0;
  int redirects_followed = 0;
  char current_url[L_MAX_URL_LENGTH];
  strncpy_s(current_url, sizeof(current_url), url, _TRUNCATE);

  hSession = WinHttpOpen(L"PlatformHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_BYPASS, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return 0;

redirect_loop: {
  URL_COMPONENTS urlComp;
  ZeroMemory(&urlComp, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);

  wchar_t szHostName[256];
  wchar_t szUrlPath[2048];

  urlComp.lpszHostName = szHostName;
  urlComp.dwHostNameLength = ARRAYSIZE(szHostName);
  urlComp.lpszUrlPath = szUrlPath;
  urlComp.dwUrlPathLength = ARRAYSIZE(szUrlPath);

  wchar_t wUrl[L_MAX_URL_LENGTH];
  MultiByteToWideChar(CP_UTF8, 0, current_url, -1, wUrl, ARRAYSIZE(wUrl));

  if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    WinHttpCloseHandle(hSession);
    return 0;
  }

  if (hConnect)
    WinHttpCloseHandle(hConnect);
  hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return 0;
  }

  DWORD dwFlags =
      (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

  wchar_t wMethod[32];
  MultiByteToWideChar(CP_UTF8, 0, method ? method : "GET", -1, wMethod,
                      ARRAYSIZE(wMethod));

  if (hRequest)
    WinHttpCloseHandle(hRequest);
  hRequest = WinHttpOpenRequest(hConnect, wMethod, urlComp.lpszUrlPath, NULL,
                                WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
  }

  wchar_t *wHeaders = NULL;
  DWORD wHeadersLen = 0;
  if (headers && headers[0]) {
    int len = MultiByteToWideChar(CP_UTF8, 0, headers, -1, NULL, 0);
    wHeaders = (wchar_t *)malloc(len * sizeof(wchar_t));
    if (wHeaders) {
      MultiByteToWideChar(CP_UTF8, 0, headers, -1, wHeaders, len);
      wHeadersLen = (DWORD)(len - 1);
    }
  }

  DWORD bodyLen = body ? (DWORD)strlen(body) : 0;
  BOOL bResults =
      WinHttpSendRequest(hRequest, wHeaders, wHeadersLen,
                         (LPVOID)(bodyLen > 0 ? body : WINHTTP_NO_REQUEST_DATA),
                         bodyLen, bodyLen, 0);
  if (wHeaders)
    free(wHeaders);

  if (!bResults || !WinHttpReceiveResponse(hRequest, NULL)) {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 0;
  }

  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                      WINHTTP_NO_HEADER_INDEX);
  status_code = (int)dwStatusCode;

  if ((status_code == 301 || status_code == 302 || status_code == 303 ||
       status_code == 307 || status_code == 308) &&
      redirects_followed < max_redirects) {

    wchar_t szLocation[L_MAX_URL_LENGTH];
    DWORD dwLocSize = sizeof(szLocation);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                            WINHTTP_HEADER_NAME_BY_INDEX, szLocation,
                            &dwLocSize, WINHTTP_NO_HEADER_INDEX)) {
      WideCharToMultiByte(CP_UTF8, 0, szLocation, -1, current_url,
                          sizeof(current_url), NULL, NULL);
      redirects_followed++;
      if (status_code == 303 || ((status_code == 301 || status_code == 302) &&
                                 strcmp(method, "POST") == 0)) {
        method = "GET";
        body = NULL;
      }
      goto redirect_loop;
    }
  }

  while (TRUE) {
    DWORD dwSizeAvailable = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvailable) ||
        dwSizeAvailable == 0) {
      break;
    }

    LPSTR pszOutBuffer = (LPSTR)malloc(dwSizeAvailable + 1);
    if (!pszOutBuffer)
      break;

    ZeroMemory(pszOutBuffer, dwSizeAvailable + 1);
    DWORD dwRead = 0;
    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSizeAvailable,
                        &dwRead) &&
        dwRead > 0) {
      if (file_out) {
        fwrite(pszOutBuffer, 1, dwRead, file_out);
      } else if (resp_buf) {
        size_t space_left = resp_buf->limit > resp_buf->len
                                ? resp_buf->limit - resp_buf->len
                                : 0;
        if (dwRead > space_left) {
          resp_buf->overflow = true;
          dwRead = (DWORD)space_left;
        }
        if (dwRead > 0) {
          memcpy(resp_buf->buf + resp_buf->len, pszOutBuffer, dwRead);
          resp_buf->len += dwRead;
          resp_buf->buf[resp_buf->len] = '\0';
        }
      }
    }
    free(pszOutBuffer);
    if (resp_buf && resp_buf->overflow)
      break;
  }
}

  if (hRequest)
    WinHttpCloseHandle(hRequest);
  if (hConnect)
    WinHttpCloseHandle(hConnect);
  if (hSession)
    WinHttpCloseHandle(hSession);
  return status_code;
}

char *Platform_HttpGetLimited(const char *url, size_t max_bytes,
                              size_t *out_len) {
  if (!url || !url[0]) {
    if (out_len)
      *out_len = 0;
    return NULL;
  }
  HttpBuf buf;
  Platform_http_buf_init(&buf, max_bytes ? max_bytes : HTTP_DEFAULT_LIMIT);
  int ok = Platform_http_perform(url, "GET", NULL, NULL, &buf, NULL,
                                 HTTP_MAX_REDIRECTS);
  if (out_len)
    *out_len = buf.len;
  if (ok < 200 || ok >= 300) {
    free(buf.buf);
    return NULL;
  }
  if (!buf.buf)
    buf.buf = (char *)calloc(1, 1);
  return buf.buf;
}

char *Platform_HttpGet(const char *url) {
  return Platform_HttpGetLimited(url, HTTP_DEFAULT_LIMIT, NULL);
}

char *Platform_HttpPost(const char *url, const char *content_type,
                        const char *body) {
  if (!url || !url[0])
    return NULL;
  HttpBuf buf;
  Platform_http_buf_init(&buf, HTTP_DEFAULT_LIMIT);
  int ok = Platform_http_perform(url, "POST", content_type, body ? body : "",
                                 &buf, NULL, HTTP_MAX_REDIRECTS);
  if (ok < 200 || ok >= 300) {
    free(buf.buf);
    return NULL;
  }
  if (!buf.buf)
    buf.buf = (char *)calloc(1, 1);
  return buf.buf;
}

bool Platform_HttpDownload(const char *url, const char *save_path) {
  if (!url || !url[0] || !save_path || !save_path[0])
    return false;
  char tmp[MAX_PATH];
  snprintf(tmp, sizeof(tmp), "%s.part", save_path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return false;

  HttpBuf unused;
  memset(&unused, 0, sizeof(unused));
  int ok = Platform_http_perform(url, "GET", NULL, NULL, &unused, f,
                                 HTTP_MAX_REDIRECTS);
  fclose(f);

  if (ok < 200 || ok >= 300) {
    remove(tmp);
    return false;
  }
  remove(save_path);
  if (rename(tmp, save_path) != 0) {
    remove(tmp);
    return false;
  }
  return true;
}

typedef void (*PlatformHttpCallback)(const char *body, bool success,
                                     void *userdata);

typedef struct {
  char *url;
  PlatformHttpCallback cb;
  void *userdata;
} HttpAsyncJob;

static DWORD WINAPI http_async_worker(LPVOID arg) {
  HttpAsyncJob *job = (HttpAsyncJob *)arg;
  char *body = Platform_HttpGet(job->url);
  if (job->cb) {
    job->cb(body ? body : "", body != NULL, job->userdata);
  }
  free(body);
  free(job->url);
  free(job);
  return 0;
}

void Platform_HttpGetAsync(const char *url, PlatformHttpCallback cb,
                           void *userdata) {
  if (!url || !url[0] || !cb)
    return;
  HttpAsyncJob *job = (HttpAsyncJob *)calloc(1, sizeof(*job));
  if (!job)
    return;
  job->url = _strdup(url);
  job->cb = cb;
  job->userdata = userdata;
  if (!job->url) {
    free(job);
    return;
  }

  HANDLE t = CreateThread(NULL, 0, http_async_worker, job, 0, NULL);
  if (!t) {
    free(job->url);
    free(job);
    return;
  }
  CloseHandle(t);
}
bool Platform_FontInitalized(FontHandle font) { return font.hfont != NULL; }

PlTimer Platform_StartTimer(void) {
  PlTimer t = {0};
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  t.ticks = (uint64_t)c.QuadPart;
  return t;
}

double Platform_EndTimer(PlTimer t) {
  LARGE_INTEGER now;
  LARGE_INTEGER freq;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&freq);
  if (freq.QuadPart == 0)
    return 0.0;
  uint64_t start = t.ticks;
  uint64_t end = (uint64_t)now.QuadPart;
  if (end < start)
    return 0.0;
  return (double)(end - start) * 1000.0 / (double)freq.QuadPart;
}

typedef struct {
  void *(*fn)(void *);
  void *arg;
} WinThreadStart;

static DWORD WINAPI win_thread_entry(LPVOID p) {

  WinThreadStart *s = (WinThreadStart *)p;
  void *(*fn)(void *) = s->fn;
  void *arg = s->arg;
  free(s);
  fn(arg);
  return 0;
}

void *Platform_ThreadCreate(void *(*fn)(void *), void *arg) {
  if (!fn)
    return NULL;
  WinThreadStart *s = malloc(sizeof(*s));
  if (!s)
    return NULL;
  s->fn = fn;
  s->arg = arg;
  HANDLE h = CreateThread(NULL, 0, win_thread_entry, s, 0, NULL);
  if (!h) {
    free(s);
    return NULL;
  }
  return (void *)h;
}

void Platform_ThreadJoin(void *thread) {
  if (!thread) {
    return;
  }
  DWORD w = WaitForSingleObject((HANDLE)thread, 250);
  if (w == WAIT_TIMEOUT) {
    TerminateThread((HANDLE)thread, 0);
    WaitForSingleObject((HANDLE)thread, INFINITE);
  }
  CloseHandle((HANDLE)thread);
}

void *Platform_MutexCreate(void) {
  CRITICAL_SECTION *cs = malloc(sizeof(*cs));
  if (!cs)
    return NULL;
  InitializeCriticalSection(cs);
  return cs;
}

void Platform_MutexDestroy(void *mutex) {
  if (!mutex) {
    return;
  }
  CRITICAL_SECTION *cs = (CRITICAL_SECTION *)mutex;
  DeleteCriticalSection(cs);
  free(cs);
}

void Platform_MutexLock(void *mutex) {
  if (!mutex) {
    return;
  }
  EnterCriticalSection((CRITICAL_SECTION *)mutex);
}

void Platform_MutexUnlock(void *mutex) {
  if (!mutex) {
    return;
  }
  LeaveCriticalSection((CRITICAL_SECTION *)mutex);
}

bool Platform_MutexTryLock(void *mutex) {
  if (!mutex)
    return false;
  return TryEnterCriticalSection((CRITICAL_SECTION *)mutex) != 0;
}

static PlatformKeyInterceptorFn g_platform_key_interceptor = NULL;

void Platform_RegisterKeyInterceptor(PlatformKeyInterceptorFn fn) {
  g_platform_key_interceptor = fn;
}

bool Platform_InvokeKeyInterceptor(const PopupKeyEvent *key) {
  if (!g_platform_key_interceptor)
    return false;
  return g_platform_key_interceptor(key);
}

PlMetrics *Platform_GetMetrics(void) {
  static PlMetrics m = {0};
  static int init = 0;
  if (!init) {
    m.edit_cursor_width = 0;
    m.edit_cursor_height = PromptCaretHeight();
    m.editbox_padding = PROMPT_EDIT_INSET;
    m.listbox_padding = PromptListBottomMargin();
    m.listbox_item_height = PromptRowHeight();
    init = 1;
  }
  return &m;
}

void Platform_InvalidatePrompt(void) {
  HWND hwnd = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hwnd)
    InvalidateRect(hwnd, NULL, FALSE);
}
