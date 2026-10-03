#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/timer.h"
#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <string.h>

#define WIN32_SHADER_SMOOTHNESS 10

HFONT CreateCustomNerdFont(int height, const char* face_name) {
  return FH_TO_HFONT(Platform_CreateFont(height, face_name));
}

void win32_apply_rounded_corners(HWND hwnd, int w, int h) {
  if (!hwnd || !IsWindow(hwnd)) return;
  if (w <= 0 || h <= 0) {
    RECT rc; GetWindowRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
  }

  HMODULE dwm = LoadLibraryA("dwmapi.dll");
  if (dwm) {
    HRESULT (WINAPI *pDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD) = (void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (pDwmSetWindowAttribute) {
      const DWORD DWMWA_BORDER_COLOR = 34;
      COLORREF border = 0xFFFFFFFE;
      pDwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
      if (!RoundedCorners) {
        FreeLibrary(dwm);
        return;
      }
      const int DWMWCP_ROUND = 2;
      const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
      HRESULT hr = pDwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));
      if (SUCCEEDED(hr)) {
        FreeLibrary(dwm);
        return;
      }
    }
    FreeLibrary(dwm);
  }
  if (!RoundedCorners) return;

  int radius = CornerRadius;
  if (radius < 0) radius = 0;
  if (radius > 32) radius = 32;
  HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius*2, radius*2);
  if (rgn) SetWindowRgn(hwnd, rgn, TRUE);
}

static inline unsigned int win32_to_rgb(COLORREF c) {
  return ((unsigned int)GetRValue(c) << 16) | ((unsigned int)GetGValue(c) << 8) | (unsigned int)GetBValue(c);
}
static COLORREF LerpColor(COLORREF a, COLORREF b, float t) {
  unsigned int mixed = Color_Lerp(win32_to_rgb(a), win32_to_rgb(b), t);
  return RGB((mixed >> 16) & 0xFF, (mixed >> 8) & 0xFF, mixed & 0xFF);
}
static COLORREF DeriveLighterWin(COLORREF base, float intensity) {
  unsigned char r, g, b;
  Color_DeriveLighter(win32_to_rgb(base), intensity, &r, &g, &b);
  return RGB(r, g, b);
}
static unsigned int HashNoiseWin(int x, int y) {
  return Color_HashNoise(x, y);
}
static float win32_compute_gamma(void) {
  return Color_ShaderGamma(WIN32_SHADER_SMOOTHNESS);
}
static inline float win32_dither(int x, int y) {
  return Color_BayerDither(x, y);
}

typedef struct { HBITMAP bmp; HDC dc; int w; int h; COLORREF base; int preset; float gamma; float intensity; } win32_gradient_cache;
static win32_gradient_cache g_bgCache = {0};
static win32_gradient_cache g_selCache = {0};

static void win32_free_gradient_cache(win32_gradient_cache* c) {
  if (!c) return;
  if (c->dc) { DeleteDC(c->dc); c->dc = NULL; }
  if (c->bmp) { DeleteObject(c->bmp); c->bmp = NULL; }
  c->w = c->h = 0;
  c->preset = -1;
}

void win32_free_bg_cache(void) {
  win32_free_gradient_cache(&g_bgCache);
  win32_free_gradient_cache(&g_selCache);
}

static void win32_ensure_gradient_cache(win32_gradient_cache* c, HDC refDC, int containerW, int containerH, COLORREF base, int preset) {
  float gamma = win32_compute_gamma();
  float intensity = ShaderIntensity;
  if (c->bmp && c->dc && c->w == containerW && c->h == containerH && c->base == base && c->preset == preset && fabsf(c->gamma - gamma) < 0.001f && fabsf(c->intensity - intensity) < 0.001f) {
    return;
  }
  win32_free_gradient_cache(c);
  if (containerW <= 0 || containerH <= 0) return;
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = containerW;
  bmi.bmiHeader.biHeight = -containerH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = NULL;
  HBITMAP hBmp = CreateDIBSection(refDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!hBmp || !bits) return;
  uint32_t* pixels = (uint32_t*)bits;
  COLORREF light = DeriveLighterWin(base, 1.0f);
  if (preset == 1) {
    for (int y = 0; y < containerH; y++) {
      float t = containerH > 1 ? (float)y / (float)(containerH - 1) : 0;
      t = powf(t, gamma);
      COLORREF col = LerpColor(base, light, t * 0.9f);
      BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
      float d = win32_dither(0, y);
      int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
      if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
      uint32_t pix = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      uint32_t* row = pixels + y * containerW;
      for (int x = 0; x < containerW; x++) row[x] = pix;
    }
  } else if (preset == 2) {
    float denom = (float)(containerW + containerH); if (denom < 1) denom = 1;
    for (int y = 0; y < containerH; y++) {
      uint32_t* row = pixels + y * containerW;
      for (int x = 0; x < containerW; x++) {
        float t = ((float)x + (float)y) / denom;
        t = powf(t, gamma);
        COLORREF col = LerpColor(base, light, t * 0.9f);
        BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
        float d = win32_dither(x, y);
        int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
        if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
        row[x] = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      }
    }
  } else if (preset == 3) {
    float cx = containerW * 0.5f, cy = containerH * 0.35f;
    float maxDist = sqrtf(cx*cx + cy*cy); if (maxDist < 1) maxDist = 1;
    for (int y = 0; y < containerH; y++) {
      uint32_t* row = pixels + y * containerW;
      for (int x = 0; x < containerW; x++) {
        float dx = x - cx, dy = y - cy;
        float dist = sqrtf(dx*dx + dy*dy) / maxDist; if (dist > 1) dist = 1;
        float t = powf(dist, gamma) * 0.85f;
        COLORREF col = LerpColor(light, base, t);
        BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
        float d = win32_dither(x, y);
        int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
        if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
        row[x] = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      }
    }
  } else if (preset == 4) {
    for (int y = 0; y < containerH; y++) {
      float tBase = containerH > 1 ? (float)y / (float)(containerH - 1) : 0;
      tBase = powf(tBase, gamma);
      uint32_t* row = pixels + y * containerW;
      for (int x = 0; x < containerW; x++) {
        float t = tBase * 0.85f;
        COLORREF col = LerpColor(base, light, t);
        unsigned int n = HashNoiseWin(x, y);
        int delta = (int)(n & 0x1F) - 16;
        delta = (int)(delta * ShaderIntensity * 0.5f);
        int r = GetRValue(col) + delta, g = GetGValue(col) + delta, b = GetBValue(col) + delta;
        float d = win32_dither(x, y);
        r = (int)(r + d); g = (int)(g + d); b = (int)(b + d);
        if (r < 0) r = 0; if (r > 255) r = 255; if (g < 0) g = 0; if (g > 255) g = 255; if (b < 0) b = 0; if (b > 255) b = 255;
        row[x] = (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
      }
    }
  } else {
    uint32_t pix = (uint32_t)GetBValue(base) | ((uint32_t)GetGValue(base) << 8) | ((uint32_t)GetRValue(base) << 16);
    for (int i = 0; i < containerW * containerH; i++) pixels[i] = pix;
  }
  HDC memDC = CreateCompatibleDC(refDC);
  HBITMAP old = (HBITMAP)SelectObject(memDC, hBmp);
  c->bmp = hBmp;
  c->dc = memDC;
  c->w = containerW; c->h = containerH;
  c->base = base; c->preset = preset;
  c->gamma = gamma; c->intensity = intensity;
}

static void win32_draw_gradient_dib(HDC hdc, RECT* rc, COLORREF base, int preset, int containerW, int containerH, int offsetX, int offsetY) {
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) return;
  if (preset == 1 || preset == 2 || preset == 3 || preset == 4) {
    if (containerW > 0 && containerH > 0 && (containerW != w || containerH != h || offsetX != 0 || offsetY != 0)) {
      win32_ensure_gradient_cache(&g_bgCache, hdc, containerW, containerH, base, preset);
      if (g_bgCache.dc && g_bgCache.bmp) {
        BitBlt(hdc, rc->left, rc->top, w, h, g_bgCache.dc, offsetX, offsetY, SRCCOPY);
        return;
      }
    }
  }
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = NULL;
  HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!hBmp || !bits) {
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    return;
  }
  uint32_t* pixels = (uint32_t*)bits;
  COLORREF light = DeriveLighterWin(base, 1.0f);
  float gamma = win32_compute_gamma();
  if (preset == 1) {
    for (int y = 0; y < h; y++) {
      float t;
      if (containerW > 0 && containerH > 0) {
        int globalY = offsetY + y;
        t = containerH > 1 ? (float)globalY / (float)(containerH - 1) : 0;
      } else {
        t = h > 1 ? (float)y / (float)(h - 1) : 0;
      }
      t = powf(t, gamma);
      COLORREF col = LerpColor(base, light, t * 0.9f);
      BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
      int dY = (containerW > 0 && containerH > 0) ? (offsetY + y) : y;
      float d = win32_dither(0, dY);
      int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
      if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
      uint32_t pix = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      uint32_t* row = pixels + y * w;
      for (int x = 0; x < w; x++) row[x] = pix;
    }
  } else if (preset == 2) {
    float denom;
    if (containerW > 0 && containerH > 0) denom = (float)(containerW + containerH); else denom = (float)(w + h);
    if (denom < 1) denom = 1;
    for (int y = 0; y < h; y++) {
      uint32_t* row = pixels + y * w;
      for (int x = 0; x < w; x++) {
        float t;
        if (containerW > 0 && containerH > 0) {
          int gx = offsetX + x; int gy = offsetY + y;
          t = ((float)gx + (float)gy) / denom;
        } else {
          t = ((float)x + (float)y) / denom;
        }
        t = powf(t, gamma);
        COLORREF col = LerpColor(base, light, t * 0.9f);
        BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
        int dX2 = (containerW > 0 && containerH > 0) ? (offsetX + x) : x;
        int dY2 = (containerW > 0 && containerH > 0) ? (offsetY + y) : y;
        float d = win32_dither(dX2, dY2);
        int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
        if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
        row[x] = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      }
    }
  } else if (preset == 3) {
    float cx, cy, maxDist;
    if (containerW > 0 && containerH > 0) { cx = containerW * 0.5f; cy = containerH * 0.35f; maxDist = sqrtf(cx*cx + cy*cy); } else { cx = w * 0.5f; cy = h * 0.35f; maxDist = sqrtf(cx*cx + cy*cy); }
    if (maxDist < 1) maxDist = 1;
    for (int y = 0; y < h; y++) {
      uint32_t* row = pixels + y * w;
      for (int x = 0; x < w; x++) {
        float dx, dy;
        if (containerW > 0 && containerH > 0) { dx = (offsetX + x) - cx; dy = (offsetY + y) - cy; } else { dx = x - cx; dy = y - cy; }
        float dist = sqrtf(dx*dx + dy*dy) / maxDist; if (dist > 1) dist = 1;
        float t = powf(dist, gamma) * 0.85f;
        COLORREF col = LerpColor(light, base, t);
        BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
        int dX3 = (containerW > 0 && containerH > 0) ? (offsetX + x) : x;
        int dY3 = (containerW > 0 && containerH > 0) ? (offsetY + y) : y;
        float d = win32_dither(dX3, dY3);
        int nr = (int)(r + d); int ng = (int)(g + d); int nb = (int)(b + d);
        if (nr < 0) nr = 0; if (nr > 255) nr = 255; if (ng < 0) ng = 0; if (ng > 255) ng = 255; if (nb < 0) nb = 0; if (nb > 255) nb = 255;
        row[x] = (uint32_t)nb | ((uint32_t)ng << 8) | ((uint32_t)nr << 16);
      }
    }
  } else if (preset == 4) {
    for (int y = 0; y < h; y++) {
      float tBase;
      if (containerW > 0 && containerH > 0) {
        int gy = offsetY + y;
        tBase = containerH > 1 ? (float)gy / (float)(containerH - 1) : 0;
      } else {
        tBase = h > 1 ? (float)y / (float)(h - 1) : 0;
      }
      tBase = powf(tBase, gamma);
      uint32_t* row = pixels + y * w;
      for (int x = 0; x < w; x++) {
        float t = tBase * 0.85f;
        COLORREF col = LerpColor(base, light, t);
        unsigned int gx = (containerW > 0 && containerH > 0) ? (unsigned int)(offsetX + x) : (unsigned int)x;
        unsigned int gy = (containerW > 0 && containerH > 0) ? (unsigned int)(offsetY + y) : (unsigned int)y;
        unsigned int n = HashNoiseWin((int)gx, (int)gy);
        int delta = (int)(n & 0x1F) - 16;
        delta = (int)(delta * ShaderIntensity * 0.5f);
        int r = GetRValue(col) + delta, g = GetGValue(col) + delta, b = GetBValue(col) + delta;
        float d = win32_dither((int)gx, (int)gy);
        r = (int)(r + d); g = (int)(g + d); b = (int)(b + d);
        if (r < 0) r = 0; if (r > 255) r = 255; if (g < 0) g = 0; if (g > 255) g = 255; if (b < 0) b = 0; if (b > 255) b = 255;
        row[x] = (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
      }
    }
  } else {
    uint32_t pix = (uint32_t)GetBValue(base) | ((uint32_t)GetGValue(base) << 8) | ((uint32_t)GetRValue(base) << 16);
    for (int i = 0; i < w*h; i++) pixels[i] = pix;
  }
  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP old = (HBITMAP)SelectObject(memDC, hBmp);
  BitBlt(hdc, rc->left, rc->top, w, h, memDC, 0, 0, SRCCOPY);
  SelectObject(memDC, old);
  DeleteDC(memDC);
  DeleteObject(hBmp);
}

void win32_draw_selection_background(HDC hdc, RECT* rc, COLORREF base) {
  PROFILE_START(timer);
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "WinSel solid"); return; }
  HBRUSH br = CreateSolidBrush(base);
  FillRect(hdc, rc, br);
  DeleteObject(br);
  PROFILE_END_VAR(timer, "sel-solid");
  return;
}

void win32_draw_background_slice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY) {
  int preset = ShaderPreset;
  if (!EnableShaders || preset == 0) {
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    return;
  }
  win32_draw_gradient_dib(hdc, rc, base, preset, containerW, containerH, offsetX, offsetY);
  return;

}

void win32_fill_rounded_rect(HDC hdc, RECT* rc, COLORREF col, int radius) {
  if (!RoundedCorners || radius <= 0) {
    HBRUSH br = CreateSolidBrush(col);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    return;
  }
  if (radius > (rc->right - rc->left)/2) radius = (rc->right - rc->left)/2;
  if (radius > (rc->bottom - rc->top)/2) radius = (rc->bottom - rc->top)/2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right+1, rc->bottom+1, radius*2, radius*2);
  SelectClipRgn(hdc, rgn);
  HBRUSH br = CreateSolidBrush(col);
  FillRect(hdc, rc, br);
  DeleteObject(br);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

void win32_fill_rounded_rect_slice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY, int radius) {
  if (!RoundedCorners || radius <= 0) {
    win32_draw_background_slice(hdc, rc, base, containerW, containerH, offsetX, offsetY);
    return;
  }
  if (radius > (rc->right - rc->left)/2) radius = (rc->right - rc->left)/2;
  if (radius > (rc->bottom - rc->top)/2) radius = (rc->bottom - rc->top)/2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right+1, rc->bottom+1, radius*2, radius*2);
  SelectClipRgn(hdc, rgn);
  win32_draw_background_slice(hdc, rc, base, containerW, containerH, offsetX, offsetY);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

void win32_draw_rounded_selection(HDC hdc, RECT* rc, COLORREF base, int radius) {
  if (!RoundedCorners || radius <= 0) {
    win32_draw_selection_background(hdc, rc, base);
    return;
  }
  if (radius > (rc->right - rc->left)/2) radius = (rc->right - rc->left)/2;
  if (radius > (rc->bottom - rc->top)/2) radius = (rc->bottom - rc->top)/2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right+1, rc->bottom+1, radius*2, radius*2);
  SelectClipRgn(hdc, rgn);
  win32_draw_selection_background(hdc, rc, base);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

void win32_draw_background(HDC hdc, RECT* rc) {
  PROFILE_START(timer);
  const char* name = "solid";
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "WinBg solid"); return; }
  COLORREF base = COLOR_BG;
  if (!EnableShaders || ShaderPreset == 0) {
    name = "solid";
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    PROFILE_END_VAR(timer, name);
    return;
  }
  int preset = ShaderPreset;
  if (preset == 1) name = "vertical";
  else if (preset == 2) name = "diagonal";
  else if (preset == 3) name = "radial";
  else if (preset == 4) name = "noise";
  win32_ensure_gradient_cache(&g_bgCache, hdc, w, h, base, preset);
  if (g_bgCache.dc && g_bgCache.bmp) {
    BitBlt(hdc, rc->left, rc->top, w, h, g_bgCache.dc, 0, 0, SRCCOPY);
    PROFILE_END_VAR(timer, name);
    return;
  }
  win32_draw_gradient_dib(hdc, rc, base, preset, 0, 0, 0, 0);
  PROFILE_END_VAR(timer, name);
  return;

  PROFILE_END_VAR(timer, name);
}

void UpdateTextCaret(WindowHandle hwndEditControl)
{
  HWND hwnd = WH_TO_HWND(hwndEditControl);
  if (!hwnd || !IsWindow(hwnd)) return;
  InvalidateRect(hwnd, NULL, FALSE);
}

bool UI_DrawCaret(HDC hdc, HWND hwnd) {
  if (!hdc || !hwnd || !IsWindow(hwnd)) return false;
  DWORD selStart = 0, selEnd = 0;
  SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
  if (selStart != selEnd) return false;

  HFONT hFont = (HFONT)SendMessageA(hwnd, WM_GETFONT, 0, 0);
  HFONT oldFont = hFont ? (HFONT)SelectObject(hdc, hFont) : NULL;
  TEXTMETRIC tm;
  memset(&tm, 0, sizeof(tm));
  GetTextMetrics(hdc, &tm);

  RECT rc;
  GetClientRect(hwnd, &rc);
  int lineBox = tm.tmAscent + tm.tmDescent;
  if (lineBox <= 0) lineBox = FontSize;
  int caretHeight = FontSize;
  if (caretHeight > rc.bottom && rc.bottom > 0) caretHeight = rc.bottom;
  int caretWidth = IsInsertMode() ? PROMPT_CARET_W_INSERT : PromptNormalCaretWidth();

  int cx = 0;
  int lineTop = 0;
  struct { DWORD left; DWORD right; } margins;
  margins.left = 0;
  margins.right = 0;
  SendMessageA(hwnd, EM_GETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, (LPARAM)&margins);

  char text[2048] = {0};
  GetWindowTextA(hwnd, text, (int)sizeof(text) - 1);
  int tlen = (int)strlen(text);
  int qi = (int)selStart;
  if (qi > tlen) qi = tlen;
  if (qi < 0) qi = 0;

  SIZE tsz;
  memset(&tsz, 0, sizeof(tsz));
  GetTextExtentPoint32A(hdc, text, qi, &tsz);
  int bias = tm.tmAveCharWidth * PROMPT_CARET_X_BIAS_NUM / PROMPT_CARET_X_BIAS_DEN;
  cx = (int)margins.left + tsz.cx + bias;

  {
    LRESULT ep = SendMessageA(hwnd, EM_POSFROMCHAR, 0, 0);
    lineTop = (ep != (LRESULT)-1) ? (short)HIWORD(ep) : 0;
  }
  if (cx < (int)margins.left) cx = (int)margins.left;
  int rightLimit = rc.right - (int)margins.right;
  if (cx + caretWidth > rightLimit && rightLimit > caretWidth) cx = rightLimit - caretWidth;
  int cy = lineTop + (lineBox - caretHeight) / 2;
  if (cy + caretHeight > rc.bottom && rc.bottom > 0) cy = rc.bottom - caretHeight;
  if (cy < 0) cy = 0;

  RECT cr2;
  cr2.left = cx;
  cr2.top = cy;
  cr2.right = cx + caretWidth;
  cr2.bottom = cy + caretHeight;
  HBRUSH br = CreateSolidBrush(COLOR_CURSOR);
  FillRect(hdc, &cr2, br);
  DeleteObject(br);
  if (oldFont) SelectObject(hdc, oldFont);
  return true;
}

void CalculateWindowMetrics(WindowLayout* layout,int height , int width,int item_height) {
  POINT pt = { 0, 0 };
  GetCursorPos(&pt);
  HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoA(hMonitor, &mi);

  int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  float ui_scale = Platform_GetUIScale();

  layout->w = (int)(PROMPT_W * ui_scale);
  layout->item_height = item_height;

  layout->h = (int)(PROMPT_H * ui_scale);

  if (layout->h < height) layout->h = height;
  if (layout->h > width) layout->h = width;

  layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
  layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
  log_info("CalculateWindowMetrics cursor=(%ld,%ld) mon=(%ld,%ld %ldx%ld) layout=(%d,%d %dx%d)",
      pt.x, pt.y, mi.rcMonitor.left, mi.rcMonitor.top, monitorWidth, monitorHeight,
      layout->x, layout->y, layout->w, layout->h);
}
void ForceWindowFocus(WindowHandle hwndTarget, WindowHandle hwndInput) {
  ShowWindow(WH_TO_HWND(hwndTarget), SW_SHOW);

  SetWindowPos(WH_TO_HWND(hwndTarget), HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

  HWND hForeground = GetForegroundWindow();
  DWORD foregroundThreadID = GetWindowThreadProcessId(hForeground, NULL);
  DWORD targetThreadID = GetWindowThreadProcessId(WH_TO_HWND(hwndTarget), NULL);
  DWORD currentThreadID = GetCurrentThreadId();

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, TRUE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, TRUE);

  SetForegroundWindow(WH_TO_HWND(hwndTarget));
  SetActiveWindow(WH_TO_HWND(hwndTarget));
  BringWindowToTop(WH_TO_HWND(hwndTarget));

  if (WH_TO_HWND(hwndInput) != NULL && IsWindowVisible(WH_TO_HWND(hwndInput))) {
    DWORD selStart = 0, selEnd = 0;
    SendMessageA(WH_TO_HWND(hwndInput), EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    SetFocus(WH_TO_HWND(hwndInput));
    UI_Edit_UpdateUIState(hwndInput, UIS_CLEAR, UISF_HIDEFOCUS);
    UI_Edit_SetSel(hwndInput, selStart, selEnd);
    UpdateTextCaret(hwndInput);
  } else if (WH_TO_HWND(hwndTarget) != NULL && IsWindowVisible(WH_TO_HWND(hwndTarget))) {
    SetFocus(WH_TO_HWND(hwndTarget));
  }

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}

void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb) {

  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  int lineHeight = FontSize + PROMPT_EDIT_H_BONUS;
  HDC metricDC = GetDC(NULL);
  if (metricDC) {
    HDC probe = CreateCompatibleDC(metricDC);
    if (probe) {
      HFONT hOld = hFont ? (HFONT)SelectObject(probe, hFont) : NULL;
      TEXTMETRIC tm;
      memset(&tm, 0, sizeof(tm));
      if (GetTextMetrics(probe, &tm) && tm.tmHeight > 0) lineHeight = tm.tmHeight;
      if (hOld) SelectObject(probe, hOld);
      DeleteDC(probe);
    }
    ReleaseDC(NULL, metricDC);
  }

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      PROMPT_EDIT_INSET, PROMPT_EDIT_INSET,
      layout->w - PROMPT_EDIT_SCROLL_PAD, lineHeight,
      WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));
  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) {
    return;
  }

  SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EM_SETMARGINS,
      EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0, 0));

  FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EditSubclassProc, 0, 0);

  if (count >= 0) {
    int listTop = PromptListTop();
    int listH = layout->h - listTop - PromptListBottomMargin();
    if (listH < 0) listH = 0;
    FlintContext_Get()->window.hListBox = WH_FROM_HWND(CreateWindowExA(
        WS_EX_COMPOSITED, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
        0, listTop, layout->w, listH, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
        ));

    if (WH_TO_HWND(FlintContext_Get()->window.hListBox) == NULL) {
      return;
    }

    SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hListBox), ListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(FlintContext_Get()->window.hListBox, hFont);
    UI_LB_SetItemHeight(FlintContext_Get()->window.hListBox, layout->item_height);

    int max_width = layout->w;

    HDC hdc = GetDC(WH_TO_HWND(FlintContext_Get()->window.hListBox));
    if (!hdc) {
      return;
    }

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);


    if (lb) {
        ListBoxInfo_Init(lb);
        if (list) {
            lb->populated_count = count;
            lb->total_matches = count;
            lb->last_item_count = count;
            lb->selected_index = -1;
            for (int i = 0; i < count && i < MAX_VISIBLE_ITEMS; i++) {
                if (list[i]) {
                    strncpy(lb->populated_cache[i], list[i], 511);
                    lb->populated_cache[i][511] = '\0';
                    lb->populated_cache[i][strcspn(lb->populated_cache[i], "\r\n")] = '\0';
                } else {
                    lb->populated_cache[i][0] = '\0';
                }
                lb->matching_indices[i] = i;
            }
            BuildAliasDisplayItems(lb, list, count);
        } else {
            lb->populated_count = 0;
            lb->total_matches = 0;
            lb->last_item_count = 0;
            lb->selected_index = -1;
            for (int i=0;i<MAX_VISIBLE_ITEMS;i++) lb->matching_indices[i]=i;
            BuildAliasDisplayItems(lb, NULL, 0);
            log_info("list NULL count=%d lb_count=0", count);
        }
    }
    SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hListBox), LB_SETCOUNT, (lb ? lb->populated_count : count), 0);

    UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox));

    SelectObject(hdc, hOldFont);
    ReleaseDC(WH_TO_HWND(FlintContext_Get()->window.hListBox), hdc);

    UI_SetHorizontalExtent(FlintContext_Get()->window.hListBox, max_width);
  }
}
