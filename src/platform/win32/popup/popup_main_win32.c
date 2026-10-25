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
#include "core/feature_data.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/win32/common_ui_win32.h"
#include "platform/win32/bg_image_win32.h"
#include "platform/input_state.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "core/prompt_controller.h"
#include "bindings/vim_state.h"

#include "core/timer.h"
#include "core/listbox_hover.h"
#include "core/prompt_keys.h"
#include "core/checkbox.h"
#include "core/listbox_hover.h"
static bool s_swallow_next_char = false;

static bool s_repaint_queued = false;

void win32_popup_request_repaint(HWND hPopup) {
    if (!hPopup || !IsWindow(hPopup)) return;
    if (s_repaint_queued) return;
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

static bool PromptWindow_OnKey(HWND hwnd, const PopupKeyEvent* key) {
        PROFILE_START(_pt);
    int wParam = key->vk;

    if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) { PROFILE_END_VAR(_pt, "PromptWindow_OnKey"); return false; }

    extern PromptState* Platform_GetWin32PromptState(void);
    PromptState* state = Platform_GetWin32PromptState();
    int old_font = FontSize;

    if (wParam == VK_ESCAPE) {
        if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
            KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
            DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
        }
    }

    bool hide_popup = (wParam == VK_RETURN);
    HWND popup_wnd = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
    if (hide_popup && popup_wnd != NULL) ShowWindow(popup_wnd, SW_HIDE);

    bool handled = false;
    if (state) handled = Prompt_HandleKey(state, key);

    if (!handled) { PROFILE_END_VAR(_pt, "PromptWindow_OnKey"); return false; }

    if (hide_popup && !FlintContext_Get()->window.selection_complete) {
        if (popup_wnd != NULL) ShowWindow(popup_wnd, SW_SHOW);
        if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, WH_NULL);
    }
    if (FontSize != old_font) {
        if (g_win32_hMainFont) { DeleteObject(g_win32_hMainFont); g_win32_hMainFont = NULL; }
        g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
        if (WH_TO_HWND(FlintContext_Get()->window.hListBox) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox))) {
            UI_LB_SetFont(FlintContext_Get()->window.hListBox, g_win32_hMainFont);
            UI_LB_SetItemHeight(FlintContext_Get()->window.hListBox, PromptRowHeight());
        }
        HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
        if (hPopup && IsWindow(hPopup)) {
            InvalidateRect(hPopup, NULL, TRUE);
            if (WH_TO_HWND(FlintContext_Get()->window.hListBox)) InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hListBox), NULL, TRUE);
            UpdateWindow(hPopup);
        }
    }
    if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        HWND hPar = GetParent(hwnd);
        if (hPar && IsWindow(hPar)) { InvalidateRect(hPar, NULL, FALSE); }
    } else {
        InvalidateRect(hwnd, NULL, FALSE);
    }
    PROFILE_END_VAR(_pt, "PromptWindow_OnKey");
    return true;
}

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData){
    PROFILE_START(_pt);
static HBRUSH s_bg_brush = NULL;
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                            break;
    case WM_ERASEBKGND:
                       return TRUE;
    case WM_LBUTTONDOWN: {
      extern PromptState* Platform_GetWin32PromptState(void);
      PromptState* hst = Platform_GetWin32PromptState();
      if (hst && hst->has_listbox) {
        POINT pt; GetCursorPos(&pt);
        ListBox_PublishMousePos(pt.x, pt.y);
        ListBox_SetGeometry(PromptListTop(), UI_LB_GetItemHeight(WH_FROM_HWND(hWnd)), UI_LB_GetVisibleCount());
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        Prompt_HandleListClick(hst, ctrl, shift, alt);
      }
      return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
    case WM_MOUSEMOVE: {
      extern PromptState* Platform_GetWin32PromptState(void);
      POINT pt;
      if (GetCursorPos(&pt)) ListBox_PublishMousePos((int)pt.x, (int)pt.y);
      PromptState* hst = Platform_GetWin32PromptState();
      if (hst && hst->has_listbox) {
        ListBoxInfo* hlb = &hst->listbox;
        int hItemH = UI_LB_GetItemHeight(WH_FROM_HWND(hWnd));
        int hVisible = UI_LB_GetVisibleCount();
        int hTop = UI_LB_GetTopIndex(WH_FROM_HWND(hWnd));
        ListBox_SetGeometry(PromptListTop(), hItemH, hVisible);
        ListBox_HoverFocus(hlb, PromptListTop(), hItemH, hTop, hVisible);
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
      if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hWnd, &ps); return 0; }
      HDC memDC = CreateCompatibleDC(hdc);
      HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
      if (!memDC || !memBmp) {
        if (memDC) DeleteDC(memDC);
        if (memBmp) DeleteObject(memBmp);
        EndPaint(hWnd, &ps);
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
      }
      HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
      HWND hParent = GetParent(hWnd);
      RECT winRc; GetClientRect(hParent ? hParent : hWnd, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT lbOrigin = {0,0};
      if (hParent) { ClientToScreen(hWnd, &lbOrigin); ScreenToClient(hParent, &lbOrigin); }
      if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
        RECT full = rc;
        if (!win32_draw_bg_image_slice(memDC, &full, winW, winH, lbOrigin.x, lbOrigin.y)) {
          win32_draw_background_slice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
        }
      } else {
        RECT full = rc;
        if (!win32_draw_bg_image_slice(memDC, &full, winW, winH, lbOrigin.x, lbOrigin.y)) {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
          FillRect(memDC, &rc, s_bg_brush);
        }
      }
      SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
      {
        int lbCount = (int)SendMessageA(hWnd, LB_GETCOUNT, 0, 0);
        if (lbCount > 0) {
          HDC prevDC = NULL;
          Platform_GetDrawContextWin32(&prevDC);
          int prevOx = 0, prevOy = 0;
          Platform_GetDrawOffset(&prevOx, &prevOy);
          Platform_SetDrawContextWin32(memDC);
          Platform_SetDrawOffset(0, 0);
          Platform_DrawPromptSeparator(0, winW);
          int _listTop = PromptListTop();
          int listH = winH - _listTop - PromptListBottomMargin();
          Platform_DrawPromptSeparator(_listTop + listH, winW);
          Platform_SetDrawOffset(prevOx, prevOy);
          Platform_SetDrawContextWin32(prevDC);
        }
      }
      {
        int itemH = (int)SendMessageA(hWnd, LB_GETITEMHEIGHT, 0, 0);
        int count = (int)SendMessageA(hWnd, LB_GETCOUNT, 0, 0);
        if (itemH > 0 && count >= 0) {
          LONG itemsBottom = (LONG)count * itemH;
          if (itemsBottom < 0) itemsBottom = 0;
          if (itemsBottom < rc.bottom) {
            RECT empty = { rc.left, itemsBottom, rc.right, rc.bottom };
            RECT inter;
            if (empty.top < ps.rcPaint.bottom && empty.bottom > ps.rcPaint.top) {
              if (!IntersectRect(&inter, &empty, &ps.rcPaint)) inter = empty;
              int gxE = lbOrigin.x + empty.left;
              int gyE = lbOrigin.y + empty.top;
              if (!win32_draw_bg_image_slice(memDC, &empty, winW, winH, lbOrigin.x, lbOrigin.y)) {
                if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                  win32_draw_background_slice(memDC, &empty, COLOR_BG, winW, winH, gxE, gyE);
                } else {
                  if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                  FillRect(memDC, &empty, s_bg_brush);
                }
              }
            }
          }
        } else if (count == 0) {
          RECT full = rc;
          if (!win32_draw_bg_image_slice(memDC, &full, winW, winH, lbOrigin.x, lbOrigin.y)) {
            if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
              win32_draw_background_slice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
            } else {
              if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
              FillRect(memDC, &rc, s_bg_brush);
            }
          }
        }
      }
      {
        RECT borderRegion;
        borderRegion.left = lbOrigin.x;
        borderRegion.top = lbOrigin.y;
        borderRegion.right = lbOrigin.x + rc.right;
        borderRegion.bottom = lbOrigin.y + rc.bottom;
        if (!prompt_win_no_bg()) {
          win32_draw_window_border(memDC, -lbOrigin.x, -lbOrigin.y, winW, winH, &borderRegion);
        }
      }
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
      SelectObject(memDC, oldBmp);
      DeleteObject(memBmp);
      DeleteDC(memDC);
      EndPaint(hWnd, &ps);
      return 0;
    }
    default:
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
  { PROFILE_END_VAR(_pt, "ListBoxSubclassProc"); return 0; }
}

extern PromptState* g_win32_prompt_state;

static HDC s_bb_dc = NULL;
static HBITMAP s_bb_bmp = NULL;
static HBITMAP s_bb_prev = NULL;
static int s_bb_w = 0;
static int s_bb_h = 0;

static void popup_free_back_buffer(void) {
    if (s_bb_dc) { DeleteDC(s_bb_dc); s_bb_dc = NULL; }
    if (s_bb_bmp) { DeleteObject(s_bb_bmp); s_bb_bmp = NULL; }
    s_bb_prev = NULL;
    s_bb_w = 0;
    s_bb_h = 0;
}

static HDC popup_back_buffer(HDC hdc, int w, int h, HBITMAP* out_prev) {
    if (w <= 0 || h <= 0) return NULL;
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
        PROFILE_START(_pt);
if (g_win32_prompt_state && g_win32_prompt_state->no_bg) { PROFILE_END_VAR(_pt, "prompt_win_no_bg"); return true; }
    { PROFILE_END_VAR(_pt, "prompt_win_no_bg"); return false; }
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PROFILE_START(_pt);
if(msg==WM_KEYDOWN){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,true,mods); }
  if(msg==WM_KEYUP){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,false,mods); }
  ListBoxInfo* lb = GetLBForPopupWindow(hwnd);
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_bg_gradient_brushes[128] = {0};
    static bool s_bg_gradient_ready = false;
    static HBRUSH s_sel_brush = NULL;
    (void)s_bg_gradient_brushes; (void)s_bg_gradient_ready;

    
    switch (msg) {
    case WM_LBUTTONDOWN: {
      POINT pt;
      if (GetCursorPos(&pt)) ListBox_PublishMousePos((int)pt.x, (int)pt.y);
      InvalidateRect(hwnd, NULL, FALSE);
      break;
    }
    case WM_MOUSEMOVE: {
      extern PromptState* Platform_GetWin32PromptState(void);
      POINT pt;
      if (GetCursorPos(&pt)) ListBox_PublishMousePos((int)pt.x, (int)pt.y);
      PromptState* hst = Platform_GetWin32PromptState();
      if (hst && hst->has_listbox) {
        ListBoxInfo* hlb = &hst->listbox;
        int list_top = 0, item_h = 0, visible = 0;
        ListBox_GetGeometry(&list_top, &item_h, &visible);
        int hTop = UI_LB_GetViewTop(hlb);
        ListBox_HoverFocus(hlb, list_top, item_h, hTop, visible);
      }
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      extern PromptState* Platform_GetWin32PromptState(void);
      short delta = (short)HIWORD(wParam);
      int rows = (delta > 0) ? -LISTBOX_WHEEL_ROWS : LISTBOX_WHEEL_ROWS;
      ListBox_PublishMousePos((short)(lParam & 0xFFFF), (short)((lParam >> 16) & 0xFFFF));
      HWND hLB = WH_TO_HWND(FlintContext_Get()->window.hListBox);
      PromptState* hst = Platform_GetWin32PromptState();
      if (hLB && IsWindow(hLB) && IsWindowVisible(hLB) && hst && hst->has_listbox) {
        ListBox_ScrollBy(&hst->listbox, rows, UI_LB_GetVisibleCount());
        InvalidateRect(hLB, NULL, FALSE);
      } else {
        ListBox_QueueScroll(rows);
        InvalidateRect(hwnd, NULL, FALSE);
      }
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
        PromptState* ps = g_win32_prompt_state;
        if (ps) {
          strncpy(FlintContext_Get()->window.filter, ps->filter, sizeof(FlintContext_Get()->window.filter) - 1);
          FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
          FlintContext_Get()->window.filter_cursor = (int)ps->cursor;
          if (FlintContext_Get()->prompt_callbacks.refresh) {
            FlintContext_Get()->prompt_callbacks.refresh(ps->filter, ps->has_listbox ? &ps->listbox : NULL);
          }
        }
      }
      if (!consumed) s_swallow_next_char = false;
      if (wParam == VK_DELETE) {
        UI_DeleteForward();
        PromptState* ps = g_win32_prompt_state;
        if (ps) {
          strncpy(FlintContext_Get()->window.filter, ps->filter, sizeof(FlintContext_Get()->window.filter) - 1);
          FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
          FlintContext_Get()->window.filter_cursor = (int)ps->cursor;
        }
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      if (consumed) {
        popup_request_repaint(hwnd);
        return 0;
      }
      if (IsInsertMode() && wParam >= 0x20 && wParam < 0x7F) break;
      popup_request_repaint(hwnd);
      break;
    }
case WM_CHAR: {
      if (s_swallow_next_char) { s_swallow_next_char = false; return 0; }
      if (wParam == 0x08 || wParam == 0x7F) return 0;
      if (wParam >= 0x20 && IsInsertMode()) {
        PromptState* ps = g_win32_prompt_state;
        if (ps && Prompt_DeleteSelection(ps)) {
            UI_SetText(ps->filter);
            UI_SetCursorPosition(ps->cursor);
        }
        if (UI_InsertChar((wchar_t)wParam) > 0) {
        PromptState* ps = g_win32_prompt_state;
        if (ps) {
          strncpy(FlintContext_Get()->window.filter, ps->filter, sizeof(FlintContext_Get()->window.filter) - 1);
          FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
          FlintContext_Get()->window.filter_cursor = (int)ps->cursor;
        }
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        popup_request_repaint(hwnd);
        return 0;
      }
      }
      break;
    }
    case WM_ACTIVATE: {
        if (CloseOnLossOfFocus && LOWORD(wParam) == WA_INACTIVE) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
        }
        break;
    }
    case WM_KILLFOCUS: {
        if (CloseOnLossOfFocus) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wParam;
      COLORREF tc = RainbowText ? GetRainbowColor(0.5f, 0.85f, GradientBrightness) : COLOR_TEXT;
      if (prompt_win_no_bg()) {
        SetTextColor(hdc, tc);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      }
      if ((EnableShaders && ShaderPreset != 0) || win32_bg_image_enabled()) {
        SetTextColor(hdc, tc);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, tc);
        SetBkColor(hdc, COLOR_BG);
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CTLCOLORLISTBOX: {
      HDC hdc = (HDC)wParam;
      if (prompt_win_no_bg()) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      }
      if ((EnableShaders && ShaderPreset != 0) || win32_bg_image_enabled()) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CREATE: {
      if (g_win32_hMainFont) {
        DeleteObject(g_win32_hMainFont);
        g_win32_hMainFont = NULL;
      }
      g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      if (only_once) selection_feature = FlintContext_Get()->window.current_feature;
        only_once = false;
          break;
    }
    case WM_SHOWWINDOW: {
      if (wParam && g_pending_navigate >= 0) {
        int idx = g_pending_navigate;
          g_pending_navigate = -1;
          if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox))) {
            UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, idx);
              UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox));
          }
      }
      break;
    }
    case WM_COMMAND: {
      int wmId   = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
          break; 
        }
      
        break;
    }
    case WM_TIMER: {
      if (wParam == UI_DEBOUNCE_TIMER_ID) {
        KillTimer(hwnd, UI_DEBOUNCE_TIMER_ID);
          UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
          if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
          else UI_LegacySyncRepaint();
          return 0;
      }
      if (wParam == UI_TICK_TIMER_ID) {
          if (PromptController_PollSequenceTimeout()) {
              if (lb && WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) UI_Redraw(lb, FlintContext_Get()->window.hListBox);
          }
          if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
          return 0;
      }
      break;
    }
    case WM_MEASUREITEM: {
      MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
        if (pmis->CtlID == IDC_LIST_BOX) {
          int h = PromptRowHeight();
          pmis->itemHeight = h;
            return TRUE;
        }
      break;
    }
    case WM_DRAWITEM: {
      DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlID == IDC_LIST_BOX && pdis->itemID != -1) {
          BOOL isSelected = (pdis->itemID == (lb ? lb->selected_index : -1));
            if (pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          if (RoundedCorners && CornerRadius > 0) {
            int R = PROMPT_SEL_RADIUS;
            if (R > CornerRadius) R = CornerRadius;
            int t = PROMPT_SEL_INSET;
            RECT inner = pdis->rcItem;
            inner.left += PROMPT_SEL_X + t;
            inner.top += PROMPT_SEL_TOP_INSET + t;
            inner.right -= PROMPT_SEL_X + t;
            inner.bottom -= PROMPT_SEL_TOP_INSET + t;
            if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = pdis->rcItem; t = 0; }
            int innerR = R - 1; if (innerR < 0) innerR = 0;
            {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin2 = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin2);
              ScreenToClient(hwnd, &lbOrigin2);
              int gx = lbOrigin2.x + pdis->rcItem.left;
              int gy = lbOrigin2.y + pdis->rcItem.top;
              if (!win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH, lbOrigin2.x, lbOrigin2.y)) {
                if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                  win32_draw_background_slice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, gx, gy);
                }
              }
            }
            if (isSelected) {
              win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_SELECTED_TEXT, innerR);
            } else {
              if (EnableShaders && ShaderPreset != 0) {
                RECT winRc; GetClientRect(hwnd, &winRc);
                int winW = winRc.right - winRc.left;
                int winH = winRc.bottom - winRc.top;
                POINT lbOrigin = {0,0};
                ClientToScreen(pdis->hwndItem, &lbOrigin);
                ScreenToClient(hwnd, &lbOrigin);
                int globalX = lbOrigin.x + inner.left;
                int globalY = lbOrigin.y + inner.top;
                if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
                if (winH <= 0) { RECT tr; GetClientRect(hwnd,&tr); winH = tr.bottom - tr.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
                if (!win32_draw_bg_image_slice(pdis->hDC, &inner, winW, winH, lbOrigin.x, lbOrigin.y)) {
                  if (EnableShaders && ShaderPreset != 0) {
                    win32_fill_rounded_rect_slice(pdis->hDC, &inner, COLOR_BG, winW, winH, globalX, globalY, innerR);
                  } else {
                    win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_BG, innerR);
                  }
                }
              } else {
                RECT winRcI; GetClientRect(hwnd, &winRcI);
                int iwW = winRcI.right - winRcI.left;
                int iwH = winRcI.bottom - winRcI.top;
                POINT lbOriginI = {0,0};
                ClientToScreen(pdis->hwndItem, &lbOriginI);
                ScreenToClient(hwnd, &lbOriginI);
                if (iwW <= 0) iwW = pdis->rcItem.right - pdis->rcItem.left;
                if (iwH <= 0) iwH = pdis->rcItem.bottom - pdis->rcItem.top + 600;
                if (!win32_draw_bg_image_slice(pdis->hDC, &inner, iwW, iwH, lbOriginI.x, lbOriginI.y)) {
                  win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_BG, innerR);
                }
              }
            }
          } else if (EnableShaders && ShaderPreset != 0) {
            if (isSelected) {
              if (!s_sel_brush) s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin);
              ScreenToClient(hwnd, &lbOrigin);
              int globalX = lbOrigin.x + pdis->rcItem.left;
              int globalY = lbOrigin.y + pdis->rcItem.top;
              if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
              if (winH <= 0) { RECT tr2; GetClientRect(hwnd,&tr2); winH = tr2.bottom - tr2.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
              if (!win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH, lbOrigin.x, lbOrigin.y)) {
                win32_draw_background_slice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, globalX, globalY);
              }
            }
          } else {
            if (isSelected) {
              if (!s_sel_brush) s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin);
              ScreenToClient(hwnd, &lbOrigin);
              if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
              if (winH <= 0) { RECT tr2; GetClientRect(hwnd,&tr2); winH = tr2.bottom - tr2.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
              if (!win32_draw_bg_image_slice(pdis->hDC, &pdis->rcItem, winW, winH, lbOrigin.x, lbOrigin.y)) {
                if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                FillRect(pdis->hDC, &pdis->rcItem, s_bg_brush);
              }
            }
          }
          if (UI_IsListBoxSelected(lb)) {
                  char measureBuffer[512];
                    strncpy(measureBuffer, lb->populated_cache[pdis->itemID], sizeof(measureBuffer) - 1);
                    measureBuffer[sizeof(measureBuffer) - 1] = '\0';
                    char* pos = strchr(measureBuffer, '|');
                    if (pos) {
                      *pos = '\0';
                    }
                  if (!FullTextNeedsKeyPress) {
                    if (isSelected && strlen(measureBuffer) >= 70) {
                      if (g_last_selected_index != pdis->itemID || WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) == NULL) {
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
                      FadeWindowData* msgData = (FadeWindowData*)GetWindowLongPtrA(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), GWLP_USERDATA);
                      bool isIndefinite = msgData && msgData->timeout == 6969.0;
                      if (!isIndefinite) {
                        KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                          DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                          FlintContext_Get()->window.hDisplayMessage = WH_NULL;
                      }
                    }
                  } 
                } 
           
            const char* textBuffer = "";
            if (pdis->itemID < (lb ? lb->populated_count : 0)) {
                textBuffer = lb->populated_cache[pdis->itemID];
            }
          
            if (pdis->itemID == -1 || pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          
            
            SetBkMode(pdis->hDC, TRANSPARENT);
            RECT textRect = pdis->rcItem;
            textRect.left += PROMPT_ITEM_TEXT_X;
          
            Platform_SetDrawContextWin32(pdis->hDC);
            if (FlintContext_Get()->prompt_callbacks.render != NULL) {
              return TRUE;
            } else {
                HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, g_win32_hMainFont);
                unsigned long ncol, bcol, pcol;
                int orig_idx = (int)pdis->itemID;
                int orig_cnt = lb ? lb->populated_count : 0;
                if (lb && (int)pdis->itemID >= 0 && pdis->itemID < (UINT)lb->populated_count) {
                  orig_idx = lb->matching_indices[pdis->itemID];
                  orig_cnt = lb->last_item_count > 0 ? lb->last_item_count : lb->populated_count;
                }
                Platform_GetItemColors(orig_idx, orig_cnt, &ncol, &bcol, &pcol);
                SetTextColor(pdis->hDC, ncol);
                DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
              SelectObject(pdis->hDC, hOldMainFont);
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
            imgDrawn = win32_draw_bg_image_slice(paintDC, &visRect, rect.right - rect.left, rect.bottom - rect.top, 0, 0);
          }
          if (!imgDrawn) {
            if (EnableShaders && ShaderPreset != 0) {
              win32_draw_background(paintDC, &rect);
            } else {
              if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                FillRect(paintDC, &ps.rcPaint, s_bg_brush);
            }
          }
          win32_draw_window_border(paintDC, 0, 0, rect.right, rect.bottom, NULL);
        }
        if (FlintContext_Get()->prompt_callbacks.render) {
            FlintContext_Get()->window.editbox_placed = false;
            extern PromptState* Platform_GetWin32PromptState(void);
            PromptState *state = Platform_GetWin32PromptState();
            if (state && state->render) {
                Checkbox_FrameBegin();
                Platform_SetDrawContextWin32(paintDC);
                state->render(state);
            }
        } else {
            extern PromptState* Platform_GetWin32PromptState(void);
            PromptState *estate = Platform_GetWin32PromptState();
            if (estate) {
                Platform_SetDrawContextWin32(paintDC);
                extern void Platform_DrawPromptEditbox(PromptState* state);
                Platform_DrawPromptEditbox(estate);
                if (PromptListBottomMargin() > 0 && estate->has_listbox) {
                    Platform_DrawPromptSeparator(rect.bottom - PromptListBottomMargin(), rect.right - rect.left);
                }
                Platform_SetDrawContextWin32(NULL);
            }
        }
      if (g_vim_mode != NULL && VimModeText) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          HFONT hOldFont = (HFONT)SelectObject(paintDC, g_win32_hMainFont);
          SetBkMode(paintDC, TRANSPARENT);
          if (g_current_mode == MODE_NORMAL) {
            SetTextColor(paintDC,COLOR_NORMAL_MODE_TEXT);
          } else if (g_current_mode == MODE_INSERT) {
            SetTextColor(paintDC,COLOR_INSERT_MODE_TEXT);
          } else {
            SetTextColor(paintDC,COLOR_VISUAL_MODE_TEXT);
          }
          SIZE sz;
          GetTextExtentPoint32W(paintDC, wmode, (int)wcslen(wmode), &sz);
          RECT modeRect;
          modeRect.right = rect.right - PROMPT_FOOTER_PAD;
          modeRect.left = modeRect.right - sz.cx;
          modeRect.top = rect.bottom - PROMPT_FOOTER_Y_FROM_BOTTOM;
          modeRect.bottom = modeRect.top + sz.cy;
          DrawTextW(paintDC, wmode, -1, &modeRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
          SelectObject(paintDC, hOldFont);
      }
      if (FlintContext_Get()->window.title != NULL && strlen(FlintContext_Get()->window.title) > 0 && WindowTitleText) {
        wchar_t wtitle[256];
          MultiByteToWideChar(CP_UTF8, 0, FlintContext_Get()->window.title, -1, wtitle, 256);
          HFONT hOldFont = (HFONT)SelectObject(paintDC, g_win32_hMainFont);
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
      }
        if (useRenderBuf) {
          BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
          SelectObject(memDC, oldBmp);
        }
        EndPaint(hwnd, &ps);
        break;
    }
                         break;              
    case WM_ERASEBKGND: {
      return TRUE;
    }
    case WM_DESTROY: {
      static const WORD held_modifiers[] = { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN };
        last_index = 0;
        if (lb) lb->selected_index = 0;
      
        selection_feature = FlintContext_Get()->window.current_feature;
        for (int i = 0; i < (int)ARRAY_LEN(held_modifiers); i++) {
          if (GetAsyncKeyState(held_modifiers[i]) & 0x8000) {
            INPUT up = {0};
              up.type = INPUT_KEYBOARD;
              up.ki.wVk = held_modifiers[i];
              up.ki.dwFlags = KEYEVENTF_KEYUP;
              SendInput(1, &up, sizeof(INPUT));
          }
        }
      if (s_sel_brush) { DeleteObject(s_sel_brush); s_sel_brush = NULL; }
        popup_free_back_buffer();
        s_repaint_queued = false;
        rainbow_init = false;
        break;
    }
    default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
              
    }
  { PROFILE_END_VAR(_pt, "PopupWndProc"); return 0; }
}
