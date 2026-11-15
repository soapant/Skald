#include "helpers/win32/raw_platform_win32.h"

DWORD WINAPI win32_stdin_writer(LPVOID arg) {
  StdinWriteCtx *ctx = (StdinWriteCtx *)arg;
  size_t off = 0;
  while (off < ctx->len) {
    DWORD written = 0;
    DWORD chunk = (DWORD)(ctx->len - off > 16384 ? 16384 : ctx->len - off);
    if (!WriteFile(ctx->pipe_write, ctx->data + off, chunk, &written, NULL) ||
        written == 0)
      break;
    off += written;
  }
  CloseHandle(ctx->pipe_write);
  free(ctx);
  return 0;
}

bool win32_cmdline_append_arg(char *cmd, size_t cap, size_t *off,
                              const char *arg) {
  size_t pos = *off;
  size_t alen = strlen(arg);
  bool needs_quotes = alen == 0 || strpbrk(arg, " \t\"\n\v") != NULL;
  if (!needs_quotes) {
    if (pos + alen >= cap)
      return false;
    memcpy(cmd + pos, arg, alen);
    *off = pos + alen;
    return true;
  }
  if (pos >= cap)
    return false;
  cmd[pos++] = '"';
  const char *p = arg;
  while (*p) {
    int slashes = 0;
    while (*p == '\\') {
      slashes++;
      p++;
    }
    if (*p == '\0') {
      size_t need = (size_t)slashes * 2;
      if (pos + need + 2 > cap)
        return false;
      for (size_t i = 0; i < need; i++)
        cmd[pos++] = '\\';
      break;
    } else if (*p == '"') {
      size_t need = (size_t)slashes * 2 + 1;
      if (pos + need + 2 > cap)
        return false;
      for (size_t i = 0; i < need; i++)
        cmd[pos++] = '\\';
      cmd[pos++] = '"';
      p++;
    } else {
      if (pos + (size_t)slashes + 2 > cap)
        return false;
      for (int i = 0; i < slashes; i++)
        cmd[pos++] = '\\';
      cmd[pos++] = *p++;
    }
  }
  if (pos + 2 > cap)
    return false;
  cmd[pos++] = '"';
  *off = pos;
  return true;
}

BOOL CALLBACK win32_enum_windows_proc_title(HWND hwnd, LPARAM lParam) {

  FindWindowData *data = (FindWindowData *)lParam;

  if (!IsWindowVisible(hwnd)) {
    return TRUE;
  }

  char window_title[512];
  if (GetWindowTextA(hwnd, window_title, sizeof(window_title)) == 0)
    return TRUE;
  if (window_title[0] == '\0')
    return TRUE;

  if (string_contains_ci(window_title, data->target_title)) {
    data->found_hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

HWND win32_foreground_target(void) {

  HWND hwnd = GetForegroundWindow();
  if (!hwnd || !IsWindow(hwnd))
    return NULL;
  char cls[64];
  cls[0] = '\0';
  GetClassNameA(hwnd, cls, sizeof(cls));
  if (strcmp(cls, QUICKSWITCHER_CLASS) == 0)
    return NULL;
  if (strcmp(cls, MESSAGE_CLASS) == 0)
    return NULL;
  if (strcmp(cls, SEARCH_POPUP_CLASS) == 0)
    return NULL;
  return hwnd;
}

BOOL g_win32_fs_active = FALSE;

char g_win32_project_root[MAX_PATH] = {0};

HWND g_win32_last_min = NULL;

RECT g_win32_fs_rect;

LONG g_win32_fs_style;

LONG g_win32_fs_exstyle;

typedef struct {
  RECT rects[16];
  int count;
} win32_mon_list;

BOOL CALLBACK win32_mon_enum(HMONITOR hMon, HDC hdc, LPRECT rc, LPARAM lp) {

  (void)hMon;
  (void)hdc;
  win32_mon_list *out = (win32_mon_list *)lp;
  if (!out || out->count >= 16 || !rc)
    return TRUE;
  out->rects[out->count++] = *rc;
  return TRUE;
}

static bool win32_is_cloaked(HWND hwnd) {
  int cloaked = 0;
  HMODULE dwm = GetModuleHandleA("dwmapi.dll");
  if (!dwm)
    return false;
  HRESULT(WINAPI * p)(HWND, DWORD, PVOID,
                      DWORD) = (void *)GetProcAddress(dwm,
                                                      "DwmGetWindowAttribute");
  return (p && SUCCEEDED(p(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked)
             ? true
             : false;
}

static bool win32_is_hidden_style(HWND hwnd) {
  LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
  if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) {
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner)
      return true;
  }
  return false;
}

typedef struct {
  int fcx;
  int fcy;
  HWND best;
  int best_gap;
  int best_vdist;
  int dir;
} win32_focus_search;

BOOL CALLBACK win32_focus_enum(HWND hwnd, LPARAM lp) {
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
    if (strcmp(cls, QUICKSWITCHER_CLASS) == 0 ||
        strcmp(cls, MESSAGE_CLASS) == 0 ||
        strcmp(cls, SEARCH_POPUP_CLASS) == 0 ||
        strcmp(cls, WIDGET_CLASS) == 0) {
      return TRUE;
    }
  if (win32_is_cloaked(hwnd) || win32_is_hidden_style(hwnd))
    return TRUE;
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
  int gap = 0;
  int cross = 0;
  switch (s->dir) {
  case WIN32_FOCUS_LEFT:
    if (cx >= s->fcx)
      return TRUE;
    gap = s->fcx - cx;
    cross = cy >= s->fcy ? cy - s->fcy : s->fcy - cy;
    break;
  case WIN32_FOCUS_RIGHT:
    if (cx <= s->fcx)
      return TRUE;
    gap = cx - s->fcx;
    cross = cy >= s->fcy ? cy - s->fcy : s->fcy - cy;
    break;
  case WIN32_FOCUS_UP:
    if (cy >= s->fcy)
      return TRUE;
    gap = s->fcy - cy;
    cross = cx >= s->fcx ? cx - s->fcx : s->fcx - cx;
    break;
  default:
    if (cy <= s->fcy)
      return TRUE;
    gap = cy - s->fcy;
    cross = cx >= s->fcx ? cx - s->fcx : s->fcx - cx;
    break;
  }
  if (gap < s->best_gap || (gap == s->best_gap && cross < s->best_vdist)) {
    s->best_gap = gap;
    s->best_vdist = cross;
    s->best = hwnd;
  }
  return TRUE;
}

bool win32_move_active_window_direction(int dir) {
  bool horizontal = (dir == WIN32_MON_LEFT || dir == WIN32_MON_RIGHT);
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
      int li = horizontal ? list.rects[i].left : list.rects[i].top;
      int lj = horizontal ? list.rects[j].left : list.rects[j].top;
      bool before =
          li > lj ||
          (li == lj && (horizontal ? list.rects[j].top < list.rects[i].top
                                   : list.rects[j].left < list.rects[i].left));
      if (before) {
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
    if (horizontal ? list.rects[i].left <= cx : list.rects[i].top <= cy)
      cur = i;
  }
  int dst =
      (dir == WIN32_MON_RIGHT || dir == WIN32_MON_DOWN) ? cur + 1 : cur - 1;
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

bool win32_activate_hwnd(HWND h) {

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

bool win32_focus_direction(int dir) {
  HWND fg = win32_foreground_target();
  if (!fg)
    fg = GetForegroundWindow();
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
  s.dir = dir;
  EnumWindows(win32_focus_enum, (LPARAM)&s);
  if (!s.best)
    return false;
  return win32_activate_hwnd(s.best);
}

bool win32_placeholder_icon(char *out, size_t out_len) {

  char cand[MAX_PATH];
  snprintf(cand, sizeof(cand), "resources\\IconFallback.png");
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
    snprintf(cand, sizeof(cand), "%s\\resources\\IconFallback.png", exe);
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

ULONG_PTR g_gdiplusToken = 0;

bool win32_is_shell_icon(const char *p) {
  const char *dot = strrchr(p, '.');
  if (!dot)
    return false;
  return (_stricmp(dot, ".lnk") == 0 || _stricmp(dot, ".exe") == 0 ||
          _stricmp(dot, ".url") == 0);
}

unsigned win32_icon_hash(const char *s) {
  unsigned h = 2166136261u;
  while (s && *s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

bool win32_icon_cache_dir(char *out, size_t n) {
  char base[MAX_PATH];
  snprintf(base, sizeof(base), "%s\\AppData\\Roaming\\Skald",
           Platform_GetRealHome());
  Platform_MkdirIfNotExists(base);
  snprintf(out, n, "%s\\icon_cache", base);
  Platform_MkdirIfNotExists(out);
  return true;
}

bool win32_icon_file_ok(const char *path) {
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

bool win32_icon_png_path(const char *src, char *out, size_t n) {
  char dir[MAX_PATH];
  win32_icon_cache_dir(dir, sizeof(dir));
  snprintf(out, n, "%s\\%08x_v2.png", dir, win32_icon_hash(src));
  return true;
}

void win32_gdiplus_ready(void) {
  if (g_gdiplusToken != 0)
    return;
  GdiplusStartupInput in;
  in.GdiplusVersion = 1;
  in.DebugEventCallback = NULL;
  in.SuppressBackgroundThread = FALSE;
  in.SuppressExternalCodecs = FALSE;
  GdiplusStartup(&g_gdiplusToken, &in, NULL);
}

bool win32_export_shell_icon_png(const char *native_path) {
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
    ULONGLONG sz = 0;
    if (GetFileAttributesExA(png, GetFileExInfoStandard, &fad))
      sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    log_info("win32_export_shell_icon_png saved='%s' size=%llu", png, sz);
  }

  GdipDisposeImage(flat);
  free(zeros);
  return ok;
}

typedef struct {
  PlWindowList *titles;
  PlWindowList *names;
} WinPair;

BOOL CALLBACK win32_enum_windows_proc_pair(HWND hwnd, LPARAM lParam) {

  WinPair *pair = (WinPair *)lParam;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  char cls[256];
  if (GetClassNameA(hwnd, cls, sizeof(cls)) > 0) {
    if (strcmp(cls, "Progman") == 0 || strcmp(cls, "WorkerW") == 0)
      return TRUE;
    if (strcmp(cls, QUICKSWITCHER_CLASS) == 0 ||
        strcmp(cls, MESSAGE_CLASS) == 0 ||
        strcmp(cls, SEARCH_POPUP_CLASS) == 0 || strcmp(cls, WIDGET_CLASS) == 0)
      return TRUE;
    if (strcmp(cls, "ApplicationFrameWindow") == 0) {
      if (win32_is_cloaked(hwnd))
        return TRUE;
    }
  }
  if (win32_is_cloaked(hwnd) || win32_is_hidden_style(hwnd))
    return TRUE;
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
    const char *base = strrchr(exePath, '\\');
    if (base) {
      snprintf(exe, sizeof(exe), "%s", base + 1);
      len = strlen(exe);
    } else {
      snprintf(exe, sizeof(exe), "%s", exePath);
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
  if (!t1)
    return TRUE;
  pair->titles->windows = t1;
  char **t2 =
      realloc(pair->names->windows, (pair->names->count + 1) * sizeof(char *));
  if (!t2)
    return TRUE;
  pair->names->windows = t2;
  char *title_dup = strdup(title);
  char *exe_dup = strdup(exe);
  if (!title_dup || !exe_dup) {
    free(title_dup);
    free(exe_dup);
    return TRUE;
  }
  pair->titles->windows[pair->titles->count++] = title_dup;
  pair->names->windows[pair->names->count++] = exe_dup;
  return TRUE;
}

void win32_collect_pair(PlWindowList *out_titles, PlWindowList *out_names) {

  WinPair p = {out_titles, out_names};
  EnumWindows(win32_enum_windows_proc_pair, (LPARAM)&p);
}

HANDLE g_singleton_mtx = NULL;

HHOOK s_win32_keyboard_hook = NULL;

HHOOK s_win32_mouse_hook = NULL;

uint32_t s_main_thread_id = 0;

WindowHandle s_tray_wnd = {0};

bool win32_is_main_thread(void) {
  return GetCurrentThreadId() == s_main_thread_id;
}

WindowHandle win32_tray_window(void) { return s_tray_wnd; }

int win32_dir_exists(const char *path) {

  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

int win32_is_project_root(const char *dir) {

  char probe[MAX_PATH];
  snprintf(probe, sizeof(probe), "%s\\config", dir);
  if (!win32_dir_exists(probe))
    return 0;
  snprintf(probe, sizeof(probe), "%s\\build", dir);
  if (!win32_dir_exists(probe))
    return 0;
  return 1;
}

DWORD win32_button_flag(int button, bool down) {

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
