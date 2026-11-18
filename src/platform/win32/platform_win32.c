#include "helpers/win32/raw_platform_win32.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_text.h"
#include "helpers/win32/win32_state.h"
#include "helpers/helpers_fs.h"
#include "helpers/win32/subclasses_win32.h"
#include "wm/wm.h"

#ifdef _MSC_VER

#pragma comment(lib, "winhttp.lib")

#endif

bool Platform_LaunchTerminalAtPath(const char *working_dir,
                                   const char *extra_args) {

  if (!terminal_guard(working_dir, extra_args))
    return false;
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
  return true;
}

static bool win32_grow_buf(char **out, size_t *cap, size_t need) {
  if (need <= *cap)
    return true;
  size_t ncap = *cap;
  while (need > ncap)
    ncap *= 2;
  char *n = (char *)realloc(*out, ncap);
  if (!n)
    return false;
  *out = n;
  *cap = ncap;
  return true;
}

static bool win32_drain_read_pipe(HANDLE hRead, char **out, size_t *len,
                                  size_t *cap) {
  for (;;) {
    if (!win32_grow_buf(out, cap, *len + 1024 + 1))
      return false;
    DWORD bytesRead = 0;
    if (!ReadFile(hRead, *out + *len, (DWORD)(*cap - *len - 1), &bytesRead,
                  NULL) ||
        bytesRead == 0)
      break;
    *len += bytesRead;
  }
  return true;
}

static bool win32_create_child_pipe(HANDLE *hRead, HANDLE *hWrite) {
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  if (!CreatePipe(hRead, hWrite, &sa, 0))
    return false;
  SetHandleInformation(*hRead, HANDLE_FLAG_INHERIT, 0);
  return true;
}

static BOOL win32_spawn_capture(const char *command, DWORD flags,
                                PROCESS_INFORMATION *pi, HANDLE hWrite) {
  STARTUPINFOA si = {0};
  si.cb = sizeof(si);
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;
  si.dwFlags = STARTF_USESTDHANDLES;
  char cmdLine[4096];
  snprintf(cmdLine, sizeof(cmdLine), "cmd /c %s", command);
  return CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, flags, NULL, NULL, &si,
                        pi);
}

static void win32_add_font_dir(const char *glob, const char *prefix) {
  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA(glob, &fd);
  if (hFind == INVALID_HANDLE_VALUE)
    return;
  do {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", prefix, fd.cFileName);
    AddFontResourceExA(path, FR_PRIVATE, 0);
  } while (FindNextFileA(hFind, &fd));
  FindClose(hFind);
}

char *Platform_RunTerminalCommand(const char *command, bool open_terminal) {
  if (!command) {
    log_error("Platform_RunTerminalCommand: NULL command");
    return NULL;
  }
  if (open_terminal) {
    char clean_cmd[512];
    safe_copy(clean_cmd, sizeof(clean_cmd), command ? command : "");
    const char *p = clean_cmd;
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
    DWORD n = GetTempPathA(MAX_PATH, temp_file);
    if (n == 0 || n >= MAX_PATH) {
      log_error("open_terminal: GetTempPathA failed err=%lu", GetLastError());
      return NULL;
    }
    snprintf(temp_file + strlen(temp_file), MAX_PATH - strlen(temp_file),
             "temp_cmd.sh");
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
  HANDLE hRead, hWrite;
  if (!win32_create_child_pipe(&hRead, &hWrite)) {
    log_error("CreatePipe failed err=%lu", GetLastError());
    return NULL;
  }
  PROCESS_INFORMATION pi = {0};
  BOOL ok = win32_spawn_capture(command, CREATE_NO_WINDOW, &pi, hWrite);
  CloseHandle(hWrite);
  if (!ok) {
    log_error("CreateProcess failed for '%s' err=%lu", command, GetLastError());
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
      if (!win32_grow_buf(&out, &cap, len + avail + 1))
        break;
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
      win32_drain_read_pipe(hRead, &out, &len, &cap);
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

bool Platform_LaunchDetached(const char *command) {
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

char *Platform_RunCommandAndGetOutput(const char *command) {
  if (command == NULL || strlen(command) == 0)
    return NULL;
  HANDLE hRead, hWrite;
  if (!win32_create_child_pipe(&hRead, &hWrite))
    return NULL;
  PROCESS_INFORMATION pi = {0};
  DWORD creation_flags = CREATE_NO_WINDOW;
  BOOL ok = win32_spawn_capture(command, creation_flags, &pi, hWrite);
  CloseHandle(hWrite);
  if (!ok) {
    CloseHandle(hRead);
    return NULL;
  }
  size_t cap = 8192;
  size_t len = 0;
  char *out = (char *)malloc(cap);
  if (!out) {
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return NULL;
  }
  bool read_ok = win32_drain_read_pipe(hRead, &out, &len, &cap);
  WaitForSingleObject(pi.hProcess, INFINITE);
  if (!read_ok) {
    if (out)
      free(out);
    out = NULL;
  }
  if (!out) {
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return NULL;
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
  char cmdline[4096];
  size_t off = 0;
  cmdline[0] = '\0';
  for (int i = 0; argv[i]; i++) {
    if (off > 0) {
      if (off + 1 >= sizeof(cmdline)) {
        log_error("Platform_RunArgvCapture command line too long");
        return NULL;
      }
      cmdline[off++] = ' ';
    }
    if (!win32_cmdline_append_arg(cmdline, sizeof(cmdline), &off, argv[i])) {
      log_error("Platform_RunArgvCapture command line too long");
      return NULL;
    }
  }
  cmdline[off] = '\0';

  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  HANDLE rd_in = NULL, wr_in = NULL, rd_out = NULL, wr_out = NULL;
  if (!CreatePipe(&rd_in, &wr_in, &sa, 0))
    return NULL;
  SetHandleInformation(wr_in, HANDLE_FLAG_INHERIT, 0);
  if (!CreatePipe(&rd_out, &wr_out, &sa, 0)) {
    CloseHandle(rd_in);
    CloseHandle(wr_in);
    return NULL;
  }
  SetHandleInformation(rd_out, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = rd_in;
  si.hStdOutput = wr_out;
  si.hStdError = wr_out;
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                           NULL, NULL, &si, &pi);
  CloseHandle(rd_in);
  CloseHandle(wr_out);
  if (!ok) {
    log_error("Platform_RunArgvCapture CreateProcess failed for '%s'", argv[0]);
    CloseHandle(wr_in);
    CloseHandle(rd_out);
    return NULL;
  }

  HANDLE stdin_thread = NULL;
  if (stdin_data && stdin_data[0]) {
    StdinWriteCtx *ctx = (StdinWriteCtx *)malloc(sizeof(*ctx));
    if (ctx) {
      ctx->pipe_write = wr_in;
      ctx->data = stdin_data;
      ctx->len = strlen(stdin_data);
      stdin_thread = CreateThread(NULL, 0, win32_stdin_writer, ctx, 0, NULL);
      if (!stdin_thread)
        free(ctx);
    }
    if (!stdin_thread)
      CloseHandle(wr_in);
  } else {
    CloseHandle(wr_in);
  }

  size_t cap = 4096, len = 0;
  char *out = (char *)malloc(cap);
  if (!out) {
    if (stdin_thread) {
      WaitForSingleObject(stdin_thread, INFINITE);
      CloseHandle(stdin_thread);
    }
    CloseHandle(rd_out);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return NULL;
  }
  if (!win32_drain_read_pipe(rd_out, &out, &len, &cap)) {
    free(out);
    out = NULL;
  }
  CloseHandle(rd_out);
  if (stdin_thread) {
    WaitForSingleObject(stdin_thread, INFINITE);
    CloseHandle(stdin_thread);
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  if (out_status)
    *out_status = (int)code;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (!out)
    return NULL;
  out[len] = '\0';
  return out;
}

static bool s_bundled_font_done = false;

static bool win32_is_bundled_face(const char *face_name) {
  return face_name &&
         (_stricmp(face_name, "0xProto Nerd Font") == 0 ||
          _stricmp(face_name, "0xProtoNerdFont") == 0);
}

static bool win32_register_bundled_font(const char *face_name) {
  if (s_bundled_font_done)
    return true;
  if (!win32_is_bundled_face(face_name))
    return false;
  const char *path = "resources/fonts/0xProtoNerdFont-Regular.ttf";
  int added = AddFontResourceExA(path, FR_PRIVATE | FR_NOT_ENUM, 0);
  s_bundled_font_done = added > 0;
  return s_bundled_font_done;
}

FontHandle Platform_CreateFont(int size, const char *face_name) {
  win32_register_bundled_font(face_name);

  int h = -size;
  static bool s_font_loaded = false;
  if (!s_font_loaded) {
    s_font_loaded = true;
    win32_add_font_dir("resources\\*.ttf", "resources\\");
    win32_add_font_dir("data\\*.ttf", "data\\");
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

void Platform_GetTextExtents(FontHandle font, const char *text, int max_width,
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
  if (max_width > 0) {
    RECT measure = {0, 0, 0, 0};
    DrawTextA(hdc, text, -1, &measure, DT_CALCRECT);
    RECT wrapped = {0, 0, max_width, 0x7FFFFFFF};
    DrawTextA(hdc, text, -1, &wrapped, DT_CALCRECT | DT_WORDBREAK);
    height = wrapped.bottom - wrapped.top;
    width = measure.right - measure.left;
    if (width > max_width)
      width = max_width;
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

static HWND win32_find_window_by_title(const char *title) {
  if (!title || !title[0])
    return NULL;
  char trimmed[512];
  const char *s = title;
  while (*s && isspace((unsigned char)*s))
    s++;
  const char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)*(e - 1)))
    e--;
  size_t len = (size_t)(e - s);
  if (len == 0)
    return NULL;
  if (len >= sizeof(trimmed))
    len = sizeof(trimmed) - 1;
  memcpy(trimmed, s, len);
  trimmed[len] = '\0';

  HWND hwnd = FindWindowA(NULL, trimmed);
  if (hwnd == NULL) {
    FindWindowData data;
    data.target_title = trimmed;
    data.found_hwnd = NULL;
    EnumWindows(win32_enum_windows_proc_title, (LPARAM)&data);
    hwnd = data.found_hwnd;
  }
  return hwnd;
}

WindowHandle Platform_GetWindowByRootAtom(const char *atom) {
  (void)atom;
  return WH_NULL;
}

WindowHandle Platform_GetWindowByTitle(const char *title) {
  HWND hwnd = win32_find_window_by_title(title);
  if (hwnd == NULL)
    return WH_NULL;
  return WH_FROM_HWND(hwnd);
}

bool Platform_SetWindowStringProperty(WindowHandle window, const char *atom,
                                      const char *value) {
  (void)atom;
  if (WH_EQUALS(window, WH_NULL) || !value)
    return false;
  HWND hwnd = WH_TO_HWND(window);
  if (!IsWindow(hwnd))
    return false;
  COPYDATASTRUCT cds;
  memset(&cds, 0, sizeof(cds));
  cds.dwData = 0;
  cds.cbData = (DWORD)(strlen(value) + 1);
  cds.lpData = (void *)value;
  SendMessageA(hwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
  return true;
}

bool Platform_SetWindowLongProperty(WindowHandle window, const char *atom,
                                    long value) {
  (void)atom;
  if (WH_EQUALS(window, WH_NULL))
    return false;
  HWND hwnd = WH_TO_HWND(window);
  if (!IsWindow(hwnd))
    return false;
  return PostMessageA(hwnd, WM_PLAYER_CMD, (WPARAM)value, 0) != 0;
}

bool Platform_SendWindowMessage(WindowHandle window, const char *atom,
                                long data) {
  return Platform_SetWindowLongProperty(window, atom, data);
}

bool Platform_FocusWindow(const char *title) {
  if (!title || !title[0]) {
    log_error("operation failed, returning error title='%s'",
              title ? title : "null");
    return false;
  }

  HWND hwnd = win32_find_window_by_title(title);

  if (hwnd == NULL) {
    log_error("Platform_FocusWindow no window for '%s'", title);
    return false;
  }

  HWND hPopup = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (hPopup && IsWindow(hPopup)) {
    win32_set_prev_fg(hwnd);
    log_info("Platform_FocusWindow deferred via g_prev_fg hwnd=%p title='%s'",
             hwnd, title);
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

  log_info("Platform_FocusWindow title='%s' hwnd=%p", title ? title : "", hwnd);
  return true;
}

bool Platform_CloseActiveWindow(void) {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd || !IsWindow(hwnd))
    return false;
  char cls[64];
  cls[0] = '\0';
  GetClassNameA(hwnd, cls, sizeof(cls));
  if (strcmp(cls, SKALD_CLASS) == 0) {
    SkaldContext_Get()->window.selection_complete = true;
    return true;
  }
  PostMessageA(hwnd, WM_CLOSE, 0, 0);
  return true;
}

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

#define WIN32_FOCUS_LEFT 0

#define WIN32_FOCUS_RIGHT 1

#define WIN32_FOCUS_UP 2

#define WIN32_FOCUS_DOWN 3

#define WIN32_MON_LEFT 0

#define WIN32_MON_RIGHT 1

#define WIN32_MON_UP 2

#define WIN32_MON_DOWN 3

bool Platform_MoveActiveWindowToMonitor(bool right) {
  return win32_move_active_window_direction(right ? WIN32_MON_RIGHT
                                                  : WIN32_MON_LEFT);
}

bool Platform_MoveActiveWindowLeft(void) {
  return win32_move_active_window_direction(WIN32_MON_LEFT);
}

bool Platform_MoveActiveWindowRight(void) {
  return win32_move_active_window_direction(WIN32_MON_RIGHT);
}

bool Platform_FocusWindowLeft(void) {
  return win32_focus_direction(WIN32_FOCUS_LEFT);
}

bool Platform_FocusWindowRight(void) {
  return win32_focus_direction(WIN32_FOCUS_RIGHT);
}

bool Platform_FocusWindowUp(void) {
  return win32_focus_direction(WIN32_FOCUS_UP);
}

bool Platform_FocusWindowDown(void) {
  return win32_focus_direction(WIN32_FOCUS_DOWN);
}

bool Platform_MoveActiveWindowUp(void) {
  return win32_move_active_window_direction(WIN32_MON_UP);
}

bool Platform_MoveActiveWindowDown(void) {
  return win32_move_active_window_direction(WIN32_MON_DOWN);
}

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

char *Platform_GetActiveWindowId(void) {
  static char id[256];
  id[0] = '\0';
  HWND hForeground = GetForegroundWindow();
  if (hForeground == NULL)
    return id;
  DWORD pid = 0;
  GetWindowThreadProcessId(hForeground, &pid);
  if (pid == 0)
    return id;
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h)
    return id;
  char exe[MAX_PATH];
  DWORD sz = sizeof(exe);
  if (QueryFullProcessImageNameA(h, 0, exe, &sz) && sz > 0) {
    const char *base = strrchr(exe, '\\');
    base = base ? base + 1 : exe;
    safe_copy(id, sizeof(id), base);
    size_t n = strlen(id);
    if (n > 4 && _stricmp(id + n - 4, ".exe") == 0)
      id[n - 4] = '\0';
  }
  CloseHandle(h);
  return id;
}

char *Platform_GetIconPath(char *app_path) {
  if (!app_path || !app_path[0]) {
    log_error("operation failed, returning error app_path='%s'",
              app_path ? app_path : "null");
    return NULL;
  }
  char expanded[MAX_PATH];
  const char *src = app_path;
  if (expand_path(app_path, expanded, sizeof(expanded))) {
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

WindowList Platform_GetWindowTitles(void) {
  WindowList titles = {0}, names = {0};
  win32_collect_pair(&titles, &names);
  for (int i = 0; i < names.count; i++)
    free(names.windows[i]);
  free(names.windows);
  log_info("Platform_GetWindowTitles count=%d", titles.count);
  return titles;
}

WindowList Platform_GetWindowIds(void) {
  WindowList titles = {0}, names = {0};
  win32_collect_pair(&titles, &names);
  for (int i = 0; i < titles.count; i++)
    free(titles.windows[i]);
  free(titles.windows);
  log_info("Platform_GetWindowIds count=%d", names.count);
  return names;
}

bool Platform_TryAcquireSingleInstance(void) {
  g_singleton_mtx = CreateMutexA(NULL, TRUE, "skald_single_instance_mutex");
  if (g_singleton_mtx == NULL) {
    log_error("singleton mutex creation failed (error=%lu)", GetLastError());
    return false;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    log_info("Another instance is already running. Exiting.");
    CloseHandle(g_singleton_mtx);
    g_singleton_mtx = NULL;
    return false;
  }
  return true;
}

bool Platform_InitDisplaySafe(void) { return true; }

void Platform_CleanupDisplaySafe(void) {}

void Platform_InitMainThreadContext(void) {
  s_main_thread_id = GetCurrentThreadId();
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

  char cwd[MAX_PATH] = {0};
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

RgbColor Platform_GetPixelColor(int x, int y) {
  RgbColor out = {0, 0, 0};
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
  return (SendInput(2, inputs, sizeof(INPUT)) == 2);
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

bool Platform_GetVirtualBounds(int *out_x, int *out_y, int *out_w, int *out_h) {
  Platform_BoundsZero(out_x, out_y, out_w, out_h);
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
  return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool Platform_MouseScroll(int notches) {
  if (notches == 0)
    return true;
  INPUT in = {0};
  in.type = INPUT_MOUSE;
  in.mi.dwFlags = MOUSEEVENTF_WHEEL;
  in.mi.mouseData = (DWORD)((long)notches * WHEEL_DELTA);
  return SendInput(1, &in, sizeof(INPUT)) == 1;
}

char *Platform_GetWorkingDir(void) {
  return Platform_RunTerminalCommand("cd", false);
}

void Platform_InitDataFolder(void) {
  char dir_path[512];
  if (!data_dir(dir_path, sizeof(dir_path)))
    return;
  Platform_MkdirIfNotExists(dir_path);
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

void Platform_PrewarmBackgroundPixmap(void) {
  win32_bg_image_purge();
  win32_bg_image_prewarm(PromptWidth, PromptHeight);
}

int Platform_InitializeTrayIcon(void) {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (Tray) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = win32_wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) {
      log_error("operation failed, returning error");
      return -1;
    }

    s_tray_wnd = WH_FROM_HWND(
        CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName, "TrayApp", WS_POPUP,
                        0, 0, 0, 0, NULL, NULL, hInstance, NULL));

    if (WH_TO_HWND(s_tray_wnd) == NULL) {
      return -1;
    }
  }
  log_info("Platform_InitializeTrayIcon completed");
  return 0;
}

void Platform_InitUI(void) {
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
  wc.lpfnWndProc = win32_popup_wnd_proc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(NULL_BRUSH);
  wc.lpszClassName = SKALD_CLASS;

  if (RegisterClassA(&wc)) {
    log_info("%s registered successfully with OS.", SKALD_CLASS);
  } else {
    log_error("CRITICAL WARNING: Failed to register window class! Error: %lu",
              GetLastError());
  }

  WNDCLASSA wcFade = {0};
  wcFade.lpfnWndProc = win32_display_message_proc;
  wcFade.hInstance = GetModuleHandle(NULL);
  wcFade.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcFade.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
  wcFade.lpszClassName = MESSAGE_CLASS;
  RegisterClassA(&wcFade);

  s_win32_keyboard_hook =
      SetWindowsHookExA(WH_KEYBOARD_LL, win32_handle_global_key, NULL, 0);
  if (!s_win32_keyboard_hook) {
    log_warn("WH_KEYBOARD_LL hook FAILED to install err=%lu", GetLastError());
  } else {
    log_warn("WH_KEYBOARD_LL hook installed handle=%p",
             (void *)s_win32_keyboard_hook);
  }

  win32_register_raw_mouse(WH_TO_HWND(s_tray_wnd));
  s_win32_mouse_hook = NULL;

  FontHandle warm = Platform_CreateFont(FontSize, UsedFont);
  log_warn("warm font size=%d face='%s' created=%d", FontSize,
           UsedFont ? UsedFont : "", FH_TO_HFONT(warm) ? 1 : 0);
  Platform_DestroyFont(warm);
  log_info("Platform_InitUI completed");
}

void Platform_ShutdownUI(void) {
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

static void win32_http_buf_init(HttpBuf *buf, size_t limit) {
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

static void win32_http_buf_init(HttpBuf *buf, size_t limit);
static int win32_http_perform(const char *url, const char *method,
                              const char *headers, const char *body,
                              HttpBuf *resp_buf, FILE *file_out,
                              long max_redirects);

char *Platform_HttpCollect(const char *url, const char *method,
                           const char *content_type, const char *body,
                           size_t max_bytes, size_t *out_len) {
  HttpBuf buf;
  win32_http_buf_init(&buf, max_bytes ? max_bytes : HTTP_DEFAULT_LIMIT);
  int ok = win32_http_perform(url, method, content_type, body, &buf, NULL,
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

static int win32_http_perform(const char *url, const char *method,
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
  if (MultiByteToWideChar(CP_UTF8, 0, current_url, -1, wUrl, ARRAYSIZE(wUrl)) <=
      0) {
    log_error("MultiByteToWideChar failed for url (len=%d)",
              (int)strlen(current_url));
    WinHttpCloseHandle(hSession);
    return 0;
  }

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

bool Platform_HttpSaveToFile(const char *url, const char *path) {
  if (!url || !url[0] || !path || !path[0])
    return false;
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;
  HttpBuf unused;
  memset(&unused, 0, sizeof(unused));
  int ok = win32_http_perform(url, "GET", NULL, NULL, &unused, f,
                              HTTP_MAX_REDIRECTS);
  fclose(f);
  if (ok < 200 || ok >= 300) {
    log_warn("http download failed with status %d for %s", ok, url);
    remove(path);
    return false;
  }
  return true;
}

bool Platform_FontInitialized(FontHandle font) { return font.hfont != NULL; }

Timer Platform_StartTimer(void) {
  Timer t = {0};
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  t.ticks = (uint64_t)c.QuadPart;
  return t;
}

double Platform_EndTimer(Timer t) {
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

static DWORD WINAPI win32_thread_entry(LPVOID p) {
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
  HANDLE h = CreateThread(NULL, 0, win32_thread_entry, s, 0, NULL);
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
  DWORD w = WaitForSingleObject((HANDLE)thread, 5000);
  if (w == WAIT_TIMEOUT) {
    log_error("Platform_ThreadJoin: thread %p still running after 5s, "
              "leaking handle instead of terminating",
              thread);
    return;
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

void Platform_InvalidatePrompt(void) {
  HWND hwnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (hwnd)
    InvalidateRect(hwnd, NULL, FALSE);
}

void Platform_EmitKey(int vk_code, int down) {
  log_warn("Platform_EmitKey win32 vk=%d down=%d", vk_code, down);
  INPUT inp = {0};
  inp.type = INPUT_KEYBOARD;
  inp.ki.wVk = (WORD)vk_code;
  inp.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  if (vk_code == VK_UP || vk_code == VK_DOWN || vk_code == VK_LEFT ||
      vk_code == VK_RIGHT || vk_code == VK_RCONTROL || vk_code == VK_RMENU) {
    inp.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  UINT sent = SendInput(1, &inp, sizeof(INPUT));
  if (sent != 1) {
    log_error("Platform_EmitKey FAILED vk=%d down=%d sent=%u err=%lu", vk_code,
              down, (unsigned)sent, GetLastError());
  }
}

bool Platform_HttpThreadStart(void *(*fn)(void *), void *ud) {
  HANDLE t =
      CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(void *)fn, ud, 0, NULL);
  if (!t)
    return false;
  CloseHandle(t);
  return true;
}

bool Platform_GetMousePos(int *out_x, int *out_y) {
  POINT pt;
  if (!GetCursorPos(&pt))
    return false;
  if (out_x)
    *out_x = (int)pt.x;
  if (out_y)
    *out_y = (int)pt.y;
  return true;
}

static bool win32_lookup_is_file(const char *path) {
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool win32_lookup_is_dir(const char *path) {
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES &&
         (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static const char *win32_lookup_pathext(void) {
  const char *pathext = getenv("PATHEXT");
  return (pathext && pathext[0]) ? pathext : ".COM;.EXE;.BAT;.CMD";
}

static void win32_lookup_each_ext(const char *pathext, char exts[][32],
                                  size_t *count) {
  *count = 0;
  const char *p = pathext;
  while (p && *p && *count < 16) {
    const char *end = strchr(p, ';');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0 && len < 31) {
      memcpy(exts[*count], p, len);
      exts[*count][len] = '\0';
      (*count)++;
    }
    if (!end)
      break;
    p = end + 1;
  }
}

static bool win32_lookup_has_ext(const char *name, const char *pathext) {
  const char *base = strrchr(name, '\\');
  base = base ? base + 1 : name;
  const char *dot = strrchr(base, '.');
  if (!dot)
    return false;
  char exts[16][32];
  size_t count = 0;
  win32_lookup_each_ext(pathext, exts, &count);
  for (size_t i = 0; i < count; i++)
    if (lookup_ci_eq(dot, exts[i]))
      return true;
  return false;
}

static char *win32_lookup_try_full(const char *full, const char *pathext) {
  if (win32_lookup_is_file(full))
    return _strdup(full);
  if (win32_lookup_has_ext(full, pathext))
    return NULL;
  char exts[16][32];
  size_t count = 0;
  win32_lookup_each_ext(pathext, exts, &count);
  char cand[4096];
  for (size_t i = 0; i < count; i++) {
    int rc = snprintf(cand, sizeof(cand), "%s%s", full, exts[i]);
    if (rc > 0 && (size_t)rc < sizeof(cand) && win32_lookup_is_file(cand))
      return _strdup(cand);
  }
  return NULL;
}

static char *win32_lookup_try_dir(const char *dir, const char *name,
                                  const char *pathext) {
  if (!dir || !dir[0])
    return NULL;
  char cand[4096];
  int rc = snprintf(cand, sizeof(cand), "%s\\%s", dir, name);
  if (rc > 0 && (size_t)rc < sizeof(cand)) {
    char *hit = win32_lookup_try_full(cand, pathext);
    if (hit)
      return hit;
  }
  return NULL;
}

static char *win32_lookup_find_in_path(const char *name, const char *pathext) {
  const char *path = getenv("PATH");
  if (path && path[0]) {
    const char *cursor = path;
    char dirbuf[4096];
    while (path_next_entry(&cursor, ';', dirbuf, sizeof(dirbuf))) {
      size_t n = strlen(dirbuf);
      if (n >= 2 && dirbuf[0] == '"' && dirbuf[n - 1] == '"') {
        dirbuf[n - 1] = '\0';
        memmove(dirbuf, dirbuf + 1, n - 1);
      }
      char *hit = win32_lookup_try_dir(dirbuf, name, pathext);
      if (hit)
        return hit;
    }
  }
  const char *sysroot = getenv("SystemRoot");
  if (!sysroot || !sysroot[0])
    sysroot = "C:\\Windows";
  char buf[MAX_PATH];
  snprintf(buf, sizeof(buf), "%s\\System32", sysroot);
  char *hit = win32_lookup_try_dir(buf, name, pathext);
  if (hit)
    return hit;
  hit = win32_lookup_try_dir(sysroot, name, pathext);
  if (hit)
    return hit;
  snprintf(buf, sizeof(buf), "%s\\System32\\Wbem", sysroot);
  hit = win32_lookup_try_dir(buf, name, pathext);
  if (hit)
    return hit;
  const char *local = getenv("LOCALAPPDATA");
  const char *home = Platform_GetRealHome();
  if (local && local[0])
    snprintf(buf, sizeof(buf), "%s\\Microsoft\\WindowsApps", local);
  else if (home && home[0])
    snprintf(buf, sizeof(buf), "%s\\AppData\\Local\\Microsoft\\WindowsApps",
             home);
  else
    buf[0] = '\0';
  return win32_lookup_try_dir(buf, name, pathext);
}

static char *win32_lookup_walk_programs(const char *name) {
  const char *roots[3];
  size_t n = 0;
  const char *pf = getenv("ProgramFiles");
  const char *pf86 = getenv("ProgramFiles(x86)");
  const char *local = getenv("LOCALAPPDATA");
  char local_progs[MAX_PATH];
  roots[n++] = (pf && pf[0]) ? pf : "C:\\Program Files";
  roots[n++] = (pf86 && pf86[0]) ? pf86 : "C:\\Program Files (x86)";
  if (local && local[0]) {
    snprintf(local_progs, sizeof(local_progs), "%s\\Programs", local);
    roots[n++] = local_progs;
  }
  char *allowed[] = {"exe", "bat", "cmd", NULL};
  for (size_t i = 0; i < n; i++) {
    if (!win32_lookup_is_dir(roots[i]))
      continue;
    ListOfFiles found = {0};
    if (file_walk_all_files_from_dir((char *)roots[i], allowed, &found, NULL,
                                     true, 3)) {
      for (size_t j = 0; j < found.count; j++) {
        if (string_equals_ci(found.files[j].name, name)) {
          char *hit = _strdup(found.files[j].path);
          free(found.files);
          return hit;
        }
      }
    }
    free(found.files);
  }
  return NULL;
}

char *Platform_FindExecutable(const char *name) {
  if (!name || !name[0])
    return NULL;

  const char *pathext = win32_lookup_pathext();

  if (strchr(name, '\\') || strchr(name, '/')) {
    char expanded[4096];
    if (!expand_path(name, expanded, sizeof(expanded)))
      return NULL;
    return win32_lookup_try_full(expanded, pathext);
  }

  char *hit = win32_lookup_find_in_path(name, pathext);
  if (hit)
    return hit;

  hit = win32_lookup_walk_programs(name);
  if (hit)
    return hit;

  int status = 0;
  char *out = Platform_RunArgvCapture(
      (const char *const[]){"where", name, NULL}, NULL, &status);
  if (!out)
    return NULL;
  char *line = lookup_first_line(out);
  if (line[0] && win32_lookup_is_file(line))
    hit = _strdup(line);
  free(out);
  return hit;
}

bool Platform_ToggleMaximizeActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  if (IsZoomed(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
    return true;
  }
  ShowWindow(hwnd, SW_SHOWMAXIMIZED);
  return true;
}

bool Platform_ToggleAlwaysOnTopActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  bool topmost = (ex & WS_EX_TOPMOST) != 0;
  SetWindowPos(hwnd, topmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  return true;
}

bool Platform_RaiseActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  return true;
}

bool Platform_LowerActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  return true;
}

bool Platform_CenterActiveWindow(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  RECT r;
  if (!GetWindowRect(hwnd, &r))
    return false;
  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!GetMonitorInfoA(mon, &mi))
    return false;
  int x = mi.rcWork.left +
          ((mi.rcWork.right - mi.rcWork.left) - (r.right - r.left)) / 2;
  int y = mi.rcWork.top +
          ((mi.rcWork.bottom - mi.rcWork.top) - (r.bottom - r.top)) / 2;
  SetWindowPos(hwnd, NULL, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  return true;
}


bool Platform_SnapActiveWindow(SnapPreset preset) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!GetMonitorInfoA(mon, &mi))
    return false;
  if (IsIconic(hwnd) || IsZoomed(hwnd))
    ShowWindow(hwnd, SW_RESTORE);
  RECT w = mi.rcWork;
  SnapRect r = wm_snap_rect(w.left, w.top, w.right - w.left, w.bottom - w.top,
                            preset);
  SetWindowPos(hwnd, HWND_TOP, r.x, r.y, r.w, r.h,
               SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  return true;
}

long Platform_GetActiveWindowPid(void) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return 0;
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  return (long)pid;
}

static void win32_fill_key(INPUT *in, WORD vk, BOOL down) {
  in->type = INPUT_KEYBOARD;
  in->ki.wVk = vk;
  in->ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
}

bool Platform_SwitchToWorkspace(int index) {
  if (index < 1 || index > 9)
    return false;
  WORD digit = (WORD)('0' + index);
  INPUT inputs[6] = {0};
  win32_fill_key(&inputs[0], VK_CONTROL, TRUE);
  win32_fill_key(&inputs[1], VK_LWIN, TRUE);
  win32_fill_key(&inputs[2], digit, TRUE);
  win32_fill_key(&inputs[3], digit, FALSE);
  win32_fill_key(&inputs[4], VK_LWIN, FALSE);
  win32_fill_key(&inputs[5], VK_CONTROL, FALSE);
  return SendInput(6, inputs, sizeof(INPUT)) == 6;
}

bool Platform_MoveActiveWindowToWorkspace(int index) {
  (void)index;
  log_error("win32 has no public API to move a window to an absolute virtual "
            "desktop (IVirtualDesktopManager cannot enumerate; use adjacent)");
  return false;
}

bool Platform_SetActiveWindowOpacity(int percent) {
  HWND hwnd = win32_foreground_target();
  if (!hwnd)
    return false;
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  SetWindowLongA(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
  BYTE alpha = (BYTE)((255.0 * percent) / 100.0 + 0.5);
  SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
  if (percent == 100) {
    SetWindowLongA(hwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  }
  return true;
}

typedef struct {
  const char *needle;
  bool by_id;
  int count;
} win32_close_ctx;

static BOOL CALLBACK win32_close_enum(HWND hwnd, LPARAM lp) {
  win32_close_ctx *c = (win32_close_ctx *)lp;
  char buf[512];
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (c->by_id) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
      return TRUE;
    DWORD n = GetModuleFileNameA(h, buf, sizeof(buf));
    CloseHandle(h);
    if (!n)
      return TRUE;
    char *base = strrchr(buf, '\\');
    base = base ? base + 1 : buf;
    size_t nb = strlen(base);
    if (nb > 4 && _stricmp(base + nb - 4, ".exe") == 0)
      base[nb - 4] = '\0';
    if (!string_contains_ci(base, c->needle))
      return TRUE;
  } else {
    if (!GetWindowTextA(hwnd, buf, sizeof(buf)))
      return TRUE;
    if (!string_contains_ci(buf, c->needle))
      return TRUE;
  }
  PostMessageA(hwnd, WM_CLOSE, 0, 0);
  c->count++;
  return TRUE;
}

bool Platform_CloseWindowsByTitle(const char *needle) {
  if (!needle || !needle[0])
    return false;
  win32_close_ctx ctx = {needle, false, 0};
  EnumWindows(win32_close_enum, (LPARAM)&ctx);
  return ctx.count > 0;
}

bool Platform_CloseWindowsById(const char *needle) {
  if (!needle || !needle[0])
    return false;
  win32_close_ctx ctx = {needle, true, 0};
  EnumWindows(win32_close_enum, (LPARAM)&ctx);
  return ctx.count > 0;
}
