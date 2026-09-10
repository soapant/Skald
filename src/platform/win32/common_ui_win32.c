#include "core/context.h"
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
void Win32_DrawSelectionBackground(HDC hdc, RECT* rc, COLORREF base) {
  PROFILE_START(timer);
  const char* name = "sel-solid";
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "WinSel solid"); return; }
  COLORREF light = DeriveLighterWin(base, 1.0f);
  if (!EnableShaders || ShaderPreset == 0) {
    name = "sel-solid";
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    PROFILE_END_VAR(timer, name);
    return;
  }
  if (ShaderPreset == 1) {
    name = "sel-vertical";
    for (int y = rc->top; y < rc->bottom; y++) {
      float t = h > 1 ? (float)(y - rc->top) / (float)(h - 1) : 0;
      COLORREF col = LerpColor(base, light, t * 0.9f);
      HBRUSH br = CreateSolidBrush(col);
      RECT band = {rc->left, y, rc->right, y+1};
      FillRect(hdc, &band, br);
      DeleteObject(br);
    }
  } else if (ShaderPreset == 3) {
    name = "sel-diagonal";
    for (int y = rc->top; y < rc->bottom; y++) {
      float t = (float)(y - rc->top) / (float)(h - 1) * 0.7f + 0.5f * 0.3f;
      COLORREF col = LerpColor(base, light, t);
      HBRUSH br = CreateSolidBrush(col);
      RECT band = {rc->left, y, rc->right, y+1};
      FillRect(hdc, &band, br);
      DeleteObject(br);
    }
  } else if (ShaderPreset == 2) {
    name = "sel-radial";
    float cx = w * 0.5f, cy = h * 0.35f;
    float maxDist = sqrtf(cx*cx + cy*cy);
    if (maxDist < 1) maxDist = 1;
    for (int y = rc->top; y < rc->bottom; y++) {
      for (int x = rc->left; x < rc->right; x++) {
        float dx = x - rc->left - cx, dy = y - rc->top - cy;
        float dist = sqrtf(dx*dx + dy*dy) / maxDist;
        if (dist > 1) dist = 1;
        float t = dist * 0.85f;
        COLORREF col = LerpColor(light, base, t);
        SetPixel(hdc, x, y, col);
      }
    }
  } else if (ShaderPreset == 4) {
    name = "sel-noise";
    for (int y = rc->top; y < rc->bottom; y++) {
      float tBase = h > 1 ? (float)(y - rc->top) / (float)(h - 1) : 0;
      for (int x = rc->left; x < rc->right; x++) {
        float t = tBase * 0.85f;
        COLORREF col = LerpColor(base, light, t);
        unsigned int n = HashNoiseWin(x, y);
        int delta = (int)(n & 0x1F) - 16;
        delta = (int)(delta * ShaderIntensity * 0.5f);
        int r = GetRValue(col) + delta, g = GetGValue(col) + delta, b = GetBValue(col) + delta;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        SetPixel(hdc, x, y, RGB(r,g,b));
      }
    }
  } else {
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
  }
  PROFILE_END_VAR(timer, name);
}

void Win32_DrawBackground(HDC hdc, RECT* rc) {
  PROFILE_START(timer);
  const char* name = "solid";
  int w = rc->right - rc->left;
  int h = rc->bottom - rc->top;
  if (w <= 0 || h <= 0) { PROFILE_END_VAR(timer, "WinBg solid"); return; }
  COLORREF base = COLOR_BG;
  COLORREF light = DeriveLighterWin(base, 1.0f);
  if (!EnableShaders || ShaderPreset == 0) {
    name = "solid";
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
    PROFILE_END_VAR(timer, name);
    return;
  }
  if (ShaderPreset == 1) {
    name = "vertical";
    for (int y = rc->top; y < rc->bottom; y++) {
      float t = h > 1 ? (float)(y - rc->top) / (float)(h - 1) : 0;
      COLORREF col = LerpColor(base, light, t * 0.9f);
      HBRUSH br = CreateSolidBrush(col);
      RECT band = {rc->left, y, rc->right, y+1};
      FillRect(hdc, &band, br);
      DeleteObject(br);
    }
  } else if (ShaderPreset == 3) {
    name = "diagonal";
    for (int y = rc->top; y < rc->bottom; y++) {
      float t = (float)(y - rc->top) / (float)(h - 1) * 0.7f + 0.5f * 0.3f;
      COLORREF col = LerpColor(base, light, t);
      HBRUSH br = CreateSolidBrush(col);
      RECT band = {rc->left, y, rc->right, y+1};
      FillRect(hdc, &band, br);
      DeleteObject(br);
    }
  } else if (ShaderPreset == 2) {
    name = "radial";
    float cx = w * 0.5f, cy = h * 0.35f;
    float maxDist = sqrtf(cx*cx + cy*cy);
    if (maxDist < 1) maxDist = 1;
    for (int y = rc->top; y < rc->bottom; y++) {
      for (int x = rc->left; x < rc->right; x++) {
        float dx = x - rc->left - cx, dy = y - rc->top - cy;
        float dist = sqrtf(dx*dx + dy*dy) / maxDist;
        if (dist > 1) dist = 1;
        float t = dist * 0.85f;
        COLORREF col = LerpColor(light, base, t);
        SetPixel(hdc, x, y, col);
      }
    }
  } else if (ShaderPreset == 4) {
    name = "noise";
    for (int y = rc->top; y < rc->bottom; y++) {
      float tBase = h > 1 ? (float)(y - rc->top) / (float)(h - 1) : 0;
      for (int x = rc->left; x < rc->right; x++) {
        float t = tBase * 0.85f;
        COLORREF col = LerpColor(base, light, t);
        unsigned int n = HashNoiseWin(x, y);
        int delta = (int)(n & 0x1F) - 16;
        delta = (int)(delta * ShaderIntensity * 0.5f);
        int r = GetRValue(col) + delta, g = GetGValue(col) + delta, b = GetBValue(col) + delta;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        SetPixel(hdc, x, y, RGB(r,g,b));
      }
    }
  } else {
    HBRUSH br = CreateSolidBrush(base);
    FillRect(hdc, rc, br);
    DeleteObject(br);
  }
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
  POINT ptPrimary = { 0, 0 };
  HMONITOR hMonitor = MonitorFromPoint(ptPrimary, MONITOR_DEFAULTTOPRIMARY);

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(MONITORINFO);
  GetMonitorInfoA(hMonitor, &mi);

  int monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
  int monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

  layout->w = 900;
  layout->item_height = item_height;

  layout->h = 95 + (count > 0 ? (count * layout->item_height) : 0);

  if (layout->h < height) layout->h = height;
  if (layout->h > width) layout->h = width;

  layout->x = mi.rcMonitor.left + (monitorWidth - layout->w) / 2;
  layout->y = mi.rcMonitor.top + (monitorHeight - layout->h) / 2;
}
void SimpleCreateChildControls(WindowHandle hwndParent, HFONT hFont, const char* string, int count, const WindowLayout* layout) {
  HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(WH_TO_HWND(hwndParent), GWLP_HINSTANCE);

  FlintContext_Get()->window.hEditControl = WH_FROM_HWND(CreateWindowExA(
      0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));

  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) return;

  FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), SimpleEditSubclassProc, 0, 0);

  if (count > 0) {
    FlintContext_Get()->window.hListBox = WH_FROM_HWND(CreateWindowExA(
        0, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOSEL,
        20, 105, layout->w - 40, layout->h - 115, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
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
      20, 50, layout->w - 40, 40, WH_TO_HWND(hwndParent), (HMENU)IDC_INPUT_BOX, hInstance, NULL
      ));
  if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) == NULL) {
    return;
  }

  FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
  UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, hFont);
  SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EditSubclassProc, 0, 0);

  if (count >= 0) {
    FlintContext_Get()->window.hListBox = WH_FROM_HWND(CreateWindowExA(
        0, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_NODATA | LBS_OWNERDRAWFIXED,
        20, 105, layout->w - 40, layout->h - 115, WH_TO_HWND(hwndParent), (HMENU)IDC_LIST_BOX, hInstance, NULL
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
            }
        }
    }
    SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hListBox), LB_SETCOUNT, count, 0);

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