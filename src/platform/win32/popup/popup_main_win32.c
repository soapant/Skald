#include <windows.h>
#include <string.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "core/prompt_geometry.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/bg_image_win32.h"
#include "platform/input_state.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"

#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/checkbox.h"
#include "core/listbox_hover.h"
static bool s_swallow_next_char = false;

static bool s_repaint_queued = false;

void win32_popup_request_repaint(HWND hPopup) {
  if (!hPopup || !IsWindow(hPopup))
    return;
  if (s_repaint_queued)
    return;
  s_repaint_queued = true;
  PostMessageA(hPopup, WM_APP + 41, 0, 0);
}

static void popup_request_repaint(HWND hwnd) {
  if (FlintContext_Get()->prompt_callbacks.render == NULL) {
    UI_LegacySyncRepaint();
    return;
  }
  win32_popup_request_repaint(hwnd);
}

static bool prompt_win_no_bg(void);

static bool PromptWindow_OnKey(HWND hwnd, const PopupKeyEvent *key) {

  WindowManager *wm = WindowManager_Get();
  Popup *top = WindowManager_GetTopPopup(wm);
  if (!top || WH_TO_HWND(top->handle) != hwnd) {
    return false;
  }

  int wParam = key->vk;

  if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU)
    return false;

  extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
  PromptState *state = Platform_GetPromptStateForHWND(hwnd);
  int old_font = FontSize;

  if (wParam == VK_ESCAPE) {
    if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
      DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
    }
  }

  bool hide_popup = (wParam == VK_RETURN);
  HWND popup_wnd = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  if (hide_popup && popup_wnd != NULL)
    ShowWindow(popup_wnd, SW_HIDE);

  bool handled = false;
  if (state)
    handled = Prompt_HandleKey(state, key);

  if (!handled)
    return false;

  if (hide_popup && !FlintContext_Get()->window.selection_complete) {
    if (popup_wnd != NULL)
      ShowWindow(popup_wnd, SW_SHOW);
    if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL)
      ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
  }
  if (FontSize != old_font) {
    HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
  }
  if (FlintContext_Get()->prompt_callbacks.render != NULL) {
    HWND hPar = GetParent(hwnd);
    if (hPar && IsWindow(hPar)) {
      InvalidateRect(hPar, NULL, FALSE);
    }
  } else {
    InvalidateRect(hwnd, NULL, FALSE);
  }
  return true;
}


static HDC s_bb_dc = NULL;
static HBITMAP s_bb_bmp = NULL;
static HBITMAP s_bb_prev = NULL;
static int s_bb_w = 0;
static int s_bb_h = 0;

static void popup_free_back_buffer(void) {
  if (s_bb_dc) {
    DeleteDC(s_bb_dc);
    s_bb_dc = NULL;
  }
  if (s_bb_bmp) {
    DeleteObject(s_bb_bmp);
    s_bb_bmp = NULL;
  }
  s_bb_prev = NULL;
  s_bb_w = 0;
  s_bb_h = 0;
}

static HDC popup_back_buffer(HDC hdc, int w, int h, HBITMAP *out_prev) {
  if (w <= 0 || h <= 0)
    return NULL;
  if (!s_bb_dc || !s_bb_bmp || s_bb_w != w || s_bb_h != h) {
    popup_free_back_buffer();
    s_bb_dc = CreateCompatibleDC(hdc);
    s_bb_bmp = CreateCompatibleBitmap(hdc, w, h);
    s_bb_w = w;
    s_bb_h = h;
  }
  if (!s_bb_dc || !s_bb_bmp) {
    popup_free_back_buffer();
    return NULL;
  }
  *out_prev = (HBITMAP)SelectObject(s_bb_dc, s_bb_bmp);
  return s_bb_dc;
}

static bool prompt_win_no_bg(void) {

  extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
  PromptState *st = Platform_GetPromptStateForHWND(GetFocus());
  if (st && st->no_bg)
    return true;
  return false;
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                              LPARAM lParam) {

  if (msg == WM_KEYDOWN) {
    int mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000)
      mods |= HOTKEY_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
      mods |= HOTKEY_MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)
      mods |= HOTKEY_MOD_SHIFT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000)
      mods |= HOTKEY_MOD_SUPER;
    Platform_InputState_Update((int)wParam, true, mods);
  }
  if (msg == WM_KEYUP) {
    int mods = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000)
      mods |= HOTKEY_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
      mods |= HOTKEY_MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)
      mods |= HOTKEY_MOD_SHIFT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000)
      mods |= HOTKEY_MOD_SUPER;
    Platform_InputState_Update((int)wParam, false, mods);
  }
  ListBoxInfo *lb = GetLBFromPopup(hwnd);
  WindowManager *_wm_guard = WindowManager_Get();
  Popup *_top_guard = _wm_guard ? WindowManager_GetTopPopup(_wm_guard) : NULL;
  if (_top_guard && _top_guard->type == POPUP_PROMPT &&
      WH_TO_HWND(_top_guard->handle) != NULL &&
      WH_TO_HWND(_top_guard->handle) != hwnd) {
    if (msg == WM_PAINT) {
      log_info("PopupWndProc SKIP PAINT hwnd=%p top=%p", (void *)hwnd,
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
  static HBRUSH s_bg_brush = NULL;
  static HBRUSH s_bg_gradient_brushes[128] = {0};
  static bool s_bg_gradient_ready = false;
  static HBRUSH s_sel_brush = NULL;
  (void)s_bg_gradient_brushes;
  (void)s_bg_gradient_ready;

  switch (msg) {
  case WM_LBUTTONDOWN: {
    POINT pt;
    if (GetCursorPos(&pt))
      ListBox_PublishMousePos((int)pt.x, (int)pt.y);
    InvalidateRect(hwnd, NULL, FALSE);
    break;
  }
  case WM_MOUSEMOVE: {
    extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
    POINT pt;
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (GetCursorPos(&pt)) {
      ListBox_PublishMousePos(pt.x, pt.y);
    }

    if (FlintContext_Get()->prompt_callbacks.render == NULL) {
      PromptState *hst = Platform_GetPromptStateForHWND(hwnd);
      if (hst && hst->listbox != NULL) {
        ListBoxInfo *hlb = &hst->listbox;
        int list_top = PromptListTop();
        int item_h = PromptRowHeight();
        int visible =
            (rect.bottom - PromptListBottomMargin() - list_top) / item_h;

        ListBox_SetGeometry(list_top, item_h, visible);

        ListBox_HoverFocus(hlb, list_top, item_h, hlb->view_offset, visible);
      }
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_MOUSEWHEEL: {
    extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
    short delta = (short)HIWORD(wParam);
    int rows = (delta > 0) ? -LISTBOX_WHEEL_ROWS : LISTBOX_WHEEL_ROWS;
    ListBox_PublishMousePos((short)(lParam & 0xFFFF),
                            (short)((lParam >> 16) & 0xFFFF));
    PromptState *hst = Platform_GetPromptStateForHWND(hwnd);
    ListBox_QueueScroll(rows);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_KEYDOWN: {
    s_swallow_next_char = false;
    HWND hKey = hwnd;
    PopupKeyEvent pkey;
    memset(&pkey, 0, sizeof(pkey));
    pkey.vk = (int)wParam;
    pkey.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    pkey.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    pkey.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    s_swallow_next_char = true;
    bool consumed = PromptWindow_OnKey(hKey, &pkey);
    if (wParam == VK_BACK) {
      extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
      PromptState *ps = Platform_GetPromptStateForHWND(hwnd);
      if (ps) {
        strncpy(FlintContext_Get()->window.filter, ps->editbox->filter,
                sizeof(FlintContext_Get()->window.filter) - 1);
        FlintContext_Get()
            ->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] =
            '\0';
        FlintContext_Get()->window.filter_cursor = (int)ps->editbox->cursor;
        if (FlintContext_Get()->prompt_callbacks.refresh) {
          FlintContext_Get()->prompt_callbacks.refresh(ps);
        }
      }
    }
    if (!consumed)
      s_swallow_next_char = false;
    if (wParam == VK_DELETE) {
      UI_DeleteForward();
      extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
      PromptState *ps = Platform_GetPromptStateForHWND(hwnd);
      if (ps) {
        strncpy(FlintContext_Get()->window.filter, ps->editbox->filter,
                sizeof(FlintContext_Get()->window.filter) - 1);
        FlintContext_Get()
            ->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] =
            '\0';
        FlintContext_Get()->window.filter_cursor = (int)ps->editbox->cursor;

        if (FlintContext_Get()->prompt_callbacks.refresh) {
          FlintContext_Get()->prompt_callbacks.refresh(ps);
        }
      }
      UI_Redraw(lb);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (consumed) {
      popup_request_repaint(hwnd);
      return 0;
    }
    if (true && wParam >= 0x20 && wParam < 0x7F)
      break;
    popup_request_repaint(hwnd);
    break;
  }
  case WM_CHAR: {
    if (s_swallow_next_char) {
      s_swallow_next_char = false;
      return 0;
    }
    if (wParam == 0x08 || wParam == 0x7F)
      return 0;
    if (wParam >= 0x20 && true) {
      extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
      PromptState *ps = Platform_GetPromptStateForHWND(hwnd);
      if (ps && Prompt_DeleteSelection(ps)) {
        UI_SetText(ps->editbox->filter);
        UI_SetCursorPosition(ps->editbox->cursor);
      }
      if (UI_InsertChar((wchar_t)wParam) > 0) {
        ps = Platform_GetPromptStateForHWND(hwnd);
        if (ps) {
          strncpy(FlintContext_Get()->window.filter, ps->editbox->filter,
                  sizeof(FlintContext_Get()->window.filter) - 1);
          FlintContext_Get()
              ->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] =
              '\0';
          FlintContext_Get()->window.filter_cursor = (int)ps->editbox->cursor;

          if (FlintContext_Get()->prompt_callbacks.refresh) {
            FlintContext_Get()->prompt_callbacks.refresh(ps);
          }
        }
        UI_Redraw(lb);
        popup_request_repaint(hwnd);
        return 0;
      }
    }
    break;
  }
  case WM_ACTIVATE: {
    if (LOWORD(wParam) == WA_INACTIVE) {
      WindowManager *_wm = WindowManager_Get();
      if (_wm && _wm->popup_count > 1) {
        Popup *_top = WindowManager_GetTopPopup(_wm);
        if (_top && WH_TO_HWND(_top->handle) == hwnd) {
          WindowManager_PopPopup(_wm);
          return 0;
        }
        return 0;
      }
      if (CloseOnLossOfFocus) {
        FlintContext_Get()->window.output_buffer[0] = '\0';
        FlintContext_Get()->window.selection_complete = true;
      }
    }
    break;
  }
  case WM_KILLFOCUS: {
    WindowManager *_wm = WindowManager_Get();
    if (_wm && _wm->popup_count > 1) {
      Popup *_top = WindowManager_GetTopPopup(_wm);
      if (_top && WH_TO_HWND(_top->handle) == hwnd) {
        WindowManager_PopPopup(_wm);
        return 0;
      }
      return 0;
    }
    if (CloseOnLossOfFocus) {
      FlintContext_Get()->window.output_buffer[0] = '\0';
      FlintContext_Get()->window.selection_complete = true;
    }
    break;
  }
  case WM_CTLCOLOREDIT: {
    HDC hdc = (HDC)wParam;
    COLORREF tc = RainbowText ? GetRainbowColor(0.5f, 0.85f, GradientBrightness)
                              : COLOR_TEXT;
    if (prompt_win_no_bg()) {
      SetTextColor(hdc, tc);
      SetBkMode(hdc, TRANSPARENT);
      return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    SetTextColor(hdc, tc);
    SetBkColor(hdc, COLOR_BG);
    if (!s_bg_brush)
      s_bg_brush = CreateSolidBrush(COLOR_BG);
    return (LRESULT)s_bg_brush;
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
    if (!s_bg_brush)
      s_bg_brush = CreateSolidBrush(COLOR_BG);
    return (LRESULT)s_bg_brush;
  }
  case WM_CREATE: {
    if (only_once) {
      only_once = false;
      break;
    }
  }
  case WM_SHOWWINDOW: {
    if (wParam && g_pending_navigate >= 0) {
      int idx = g_pending_navigate;
      g_pending_navigate = -1;
      UI_NavigateTo(lb, idx);
    }
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
      UI_RedrawNow();
      if (FlintContext_Get()->prompt_callbacks.render != NULL)
        InvalidateRect(hwnd, NULL, FALSE);
      else
        UI_LegacySyncRepaint();
      return 0;
    }
    if (wParam == UI_TICK_TIMER_ID) {
        UI_Redraw(lb);
      }
      if (FlintContext_Get()->prompt_callbacks.render != NULL)
        InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    break;
  case WM_MEASUREITEM: {
    MEASUREITEMSTRUCT *pmis = (MEASUREITEMSTRUCT *)lParam;
    if (pmis->CtlID == IDC_LIST_BOX) {
      int h = PromptRowHeight();
      pmis->itemHeight = h;
      return TRUE;
    }
    break;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT *pdis = (DRAWITEMSTRUCT *)lParam;
    if (pdis->CtlID == IDC_LIST_BOX && pdis->itemID != -1) {
      BOOL isSelected = (pdis->itemID == (lb ? lb->selected_index : -1));
      if (pdis->itemID >= (lb ? lb->populated_count : 0)) {
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
          int winW = winRc.right - winRc.left;
          int winH = winRc.bottom - winRc.top;
          POINT lbOrigin2 = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOrigin2);
          ScreenToClient(hwnd, &lbOrigin2);
          int gx = lbOrigin2.x + pdis->rcItem.left;
          int gy = lbOrigin2.y + pdis->rcItem.top;
          if (!win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH,
                                         lbOrigin2.x, lbOrigin2.y)) {
          }
        }
        if (isSelected) {
          win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_SELECTED_TEXT,
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
          if (!win32_draw_bg_image_slice(pdis->hDC, &inner, iwW, iwH,
                                         lbOriginI.x, lbOriginI.y)) {
            win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_BG, innerR);
          }
        }
      } else {
        if (isSelected) {
          if (!s_sel_brush)
            s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
          FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
        } else {
          RECT winRc;
          GetClientRect(hwnd, &winRc);
          int winW = winRc.right - winRc.left;
          int winH = winRc.bottom - winRc.top;
          POINT lbOrigin = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOrigin);
          ScreenToClient(hwnd, &lbOrigin);
          if (winW <= 0)
            winW = pdis->rcItem.right - pdis->rcItem.left;
          if (winH <= 0) {
            RECT tr2;
            GetClientRect(hwnd, &tr2);
            winH = tr2.bottom - tr2.top;
            if (winH <= 0)
              winH = pdis->rcItem.bottom - pdis->rcItem.top + 600;
          }
          if (!win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH,
                                         lbOrigin.x, lbOrigin.y)) {
            if (!s_bg_brush)
              s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(pdis->hDC, &pdis->rcItem, s_bg_brush);
          }
        }
      }
      if (UI_IsListBoxSelected(lb)) {
        char measureBuffer[512];
        strncpy(measureBuffer, lb->populated_cache[pdis->itemID],
                sizeof(measureBuffer) - 1);
        measureBuffer[sizeof(measureBuffer) - 1] = '\0';
        char *pos = strchr(measureBuffer, '|');
        if (pos) {
          *pos = '\0';
        }
        if (!FullTextNeedsKeyPress) {
          if (isSelected && strlen(measureBuffer) >= 70) {
            if (g_last_selected_index != pdis->itemID ||
                WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) ==
                    NULL) {
              g_last_selected_index = pdis->itemID;
              PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)pdis->itemID);
            }
          } else {
            if (g_last_selected_index == pdis->itemID) {
              g_last_selected_index = -1;
              PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)-1);
            }
          }
        } else {
          if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
            FadeWindowData *msgData = (FadeWindowData *)GetWindowLongPtrA(
                WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage),
                GWLP_USERDATA);
            bool isIndefinite = msgData && msgData->timeout == 6969.0;
            if (!isIndefinite) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage),
                        999);
              DestroyWindow(
                  WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
              FlintContext_Get()->window.hDisplayMessage = WH_NULL;
            }
          }
        }
      }

      const char *textBuffer = "";
      if (pdis->itemID < (lb ? lb->populated_count : 0)) {
        textBuffer = lb->populated_cache[pdis->itemID];
      }

      if (pdis->itemID == -1 ||
          pdis->itemID >= (lb ? lb->populated_count : 0)) {
        return TRUE;
      }

      SetBkMode(pdis->hDC, TRANSPARENT);
      RECT textRect = pdis->rcItem;
      textRect.left += PROMPT_ITEM_TEXT_X;

      Platform_SetDrawContextWin32(pdis->hDC);
      if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        return TRUE;
      } else {
        HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
        HFONT hOldMainFont = hMainFont ? (HFONT)SelectObject(pdis->hDC, hMainFont) : NULL;
        unsigned long ncol, bcol, pcol;
        int orig_idx = (int)pdis->itemID;
        int orig_cnt = lb ? lb->populated_count : 0;
        if (lb && (int)pdis->itemID >= 0 &&
            pdis->itemID < (UINT)lb->populated_count) {
          orig_idx = lb->matching_indices[pdis->itemID];
          orig_cnt = lb->last_item_count > 0 ? lb->last_item_count
                                             : lb->populated_count;
        }
        Platform_GetItemColors((PlGetItemColorsSpec){.idx = orig_idx,
                                                     .count = orig_cnt,
                                                     .num_color = &ncol,
                                                     .body_color = &bcol,
                                                     .plain_color = &pcol});
        SetTextColor(pdis->hDC, ncol);
        DrawTextA(pdis->hDC, textBuffer, -1, &textRect,
                  DT_SINGLELINE | DT_VCENTER);
        SelectObject(pdis->hDC, hOldMainFont);
        if (hMainFont)
          DeleteObject(hMainFont);
      }
      return TRUE;
    }
    break;
  }
  case WM_APP + 41: {
    s_repaint_queued = false;
    if (FlintContext_Get()->prompt_callbacks.render == NULL) {
      UI_LegacySyncRepaint();
      return 0;
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rect;
    GetClientRect(hwnd, &rect);
    HDC paintDC = hdc;
    HDC memDC = NULL;
    HBITMAP oldBmp = NULL;
    bool useRenderBuf = (rect.right > 0 && rect.bottom > 0);
    if (useRenderBuf) {
      HBITMAP prev = NULL;
      HDC buf = popup_back_buffer(hdc, rect.right, rect.bottom, &prev);
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
            win32_draw_bg_image_slice(paintDC, &visRect, rect.right - rect.left,
                                      rect.bottom - rect.top, 0, 0);
      }
      if (!imgDrawn) {
        if (!s_bg_brush)
          s_bg_brush = CreateSolidBrush(COLOR_BG);
        FillRect(paintDC, &ps.rcPaint, s_bg_brush);
      }
      win32_draw_window_border(paintDC, 0, 0, rect.right, rect.bottom, NULL);
    }
    if (FlintContext_Get()->prompt_callbacks.render) {
      FlintContext_Get()->window.editbox_placed = false;
      extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
      PromptState *state = Platform_GetPromptStateForHWND(hwnd);
      if (state && state->render) {
        Checkbox_FrameBegin();
        Platform_SetDrawContextWin32(paintDC);
        state->render(state);
      }
    } else {
      extern PromptState *Platform_GetPromptStateForHWND(HWND hwnd);
      PromptState *estate = Platform_GetPromptStateForHWND(hwnd);
      if (estate) {
        Platform_SetDrawContextWin32(paintDC);
        extern void Platform_DrawPromptEditbox(PromptState * state);
        Platform_DrawPromptEditbox(estate);

        Platform_DrawPromptSeparator((PlDrawPromptSeparatorSpec){
            .y = PromptListTop() - 2, .width = rect.right - rect.left});

        if (estate->listbox) {
          int list_top = PromptListTop();
          int row_height = PromptRowHeight();
          int count = estate->listbox->populated_count;
          int visible_count =
              (rect.bottom - PromptListBottomMargin() - list_top) / row_height;
          int view_offset = estate->listbox->view_offset;

          for (int i = 0; i < visible_count && (i + view_offset) < count; i++) {
            int list_idx = i + view_offset;
            int y = list_top + i * row_height;

            if (list_idx == estate->listbox->selected_index) {
              RECT itemRect = {10, y + 1, rect.right - 10, y + row_height - 1};
              win32_fill_rounded_rect(paintDC, &itemRect, COLOR_SELECTED_TEXT,
                                      PROMPT_SEL_RADIUS);
            }

            const char *text = ListBoxInfo_Row(&estate->listbox, list_idx);
            if (!text)
              continue;

            unsigned long ncol, bcol, pcol;
            int orig_idx = estate->listbox->matching_indices[list_idx];
            int orig_cnt = estate->listbox->last_item_count > 0
                               ? estate->listbox->last_item_count
                               : estate->listbox->populated_count;
            Platform_GetItemColors((PlGetItemColorsSpec){.idx = orig_idx,
                                                         .count = orig_cnt,
                                                         .num_color = &ncol,
                                                         .body_color = &bcol,
                                                         .plain_color = &pcol});

            Platform_DrawTextRaw((PlDrawTextRawSpec){.x = 14,
                                                     .y = y,
                                                     .text = text,
                                                     .color = ncol,
                                                     .font = FH_NULL,
                                                     .align = "LEFT",
                                                     .pad = 0,
                                                     .fontsize = FontSize});
          }
        }

        Platform_DrawPromptSeparator((PlDrawPromptSeparatorSpec){
            .y = rect.bottom - PromptListBottomMargin(),
            .width = rect.right - rect.left});
        Platform_SetDrawContextWin32(NULL);
      }
    }
    if (FlintContext_Get()->window.title != NULL &&
        strlen(FlintContext_Get()->window.title) > 0 && WindowTitleText) {
      wchar_t wtitle[256];
      MultiByteToWideChar(CP_UTF8, 0, FlintContext_Get()->window.title, -1,
                          wtitle, 256);
      HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      HFONT hOldFont = hMainFont ? (HFONT)SelectObject(paintDC, hMainFont) : NULL;
      SetBkMode(paintDC, TRANSPARENT);
      SIZE sz;
      GetTextExtentPoint32W(paintDC, wtitle, (int)wcslen(wtitle), &sz);
      RECT titleRect;
      titleRect.left = PROMPT_FOOTER_X;
      titleRect.top = rect.bottom - PROMPT_FOOTER_Y_FROM_BOTTOM;
      titleRect.right = titleRect.left + sz.cx + PROMPT_FOOTER_PAD;
      titleRect.bottom = titleRect.top + sz.cy;
      COLORREF titleColor;
      if ((lb ? lb->populated_count : 0) < 25) {
        titleColor = RGB(0, 255, 0);
      } else if ((lb ? lb->populated_count : 0) < 50) {
        titleColor = RGB(100, 255, 0);
      } else if ((lb ? lb->populated_count : 0) < 75) {
        titleColor = RGB(200, 255, 0);
      } else if ((lb ? lb->populated_count : 0) < 100) {
        titleColor = RGB(255, 255, 0);
      } else if ((lb ? lb->populated_count : 0) < 125) {
        titleColor = RGB(255, 200, 0);
      } else if ((lb ? lb->populated_count : 0) < 150) {
        titleColor = RGB(255, 150, 0);
      } else if ((lb ? lb->populated_count : 0) < 175) {
        titleColor = RGB(255, 100, 0);
      } else if ((lb ? lb->populated_count : 0) < 200) {
        titleColor = RGB(255, 50, 0);
      } else {
        titleColor = RGB(255, 0, 0);
      }
      SetTextColor(paintDC, titleColor);
      DrawTextW(paintDC, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
      SelectObject(paintDC, hOldFont);
      if (hMainFont)
        DeleteObject(hMainFont);
    }
    if (useRenderBuf) {
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
             ps.rcPaint.right - ps.rcPaint.left,
             ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left,
             ps.rcPaint.top, SRCCOPY);
      SelectObject(memDC, oldBmp);
    }
    EndPaint(hwnd, &ps);
  } break;
  case WM_ERASEBKGND: {
    return TRUE;
  }
  case WM_DESTROY: {
    static const WORD held_modifiers[] = {VK_LSHIFT,   VK_RSHIFT, VK_LCONTROL,
                                          VK_RCONTROL, VK_LMENU,  VK_RMENU,
                                          VK_LWIN,     VK_RWIN};
    last_index = 0;
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
    }
    popup_free_back_buffer();
    s_repaint_queued = false;
    rainbow_init = false;
    break;
  }
  default:
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  return 0;
}
