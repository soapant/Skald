#include "core/skapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHOT_BUF 1024

#if defined(__linux__)
#include <unistd.h>
#include <glob.h>

static const char *shot_working_dir(void) {
  char *wd = sk_resolve_feature_path("screenshot");
  return (wd && wd[0]) ? wd : ".";
}

static void shot_imlib_env(char *buf, size_t n) {
  buf[0] = '\0';
  const char *existing = getenv("IMLIB2_LOADER_PATH");
  if (existing && existing[0]) {
    snprintf(buf, n, "IMLIB2_LOADER_PATH=%s ", existing);
    return;
  }
  static const char *patterns[] = {"/nix/store/*imlib2*/lib/imlib2/loaders",
                                   "/usr/lib/*/imlib2/loaders",
                                   "/usr/lib/Imlib2/loaders", NULL};
  for (int p = 0; patterns[p]; p++) {
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(patterns[p], 0, NULL, &g) != 0)
      continue;
    for (size_t i = 0; i < g.gl_pathc; i++) {
      char probe[SHOT_BUF * 2];
      snprintf(probe, sizeof(probe), "%s/png.so", g.gl_pathv[i]);
      if (access(probe, R_OK) != 0)
        continue;
      snprintf(buf, n, "IMLIB2_LOADER_PATH=%s ", g.gl_pathv[i]);
      break;
    }
    globfree(&g);
    if (buf[0])
      return;
  }
}

static void shot_env(char *buf, size_t n) {
  char imlib[SHOT_BUF];
  shot_imlib_env(imlib, sizeof(imlib));
  snprintf(buf, n, "LD_LIBRARY_PATH=%s/build/bundle:${LD_LIBRARY_PATH:-} %s",
           shot_working_dir(), imlib);
}

static void shot_clip_path(char *buf, size_t n) {
  snprintf(buf, n, "%s/skald_clip.png", shot_working_dir());
}

static void pk_screenshot_copy_area(void) {
  char env[SHOT_BUF];
  char path[SHOT_BUF];
  char cmd[SHOT_BUF * 3];
  shot_env(env, sizeof(env));
  shot_clip_path(path, sizeof(path));

  if (getenv("WAYLAND_DISPLAY")) {
    snprintf(cmd, sizeof(cmd), "%s%s/grim -g \"$(%s/slurp)\" -t png %s", env,
             shot_working_dir(), shot_working_dir(), path);
  } else {
    snprintf(cmd, sizeof(cmd), "%s%s/scrot -s %s", env, shot_working_dir(),
             path);
  }

  free(sk_run_terminal_command_blocking(cmd));
  sk_copy_file_path(path);
}

static void pk_screenshot_copy_screen(void) {
  char env[SHOT_BUF];
  char path[SHOT_BUF];
  char cmd[SHOT_BUF * 3];
  shot_env(env, sizeof(env));
  shot_clip_path(path, sizeof(path));

  if (getenv("WAYLAND_DISPLAY")) {
    snprintf(cmd, sizeof(cmd), "%s%s/grim -g \"$(%s/slurp)\" -t png %s", env,
             shot_working_dir(), shot_working_dir(), path);
  } else {
    snprintf(cmd, sizeof(cmd), "%s%s/scrot %s", env, shot_working_dir(), path);
  }

  free(sk_run_terminal_command_blocking(cmd));
  sk_copy_file_path(path);
}

static void pk_ocr_area_to_clipboard(void) {
  char env[SHOT_BUF];
  char cmd[SHOT_BUF * 6];
  shot_env(env, sizeof(env));

  if (getenv("WAYLAND_DISPLAY")) {
    snprintf(cmd, sizeof(cmd),
             "%s%s/grim -g \"$(%s/slurp)\" - | tesseract-ocr stdin stdout -l "
             "eng png",
             env, shot_working_dir(), shot_working_dir());
  } else {
    char temp_path[SHOT_BUF];
    snprintf(temp_path, sizeof(temp_path), "/tmp/skald_ocr_%d", (int)getpid());
    snprintf(cmd, sizeof(cmd),
             "%s%s/scrot -s %s && tesseract-ocr %s stdout -l eng; rm -f %s",
             env, shot_working_dir(), temp_path, temp_path, temp_path);
  }

  char *out = sk_run_terminal_command_blocking(cmd);
  if (!out || !out[0]) {
    free(out);
    return;
  }

  size_t n = strlen(out);
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
    out[--n] = '\0';
  if (out[0])
    sk_copy_to_clipboard(out);
  free(out);
}

#elif defined(_WIN32)

#include <windows.h>
#include <windowsx.h>
#include <wingdi.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

static ULONG_PTR shot_gdiplus = 0;

static bool shot_gdiplus_start(void) {
  if (shot_gdiplus)
    return true;
  GdiplusStartupInput in;
  memset(&in, 0, sizeof(in));
  in.GdiplusVersion = 1;
  GpStatus st = GdiplusStartup(&shot_gdiplus, &in, NULL);
  if (st != Ok) {
    printf("screenshot: GdiplusStartup failed status=%d\n", (int)st);
    shot_gdiplus = 0;
    return false;
  }
  return true;
}

static void shot_ov_release(void);

#define SHOT_SEL_CLASS "SkaldShotSelClass"
#define SHOT_SEL_FRAME 2

#define SEL_DIM 0x6E000000u
#define SEL_CLEAR 0x00000000u
#define SEL_FRAME_C 0xFFFFFFFFu

static bool sel_dragging = false;
static bool sel_done = false;
static bool sel_cancel = false;
static bool sel_has = false;
static POINT sel_anchor = {0, 0};
static RECT sel_prev = {0, 0, 0, 0};
static RECT sel_result = {0, 0, 0, 0};

static int ov_w = 0, ov_h = 0;
static HDC ov_dc = NULL;
static HBITMAP ov_bmp = NULL;
static void *ov_bits = NULL;
static HWND ov_hwnd = NULL;

static void sel_fill(int l, int t, int r, int b, unsigned int argb) {
  if (!ov_bits)
    return;
  if (l < 0)
    l = 0;
  if (t < 0)
    t = 0;
  if (r > ov_w)
    r = ov_w;
  if (b > ov_h)
    b = ov_h;
  for (int y = t; y < b; y++) {
    unsigned int *row = (unsigned int *)ov_bits + (size_t)y * (size_t)ov_w;
    for (int x = l; x < r; x++)
      row[x] = argb;
  }
}

static void sel_paint_sel(int l, int t, int r, int b) {
  sel_fill(l, t, r, b, SEL_CLEAR);
  int f = SHOT_SEL_FRAME;
  sel_fill(l, t, r, t + f, SEL_FRAME_C);
  sel_fill(l, b - f, r, b, SEL_FRAME_C);
  sel_fill(l, t, l + f, b, SEL_FRAME_C);
  sel_fill(r - f, t, r, b, SEL_FRAME_C);
}

static void sel_push(void) {
  if (!ov_hwnd || !ov_dc)
    return;
  HDC screen = GetDC(NULL);
  if (!screen)
    return;
  POINT dst;
  dst.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  dst.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  POINT src;
  src.x = 0;
  src.y = 0;
  SIZE size;
  size.cx = ov_w;
  size.cy = ov_h;
  BLENDFUNCTION blend;
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  SetLayout(ov_dc, 0);
  UpdateLayeredWindow(ov_hwnd, screen, &dst, &size, ov_dc, &src, 0, &blend,
                      ULW_ALPHA);
  ReleaseDC(NULL, screen);
}

static LRESULT CALLBACK ShotSelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_LBUTTONDOWN:
    sel_anchor.x = GET_X_LPARAM(lp);
    sel_anchor.y = GET_Y_LPARAM(lp);
    sel_dragging = true;
    sel_has = false;
    SetCapture(hwnd);
    return 0;
  case WM_MOUSEMOVE: {
    if (!sel_dragging)
      return 0;
    int cx = GET_X_LPARAM(lp);
    int cy = GET_Y_LPARAM(lp);
    if (cx < 0)
      cx = 0;
    if (cx > ov_w)
      cx = ov_w;
    if (cy < 0)
      cy = 0;
    if (cy > ov_h)
      cy = ov_h;
    int l = cx < sel_anchor.x ? cx : sel_anchor.x;
    int r = cx < sel_anchor.x ? sel_anchor.x : cx;
    int t = cy < sel_anchor.y ? cy : sel_anchor.y;
    int b = cy < sel_anchor.y ? sel_anchor.y : cy;
    if (r - l < SHOT_SEL_FRAME * 2)
      r = l + SHOT_SEL_FRAME * 2;
    if (b - t < SHOT_SEL_FRAME * 2)
      b = t + SHOT_SEL_FRAME * 2;
    if (r > ov_w)
      r = ov_w;
    if (b > ov_h)
      b = ov_h;
    if (sel_has)
      sel_fill(sel_prev.left, sel_prev.top, sel_prev.right, sel_prev.bottom,
               SEL_DIM);
    sel_prev.left = l;
    sel_prev.top = t;
    sel_prev.right = r;
    sel_prev.bottom = b;
    sel_paint_sel(l, t, r, b);
    sel_has = true;
    sel_push();
    return 0;
  }
  case WM_LBUTTONUP: {
    if (!sel_dragging)
      return 0;
    sel_dragging = false;
    ReleaseCapture();
    sel_result = sel_prev;
    if (sel_result.right - sel_result.left < 4)
      sel_cancel = true;
    if (sel_result.bottom - sel_result.top < 4)
      sel_cancel = true;
    sel_done = true;
    DestroyWindow(hwnd);
    return 0;
  }
  case WM_RBUTTONDOWN:
  case WM_KEYDOWN:
    if (wp == VK_ESCAPE || msg == WM_RBUTTONDOWN) {
      sel_cancel = true;
      sel_done = true;
      if (sel_dragging) {
        sel_dragging = false;
        ReleaseCapture();
      }
      DestroyWindow(hwnd);
    }
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(hwnd, msg, wp, lp);
}

static HDC cap_dc = NULL;
static HBITMAP cap_bmp = NULL;
static HGDIOBJ cap_old = NULL;
static int cap_vx = 0, cap_vy = 0, cap_w = 0, cap_h = 0;

static void shot_capture_release(void) {
  if (cap_dc) {
    if (cap_old)
      SelectObject(cap_dc, cap_old);
    DeleteDC(cap_dc);
    cap_dc = NULL;
  }
  if (cap_bmp) {
    DeleteObject(cap_bmp);
    cap_bmp = NULL;
  }
  cap_old = NULL;
}

static bool shot_capture_virtual(void) {
  shot_capture_release();
  cap_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  cap_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  cap_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  cap_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (cap_w <= 0 || cap_h <= 0)
    return false;

  HDC screen = GetDC(NULL);
  if (!screen)
    return false;

  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = cap_w;
  bi.bmiHeader.biHeight = -cap_h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void *bits = NULL;
  cap_dc = CreateCompatibleDC(screen);
  if (!cap_dc) {
    ReleaseDC(NULL, screen);
    return false;
  }
  cap_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!cap_bmp || !bits) {
    if (cap_bmp)
      DeleteObject(cap_bmp);
    cap_bmp = NULL;
    DeleteDC(cap_dc);
    cap_dc = NULL;
    ReleaseDC(NULL, screen);
    printf("screenshot: virtual snapshot failed %dx%d err=%lu\n", cap_w, cap_h,
           GetLastError());
    return false;
  }
  cap_old = SelectObject(cap_dc, cap_bmp);
  BitBlt(cap_dc, 0, 0, cap_w, cap_h, screen, cap_vx, cap_vy, SRCCOPY);
  GdiFlush();
  ReleaseDC(NULL, screen);
  printf("screenshot: snapshotted virtual desktop %dx%d at (%d,%d)\n", cap_w,
         cap_h, cap_vx, cap_vy);
  return true;
}

static bool shot_select_area(RECT *out) {
  int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (vw <= 0 || vh <= 0)
    return false;
  if (!shot_capture_virtual())
    printf("screenshot: continuing without snapshot\n");

  static LONG reg = 0;
  if (InterlockedCompareExchange(&reg, 1, 0) == 0) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = ShotSelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.lpszClassName = SHOT_SEL_CLASS;
    RegisterClassA(&wc);
  }

  sel_done = false;
  sel_cancel = false;
  sel_dragging = false;
  sel_has = false;

  ov_w = vw;
  ov_h = vh;
  HDC screen = GetDC(NULL);
  if (!screen)
    return false;
  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = vw;
  bi.bmiHeader.biHeight = -vh;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  ov_dc = CreateCompatibleDC(screen);
  if (!ov_dc) {
    ReleaseDC(NULL, screen);
    return false;
  }
  ov_bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &ov_bits, NULL, 0);
  ReleaseDC(NULL, screen);
  if (!ov_bmp || !ov_bits) {
    printf("screenshot: overlay DIB failed %dx%d err=%lu\n", vw, vh,
           GetLastError());
    shot_ov_release();
    return false;
  }
  SelectObject(ov_dc, ov_bmp);
  sel_fill(0, 0, vw, vh, SEL_DIM);

  HWND hwnd = CreateWindowExA(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                              SHOT_SEL_CLASS, NULL, WS_POPUP, vx, vy, vw, vh,
                              NULL, NULL, GetModuleHandle(NULL), NULL);
  if (!hwnd) {
    printf("screenshot: overlay create failed err=%lu\n", GetLastError());
    shot_ov_release();
    return false;
  }
  ov_hwnd = hwnd;

  ShowWindow(hwnd, SW_SHOW);
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  sel_push();

  MSG msg;
  while (!sel_done) {
    if (!GetMessage(&msg, NULL, 0, 0))
      break;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  ov_hwnd = NULL;
  shot_ov_release();

  if (sel_cancel)
    return false;
  if (sel_result.right - sel_result.left < 2)
    return false;
  if (sel_result.bottom - sel_result.top < 2)
    return false;
  sel_result.left += vx;
  sel_result.right += vx;
  sel_result.top += vy;
  sel_result.bottom += vy;
  if (out)
    *out = sel_result;
  return true;
}

static void shot_ov_release(void) {
  if (ov_dc) {
    DeleteDC(ov_dc);
    ov_dc = NULL;
  }
  if (ov_bmp) {
    DeleteObject(ov_bmp);
    ov_bmp = NULL;
  }
  ov_bits = NULL;
}

static bool pk_capture_rect(const RECT *r, const char *path) {
  if (!r || !path || !path[0])
    return false;
  if (!shot_gdiplus_start())
    return false;

  int w = r->right - r->left;
  int h = r->bottom - r->top;
  if (w <= 0 || h <= 0)
    return false;

  HDC hScreenDC = GetDC(NULL);
  if (!hScreenDC)
    return false;

  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void *bits = NULL;
  HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
  if (!hMemoryDC) {
    printf("screenshot: CreateCompatibleDC failed\n");
    ReleaseDC(NULL, hScreenDC);
    return false;
  }
  HBITMAP hBitmap =
      CreateDIBSection(hScreenDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!hBitmap || !bits) {
    printf("screenshot: CreateDIBSection failed err=%lu for %dx%d\n",
           GetLastError(), w, h);
    if (hBitmap)
      DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    return false;
  }

  HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);
  if (cap_dc && cap_bmp) {
    BitBlt(hMemoryDC, 0, 0, w, h, cap_dc, r->left - cap_vx, r->top - cap_vy,
           SRCCOPY);
  } else {
    BitBlt(hMemoryDC, 0, 0, w, h, hScreenDC, r->left, r->top, SRCCOPY);
  }
  GdiFlush();
  SelectObject(hMemoryDC, hOldBitmap);

  GpBitmap *pBitmap = NULL;
  GpStatus st = GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);
  bool saved = false;
  if (st != Ok || !pBitmap) {
    printf("screenshot: GdipCreateBitmapFromScan0 status=%d\n", (int)st);
  } else {
    UINT num = 0, size = 0;
    if (GdipGetImageEncodersSize(&num, &size) != Ok || num == 0 || size == 0) {
      printf("screenshot: GdipGetImageEncodersSize failed\n");
    } else {
      ImageCodecInfo *encoders = (ImageCodecInfo *)malloc(size);
      if (!encoders) {
        printf("screenshot: encoder alloc failed\n");
      } else if (GdipGetImageEncoders(num, size, encoders) != Ok) {
        printf("screenshot: GdipGetImageEncoders failed\n");
      } else {
        for (UINT j = 0; j < num; j++) {
          if (wcscmp(encoders[j].MimeType, L"image/png") != 0)
            continue;
          wchar_t wpath[MAX_PATH * 4];
          int wlen =
              MultiByteToWideChar(CP_ACP, 0, path, -1, wpath,
                                  (int)(sizeof(wpath) / sizeof(wpath[0])));
          if (wlen <= 0) {
            printf("screenshot: path widen failed\n");
            break;
          }
          GpStatus sv = GdipSaveImageToFile((GpImage *)pBitmap, wpath,
                                            &encoders[j].Clsid, NULL);
          if (sv == Ok)
            saved = true;
          else
            printf("screenshot: GdipSaveImageToFile status=%d path='%s'\n",
                   (int)sv, path);
          break;
        }
      }
      free(encoders);
    }
    GdipDisposeImage((GpImage *)pBitmap);
  }

  DeleteObject(hBitmap);
  DeleteDC(hMemoryDC);
  ReleaseDC(NULL, hScreenDC);
  if (!saved)
    remove(path);
  return saved;
}

static bool shot_dir_ok(const char *dir) {
  if (!dir || !dir[0])
    return false;
  DWORD a = GetFileAttributesA(dir);
  if (a == INVALID_FILE_ATTRIBUTES)
    return false;
  if (!(a & FILE_ATTRIBUTE_DIRECTORY))
    return false;
  return true;
}

static void shot_stage_path(char *out, size_t cap, const char *leaf) {
  char *wd = sk_get_working_dir();
  if (wd && wd[0] && shot_dir_ok(wd)) {
    snprintf(out, cap, "%s/%s", wd, leaf);
    free(wd);
    return;
  }
  if (wd && wd[0])
    printf("screenshot: working dir '%s' missing, using TEMP\n", wd);
  free(wd);
  const char *t = getenv("TEMP");
  if (!t || !t[0])
    t = getenv("TMP");
  if (!t || !t[0])
    t = "C:/Windows/Temp";
  if (!shot_dir_ok(t))
    t = "C:/Windows/Temp";
  snprintf(out, cap, "%s/%s", t, leaf);
}

static void pk_screenshot_copy_area(void) {
  RECT r;
  if (!shot_select_area(&r)) {
    printf("screenshot: no area selected\n");
    return;
  }
  char path[MAX_PATH * 2];
  shot_stage_path(path, sizeof(path), "skald_clip.png");
  if (!pk_capture_rect(&r, path))
    return;
  printf("screenshot: captured %dx%d at (%ld,%ld)\n", (int)(r.right - r.left),
         (int)(r.bottom - r.top), (long)r.left, (long)r.top);
  sk_copy_file_path(path);
  shot_capture_release();
}

static void pk_screenshot_copy_screen(void) {
  RECT r;
  r.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  r.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  r.right = r.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  r.bottom = r.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  char path[MAX_PATH * 2];
  shot_stage_path(path, sizeof(path), "skald_clip.png");
  if (!pk_capture_rect(&r, path))
    return;
  printf("screenshot: captured %dx%d at (%ld,%ld)\n", (int)(r.right - r.left),
         (int)(r.bottom - r.top), (long)r.left, (long)r.top);
  sk_copy_file_path(path);
  shot_capture_release();
}

static void pk_ocr_area_to_clipboard(void) {
  RECT full;
  HDC sc = GetDC(NULL);
  if (!sc)
    return;
  int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  full.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  full.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  full.right = full.left + vw;
  full.bottom = full.top + vh;
  ReleaseDC(NULL, sc);
  printf("screenshot: ocr capture %dx%d\n", vw, vh);

  char png[MAX_PATH * 2];
  shot_stage_path(png, sizeof(png), "skald_ocr.png");
  if (!pk_capture_rect(&full, png))
    return;

  char cmd[2 * MAX_PATH + 64];
  snprintf(cmd, sizeof(cmd), "tesseract \"%s\" stdout -l eng", png);
  char *out = sk_run_terminal_command_blocking(cmd);
  remove(png);
  if (!out || !out[0]) {
    free(out);
    return;
  }

  size_t n = strlen(out);
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
    out[--n] = '\0';
  if (out[0])
    sk_copy_to_clipboard(out);
  free(out);
}

#endif

SKALD_FEATURE_INIT {
  sk_register_hotkey(.hotkey = "Ctrl+Shift+X", .cb = pk_screenshot_copy_area);
  sk_register_hotkey(.hotkey = "Ctrl+Alt+X", .cb = pk_screenshot_copy_screen);
  sk_register_hotkey(.hotkey = "X", .cb = pk_screenshot_copy_area,
                     .whitelist = {"zathura"});
  sk_register_hotkey(.hotkey = "Alt+Shift+C", .cb = pk_ocr_area_to_clipboard);
  sk_add_to_cc(.display_name = "Screenshot Region",
               .on_enter = (bool (*)(void))pk_screenshot_copy_area,
               .type = "Screenshot", .icon = "/camera.png");
  sk_add_to_cc(.display_name = "OCR Region ",
               .on_enter = (bool (*)(void))pk_ocr_area_to_clipboard,
               .type = "Screenshot", .icon = "/scan-text.png");
}
