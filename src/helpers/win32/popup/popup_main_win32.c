#include <windows.h>
#include <string.h>
#include "core/types/keys.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "config_values.h"
#include "helpers/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/window_manager.h"
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
#include "wingdi.h"
#include "platform/platform.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/checkbox.h"
#include "core/draw_state.h"

#define IDC_LIST_BOX 102

static ListBoxInfo *win32_get_lb_from_popup(HWND hPopup) {
  if (hPopup && IsWindow(hPopup)) {
    const PromptState *state = win32_prompt_state_from_hwnd(hPopup);
    if (state) {
      return state->lb;
    }
    return (ListBoxInfo *)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
  }
  return NULL;
}

static bool s_swallow_next_char = false;

static bool s_repaint_queued = false;

static bool s_in_render = false;

void win32_popup_request_repaint(HWND hPopup) {
  if (!hPopup || !IsWindow(hPopup))
    return;
  if (s_in_render)
    return;
  if (s_repaint_queued)
    return;
  s_repaint_queued = true;
  PostMessageA(hPopup, WM_APP + 41, 0, 0);
}

static void popup_request_repaint(HWND hwnd) {
  if (SkaldContext_Get()->window.prompt_callbacks.render == NULL) {
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
  HWND popup_wnd = WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd);
  if (hide_popup && popup_wnd != NULL && IsWindow(popup_wnd))
    ShowWindow(popup_wnd, SW_HIDE);

  bool handled = false;
  if (state)
    handled = prompt_handle_key(state, key);

  if (!handled)
    return false;

  if (hide_popup && !SkaldContext_Get()->window.selection_complete) {
    if (popup_wnd != NULL && IsWindow(popup_wnd))
      ShowWindow(popup_wnd, SW_SHOW);
    if (popup_wnd && IsWindow(popup_wnd) &&
        WH_TO_HWND(SkaldContext_Get()->window.hPopupWnd) != NULL)
      win32_force_window_focus(SkaldContext_Get()->window.hPopupWnd);
  }
  if (SkaldContext_Get()->window.prompt_callbacks.render != NULL) {
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

  ListBoxInfo *lb = win32_get_lb_from_popup(hwnd);
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
    POINT pt;
    if (GetCursorPos(&pt))
      listbox_publish_mouse_pos((int)pt.x, (int)pt.y);
    RECT rect;
    GetClientRect(hwnd, &rect);
    PromptState *click_ps = win32_prompt_state_from_hwnd(hwnd);
    if (SkaldContext_Get()->window.prompt_callbacks.render == NULL &&
        click_ps && click_ps->lb != NULL) {
      int list_top = prompt_list_top();
      int item_h = prompt_row_height();
      int visible =
          (rect.bottom - prompt_list_bottom_margin() - list_top) / item_h;
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

    if (SkaldContext_Get()->window.prompt_callbacks.render == NULL) {
      PromptState *hst = win32_prompt_state_from_hwnd(hwnd);
      if (hst && hst->lb != NULL) {
        ListBoxInfo *hlb = hst->lb;
        int list_top = prompt_list_top();
        int item_h = prompt_row_height();
        int visible =
            (rect.bottom - prompt_list_bottom_margin() - list_top) / item_h;

        listbox_set_geometry(list_top, item_h, visible);

        listbox_hover_focus(hlb, list_top, item_h, hlb->view_offset, visible);
      }
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_MOUSEWHEEL: {
    short delta = (short)HIWORD(wParam);
    int rows = (delta > 0) ? -LISTBOX_WHEEL_ROWS : LISTBOX_WHEEL_ROWS;
    listbox_publish_mouse_pos((short)(lParam & 0xFFFF),
                              (short)((lParam >> 16) & 0xFFFF));
    listbox_queue_scroll(rows);
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
    bool consumed = win32_prompt_window_on_key(hKey, &pkey);
    if (wParam == VK_BACK) {
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && SkaldContext_Get()->window.prompt_callbacks.refresh) {
        SkaldContext_Get()->window.prompt_callbacks.refresh(ps);
      }
    }
    if (!consumed)
      s_swallow_next_char = false;
    if (wParam == VK_DELETE) {
      UI_DeleteForward();
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && SkaldContext_Get()->window.prompt_callbacks.refresh) {
        SkaldContext_Get()->window.prompt_callbacks.refresh(ps);
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
    if (SkaldContext_Get()->window.prompt_callbacks.render != NULL) {
      InvalidateRect(hwnd, NULL, FALSE);
      UpdateWindow(hwnd);
    } else {
      popup_request_repaint(hwnd);
    }
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
      PromptState *ps = win32_prompt_state_from_hwnd(hwnd);
      if (ps && prompt_delete_selection(ps)) {
        UI_SetText(ps->eb->filter);
        UI_SetCursorPosition(ps->eb->cursor);
      }
      if (UI_InsertChar((wchar_t)wParam) > 0) {
        ps = win32_prompt_state_from_hwnd(hwnd);
        if (ps && SkaldContext_Get()->window.prompt_callbacks.refresh) {
          SkaldContext_Get()->window.prompt_callbacks.refresh(ps);
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
    COLORREF tc = RainbowText ? GetRainbowColor(0.5f, 0.85f, 1.0f) : COLOR_TEXT;
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
      if (SkaldContext_Get()->window.prompt_callbacks.render != NULL)
        InvalidateRect(hwnd, NULL, FALSE);
      else
        UI_LegacySyncRepaint();
      return 0;
    }
    if (wParam == UI_TICK_TIMER_ID) {
      UI_RequestPopupRepaint();
    }
    if (SkaldContext_Get()->window.prompt_callbacks.render != NULL)
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
          int winW = winRc.right - winRc.left;
          int winH = winRc.bottom - winRc.top;
          POINT lbOrigin2 = {0, 0};
          ClientToScreen(pdis->hwndItem, &lbOrigin2);
          ScreenToClient(hwnd, &lbOrigin2);
          win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH,
                                    lbOrigin2.x, lbOrigin2.y);
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
          FillRect(
              pdis->hDC, &pdis->rcItem,
              cached_brush(&s_sel_brush, &s_sel_color, COLOR_SELECTED_TEXT));
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
            FillRect(pdis->hDC, &pdis->rcItem,
                     cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG));
          }
        }
      }
      if (lb && lb->selected_index != -1) {
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

      win32_set_draw_context(pdis->hDC);
      if (SkaldContext_Get()->window.prompt_callbacks.render != NULL) {
        return TRUE;
      } else {
        HFONT hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
        HFONT hOldMainFont =
            hMainFont ? (HFONT)SelectObject(pdis->hDC, hMainFont) : NULL;
        unsigned long ncol, bcol, pcol;
        int orig_idx = (int)pdis->itemID;
        int orig_cnt = lb ? lb->item_count : 0;
        if (lb && (int)pdis->itemID >= 0 &&
            (UINT)pdis->itemID < (UINT)lb->item_count) {
          orig_idx = lb->matching_indices[pdis->itemID];
          orig_cnt = lb->item_count;
        }
        item_get_colors((PlGetItemColorsSpec){.idx = orig_idx,
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
    if (SkaldContext_Get()->window.prompt_callbacks.render == NULL) {
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
        FillRect(paintDC, &ps.rcPaint,
                 cached_brush(&s_bg_brush, &s_bg_color, COLOR_BG));
      }
      win32_draw_window_border(paintDC, 0, 0, rect.right, rect.bottom, NULL);
    }
    if (SkaldContext_Get()->window.prompt_callbacks.render) {
      SkaldContext_Get()->window.editbox_placed = false;
      PromptState *state = win32_prompt_state_from_hwnd(hwnd);
      if (state && state->callbacks && state->callbacks->render) {
        checkbox_frame_begin();
        win32_set_draw_context(paintDC);
        pl_draw_state_get()->win32_listbox_surface = true;
        s_in_render = true;
        state->callbacks->render(state);
        s_in_render = false;
        pl_draw_state_get()->win32_listbox_surface = false;
      }
    } else {
      PromptState *estate = win32_prompt_state_from_hwnd(hwnd);
      if (estate) {
        win32_set_draw_context(paintDC);
        extern void Platform_DrawPromptEditbox(PromptState * state);
        Platform_DrawPromptEditbox(estate);

        if (estate->lb) {
          draw_prompt_separator((PlDrawPromptSeparatorSpec){
              .y = prompt_list_top() - 2, .width = rect.right - rect.left});

          int list_top = prompt_list_top();
          int row_height = prompt_row_height();
          int count = estate->lb->item_count;
          int visible_count =
              (rect.bottom - prompt_list_bottom_margin() - list_top) /
              row_height;
          int view_offset = estate->lb->view_offset;

          pl_draw_state_get()->win32_listbox_surface = true;
          for (int i = 0; i < visible_count && (i + view_offset) < count; i++) {
            int list_idx = i + view_offset;
            int y = list_top + i * row_height;

            if (list_idx == estate->lb->selected_index) {
              RECT itemRect = {10, y + 1, rect.right - 10, y + row_height - 1};
              win32_fill_rounded_rect(paintDC, &itemRect, COLOR_SELECTED_TEXT,
                                      PROMPT_SEL_RADIUS);
            }

            const char *text = listbox_info_row(estate->lb, list_idx);
            if (!text)
              continue;

            unsigned long ncol = rgb_to_hex(COLOR_TEXT),
                          bcol = rgb_to_hex(COLOR_TEXT),
                          pcol = rgb_to_hex(COLOR_TEXT);
            int orig_idx = estate->lb->matching_indices[list_idx];
            int orig_cnt = estate->lb->item_count;
            item_get_colors((PlGetItemColorsSpec){.idx = orig_idx,
                                                  .count = orig_cnt,
                                                  .num_color = &ncol,
                                                  .body_color = &bcol,
                                                  .plain_color = &pcol});

            int row_h_scroll = (list_idx == estate->lb->selected_index)
                                   ? estate->lb->text_hscroll
                                   : 0;
            const char *draw_text = text;
            int draw_x = 14;
            if (row_h_scroll > 0)
              draw_x -= row_h_scroll;
            Platform_DrawTextRaw((PlDrawTextRawSpec){.x = draw_x,
                                                     .y = y,
                                                     .text = draw_text,
                                                     .color = ncol,
                                                     .font = FH_NULL,
                                                     .align = "LEFT",
                                                     .pad = 0,
                                                     .fontsize = FontSize});
          }
          pl_draw_state_get()->win32_listbox_surface = false;

          draw_prompt_separator((PlDrawPromptSeparatorSpec){
              .y = rect.bottom - prompt_list_bottom_margin(),
              .width = rect.right - rect.left});
        }
        win32_set_draw_context(NULL);
      }
    }
    if (SkaldContext_Get()->window.title != NULL &&
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
      COLORREF titleColor;
      if ((lb ? lb->item_count : 0) < 25) {
        titleColor = RGB(0, 255, 0);
      } else if ((lb ? lb->item_count : 0) < 50) {
        titleColor = RGB(100, 255, 0);
      } else if ((lb ? lb->item_count : 0) < 75) {
        titleColor = RGB(200, 255, 0);
      } else if ((lb ? lb->item_count : 0) < 100) {
        titleColor = RGB(255, 255, 0);
      } else if ((lb ? lb->item_count : 0) < 125) {
        titleColor = RGB(255, 200, 0);
      } else if ((lb ? lb->item_count : 0) < 150) {
        titleColor = RGB(255, 150, 0);
      } else if ((lb ? lb->item_count : 0) < 175) {
        titleColor = RGB(255, 100, 0);
      } else if ((lb ? lb->item_count : 0) < 200) {
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
    popup_free_back_buffer();
    s_repaint_queued = false;
    break;
  }
  default:
    return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
  return 0;
}
