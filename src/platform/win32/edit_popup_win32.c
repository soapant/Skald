#include "core/edit_state.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "core/timer.h"
#include "platform/wm_platform.h"
#include "platform/ui.h"
#include "platform/draw.h"
#include "platform/win32/win32_state.h"
#include "platform/platform.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/bg_image_win32.h"
#include "core/prompt_geometry.h"
#include "core/editbox.h"
#include "core/hotkey_text.h"
#include "core/edit_popup_keys.h"
#include "helpers/helpers_ui.h"
#include "deps/log.h"
#include "config.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

void win32_fill_popup_key_text(PopupKeyEvent *key) {
  if (!key) {
    return;
  }
  BYTE state[256];
  memset(state, 0, sizeof(state));
  if (GetKeyState(VK_SHIFT) & 0x8000) {
    state[VK_SHIFT] = 0x80;
    state[VK_LSHIFT] = 0x80;
    state[VK_RSHIFT] = 0x80;
  }
  if (GetKeyState(VK_CONTROL) & 0x8000) {
    state[VK_CONTROL] = 0x80;
    state[VK_LCONTROL] = 0x80;
    state[VK_RCONTROL] = 0x80;
  }
  if (GetKeyState(VK_MENU) & 0x8000) {
    state[VK_MENU] = 0x80;
    state[VK_LMENU] = 0x80;
    state[VK_RMENU] = 0x80;
  }
  UINT sc = MapVirtualKeyA((UINT)key->vk, MAPVK_VK_TO_VSC);
  wchar_t wbuf[8];
  int wlen =
      ToUnicodeEx((UINT)key->vk, sc, state, wbuf, 8, 0, GetKeyboardLayout(0));
  if (wlen <= 0) {
    return;
  }
  char out[32];
  int olen = WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, out,
                                 (int)sizeof(out) - 1, NULL, NULL);
  if (olen <= 0) {
    return;
  }
  out[olen] = '\0';
  key->text_len =
      olen < (int)sizeof(key->text) ? olen : (int)sizeof(key->text) - 1;
  memcpy(key->text, out, (size_t)key->text_len);
}

bool Platform_EditPopupCreateWindow(Popup *self, int x, int y, int w, int h) {
  WMEditPopupData *data = (WMEditPopupData *)self;
  HINSTANCE hInst = GetModuleHandle(NULL);
  HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              "EditPopupClass", "", WS_POPUP | WS_CLIPCHILDREN,
                              x, y, w, h, NULL, NULL, hInst, data);
  if (!hwnd)
    return false;
  win32_apply_rounded_corners(hwnd, w, h);
  SetLayeredWindowAttributes(hwnd, 0, (BYTE)WindowOpacity, LWA_ALPHA);
  data->base.handle = WH_FROM_HWND(hwnd);
  return true;
}

void Platform_EditPopupShowWindow(Popup *self) {
  HWND hwnd = WH_TO_HWND(self->handle);
  if (hwnd && IsWindow(hwnd)) {
    ShowWindow(hwnd, SW_SHOWNA);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
  }
}

void Platform_EditPopupHideWindow(Popup *self) {
  HWND hwnd = WH_TO_HWND(self->handle);
  if (hwnd && IsWindow(hwnd))
    ShowWindow(hwnd, SW_HIDE);
  WindowManager *wm = WindowManager_Get();
  if (wm) {
    Popup *top = WindowManager_GetTopPopup(wm);
    if (top && WH_TO_HWND(top->handle) && IsWindow(WH_TO_HWND(top->handle))) {
      SetForegroundWindow(WH_TO_HWND(top->handle));
      SetFocus(WH_TO_HWND(top->handle));
      if (top->vtable && top->vtable->on_render)
        top->vtable->on_render(top);
    }
  }
}

void Platform_EditPopupRender(Popup *self) {
  WMEditPopupData *data = (WMEditPopupData *)self;
  HWND hwnd = WH_TO_HWND(self->handle);
  if (!data || !hwnd || !IsWindow(hwnd)) {
    return;
  }

  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rect;
  GetClientRect(hwnd, &rect);

  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP memBmp =
      memDC ? CreateCompatibleBitmap(hdc, rect.right, rect.bottom) : NULL;
  HBITMAP oldBmp = NULL;
  HDC paint = hdc;
  if (memDC && memBmp && rect.right > 0 && rect.bottom > 0) {
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
    paint = memDC;
  } else {
    if (memDC)
      DeleteDC(memDC);
    if (memBmp)
      DeleteObject(memBmp);
    memDC = NULL;
    memBmp = NULL;
  }

  if (win32_bg_image_enabled() &&
      win32_draw_bg_image_slice(paint, &rect, rect.right - rect.left,
                                rect.bottom - rect.top, 0, 0)) {
  } else {
    HBRUSH br = CreateSolidBrush(COLOR_BG);
    FillRect(paint, &rect, br);
    DeleteObject(br);
  }
  win32_draw_window_border(paint, 0, 0, rect.right, rect.bottom, NULL);

  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;

  Platform_SetDrawContextWin32(paint);
  FontHandle font = Platform_CreateFont(FontSize, UsedFont);
  if (font.hfont)
    SelectObject(paint, font.hfont);

  int cursor_x = 0;
  int text_len = data->text ? (int)strlen(data->text) : 0;
  int text_w = 0;
  int ascent = 0;
  int descent = 0;
  if (font.hfont) {
    TEXTMETRIC tm;
    memset(&tm, 0, sizeof(tm));
    GetTextMetrics(paint, &tm);
    ascent = tm.tmAscent;
    descent = tm.tmDescent;
    if (data->cursor > 0)
      cursor_x = Platform_MeasureTextWidth(
          (PlMeasureTextWidthSpec){.text = data->text ? data->text : "",
                                   .len = data->cursor,
                                   .fontsize = FontSize});
    if (text_len > 0)
      text_w = Platform_MeasureTextWidth((PlMeasureTextWidthSpec){
          .text = data->text, .len = text_len, .fontsize = FontSize});
  }
  int draw_y = h / 2 + PROMPT_EDITPOPUP_CENTER_OFFSET_Y;
  int sel_start = 0, sel_end = 0;
  bool has_sel = false;
  Editbox_ResolveSelection(NULL, text_len, false, &sel_start, &sel_end,
                           &has_sel);

  EditboxSpec eb;
  memset(&eb, 0, sizeof(eb));
  eb.text = data->text ? data->text : "";
  eb.text_len = text_len;
  eb.cursor = data->cursor;
  eb.sel_start = sel_start;
  eb.sel_end = sel_end;
  eb.has_selection = has_sel;
  eb.sel_mode = EDITBOX_SEL_INCLUSIVE;
  eb.x = 0;
  eb.y = draw_y;
  eb.w = w;
  eb.inset = PROMPT_EDITPOPUP_INSET;
  eb.visible_w = w - PROMPT_EDIT_SCROLL_PAD;
  eb.scroll = 0;
  eb.cursor_x = cursor_x;
  eb.text_w = text_w;
  eb.text_y = draw_y;
  eb.caret_y = Editbox_CaretY(draw_y, ascent, descent);
  eb.caret_h = PromptCaretHeight();
  eb.align = data->align;
  eb.placeholder_dy = 0;
  eb.rainbow_limit_x = w - PROMPT_EDITPOPUP_INSET;
  eb.placeholder = data->placeholder;
  eb.text_color = rgb_to_hex(COLOR_TEXT);
  eb.font = font;

  Editbox_Render(&eb, NULL);

  Platform_SetDrawContextWin32(NULL);
  Platform_DestroyFont(font);

  if (memDC && memBmp) {
    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
           ps.rcPaint.right - ps.rcPaint.left,
           ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left,
           ps.rcPaint.top, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
  }
  EndPaint(hwnd, &ps);
}

void Platform_EditPopupDestroyWindow(Popup *self) {
  HWND hwnd = WH_TO_HWND(self->handle);
  if (hwnd && IsWindow(hwnd))
    DestroyWindow(hwnd);
}

bool Platform_EditPopupOnKey(Popup *self, const PopupKeyEvent *key) {
  WMEditPopupData *ed = (WMEditPopupData *)self;
  if (!ed || !ed->text || !key)
    return false;
  if (EditPopup_HandleKey(ed, key))
    return true;

  int wParam = key->vk;
  bool ctrl_pressed = key->ctrl;

  if (ctrl_pressed) {
  if (wParam == 'A' || wParam == 'a') {
    EditPopup_SelectAll(ed);
    return true;
  }
  if (wParam == 'C' || wParam == 'c') {
    EditPopup_CopySelection(ed);
    return true;
  }
  if (wParam == 'C' || wParam == 'c') {
      int ss = 0, se = 0;
      if (EditState_GetSelection(&ss, &se)) {
        if (se < ss) {
          int t = ss;
          ss = se;
          se = t;
        }
        int len = (int)strlen(ed->text);
        if (ss < 0)
          ss = 0;
        if (se > len)
          se = len;
        if (se > ss) {
          char buf[1024];
          int n = se - ss;
          if (n >= (int)sizeof(buf))
            n = (int)sizeof(buf) - 1;
          memcpy(buf, ed->text + ss, n);
          buf[n] = '\0';
          Platform_CopyToClipboard(buf);
        }
      }
      return true;
    }
  }

  if (true && key->text_len > 0) {
    bool swallow = HotkeyText_ShouldSwallow(key);
    if (!swallow && HotkeyText_IsPrintable(key)) {
      EditPopup_InsertText(ed, key->text, key->text_len);
      return true;
    }
  }
  return false;
}
