#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "core/types/keys.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "config_values.h"
#include "helpers/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/editbox.h"
#include "core/window_manager.h"
#include "core/widget_registry.h"
#include <commctrl.h>
#include "helpers/helpers_text.h"
#include <shellapi.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "helpers/win32/subclasses_win32.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/bg_image_win32.h"
#include "helpers/helpers_input_state.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "core/listbox_hover.h"
#include "core/scroll_area.h"
#include "core/prompt_keys.h"
#include "core/checkbox.h"
#include "core/focus.h"
#include "core/slider.h"
#include "core/draw_state.h"
#include "core/draw_state.h"

#define IDC_LIST_BOX 102

static ListBox *win32_get_lb_from_popup(HWND hPopup) {
  if (hPopup && IsWindow(hPopup)) {
    const PromptState *state = win32_prompt_state_from_hwnd(hPopup);
    if (state) {
      return state->lb;
    }
    return (ListBox *)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
  }
  return NULL;
}

static bool s_swallow_next_char = false;

static bool s_repaint_queued = false;
static bool s_repaint_pending = false;

static bool s_in_render = false;

void win32_popup_request_repaint(HWND hPopup) {
  if (!hPopup || !IsWindow(hPopup))
    return;
  if (s_in_render) {
    s_repaint_pending = true;
    return;
  }
  if (s_repaint_queued)
    return;
  s_repaint_queued = true;
  PostMessageA(hPopup, WM_APP + 41, 0, 0);
}

static void popup_request_repaint(HWND hwnd) {
  if (SkaldContext_Get()->window.prompt_callbacks->render == NULL) {
    UI_LegacySyncRepaint();
    return;
  }
  win32_popup_request_repaint(hwnd);
}

static bool prompt_win_no_bg(void);

static bool win32_prompt_window_on_key(HWND hwnd, const PopupKeyEvent *key) {

  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || WH_TO_HWND(top->handle) != hwnd) {
    return false;
  }

  int wParam = key->vk;

  if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU)
    return false;

  PromptState *state = win32_prompt_state_from_hwnd(hwnd);

  bool hide_popup = (wParam == VK_RETURN);
  bool handled = false;
  if (state)
    handled = prompt_handle_key(state, key);
  if (!handled)
    return false;

  HWND popup_wnd_now = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (hide_popup && SkaldContext_Get()->window.selection_complete) {
  } else if (hide_popup && popup_wnd_now != NULL && IsWindow(popup_wnd_now)) {
    ShowWindow(popup_wnd_now, SW_SHOW);
    win32_force_window_focus(SkaldContext_Get()->window.hPopupWnd);
  }
  if (SkaldContext_Get()->window.prompt_callbacks->render != NULL) {
    HWND hPar = GetParent(hwnd);
    if (hPar && IsWindow(hPar)) {
      InvalidateRect(hPar, NULL, FALSE);
    }
  } else {
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return true;
}

static bool win32_hwnd_is_popup(WindowManager *wm, HWND h);

static void win32_prompt_deactivate(HWND hwnd, HWND other) {
  WindowManager *_wm = WindowManager_Get();
  if (_wm && _wm->popup_count > 1) {
    Popup *_top = WindowManager_GetTopPopup(_wm);
    if (_top && WH_TO_HWND(_top->handle) == hwnd) {
      if (!win32_hwnd_is_popup(_wm, other)) {
        SkaldContext_Get()->window.output_buffer[0] = '\0';
        WindowManager_PopPopup(_wm);
        return;
      }
      SkaldContext_Get()->window.selection_complete = true;
    }
    return;
  }
  if (CloseOnLossOfFocus) {
    SkaldContext_Get()->window.output_buffer[0] = '\0';
    SkaldContext_Get()->window.selection_complete = true;
  }
}

static bool win32_hwnd_is_popup(WindowManager *wm, HWND h) {
  if (!wm || !h)
    return false;
  for (int i = 0; i < wm->popup_count; i++) {
    Popup *p = wm->popups[i];
    if (p && WH_TO_HWND(p->handle) == h)
      return true;
  }
  return false;
}

typedef struct {
  HWND hwnd;
  HDC dc;
  HBITMAP bmp;
  void *bits;
  void *bg_bits;
  int w, h;
} PopupBuffer;

#define POPUP_BUF_MAX 16
static PopupBuffer s_buffers[POPUP_BUF_MAX];

static PopupBuffer *popup_buffer_for(HWND hwnd) {
  for (int i = 0; i < POPUP_BUF_MAX; i++)
    if (s_buffers[i].hwnd == hwnd)
      return &s_buffers[i];
  return NULL;
}

static void popup_free_back_buffer(PopupBuffer *b) {
  if (!b)
    return;
  if (b->dc)
    DeleteDC(b->dc);
  if (b->bmp)
    DeleteObject(b->bmp);
  free(b->bg_bits);
  memset(b, 0, sizeof(*b));
}

static HDC popup_back_buffer(HWND hwnd, int w, int h, HBITMAP *out_prev) {
  if (w <= 0 || h <= 0)
    return NULL;
  PopupBuffer *b = popup_buffer_for(hwnd);
  if (!b) {
    for (int i = 0; i < POPUP_BUF_MAX; i++) {
      if (!s_buffers[i].hwnd) {
        b = &s_buffers[i];
        break;
      }
    }
    if (!b) {
      b = &s_buffers[0];
      popup_free_back_buffer(b);
    }
    b->hwnd = hwnd;
  }
  if (!b->dc || !b->bmp || b->w != w || b->h != h) {
    popup_free_back_buffer(b);
    b->hwnd = hwnd;
    HDC screen = GetDC(NULL);
    if (!screen)
      return NULL;
    b->dc = CreateCompatibleDC(screen);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    b->bmp = CreateDIBSection(b->dc, &bi, DIB_RGB_COLORS, &b->bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (b->bmp)
      b->bg_bits = malloc((size_t)w * (size_t)h * 4u);
    b->w = w;
    b->h = h;
  }
  if (!b->dc || !b->bmp || !b->bits || !b->bg_bits) {
    popup_free_back_buffer(b);
    b->hwnd = hwnd;
    return NULL;
  }
  *out_prev = (HBITMAP)SelectObject(b->dc, b->bmp);
  return b->dc;
}

static void popup_snapshot_background(HWND hwnd, int w, int h) {
  PopupBuffer *b = popup_buffer_for(hwnd);
  if (b && b->bits && b->bg_bits)
    memcpy(b->bg_bits, b->bits, (size_t)w * (size_t)h * 4u);
}

static void popup_apply_alpha(HWND hwnd, HDC memDC, int w, int h) {
  GdiFlush();
  PopupBuffer *b = popup_buffer_for(hwnd);
  if (!b || !b->bits || !b->bg_bits)
    return;
  unsigned int *px = (unsigned int *)b->bits;
  const unsigned int *bg = (const unsigned int *)b->bg_bits;
  float opacity = WindowOpacity;
  if (opacity < 0.0f)
    opacity = 0.0f;
  if (opacity > 255.0f)
    opacity = 255.0f;
  unsigned int bg_alpha = (unsigned int)opacity;
  int r = 0;
  if (RoundedCorners && CornerRadius > 0) {
    r = CornerRadius;
    if (r > 32)
      r = 32;
    if (r * 2 > w)
      r = w / 2;
    if (r * 2 > h)
      r = h / 2;
    if (r < 0)
      r = 0;
  }
  int rr = r * r;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      size_t i = (size_t)y * (size_t)w + (size_t)x;
      unsigned int rgb = px[i] & 0x00FFFFFFu;
      unsigned int a = (rgb == (bg[i] & 0x00FFFFFFu)) ? bg_alpha : 255u;
      if (r > 0) {
        int cx = x < r ? r : (x >= w - r ? w - r - 1 : x);
        int cy = y < r ? r : (y >= h - r ? h - r - 1 : y);
        int dx = x - cx;
        int dy = y - cy;
        if (dx * dx + dy * dy > rr)
          a = 0u;
      }
      px[i] = rgb | (a << 24);
    }
  }
  HDC screen = GetDC(NULL);
  if (!screen)
    return;
  RECT wr;
  POINT dst = {0, 0};
  if (GetWindowRect(hwnd, &wr)) {
    dst.x = wr.left;
    dst.y = wr.top;
  }
  POINT src = {0, 0};
  SIZE size = {w, h};
  BLENDFUNCTION blend = {0};
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  BOOL ok = UpdateLayeredWindow(hwnd, screen, &dst, &size, memDC, &src, 0,
                                &blend, ULW_ALPHA);
  ReleaseDC(NULL, screen);
  if (!ok)
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)opacity, LWA_ALPHA);
}

static bool prompt_win_no_bg(void) {

  const PromptState *st = win32_prompt_state_from_hwnd(GetFocus());
  if (st && st->no_bg)
    return true;
  return false;
}

static HBRUSH s_bg_brush = NULL;
static HBRUSH s_sel_brush = NULL;
static COLORREF s_bg_color = (COLORREF)-1;
static COLORREF s_sel_color = (COLORREF)-1;

static HBRUSH cached_brush(HBRUSH *cache, COLORREF *cached, COLORREF want) {
  if (!*cache || *cached != want) {
    if (*cache)
      DeleteObject(*cache);
    *cache = CreateSolidBrush(want);
    *cached = want;
  }
  return *cache;
}

LRESULT CALLBACK win32_popup_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam) {

  ListBox *lb = win32_get_lb_from_popup(hwnd);
  WindowManager *_wm_guard = WindowManager_Get();
  Popup *_top_guard = _wm_guard ? WindowManager_GetTopPopup(_wm_guard) : NULL;
  if (_top_guard && _top_guard->type == POPUP_PROMPT &&
      WH_TO_HWND(_top_guard->handle) != NULL &&
      WH_TO_HWND(_top_guard->handle) != hwnd) {
    if (msg == WM_PAINT) {
      log_info("win32_popup_wnd_proc SKIP PAINT hwnd=%p top=%p", (void *)hwnd,
               (void *)WH_TO_HWND(_top_guard->handle));
      PAINTSTRUCT _ps;
      BeginPaint(hwnd, &_ps);
      EndPaint(hwnd, &_ps);
      return 0;
    }
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_MOUSEWHEEL) {
      return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
  }
  switch (msg) {
  case WM_LBUTTONDOWN: {
    widget_key_capture_release_pin();
    POINT pt;
    if (GetCursorPos(&pt))
      listbox_publish_mouse_pos((int)pt.x, (int)pt.y);
    RECT rect;
    GetClientRect(hwnd, &rect);
    PromptState *click_ps = win32_prompt_state_from_hwnd(hwnd);
    if (SkaldContext_Get()->window.prompt_callbacks->render == NULL &&
        click_ps && click_ps->lb != NULL) {
      int list_top = prompt_list_top();
      int item_h = prompt_row_height();
      int visible = (rect.bottom - prompt_list_bottom_margin() - list_top +
                     item_h - 1) /
                    item_h;
      listbox_set_geometry(list_top, item_h, visible);
    }
    if (click_ps) {
      bool click_ctrl = (wParam & MK_CONTROL) != 0;
      bool click_shift = (wParam & MK_SHIFT) != 0;
      bool click_alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      prompt_handle_list_click(click_ps, click_ctrl, click_shift, click_alt);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    break;
  }
  case WM_MOUSEMOVE: {
    POINT pt;
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (GetCursorPos(&pt)) {
      listbox_publish_mouse_pos(pt.x, pt.y);
    }

    if (SkaldContext_Get()->window.prompt_callbacks->render == NULL) {
      PromptState *hst = win32_prompt_state_from_hwnd(hwnd);
      if (hst && hst->lb != NULL) {
        ListBox *hlb = hst->lb;
        int list_top = prompt_list_top();
        int item_h = prompt_row_height();
        int visible = (rect.bottom - prompt_list_bottom_margin() - list_top +
                       item_h - 1) /
                      item_h;

        listbox_set_geometry(list_top, item_h, visible);

        listbox_hover_focus(&(ListboxHoverFocus){.f_lb = hlb, .f_list_top = list_top, .f_item_height = item_h, .f_top_index = hlb->view_offset, .f_visible_count = visible, .f_left_x = PROMPT_SEL_X, .f_right_x = PROMPT_SEL_X + (rect.right - rect.left) - PROMPT_SEL_W_SHRINK});
      }
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_MOUSEWHEEL: {
    short delta = (short)HIWORD(wParam);
    int steps = delta / WHEEL_DELTA;
    if (steps == 0)
      steps = delta > 0 ? 1 : -1;
    int notches = -steps;
    int mx = 0, my = 0;
    if (Platform_GetMousePos(&mx, &my)) {
      listbox_publish_mouse_pos(mx, my);
      ScrollEvent scroll = {0};
      scroll.x = mx;
      scroll.y = my;
      scroll.dy = notches;
      global_scroll_push(&scroll);
      if (!scroll_area_dispatch_wheel(mx, my, 0, notches))
        listbox_queue_scroll(notches * LISTBOX_WHEEL_ROWS);
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_KEYDOWN: {
    if (widget_key_capture_active())
      return 0;
    s_swallow_next_char = false;
    HWND hKey = hwnd;
    PopupKeyEvent pkey;
    memset(&pkey, 0, sizeof(pkey));
    pkey.vk = (int)wParam;
    pkey.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    pkey.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    pkey.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    s_swallow_next_char = true;
    bool consumed = win32_prompt_window_on_key(hKey, &pkey);
    if (wParam == VK_BACK) {
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && SkaldContext_Get()->window.prompt_callbacks->refresh) {
        SkaldContext_Get()->window.prompt_callbacks->refresh(ps);
      }
    }
    if (!consumed)
      s_swallow_next_char = false;
    if (wParam == VK_DELETE) {
      UI_DeleteForward();
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && SkaldContext_Get()->window.prompt_callbacks->refresh) {
        SkaldContext_Get()->window.prompt_callbacks->refresh(ps);
      }
      UI_RequestPopupRepaint();
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (consumed) {
      popup_request_repaint(hwnd);
      return 0;
    }
    if (true && wParam >= 0x20 && wParam < 0x7F &&
        !(GetKeyState(VK_CONTROL) & 0x8000) &&
        !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_LWIN) & 0x8000) &&
        !(GetKeyState(VK_RWIN) & 0x8000))
      break;
    if (SkaldContext_Get()->window.prompt_callbacks->render != NULL) {
      InvalidateRect(hwnd, NULL, FALSE);
      UpdateWindow(hwnd);
    } else {
      popup_request_repaint(hwnd);
    }
    break;
  }
  case WM_CHAR: {
    if (widget_key_capture_active())
      return 0;
    if (s_swallow_next_char) {
      s_swallow_next_char = false;
      return 0;
    }
    if (wParam == 0x08 || wParam == 0x7F)
      return 0;
    if (wParam >= 0x20 && true) {
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && prompt_delete_selection(ps)) {
        UI_SetText(ps->eb->filter);
        UI_SetCursorPosition(ps->eb->cursor);
      }
      if (UI_InsertChar((wchar_t)wParam) > 0) {
        ps = win32_prompt_state_from_hwnd(hwnd);
        if (ps && SkaldContext_Get()->window.prompt_callbacks->refresh) {
          SkaldContext_Get()->window.prompt_callbacks->refresh(ps);
        }
        UI_RequestPopupRepaint();
        popup_request_repaint(hwnd);
        return 0;
      }
    }
    break;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wParam) == WA_INACTIVE)
      win32_prompt_deactivate(hwnd, (HWND)lParam);
    break;
  }
  case WM_KILLFOCUS: {
    win32_prompt_deactivate(hwnd, (HWND)lParam);
    break;
  }
  case WM_CTLCOLOREDIT: {
    HDC hdc = (HDC)wParam;
    COLORREF tc = COLOR_TEXT;
    if (prompt_win_no_bg()) {
      SetTextColor(hdc, tc);
      SetBkMode(hdc, TRANSPARENT);
      return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    SetTextColor(hdc, tc);
    SetBkColor(hdc, COLOR_BG);
    return (LRESULT)cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG);
  }
  case WM_CTLCOLORLISTBOX: {
    HDC hdc = (HDC)wParam;
    if (prompt_win_no_bg()) {
      SetTextColor(hdc, COLOR_TEXT);
      SetBkMode(hdc, TRANSPARENT);
      return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    SetTextColor(hdc, COLOR_TEXT);
    SetBkColor(hdc, COLOR_BG);
    return (LRESULT)cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG);
  }
  case WM_CREATE: {
    break;
  }
  case WM_SHOWWINDOW: {
    break;
  }
  case WM_COMMAND: {
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);
    if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
      break;
    }

    break;
  }
  case WM_TIMER: {
    if (wParam == UI_DEBOUNCE_TIMER_ID) {
      KillTimer(hwnd, UI_DEBOUNCE_TIMER_ID);
      UI_RequestPopupRepaint();
      if (SkaldContext_Get()->window.prompt_callbacks->render != NULL)
        InvalidateRect(hwnd, NULL, FALSE);
      else
        UI_LegacySyncRepaint();
      return 0;
    }
    if (wParam == UI_TICK_TIMER_ID) {
      UI_RequestPopupRepaint();
    }
    if (SkaldContext_Get()->window.prompt_callbacks->render != NULL)
      InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  } break;
  case WM_MEASUREITEM: {
    MEASUREITEMSTRUCT *pmis = (MEASUREITEMSTRUCT *)lParam;
    if (pmis->CtlID == IDC_LIST_BOX) {
      int h = prompt_row_height();
      pmis->itemHeight = h;
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT *pdis = (DRAWITEMSTRUCT *)lParam;
    if (pdis->CtlID == IDC_LIST_BOX && (int)pdis->itemID != -1) {
      BOOL isSelected = ((int)pdis->itemID == (lb ? lb->selected_index : -1));
      if ((int)pdis->itemID >= (lb ? lb->item_count : 0)) {
        return TRUE;
      }
      if (RoundedCorners && CornerRadius > 0) {
        int R = PROMPT_SEL_RADIUS;
        if (R > CornerRadius)
          R = CornerRadius;
        int t = PROMPT_SEL_INSET;
        RECT inner = pdis->rcItem;
        inner.left += PROMPT_SEL_X + t;
        inner.top += PROMPT_SEL_TOP_INSET + t;
        inner.right -= PROMPT_SEL_X + t;
        inner.bottom -= PROMPT_SEL_TOP_INSET + t;
        if (inner.right <= inner.left || inner.bottom <= inner.top) {
          inner = pdis->rcItem;
          t = 0;
        }
        int innerR = R - 1;
        if (innerR < 0)
          innerR = 0;
        {
          RECT winRc;
          GetClientRect(hwnd, &winRc);
          int win_w = winRc.right - winRc.left;
          int win_h = winRc.bottom - winRc.top;
          POINT lbOrigin2 = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOrigin2);
          ScreenToClient(hwnd, &lbOrigin2);
          win32_draw_bg_image_slice(&(Win32DrawBgImageSlice){.f_hdc = pdis->hDC, .f_rc = &pdis->rcItem, .f_containerW = win_w, .f_containerH = win_h, .f_originX = lbOrigin2.x, .f_originY = lbOrigin2.y});
        }
        if (isSelected) {
          win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_SELECTION_HIGHLIGHT,
                                  innerR);
        } else {
          RECT winRcI;
          GetClientRect(hwnd, &winRcI);
          int iwW = winRcI.right - winRcI.left;
          int iwH = winRcI.bottom - winRcI.top;
          POINT lbOriginI = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOriginI);
          ScreenToClient(hwnd, &lbOriginI);
          if (iwW <= 0)
            iwW = pdis->rcItem.right - pdis->rcItem.left;
          if (iwH <= 0)
            iwH = pdis->rcItem.bottom - pdis->rcItem.top + 600;
          if (!win32_draw_bg_image_slice(&(Win32DrawBgImageSlice){.f_hdc = pdis->hDC, .f_rc = &inner, .f_containerW = iwW, .f_containerH = iwH, .f_originX = lbOriginI.x, .f_originY = lbOriginI.y})) {
            win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_BG, innerR);
          }
        }
      } else {
        if (isSelected) {
          FillRect(
              pdis->hDC, &pdis->rcItem,
              cached_brush(&s_sel_brush, &s_sel_color, COLOR_SELECTION_HIGHLIGHT));
        } else {
          RECT winRc;
          GetClientRect(hwnd, &winRc);
          int win_w = winRc.right - winRc.left;
          int win_h = winRc.bottom - winRc.top;
          POINT lbOrigin = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOrigin);
          ScreenToClient(hwnd, &lbOrigin);
          if (win_w <= 0)
            win_w = pdis->rcItem.right - pdis->rcItem.left;
          if (win_h <= 0) {
            RECT tr2;
            GetClientRect(hwnd, &tr2);
            win_h = tr2.bottom - tr2.top;
            if (win_h <= 0)
              win_h = pdis->rcItem.bottom - pdis->rcItem.top + 600;
          }
          if (!win32_draw_bg_image_slice(&(Win32DrawBgImageSlice){.f_hdc = pdis->hDC, .f_rc = &pdis->rcItem, .f_containerW = win_w, .f_containerH = win_h, .f_originX = lbOrigin.x, .f_originY = lbOrigin.y})) {
            FillRect(pdis->hDC, &pdis->rcItem,
                     cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG));
          }
        }
      }
      if (lb && lb->selected_index != -1 && (int)pdis->itemID >= 0 &&
          (int)pdis->itemID < lb->item_count && lb->items[pdis->itemID]) {
        char measureBuffer[512];
        safe_copy(measureBuffer, sizeof(measureBuffer),
                  lb->items[pdis->itemID]);
        char *pos = strchr(measureBuffer, '|');
        if (pos) {
          *pos = '\0';
        }
      }

      const char *textBuffer = "";
      if ((int)pdis->itemID < (lb ? lb->item_count : 0)) {
        textBuffer = lb->items[pdis->itemID];
      }

      if ((int)pdis->itemID == -1 ||
          (int)pdis->itemID >= (lb ? lb->item_count : 0)) {
        return TRUE;
      }

      SetBkMode(pdis->hDC, TRANSPARENT);
      RECT textRect = pdis->rcItem;
      textRect.left += PROMPT_ITEM_TEXT_X;

      if (SkaldContext_Get()->window.prompt_callbacks->render != NULL) {
        return TRUE;
      }
      win32_set_draw_context(pdis->hDC);
      HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      HFONT hOldMainFont =
          hMainFont ? (HFONT)SelectObject(pdis->hDC, hMainFont) : NULL;
      SetTextColor(pdis->hDC, rgb_to_hex(COLOR_TEXT));
      DrawTextA(pdis->hDC, textBuffer, -1, &textRect,
                DT_SINGLELINE | DT_VCENTER);
      SelectObject(pdis->hDC, hOldMainFont);
      if (hMainFont)
        DeleteObject(hMainFont);
      win32_set_draw_context(NULL);
      return TRUE;
    }
    break;
  }
  case WM_APP + 41: {
    s_repaint_queued = false;
    if (SkaldContext_Get()->window.prompt_callbacks->render == NULL) {
      UI_LegacySyncRepaint();
      return 0;
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    Popup *anim_popup = prompt_popup_find_popup_by_handle(WH_FROM_HWND(hwnd));
    if (anim_popup)
      prompt_popup_tick_resize_animation_popup(anim_popup);
    bool resize_animating =
        anim_popup && prompt_popup_resize_animating(anim_popup);
    RECT rect;
    GetClientRect(hwnd, &rect);
    HDC paintDC = hdc;
    HDC memDC = NULL;
    HBITMAP oldBmp = NULL;
    bool useRenderBuf = (rect.right > 0 && rect.bottom > 0);
    if (useRenderBuf) {
      HBITMAP prev = NULL;
      HDC buf = popup_back_buffer(hwnd, rect.right, rect.bottom, &prev);
      if (buf) {
        oldBmp = prev;
        memDC = buf;
        paintDC = memDC;
      } else {
        useRenderBuf = false;
      }
    }
    if (prompt_win_no_bg()) {
      HBRUSH hKeyBrush = CreateSolidBrush(RGB(0, 0, 0));
      FillRect(paintDC, &rect, hKeyBrush);
      DeleteObject(hKeyBrush);
    } else {
      RECT visRect;
      bool haveVis = IntersectRect(&visRect, &rect, &ps.rcPaint) != FALSE;
      bool imgDrawn = false;
      if (haveVis && rect.right > 0 && rect.bottom > 0) {
        imgDrawn =
            win32_draw_bg_image_slice(&(Win32DrawBgImageSlice){.f_hdc = paintDC, .f_rc = &visRect, .f_containerW = rect.right - rect.left, .f_containerH = rect.bottom - rect.top, .f_originX = 0, .f_originY = 0});
      }
      if (!imgDrawn) {
        FillRect(paintDC, &rect,
                 cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG));
      }
    }
    if (useRenderBuf)
      popup_snapshot_background(hwnd, rect.right, rect.bottom);
    if (SkaldContext_Get()->window.prompt_callbacks->render) {
      PromptState *state = win32_prompt_state_from_hwnd(hwnd);
      Popup *owner = prompt_popup_find_popup_by_handle(WH_FROM_HWND(hwnd));
      if (owner && !owner->user_data)
        state = NULL;
      if (state && state->callbacks && state->callbacks->render) {
        long long now_ticks = Platform_TimerNowTicks();
        long long freq = Platform_TimerTickFrequency();
        if (state->last_frame_ticks && freq > 0)
          state->delta_time = (double)(now_ticks - state->last_frame_ticks) *
                              1000.0 / (double)freq;
        state->last_frame_ticks = now_ticks;
        checkbox_frame_begin();
        slider_frame_begin();
        focus_frame_begin(&(FocusFrameBegin){.f_include_listbox = state->lb != NULL, .f_lb_x = 0, .f_lb_y = prompt_list_top(), .f_lb_w = state->width, .f_lb_h = state->height - prompt_list_top() -
                              prompt_list_bottom_margin()});
        editbox_registry_register_focus(state);
        win32_set_draw_context(paintDC);
        draw_state_get()->win32_listbox_surface = true;
        s_in_render = true;
        if (!resize_animating) {
          prompt_band_take_placed();
          bool sep_prev = draw_state_get()->suppress_separators;
          draw_state_get()->suppress_separators = state->no_separators;
          editbox_registry_tick(state);
          editbox_registry_begin_frame(state);
          state->callbacks->render(state);
          prompt_band_commit_placed();
          focus_frame_end();
          draw_state_get()->suppress_separators = sep_prev;
          if (s_repaint_pending) {
            s_repaint_pending = false;
            win32_popup_request_repaint(hwnd);
          }
        }
        s_in_render = false;
        draw_state_get()->win32_listbox_surface = false;
      }
    } else {
      PromptState *estate = win32_prompt_state_from_hwnd(hwnd);
      Popup *eowner = prompt_popup_find_popup_by_handle(WH_FROM_HWND(hwnd));
      if (eowner && !eowner->user_data)
        estate = NULL;
      if (estate && !resize_animating) {
        focus_frame_begin(&(FocusFrameBegin){.f_include_listbox = estate->lb != NULL, .f_lb_x = 0, .f_lb_y = prompt_list_top(), .f_lb_w = estate->width, .f_lb_h = estate->height - prompt_list_top() - prompt_list_bottom_margin()});
        editbox_registry_register_focus(estate);
        bool sep_prev = draw_state_get()->suppress_separators;
        draw_state_get()->suppress_separators = estate->no_separators;
        win32_set_draw_context(paintDC);
        if (estate->lb)
          SkaldContext_Get()->window.listbox_placed = true;
        extern void Platform_DrawPromptEditbox(PromptState * state);
        Platform_DrawPromptEditbox(estate);

        if (estate->lb) {
          draw_prompt_separator((DrawPromptSeparator){
              .y = prompt_list_top() - 2, .width = rect.right - rect.left});

          int list_top = prompt_list_top();
          int row_height = prompt_row_height();
          int line_h = Platform_GetTextLineHeight(FontSize);
          int pad_top = (row_height - line_h) / 2;
          if (pad_top < 0)
            pad_top = 0;
          int count = estate->lb->item_count;
          int clip_bottom = rect.bottom - prompt_list_bottom_margin();
          int visible_count =
              (clip_bottom - list_top + row_height - 1) / row_height;
          if (visible_count < 1)
            visible_count = 1;
          listbox_set_geometry(list_top, row_height, visible_count);
          UI_LB_SetVisibleCount(visible_count);
          if (visible_count <= 1 && estate->lb->selected_index >= 0 &&
              estate->lb->selected_index < count)
            estate->lb->view_offset = estate->lb->selected_index;
          listbox_apply_pending_scroll(estate->lb, visible_count);
          int view_offset = estate->lb->view_offset;

          draw_state_get()->win32_listbox_surface = true;
          draw_state_set_clip(list_top, clip_bottom);
          for (int i = 0; i < visible_count && (i + view_offset) < count; i++) {
            int list_idx = i + view_offset;
            int y = list_top + i * row_height;
            int row_bottom = y + row_height;
            if (row_bottom > clip_bottom)
              row_bottom = clip_bottom;

            if (list_idx == estate->lb->selected_index) {
              RECT itemRect = {10, y + 1, rect.right - 10, row_bottom - 1};
              win32_fill_rounded_rect(paintDC, &itemRect, COLOR_SELECTION_HIGHLIGHT,
                                      PROMPT_SEL_RADIUS);
            }

            const char *text = listbox_info_row(estate->lb, list_idx);
            if (!text || y + pad_top >= clip_bottom)
              continue;

            int row_h_scroll = (list_idx == estate->lb->selected_index)
                                   ? estate->lb->text_hscroll
                                   : 0;
            const char *draw_text = text;
            int draw_x = 14;
            if (row_h_scroll > 0)
              draw_x -= row_h_scroll;
            Platform_DrawTextRaw(
                (DrawTextRaw){.x = draw_x,
                                  .y = y + pad_top,
                                  .text = draw_text,
                                  .color = rgb_to_hex(COLOR_TEXT),
                                  .font = FH_NULL,
                                  .align = "LEFT",
                                  .pad = 0,
                                  .font_size = FontSize});
          }
          draw_state_get()->win32_listbox_surface = false;
          draw_state_clear_clip();

          draw_prompt_separator((DrawPromptSeparator){
              .y = rect.bottom - prompt_list_bottom_margin(),
              .width = rect.right - rect.left});
        }
        draw_state_get()->suppress_separators = sep_prev;
        focus_frame_end();
        win32_set_draw_context(NULL);
      }
    }
    if (!resize_animating && SkaldContext_Get()->window.title != NULL &&
        strlen(SkaldContext_Get()->window.title) > 0 && WindowTitleText) {
      wchar_t wtitle[256];
      if (MultiByteToWideChar(CP_UTF8, 0, SkaldContext_Get()->window.title, -1,
                              wtitle, 256) == 0)
        wtitle[0] = L'\0';
      HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      HFONT hOldFont =
          hMainFont ? (HFONT)SelectObject(paintDC, hMainFont) : NULL;
      SetBkMode(paintDC, TRANSPARENT);
      SIZE sz;
      GetTextExtentPoint32W(paintDC, wtitle, (int)wcslen(wtitle), &sz);
      RECT titleRect;
      titleRect.left = PROMPT_FOOTER_X;
      titleRect.top = rect.bottom - PROMPT_FOOTER_Y_FROM_BOTTOM;
      titleRect.right = titleRect.left + sz.cx + PROMPT_FOOTER_PAD;
      titleRect.bottom = titleRect.top + sz.cy;
      COLORREF titleColor = COLOR_TEXT;
      SetTextColor(paintDC, titleColor);
      DrawTextW(paintDC, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
      SelectObject(paintDC, hOldFont);
      if (hMainFont)
        DeleteObject(hMainFont);
    }
    win32_draw_window_border(&(Win32DrawWindowBorder){.f_hdc = paintDC, .f_ox = 0, .f_oy = 0, .f_ww = rect.right, .f_wh = rect.bottom, .f_region = NULL});
    if (useRenderBuf) {
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
             ps.rcPaint.right - ps.rcPaint.left,
             ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left,
             ps.rcPaint.top, SRCCOPY);
      popup_apply_alpha(hwnd, memDC, rect.right, rect.bottom);
      SelectObject(memDC, oldBmp);
    }
    win32_set_draw_context(NULL);
    EndPaint(hwnd, &ps);
  } break;
  case WM_ERASEBKGND: {
    return TRUE;
  }
  case WM_DESTROY: {
    static const WORD held_modifiers[] = {VK_LSHIFT,   VK_RSHIFT, VK_LCONTROL,
                                          VK_RCONTROL, VK_LMENU,  VK_RMENU,
                                          VK_LWIN,     VK_RWIN};
    if (lb)
      lb->selected_index = 0;

    for (int i = 0; i < (int)ARRAY_LEN(held_modifiers); i++) {
      if (GetAsyncKeyState(held_modifiers[i]) & 0x8000) {
        INPUT up = {0};
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = held_modifiers[i];
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &up, sizeof(INPUT));
      }
    }
    if (s_sel_brush) {
      DeleteObject(s_sel_brush);
      s_sel_brush = NULL;
      s_sel_color = (COLORREF)-1;
    }
    if (s_bg_brush) {
      DeleteObject(s_bg_brush);
      s_bg_brush = NULL;
      s_bg_color = (COLORREF)-1;
    }
    popup_free_back_buffer(popup_buffer_for(hwnd));
    s_repaint_queued = false;
    break;
  }
  default:
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  return 0;
}


