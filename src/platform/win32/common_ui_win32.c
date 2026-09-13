#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "config.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/common_ui.h"
#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

HFONT CreateCustomNerdFont(int height, const char* face_name) {
  return FH_TO_HFONT(Platform_CreateFont(height, face_name));
}

void Win32_ApplyRoundedCorners(HWND hwnd, int w, int h) {
  if (!RoundedCorners) return;
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

  int radius = CornerRadius;
  if (radius < 0) radius = 0;
  if (radius > 32) radius = 32;
  HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius*2, radius*2);
  if (rgn) SetWindowRgn(hwnd, rgn, TRUE);
}

static COLORREF LerpColor(COLORREF a, COLORREF b, float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  BYTE ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
  BYTE br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
  BYTE rr = (BYTE)(ar + (br - ar) * t + 0.5f);
  BYTE rg = (BYTE)(ag + (bg - ag) * t + 0.5f);
  BYTE rb = (BYTE)(ab + (bb - ab) * t + 0.5f);
  return RGB(rr, rg, rb);
}
static COLORREF DeriveLighterWin(COLORREF base, float intensity) {
  float f = ShaderIntensity;
  if (f < 0) f = 0; if (f > 1) f = 1;
  float eff = f * intensity;
  BYTE r = GetRValue(base), g = GetGValue(base), b = GetBValue(base);
  BYTE nr = (BYTE)(r + (90 - r) * eff + 35 * eff + 0.5f);
  BYTE ng = (BYTE)(g + (90 - g) * eff + 35 * eff + 0.5f);
  BYTE nb = (BYTE)(b + (140 - b) * eff + 40 * eff + 0.5f);
  if (nr > 255) nr = 255; if (ng > 255) ng = 255; if (nb > 255) nb = 255;
  return RGB(nr, ng, nb);
}
static unsigned int HashNoiseWin(int x, int y) {
  unsigned int h = (unsigned int)(x * 374761393U + y * 668265263U);
  h = (h ^ (h >> 13)) * 1274126177U;
  return h & 0xFF;
}
static float Win32_ComputeGamma(void) {
  float s = ShaderSmoothness;
  if (s < 0.1f) s = 0.1f; if (s > 10.0f) s = 10.0f;
  float g = 2.0f - s * 0.14f;
  if (g < 0.45f) g = 0.45f; if (g > 2.2f) g = 2.2f;
  return g;
}
static inline float Win32_Dither(int x, int y) {
  static const int bayer[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
  return ((bayer[(y & 3)*4 + (x & 3)] / 16.0f) - 0.5f) * 1.2f;
}

// ---- fast DIB gradient helper with caching for slices ----
static HBITMAP g_bgCacheBmp = NULL;
static HDC g_bgCacheDC = NULL;
static int g_bgCacheW = 0, g_bgCacheH = 0;
static COLORREF g_bgCacheBase = 0;
static int g_bgCachePreset = -1;
static float g_bgCacheGamma = -1;
static float g_bgCacheIntensity = -1;

static void Win32_FreeBgCache(void) {
  if (g_bgCacheDC) { DeleteDC(g_bgCacheDC); g_bgCacheDC = NULL; }
  if (g_bgCacheBmp) { DeleteObject(g_bgCacheBmp); g_bgCacheBmp = NULL; }
  g_bgCacheW = g_bgCacheH = 0;
  g_bgCachePreset = -1;
}

static void Win32_EnsureBgCache(HDC refDC, int containerW, int containerH, COLORREF base, int preset) {
  float gamma = Win32_ComputeGamma();
  float intensity = ShaderIntensity;
  if (g_bgCacheBmp && g_bgCacheDC && g_bgCacheW == containerW && g_bgCacheH == containerH && g_bgCacheBase == base && g_bgCachePreset == preset && fabsf(g_bgCacheGamma - gamma) < 0.001f && fabsf(g_bgCacheIntensity - intensity) < 0.001f) {
    return;
  }
  Win32_FreeBgCache();
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
      float d = Win32_Dither(0, y);
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
        float d = Win32_Dither(x, y);
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
        float d = Win32_Dither(x, y);
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
        float d = Win32_Dither(x, y);
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
  // keep DC for blitting slices
  g_bgCacheBmp = hBmp;
  g_bgCacheDC = memDC;
  g_bgCacheW = containerW; g_bgCacheH = containerH;
  g_bgCacheBase = base; g_bgCachePreset = preset;
  g_bgCacheGamma = gamma; g_bgCacheIntensity = intensity;
  // old is NULL, no need to restore
}

static void Win32_DrawGradientDIB(HDC hdc, RECT* rc, COLORREF base, int preset, int containerW, int containerH, int offsetX, int offsetY) {
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) return;
  if (preset == 1 || preset == 2 || preset == 3 || preset == 4) {
    // for slice, use cache of full container and BitBlt slice
    if (containerW > 0 && containerH > 0 && (containerW != w || containerH != h || offsetX != 0 || offsetY != 0)) {
      Win32_EnsureBgCache(hdc, containerW, containerH, base, preset);
      if (g_bgCacheDC && g_bgCacheBmp) {
        BitBlt(hdc, rc->left, rc->top, w, h, g_bgCacheDC, offsetX, offsetY, SRCCOPY);
        return;
      }
    }
  }
  // fallback: create DIB for this rc directly (for full or selection)
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
  float gamma = Win32_ComputeGamma();
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
      float d = Win32_Dither(0, y);
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
        float d = Win32_Dither(x, y);
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
        float d = Win32_Dither(x, y);
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
        unsigned int n = HashNoiseWin(x, y);
        int delta = (int)(n & 0x1F) - 16;
        delta = (int)(delta * ShaderIntensity * 0.5f);
        int r = GetRValue(col) + delta, g = GetGValue(col) + delta, b = GetBValue(col) + delta;
        float d = Win32_Dither(x, y);
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

void Win32_DrawSelectionBackground(HDC hdc, RECT* rc, COLORREF base) {
  PROFILE_START(timer);
  const char* name = "sel-solid";
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "WinSel solid"); return; }
  bool useShader = EnableSelectionShaders && SelectionShaderPreset != 0;
  if (!useShader) {
    if (!EnableShaders || ShaderPreset == 0) {
      name = "sel-solid";
      HBRUSH br = CreateSolidBrush(base);
      FillRect(hdc, rc, br);
      DeleteObject(br);
      PROFILE_END_VAR(timer, name);
      return;
    } else {
      HBRUSH br = CreateSolidBrush(base);
      FillRect(hdc, rc, br);
      DeleteObject(br);
      PROFILE_END_VAR(timer, name);
      return;
    }
  }
  int preset = SelectionShaderPreset;
  if (preset == 1) name = "sel-vertical";
  else if (preset == 3) name = "sel-diagonal";
  else if (preset == 2) name = "sel-radial";
  else if (preset == 4) name = "sel-noise";
  Win32_DrawGradientDIB(hdc, rc, base, preset, 0, 0, 0, 0);
  PROFILE_END_VAR(timer, name);
  return;

}

// Cohesive background slice: draws the window's background gradient sampled at offset within container
void Win32_DrawBackgroundSlice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY) {
  int preset = ShaderPreset;
  if (!EnableShaders || preset == 0) {
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    return;
  }
  Win32_DrawGradientDIB(hdc, rc, base, preset, containerW, containerH, offsetX, offsetY);
  return;

}

void Win32_FillRoundedRect(HDC hdc, RECT* rc, COLORREF col, int radius) {
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

void Win32_FillRoundedRectSlice(HDC hdc, RECT* rc, COLORREF base, int containerW, int containerH, int offsetX, int offsetY, int radius) {
  if (!RoundedCorners || radius <= 0) {
    Win32_DrawBackgroundSlice(hdc, rc, base, containerW, containerH, offsetX, offsetY);
    return;
  }
  if (radius > (rc->right - rc->left)/2) radius = (rc->right - rc->left)/2;
  if (radius > (rc->bottom - rc->top)/2) radius = (rc->bottom - rc->top)/2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right+1, rc->bottom+1, radius*2, radius*2);
  SelectClipRgn(hdc, rgn);
  Win32_DrawBackgroundSlice(hdc, rc, base, containerW, containerH, offsetX, offsetY);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

void Win32_DrawRoundedSelection(HDC hdc, RECT* rc, COLORREF base, int radius) {
  if (!RoundedCorners || radius <= 0) {
    Win32_DrawSelectionBackground(hdc, rc, base);
    return;
  }
  if (radius > (rc->right - rc->left)/2) radius = (rc->right - rc->left)/2;
  if (radius > (rc->bottom - rc->top)/2) radius = (rc->bottom - rc->top)/2;
  int save = SaveDC(hdc);
  HRGN rgn = CreateRoundRectRgn(rc->left, rc->top, rc->right+1, rc->bottom+1, radius*2, radius*2);
  SelectClipRgn(hdc, rgn);
  Win32_DrawSelectionBackground(hdc, rc, base);
  SelectClipRgn(hdc, NULL);
  RestoreDC(hdc, save);
  DeleteObject(rgn);
}

void Win32_DrawBackground(HDC hdc, RECT* rc) {
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
  Win32_DrawGradientDIB(hdc, rc, base, preset, 0, 0, 0, 0);
  PROFILE_END_VAR(timer, name);
  return;

  PROFILE_END_VAR(timer, name);
}

void UpdateTextCaret(WindowHandle hwndEditControl)
{
  int caretWidth = 1;
  int caretHeight = FontSize;

  if (IsInsertMode()) {
    caretWidth = 2;
  }
  else if (IsNormalMode()) {
    caretWidth = 12;
  }
  else {
    caretWidth = 12;
  }

  static int lastWidth = -1;
  static HWND lastHwnd = NULL;

  if (caretWidth != lastWidth || WH_TO_HWND(hwndEditControl) != lastHwnd) {
    DestroyCaret();
    CreateCaret(WH_TO_HWND(hwndEditControl), NULL, caretWidth, caretHeight);
    ShowCaret(WH_TO_HWND(hwndEditControl));

    SetCaretBlinkTime(INFINITE);

    lastWidth = caretWidth;
    lastHwnd = WH_TO_HWND(hwndEditControl);
  }
}

void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height) {
  (void)count; (void)item_height;
  POINT ptPrimary = { 0, 0 };
  HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoA(hMonitor, &mi);

  int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  layout->w = 900;
  layout->item_height = 32;

  layout->h = 640;

  if (layout->h < height) layout->h = height;
  if (layout->h > width) layout->h = width;

  layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
  layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}
void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 20, layout->w - 40, 32, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) return;

  FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), SimpleEditSubclassProc, 0, 0);

  if (count > 0) {
    int bottomMargin = (WindowTitleText || VimModeText) ? 60 : 12;
    int listH = layout->h - 70 - bottomMargin;
    if (listH < 0) listH = 0;
    FlintContext_Get()->window.hListBox = WH_FROM_HWND(CreateWindowExA(
        WS_EX_COMPOSITED, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL,
        20, 70, layout->w - 40, listH, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
        ));

    if (WH_TO_HWND(FlintContext_Get()->window.hListBox) == NULL) return;

    SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hListBox), SimpleListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(FlintContext_Get()->window.hListBox, hFont);
    UI_LB_SetItemHeight(FlintContext_Get()->window.hListBox, layout->item_height);

    HDC hdc = GetDC(WH_TO_HWND(FlintContext_Get()->window.hListBox));
    if (!hdc) return;
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SendMessage(WH_TO_HWND(FlintContext_Get()->window.hListBox), LB_ADDSTRING, 0, (LPARAM)string);
    SendMessage(WH_TO_HWND(FlintContext_Get()->window.hListBox), LB_SETCURSEL, (WPARAM)-1, 0);
    UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox));

    SelectObject(hdc, hOldFont);
    ReleaseDC(WH_TO_HWND(FlintContext_Get()->window.hListBox), hdc);
  }
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

  if (WH_TO_HWND(hwndInput) != NULL) {
    SetFocus(WH_TO_HWND(hwndInput));
    UI_Edit_UpdateUIState(hwndInput, UIS_CLEAR, UISF_HIDEFOCUS);
    UI_Edit_SetSel(hwndInput, 0, 0);
  }

  if (foregroundThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, foregroundThreadID, FALSE);
  if (targetThreadID != currentThreadID)
    AttachThreadInput(currentThreadID, targetThreadID, FALSE);
}

void CreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* list[], int count, const WindowLayout* layout, ListBoxInfo* lb) {

  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 20, layout->w - 40, 32, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));
  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) {
    return;
  }

  FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EditSubclassProc, 0, 0);

  if (count >= 0) {
    int bottomMargin = (WindowTitleText || VimModeText) ? 60 : 12;
    int listH = layout->h - 70 - bottomMargin;
    if (listH < 0) listH = 0;
    FlintContext_Get()->window.hListBox = WH_FROM_HWND(CreateWindowExA(
        WS_EX_COMPOSITED, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
        20, 70, layout->w - 40, listH, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
        ));

    if (WH_TO_HWND(FlintContext_Get()->window.hListBox) == NULL) {
      return;
    }

    SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hListBox), ListBoxSubclassProc, 0, 0);
    UI_LB_SetFont(FlintContext_Get()->window.hListBox, hFont);
    UI_LB_SetItemHeight(FlintContext_Get()->window.hListBox, layout->item_height);

    int max_width = layout->w - 40;

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
            lb->last_items = list;
            lb->last_item_count = count;
            lb->selected_index = -1;
            for (int i = 0; i < count && i < MAX_ITEMS; i++) {
                if (list[i]) {
                    strncpy(lb->populated_cache[i], list[i], 511);
                    lb->populated_cache[i][511] = '\0';
                    lb->populated_cache[i][strcspn(lb->populated_cache[i], "\r\n")] = '\0';
                } else {
                    lb->populated_cache[i][0] = '\0';
                }
                lb->matching_indices[i] = i;
            }
            BuildAliasDisplayItems(list, count);
        } else {
            lb->populated_count = 0;
            lb->total_matches = 0;
            lb->last_items = NULL;
            lb->last_item_count = 0;
            lb->selected_index = -1;
            for (int i=0;i<MAX_ITEMS;i++) lb->matching_indices[i]=i;
            BuildAliasDisplayItems(NULL, 0);
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
void SettingsCreateChildControls(WindowHandle hwndParent, HFONT hFont,const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) return;

  FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), SettingsEditSubclassProc, 0, 0);
}
